#ifndef CONFIG_H
#define CONFIG_H

// ============================================================
//  BLDC Driver v2 — konfiguracja sprzętowa
//  MCU: STM32F411CEU6 (WeAct Black Pill), 96 MHz
// ============================================================

// ── Zegar systemowy ────────────────────────────────────────
#define SYSTEM_CLOCK_HZ         96000000UL
#define APB1_CLOCK_HZ           48000000UL
#define APB2_CLOCK_HZ           96000000UL

// ── PWM (TIM1) ─────────────────────────────────────────────
#define PWM_FREQ_HZ             20000UL          // 20 kHz
#define PWM_ARR                 (SYSTEM_CLOCK_HZ / PWM_FREQ_HZ - 1)  // 4799
#define PWM_DEADTIME_NS         1000            // dead-time 1 µs
// TIM1_BDTR DTG @ 96 MHz (CKD=0 → t_dtg = 1/96 MHz ≈ 10.417 ns/tick):
// Zakres liniowy (DTG[7]=0): deadtime = DTG[7:0] * t_dtg, DTG = 0..127 (maks. ≈ 1.32 µs)
// DTG = deadtime_ns * f_CK[MHz] / 1000 = 1000 * 96 / 1000 = 96 (0x60)
#define PWM_DTG_VAL             ((PWM_DEADTIME_NS * (SYSTEM_CLOCK_HZ / 1000000UL)) / 1000UL)

// ── Piny PWM (faza A / B / C) ─────────────────────────────
// High-side: PA8, PA9, PA10 — TIM1_CH1, CH2, CH3 (PWM)
// Low-side:  PB13, PB14, PB15 — TIM1_CH1N, CH2N, CH3N (komplementarne, sprzętowe)
#define PWM_PORT_HIGH           GPIOA
#define PWM_PORT_LOW            GPIOB           /* już nieużywane — LS przez TIM1_CHxN */
#define PWM_PIN_AH              8       // PA8
#define PWM_PIN_BH              9       // PA9
#define PWM_PIN_CH              10      // PA10
#define PWM_PIN_AL              13      // PB13 = TIM1_CH1N
#define PWM_PIN_BL              14      // PB14 = TIM1_CH2N
#define PWM_PIN_CL              15      // PB15 = TIM1_CH3N

// ── ADC ────────────────────────────────────────────────────
#define ADC_CH_CURRENT_A        0       // PA0 — INA240 #1
#define ADC_CH_CURRENT_B        1       // PA1 — INA240 #2
#define ADC_CH_THROTTLE         4       // PA4 — manetka
#define ADC_CH_BATTERY_V        5       // PA5 — dzielnik VBAT
#define ADC_CH_TORQUE           8       // PB0 — moment
#define ADC_CH_FET_TEMP         9       // PB1 — NTC temperatura

// Wzmocnienie INA240A2D: 50 V/V, R_shunt = 0.001 Ω → I = Vout / 50
#define INA240_GAIN             50.0f
#define SHUNT_RESISTANCE        0.001f
// Napięcie referencyjne INA240 = 3.3V / 2 = 1.65V → offset ADC = 2048 (12-bit)
#define ADC_OFFSET              2048
#define ADC_VREF                3.3f
#define ADC_RESOLUTION          4096.0f

// Dzielnik baterii: 1M (do VBAT) i 33k (do GND)
// Vadc = Vbat * (R_LOW / (R_HIGH + R_LOW))
// Vbat = Vadc * ((R_HIGH + R_LOW) / R_LOW)
#define VBAT_R_HIGH_OHM         1000000.0f
#define VBAT_R_LOW_OHM          33000.0f
#define VBAT_DIVIDER            ((VBAT_R_HIGH_OHM + VBAT_R_LOW_OHM) / VBAT_R_LOW_OHM)

// ── GPIO ───────────────────────────────────────────────────
#define LED_PORT                GPIOC
#define LED_PIN                 13              // PC13, active-low
#define SD_PORT                 GPIOB
#define SD_PIN                  4               // PB4, shutdown EG2113D
#define BRAKE_PORT              GPIOB
#define BRAKE_PIN               10              // PB10, hamulec
#define OVERCURRENT_PORT        GPIOB
#define OVERCURRENT_PIN         12              // PB12, overcurrent z LM393
#define HALL_PORT               GPIOB
#define HALL_A_PIN              5               // PB5
#define HALL_B_PIN              8               // PB8
#define HALL_C_PIN              9               // PB9
#define HALL_MASK               ((1<<5)|(1<<8)|(1<<9))

// ── Bezpieczeństwo ────────────────────────────────────────
#define OVERCURRENT_THRESHOLD_A 30.0f           // próg prądowy [A]
#define OVERTEMP_THRESHOLD_C    100.0f          // próg temperaturowy [°C]
#define UNDERVOLTAGE_THRESHOLD  10.0f           // napięcie minimalne [V]
#define OVERVOLTAGE_THRESHOLD   60.0f           // napięcie maksymalne [V]

// ── Częstotliwość pętli sterowania ────────────────────────
#define CONTROL_LOOP_HZ         1000            // 1 kHz
#define LED_BLINK_MS            500

// ── Limity testowe (zmień po zweryfikowaniu działania) ────
 //#define PWM_MAX_DUTY         0.20f           // maks. 20% na pierwszy test
#define PWM_MAX_DUTY            1.00f           // pełny zakres

#endif // CONFIG_H
