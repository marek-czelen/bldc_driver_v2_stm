/**
 * pwm.c — Obsługa PWM (TIM1) dla 3-fazowego mostka BLDC
 *
 * High-side: PA8  (TIM1_CH1),  PA9  (TIM1_CH2),  PA10 (TIM1_CH3)  — PWM
 * Low-side:  PB13 (TIM1_CH1N), PB14 (TIM1_CH2N), PB15 (TIM1_CH3N) — PWM komplementarne
 * BKIN:      PB12 — sprzętowy BREAK (overcurrent, aktywny stan niski)
 *
 * Low-side realizowany sprzętowo przez wyjścia komplementarne TIM1:
 *   - LS ON  → CCxNE=1, CCRx=0, CCxNP=0 → OCxN = NOT(CNT<0) = zawsze HIGH
 *   - LS OFF → CCxNE=0, OISxN=0 → idle state = LOW
 *
 * Konfiguracja: 20 kHz, edge-aligned, active-high,
 *               dead-time 500 ns na wyjściach komplementarnych
 */

#include <stm32f4xx.h>
#include "pwm.h"
#include "config.h"

/* ── Zmienne lokalne ───────────────────────────────────── */
static float duty_[PHASE_COUNT];      // bieżące wypełnienie per faza
static bool  hs_enabled_[PHASE_COUNT]; // czy high-side PWM aktywny

/* ─────────────────────────────────────────────────────────
 * pwm_init — inicjalizacja TIM1
 * ──────────────────────────────────────────────────────── */
