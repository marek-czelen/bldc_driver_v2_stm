/**
 * safety.c — Monitorowanie bezpieczeństwa
 *
 * Sprawdzane warunki:
 *  - Overcurrent (sprzętowy: LM393 → PB12)
 *  - Overcurrent (programowy: INA240 → ADC)
 *  - Overtemperature (NTC → ADC PB1)
 *  - Undervoltage (dzielnik VBAT → ADC PA5)
 *  - Overvoltage (dzielnik VBAT → ADC PA5)
 *  - Brake (GPIO PB10)
 */

#include <stm32f4xx.h>
#include "safety.h"
#include "adc.h"
#include "gpio.h"
#include "pwm.h"
#include "config.h"

/* ── Zmienne lokalne ───────────────────────────────────── */
static bool faulted_ = false;

/* ─────────────────────────────────────────────────────────
 * safety_init
 * ──────────────────────────────────────────────────────── */
void safety_init(void)
{
    faulted_ = false;
    g_safety_error = SAFETY_OK;
}

/* ─────────────────────────────────────────────────────────
 * safety_check — sprawdza wszystkie warunki
 * Wywołuj z pętli głównej co ~1 ms
 * ──────────────────────────────────────────────────────── */
safety_error_t safety_check(void)
{
    /* Jeśli już w stanie awarii — nie wychodź z niego */
    if (faulted_) return g_safety_error;

    /* ── Overcurrent sprzętowy (LM393 → PB12) ───────── */
    if (gpio_get_overcurrent()) {
        safety_emergency_stop(SAFETY_OVERCURRENT);
        return SAFETY_OVERCURRENT;
    }

    /* ── Overcurrent programowy (INA240 → ADC) ───────── */
    float i_a = adc_get_current(PHASE_A);
    float i_b = adc_get_current(PHASE_B);

    if (i_a > OVERCURRENT_THRESHOLD_A || i_a < -OVERCURRENT_THRESHOLD_A ||
        i_b > OVERCURRENT_THRESHOLD_A || i_b < -OVERCURRENT_THRESHOLD_A) {
        safety_emergency_stop(SAFETY_OVERCURRENT);
        return SAFETY_OVERCURRENT;
    }

    /* ── Overtemperature ─────────────────────────────── */
    float temp = adc_get_temperature();
    if (temp > OVERTEMP_THRESHOLD_C) {
        safety_emergency_stop(SAFETY_OVERTEMP);
        return SAFETY_OVERTEMP;
    }

    /* ── Napięcie baterii (tylko gdy silnik pracuje) ── */
    /* UVLO/OV nie blokują startu — pozwalają podłączyć baterię później */
    g_bus_voltage = adc_get_bus_voltage();

    if (g_driver_state == STATE_RUN) {
        if (g_bus_voltage < UNDERVOLTAGE_THRESHOLD && g_bus_voltage > 0.5f) {
            safety_emergency_stop(SAFETY_UNDERVOLTAGE);
            return SAFETY_UNDERVOLTAGE;
        }
        if (g_bus_voltage > OVERVOLTAGE_THRESHOLD) {
            safety_emergency_stop(SAFETY_OVERVOLTAGE);
            return SAFETY_OVERVOLTAGE;
        }
    }

    /* ── Hamulec ─────────────────────────────────────── */
    if (gpio_get_brake()) {
        safety_emergency_stop(SAFETY_BRAKE);
        return SAFETY_BRAKE;
    }

    return SAFETY_OK;
}

/* ─────────────────────────────────────────────────────────
 * safety_emergency_stop — awaryjne zatrzymanie
 * ──────────────────────────────────────────────────────── */
void safety_emergency_stop(safety_error_t error)
{
    faulted_       = true;
    g_safety_error = error;
    g_driver_state = STATE_FAULT;

    /* Wyłącz PWM i ustaw SD */
    pwm_all_off();
    gpio_set_sd(true);

    /* Sygnalizacja LED — szybkie miganie */
    /* (obsłużone w pętli głównej przez sprawdzenie faulted_) */
}

/* ─────────────────────────────────────────────────────────
 * safety_clear_fault — reset stanu awaryjnego
 * ──────────────────────────────────────────────────────── */
void safety_clear_fault(void)
{
    faulted_       = false;
    g_safety_error = SAFETY_OK;
    g_driver_state = STATE_IDLE;

    gpio_set_sd(false);
    pwm_output_enable(true);
}

/* ─────────────────────────────────────────────────────────
 * safety_is_faulted
 * ──────────────────────────────────────────────────────── */
bool safety_is_faulted(void)
{
    return faulted_;
}

/* ─────────────────────────────────────────────────────────
 * safety_error_string
 * ──────────────────────────────────────────────────────── */
const char* safety_error_string(safety_error_t error)
{
    switch (error) {
    case SAFETY_OK:             return "OK";
    case SAFETY_OVERCURRENT:    return "OVERCURRENT";
    case SAFETY_OVERTEMP:       return "OVERTEMP";
    case SAFETY_UNDERVOLTAGE:   return "UNDERVOLTAGE";
    case SAFETY_OVERVOLTAGE:    return "OVERVOLTAGE";
    case SAFETY_BRAKE:          return "BRAKE";
    default:                    return "UNKNOWN";
    }
}
