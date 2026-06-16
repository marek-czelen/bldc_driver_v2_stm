/**
 * adc.c — Obsługa ADC1
 *
 * Kanały:
 *   PA0 — prąd fazy A (INA240A2D)       [ADC_CH0]
 *   PA1 — prąd fazy B (INA240A2D)       [ADC_CH1]
 *   PA4 — manetka/throttle              [ADC_CH4]
 *   PA5 — napięcie baterii (dzielnik)   [ADC_CH5]
 *   PB0 — moment obrotowy               [ADC_CH8]
 *   PB1 — temperatura FET (NTC)         [ADC_CH9]
 *
 * Używa trybu scan + single conversion.
 */

#include <stm32f4xx.h>
#include <math.h>
#include "adc.h"
#include "config.h"

/* ── Bufor wyników (6 kanałów) ──────────────────────────── */
#define ADC_NUM_CHANNELS   6
static volatile uint16_t adc_raw_[ADC_NUM_CHANNELS];

/* Kolejność skanowania (SQR3..SQR1) */
static const uint8_t adc_seq_[ADC_NUM_CHANNELS] = {
    ADC_CH_CURRENT_A,   /* SQ0 */
    ADC_CH_CURRENT_B,   /* SQ1 */
    ADC_CH_THROTTLE,    /* SQ2 */
    ADC_CH_BATTERY_V,   /* SQ3 */
    ADC_CH_TORQUE,      /* SQ4 */
    ADC_CH_FET_TEMP     /* SQ5 */
};

/* ─────────────────────────────────────────────────────────
 * adc_init — inicjalizacja ADC1
 * ──────────────────────────────────────────────────────── */
void adc_init(void)
{
    /* ── Zegary ──────────────────────────────────────── */
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN
                  | RCC_AHB1ENR_GPIOBEN;
    RCC->APB2ENR |= RCC_APB2ENR_ADC1EN;
    __DSB();

    /* ── GPIO: PA0, PA1, PA4, PA5 → analog ────────────── */
    GPIOA->MODER |= GPIO_MODER_MODER0_Msk     /* analog */
                  | GPIO_MODER_MODER1_Msk
                  | GPIO_MODER_MODER4_Msk
                  | GPIO_MODER_MODER5_Msk;

    /* ── GPIO: PB0, PB1 → analog ──────────────────────── */
    GPIOB->MODER |= GPIO_MODER_MODER0_Msk
                  | GPIO_MODER_MODER1_Msk;

    /* ── ADC preskaler: APB2=96 MHz → /4 = 24 MHz (max 36 MHz) ─ */
    /* ADC_CCR ADCPRE: 00=/2, 01=/4, 10=/6, 11=/8 */
    ADC->CCR  &= ~ADC_CCR_ADCPRE;           /* wyczyść bity preskalera */
    ADC->CCR  |=  (1UL << ADC_CCR_ADCPRE_Pos); /* /4 → 24 MHz */

    /* ── ADC1: konfiguracja ───────────────────────────── */
    ADC1->CR1 = 0;
    ADC1->CR2 = 0;

    /* 12-bit resolution, scan mode, EOC after each conversion */
    ADC1->CR1 = ADC_CR1_SCAN;               /* scan mode */

    /* Kolejność kanałów w trybie scan */
    ADC1->SQR1 = ((ADC_NUM_CHANNELS - 1) << ADC_SQR1_L_Pos); /* L = 6-1 = 5 */

    ADC1->SQR3  = (adc_seq_[0] << 0)
                | (adc_seq_[1] << 5)
                | (adc_seq_[2] << 10)
                | (adc_seq_[3] << 15)
                | (adc_seq_[4] << 20)
                | (adc_seq_[5] << 25);

    /* Sample time: 480 cykli ADC (480/24 MHz = 20 µs) */
    /* ADC_SMPR2: kanały 0..9 */
    ADC1->SMPR2 = (7UL << 0)               /* CH0: 480 cycles */
                | (7UL << 3)               /* CH1 */
                | (7UL << 12)              /* CH4 */
                | (7UL << 15)              /* CH5 */
                | (7UL << 24)              /* CH8 */
                | (7UL << 27);             /* CH9 */

    /* Bez DMA — odczyt programowy po każdej konwersji (EOC) */
    ADC1->CR2 |= ADC_CR2_EOCS;             /* EOC po każdej konwersji */

    /* ── Włącz ADC i poczekaj na stabilizację ────────── */
    /* Na STM32F4 kalibracja ADC jest automatyczna po ustawieniu ADON=1.
     * Czas stabilizacji t_STAB ≈ 20 µs @ 24 MHz ADC clock. */
    ADC1->CR2 |= ADC_CR2_ADON;
    for (volatile int i = 0; i < 10000; i++) { __NOP(); }  /* ~100 µs */
}