void pwm_init(void)
{
    /* ── Włącz zegary ────────────────────────────────── */
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN     /* PA8/9/10 */
                  | RCC_AHB1ENR_GPIOBEN;     /* PB12/13/14/15 */
    RCC->APB2ENR |= RCC_APB2ENR_TIM1EN;      /* TIM1 */
    __DSB();

    /* ── GPIOA: PA8, PA9, PA10 → AF1 (TIM1) ──────────── */
    /* PA8 = AF1, push-pull, high speed */
    GPIOA->MODER   &= ~(GPIO_MODER_MODER8_Msk
                      | GPIO_MODER_MODER9_Msk
                      | GPIO_MODER_MODER10_Msk);
    GPIOA->MODER   |=  (2UL << GPIO_MODER_MODER8_Pos)   /* AF */
                     | (2UL << GPIO_MODER_MODER9_Pos)
                     | (2UL << GPIO_MODER_MODER10_Pos);
    GPIOA->OSPEEDR |=  (3UL << GPIO_OSPEEDR_OSPEED8_Pos) /* very high */
                     | (3UL << GPIO_OSPEEDR_OSPEED9_Pos)
                     | (3UL << GPIO_OSPEEDR_OSPEED10_Pos);
    /* AF1 → TIM1/2 */
    GPIOA->AFR[1] &= ~(0xFUL << 0)    /* PA8  → AF1 */
                   & ~(0xFUL << 4)    /* PA9  → AF1 */
                   & ~(0xFUL << 8);   /* PA10 → AF1 */
    GPIOA->AFR[1] |=  (1UL << 0) | (1UL << 4) | (1UL << 8);

    /* ── GPIOB: PB13/14/15 → AF1 (TIM1_CH1N/CH2N/CH3N) ── */
    /* Wyjścia komplementarne TIM1 dla low-side FETów.
     * AF1 = TIM1, push-pull, very high speed, początkowo LOW (safely). */
    GPIOB->MODER   &= ~(GPIO_MODER_MODER13_Msk
                      | GPIO_MODER_MODER14_Msk
                      | GPIO_MODER_MODER15_Msk);
    GPIOB->MODER   |=  (2UL << GPIO_MODER_MODER13_Pos)  /* AF */
                     | (2UL << GPIO_MODER_MODER14_Pos)
                     | (2UL << GPIO_MODER_MODER15_Pos);
    GPIOB->OSPEEDR |=  (3UL << GPIO_OSPEEDR_OSPEED13_Pos) /* very high */
                     | (3UL << GPIO_OSPEEDR_OSPEED14_Pos)
                     | (3UL << GPIO_OSPEEDR_OSPEED15_Pos);
    GPIOB->OTYPER  &= ~((1UL << 13) | (1UL << 14) | (1UL << 15)); /* push-pull */
    GPIOB->PUPDR   &= ~(GPIO_PUPDR_PUPD13_Msk      /* no pull */
                      | GPIO_PUPDR_PUPD14_Msk
                      | GPIO_PUPDR_PUPD15_Msk);
    /* AF1 → TIM1 */
    GPIOB->AFR[1] &= ~(0xFUL << 20)   /* PB13 */
                   & ~(0xFUL << 24)   /* PB14 */
                   & ~(0xFUL << 28);  /* PB15 */
    GPIOB->AFR[1] |=  (1UL << 20) | (1UL << 24) | (1UL << 28);

    /* ── GPIOB: PB12 → AF1 (TIM1_BKIN), pull-up ─────────── */
    GPIOB->MODER   &= ~GPIO_MODER_MODER12_Msk;
    GPIOB->MODER   |=  (2UL << GPIO_MODER_MODER12_Pos);      /* AF */
    GPIOB->PUPDR   &= ~GPIO_PUPDR_PUPD12_Msk;
    GPIOB->PUPDR   |=  (1UL << GPIO_PUPDR_PUPD12_Pos);       /* pull-up */
    GPIOB->AFR[1]  &= ~(0xFUL << 16);                        /* PB12 */
    GPIOB->AFR[1]  |=  (1UL << 16);                          /* AF1 = TIM1 */

    /* ── TIM1 konfiguracja ───────────────────────────── */
    TIM1->CR1    = 0;                    /* najpierw wyłącz */
    TIM1->PSC    = 0;                    /* preskaler = 1 (96 MHz) */
    TIM1->ARR    = PWM_ARR;             /* 4799 → 20 kHz */
    TIM1->RCR    = 0;                    /* brak repetycji */

    /* ── TIM1_CCMR1/2: PWM mode 1, edge-aligned ──────── */
    /* CH1 (PA8) */
    TIM1->CCMR1  = (6UL << TIM_CCMR1_OC1M_Pos)   /* PWM mode 1 */
                 | TIM_CCMR1_OC1PE;               /* preload enable */
    /* CH2 (PA9) */
    TIM1->CCMR1 |= (6UL << TIM_CCMR1_OC2M_Pos)
                 |  TIM_CCMR1_OC2PE;
    /* CH3 (PA10) */
    TIM1->CCMR2  = (6UL << TIM_CCMR2_OC3M_Pos)
                 | TIM_CCMR2_OC3PE;

    /* ── TIM1_CCER: wyjścia początkowo WYŁĄCZONE ────────── */
    /* Polarity: HS active-high (CCxP=0), LS complementary active-high (CCxNP=0).
       Kanały HS i LS będą włączane przez block_commutate() per krok. */
    TIM1->CCER   = 0;                   /* CC1E/CC2E/CC3E = 0, CC1NE/CC2NE/CC3NE = 0 */

    /* ── TIM1_BDTR: dead-time + BREAK (BKIN active-low) ─ */
    /* BKP=0 => break aktywny stanem niskim na BKIN */
    TIM1->BDTR   = PWM_DTG_VAL          /* dead-time */
                 | TIM_BDTR_BKE         /* BREAK input enable */
                 | TIM_BDTR_MOE;        /* main output enable */

    /* ── TIM1_CR2: OISx=0, OISxN=0 (off-state = LOW, bezpieczny) ─ */
    TIM1->CR2    = 0;   /* OIS1=OIS2=OIS3=0, OIS1N=OIS2N=OIS3N=0 */

    /* ── Wstępnie CCR = 0 → wszystkie HS OFF ─────────── */
    TIM1->CCR1   = 0;
    TIM1->CCR2   = 0;
    TIM1->CCR3   = 0;

    /* ── Uruchom TIM1 ────────────────────────────────── */
    TIM1->EGR   |= TIM_EGR_UG;          /* update event (ładuje preload) */
    TIM1->CR1   |= TIM_CR1_CEN;         /* enable counter */

    /* stan początkowy */
    for (int i = 0; i < PHASE_COUNT; i++) {
        duty_[i]        = 0.0f;
        hs_enabled_[i]  = false;
    }
}

/* ─────────────────────────────────────────────────────────
 * pwm_set_duty — ustawienie wypełnienia high-side fazy
 * ──────────────────────────────────────────────────────── */
void pwm_set_duty(phase_t phase, float duty)
{
    if (phase >= PHASE_COUNT) return;
    if (duty < 0.0f) duty = 0.0f;
    if (duty > 1.0f) duty = 1.0f;

    duty_[phase] = duty;
    uint16_t ccr = (uint16_t)(duty * (float)(PWM_ARR + 1));

    if (duty < 0.01f) ccr = 0;   /* poniżej progu → OFF */

    switch (phase) {
    case PHASE_A: TIM1->CCR1 = ccr; break;
    case PHASE_B: TIM1->CCR2 = ccr; break;
    case PHASE_C: TIM1->CCR3 = ccr; break;
    default: break;
    }
}

/* ─────────────────────────────────────────────────────────
 * pwm_hs_enable — włączenie/wyłączenie high-side PWM
 * Przy włączaniu HS czyści również CCxNE (zapobiega shoot-through).
 * ──────────────────────────────────────────────────────── */
void pwm_hs_enable(phase_t phase, bool enable)
{
    if (phase >= PHASE_COUNT) return;
    hs_enabled_[phase] = enable;

    uint32_t cce_mask  = 0;
    uint32_t ccne_mask = 0;

    switch (phase) {
    case PHASE_A:
        cce_mask  = TIM_CCER_CC1E;
        ccne_mask = TIM_CCER_CC1NE;
        break;
    case PHASE_B:
        cce_mask  = TIM_CCER_CC2E;
        ccne_mask = TIM_CCER_CC2NE;
        break;
    case PHASE_C:
        cce_mask  = TIM_CCER_CC3E;
        ccne_mask = TIM_CCER_CC3NE;
        break;
    default: return;
    }

    if (enable) {
        /* Wyłącz LS komplementarny przed włączeniem HS (bezpieczeństwo) */
        TIM1->CCER &= ~ccne_mask;
        TIM1->CCER |= cce_mask;
    } else {
        TIM1->CCER &= ~cce_mask;
    }
}

/* ─────────────────────────────────────────────────────────
 * pwm_ls_enable — włączenie/wyłączenie low-side (TIM1_CHxN)
 *
 * LS ON:  CCxE=1, CCxNE=1, CCRx=0, CCxP=0, CCxNP=0
 *         → OCx  = LOW (HS FET bezpiecznie wyłączony)
 *         → OCxN = HIGH (LS FET włączony na 100%)
 *         Oba wyjścia aktywne — gwarantuje poprawne działanie
 *         wyjścia komplementarnego na STM32F4.
 *
 * LS OFF: CCxE=0, CCxNE=0 → oba wyjścia idle = LOW (OISx=OISxN=0)
 * ──────────────────────────────────────────────────────── */
void pwm_ls_enable(phase_t phase, bool enable)
{
    uint32_t cce_mask  = 0;
    uint32_t ccne_mask = 0;

    switch (phase) {
    case PHASE_A:
        cce_mask  = TIM_CCER_CC1E;
        ccne_mask = TIM_CCER_CC1NE;
        if (enable) TIM1->CCR1 = 0;    /* 0% duty → OC1=LOW, OC1N=HIGH */
        break;
    case PHASE_B:
        cce_mask  = TIM_CCER_CC2E;
        ccne_mask = TIM_CCER_CC2NE;
        if (enable) TIM1->CCR2 = 0;
        break;
    case PHASE_C:
        cce_mask  = TIM_CCER_CC3E;
        ccne_mask = TIM_CCER_CC3NE;
        if (enable) TIM1->CCR3 = 0;
        break;
    default:
        return;
    }

    if (enable) {
        /* Włącz oba wyjścia: główne (LOW) + komplementarne (HIGH).
         * Kolejność: najpierw CCR=0, potem oba enable.
         * Dead-time sprzętowy chroni przed shoot-through. */
        TIM1->CCER |= (cce_mask | ccne_mask);
    } else {
        TIM1->CCER &= ~(cce_mask | ccne_mask);
    }
}

/* ─────────────────────────────────────────────────────────
 * pwm_output_enable — globalne MOE
 * ──────────────────────────────────────────────────────── */
void pwm_output_enable(bool enable)
{
    if (enable) {
        TIM1->BDTR |= TIM_BDTR_MOE;
    } else {
        TIM1->BDTR &= ~TIM_BDTR_MOE;
    }
}

/* ─────────────────────────────────────────────────────────
 * pwm_all_off — wszystkie 6 wyjść OFF (stan bezpieczny)
 * ──────────────────────────────────────────────────────── */
void pwm_all_off(void)
{
    TIM1->BDTR &= ~TIM_BDTR_MOE;            /* MOE = 0 */

    /* Wyłącz wszystkie kanały HS i LS */
    TIM1->CCER &= ~(TIM_CCER_CC1E  | TIM_CCER_CC2E  | TIM_CCER_CC3E
                  | TIM_CCER_CC1NE | TIM_CCER_CC2NE | TIM_CCER_CC3NE);

    /* Wyzeruj wszystkie CCR (0% duty) */
    TIM1->CCR1 = 0;
    TIM1->CCR2 = 0;
    TIM1->CCR3 = 0;

    for (int i = 0; i < PHASE_COUNT; i++) {
        duty_[i]       = 0.0f;
        hs_enabled_[i] = false;
    }
}

/* ─────────────────────────────────────────────────────────
 * pwm_get_duty
 * ──────────────────────────────────────────────────────── */
float pwm_get_duty(phase_t phase)
{
    if (phase >= PHASE_COUNT) return 0.0f;
    return duty_[phase];
}