/* ─────────────────────────────────────────────────────────
 * adc_start_conversion — rozpocznij skanowanie wszystkich
 *                        kanałów i zaczekaj na wyniki
 * ──────────────────────────────────────────────────────── */
void adc_start_conversion(void)
{
    /* Wyczyść flagę EOC */
    ADC1->SR &= ~ADC_SR_EOC;

    /* Start konwersji */
    ADC1->CR2 |= ADC_CR2_SWSTART;

    /* Czekaj na zakończenie wszystkich kanałów */
    for (int i = 0; i < ADC_NUM_CHANNELS; i++) {
        while (!(ADC1->SR & ADC_SR_EOC)) {
            __NOP();
        }
        adc_raw_[i] = ADC1->DR;
    }
}

/* ─────────────────────────────────────────────────────────
 * adc_get_raw — odczyt surowej wartości ADC
 * ──────────────────────────────────────────────────────── */
uint16_t adc_get_raw(uint8_t channel)
{
    /* Znajdź indeks w sekwencji */
    for (int i = 0; i < ADC_NUM_CHANNELS; i++) {
        if (adc_seq_[i] == channel) {
            return adc_raw_[i];
        }
    }
    return 0;
}

/* ─────────────────────────────────────────────────────────
 * adc_get_current — prąd fazy w amperach (A lub B)
 * I = (Vout - Vref/2) / (GAIN * Rshunt)
 * Vout = raw * VREF / 4096
 * ──────────────────────────────────────────────────────── */
float adc_get_current(phase_t phase)
{
    uint8_t ch = (phase == PHASE_A) ? ADC_CH_CURRENT_A : ADC_CH_CURRENT_B;
    uint16_t raw = adc_get_raw(ch);

    float vout = (float)raw * ADC_VREF / ADC_RESOLUTION;
    float vref_half = ADC_VREF * 0.5f;           /* INA240 offset = Vref/2 */
    float current = (vout - vref_half) / (INA240_GAIN * SHUNT_RESISTANCE);
    return current;
}

float adc_get_current_a(void) { return adc_get_current(PHASE_A); }
float adc_get_current_b(void) { return adc_get_current(PHASE_B); }

/* ─────────────────────────────────────────────────────────
 * adc_get_current_c — prąd fazy C wyliczony z Kirchhoffa
 * Ia + Ib + Ic = 0  →  Ic = -(Ia + Ib)
 * ──────────────────────────────────────────────────────── */
float adc_get_current_c(void)
{
    float ia = adc_get_current_a();
    float ib = adc_get_current_b();
    return -(ia + ib);
}

/* ─────────────────────────────────────────────────────────
 * adc_get_bus_voltage — napięcie baterii
 * Vbat = Vadc * ((R_HIGH + R_LOW) / R_LOW)
 * Dla 1M/33k: mnożnik ≈ 31.303
 * ──────────────────────────────────────────────────────── */
float adc_get_bus_voltage(void)
{
    uint16_t raw = adc_get_raw(ADC_CH_BATTERY_V);
    float v_measured = (float)raw * ADC_VREF / ADC_RESOLUTION;
    return v_measured * VBAT_DIVIDER;
}

/* ─────────────────────────────────────────────────────────
 * adc_get_throttle — pozycja manetki 0.0 .. 1.0
 * ──────────────────────────────────────────────────────── */
float adc_get_throttle(void)
{
    uint16_t raw = adc_get_raw(ADC_CH_THROTTLE);
    float v = (float)raw * ADC_VREF / ADC_RESOLUTION;
    /* Zakres: 0.5V (0%) .. 4.5V (100%) — typowe dla manetki */
    float throttle = (v - 0.5f) / (4.5f - 0.5f);
    if (throttle < 0.0f) throttle = 0.0f;
    if (throttle > 1.0f) throttle = 1.0f;
    return throttle;
}

/* ─────────────────────────────────────────────────────────
 * adc_get_temperature — temperatura FET z NTC
 * (uproszczona aproksymacja — do skalibrowania)
 * ──────────────────────────────────────────────────────── */
float adc_get_temperature(void)
{
    uint16_t raw = adc_get_raw(ADC_CH_FET_TEMP);
    float v = (float)raw * ADC_VREF / ADC_RESOLUTION;

    /* Prosty model NTC 10k, beta=3950, dzielnik 10k do 3.3V */
    if (v < 0.01f) v = 0.01f;
    float r_ntc = 10000.0f * (ADC_VREF / v - 1.0f);

    /* Równanie Steinharta-Harta uproszczone (beta) */
    float temp_k = 1.0f / (1.0f / 298.15f + (1.0f / 3950.0f) * logf(r_ntc / 10000.0f));
    float temp_c = temp_k - 273.15f;

    return temp_c;
}
