/**
 * sinus.c — Sterowanie sinusoidalne BLDC (SPWM)
 *
 * Zasada działania:
 *   Mapowanie kąta elektrycznego wyprowadzone BEZPOŚREDNIO z działającej
 *   komutacji BLOCK. W BLOCK sekwencja forward to:
 *
 *     Hall 001 → A+B-  (krok 0)
 *     Hall 011 → A+C-  (krok 1)
 *     Hall 010 → B+C-  (krok 2)
 *     Hall 110 → B+A-  (krok 3)
 *     Hall 100 → C+A-  (krok 4)
 *     Hall 101 → C+B-  (krok 5)
 *
 *   Dla sinusa definiuję θ_rotor tak, aby sinf(θ + 60°) generował
 *   te same wektory napięciowe co BLOCK:
 *     - sin(60°)=0.866 na A, sin(-60°)=-0.866 na B → A+B-  ✓
 *
 *   Kąt rośnie w kierunku forward (001→011→010→110→100→101).
 *   Interpolacja liniowa między zmianami Halla.
 *
 * Bezpieczeństwo:
 *   - Dead-time 1 µs sprzętowy (TIM1)
 *   - Break input (PB12 / TIM1_BKIN)
 *   - Modulacja ograniczona do PWM_MAX_DUTY
 *   - Brak Hall >200 ms → zatrzymanie
 *   - safety_check() w pętli głównej nadzoruje prąd/napięcie/temp
 */

#include <stm32f4xx.h>
#include <math.h>
#include "sinus.h"
#include "pwm.h"
#include "gpio.h"
#include "config.h"

/* ══════════════════════════════════════════════════════════
 * Stałe
 * ══════════════════════════════════════════════════════════ */

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

#define DEG2RAD(d) ((d) * (M_PI / 180.0f))

/* Kąt wyprzedzenia: 60° daje dokładnie takie same wektory
 * jak BLOCK przy przejściach Halla — gwarantuje moment. */
#define ADVANCE_DEG 60.0f

/* Timeout: brak zmiany Halla >200 ms → zatrzymanie */
#define HALL_TIMEOUT_MS 200

/* ══════════════════════════════════════════════════════════
 * Mapowanie Hall → kąt wirnika (θ_rotor)
 *
 * Sekwencja forward BLOCK: 001→011→010→110→100→101
 * Przypisuję kąty co 60°:
 *   Hall 001 → 0°, Hall 011 → 60°, Hall 010 → 120°,
 *   Hall 110 → 180°, Hall 100 → 240°, Hall 101 → 300°
 *
 * Tablica indeksowana wartością 3-bitową Halla.
 * ══════════════════════════════════════════════════════════ */
static const float hall_angle_[8] = {
    [0] =   0.0f,   /* 000 — błąd, nie używane */
    [1] =   0.0f,   /* 001 */
    [2] = 120.0f,   /* 010 */
    [3] =  60.0f,   /* 011 */
    [4] = 240.0f,   /* 100 */
    [5] = 300.0f,   /* 101 */
    [6] = 180.0f,   /* 110 */
    [7] =   0.0f    /* 111 — błąd, nie używane */
};

/* ══════════════════════════════════════════════════════════
 * Zmienne stanu
 * ══════════════════════════════════════════════════════════ */

static float    modulation_;        /* współczynnik modulacji 0..PWM_MAX_DUTY */
static float    angle_deg_;         /* bieżący kąt elektryczny [0, 360) */
static bool     running_;

static uint8_t  last_hall_;         /* ostatni stan Halla */
static uint32_t last_hall_tick_;    /* czas ostatniej zmiany Halla [ms] */
static uint32_t hall_period_ms_;    /* czas ostatniego sektora [ms] */
static bool     speed_valid_;       /* czy mamy ważny pomiar prędkości */

/* ══════════════════════════════════════════════════════════
 * sinus_init
 * ══════════════════════════════════════════════════════════ */
void sinus_init(void)
{
    modulation_    = 0.0f;
    angle_deg_     = 0.0f;
    running_       = false;
    last_hall_     = 0;
    last_hall_tick_ = 0;
    hall_period_ms_ = 0;
    speed_valid_   = false;
}

/* ══════════════════════════════════════════════════════════
 * sinus_start
 *
 * Odczytuje Hall, ustawia kąt, włącza 6 kanałów TIM1.
 * ══════════════════════════════════════════════════════════ */
void sinus_start(void)
{
    uint8_t hall = gpio_get_hall_state();

    /* Początkowy kąt — środek bieżącego sektora */
    if (hall >= 1 && hall <= 6) {
        angle_deg_ = hall_angle_[hall] + 30.0f;
        if (angle_deg_ >= 360.0f) angle_deg_ -= 360.0f;
    } else {
        angle_deg_ = 0.0f;
    }

    last_hall_      = hall;
    last_hall_tick_ = sys_tick_ms;
    hall_period_ms_ = 0;
    speed_valid_    = false;
    running_        = true;

    /* Wyczyść kanały, ustaw 50% (neutralne), włącz wszystkie 6 */
    TIM1->CCER &= ~(TIM_CCER_CC1E | TIM_CCER_CC1NE
                   | TIM_CCER_CC2E | TIM_CCER_CC2NE
                   | TIM_CCER_CC3E | TIM_CCER_CC3NE);

    pwm_set_duty(PHASE_A, 0.5f);
    pwm_set_duty(PHASE_B, 0.5f);
    pwm_set_duty(PHASE_C, 0.5f);

    TIM1->CCER |= (TIM_CCER_CC1E | TIM_CCER_CC1NE
                 | TIM_CCER_CC2E | TIM_CCER_CC2NE
                 | TIM_CCER_CC3E | TIM_CCER_CC3NE);
}

/* ══════════════════════════════════════════════════════════
 * sinus_stop
 * ══════════════════════════════════════════════════════════ */
void sinus_stop(void)
{
    running_ = false;

    TIM1->CCER &= ~(TIM_CCER_CC1E | TIM_CCER_CC1NE
                   | TIM_CCER_CC2E | TIM_CCER_CC2NE
                   | TIM_CCER_CC3E | TIM_CCER_CC3NE);
    TIM1->CCR1 = 0;
    TIM1->CCR2 = 0;
    TIM1->CCR3 = 0;

    modulation_  = 0.0f;
    speed_valid_ = false;
}

/* ══════════════════════════════════════════════════════════
 * sinus_set_duty / sinus_get_duty
 * ══════════════════════════════════════════════════════════ */
void sinus_set_duty(float duty)
{
    if (duty < 0.0f) duty = 0.0f;
    if (duty > PWM_MAX_DUTY) duty = PWM_MAX_DUTY;
    modulation_ = duty;
}

float sinus_get_duty(void)
{
    return modulation_;
}

/* ══════════════════════════════════════════════════════════
 * sinus_update — główna pętla (1 kHz)
 *
 * 1. Odczytaj Hall
 * 2. Jeśli zmiana → zmierz prędkość, skoryguj kąt
 * 3. Jeśli brak zmiany → interpoluj kąt ze zmierzonej prędkości
 * 4. Oblicz 3 sinusoidy (z advance 60°)
 * 5. Wpisz do CCR
 * ══════════════════════════════════════════════════════════ */
void sinus_update(void)
{
    if (!running_) return;

    uint32_t now = sys_tick_ms;
    uint8_t  hall = gpio_get_hall_state();

    /* ── Walidacja Halla ─────────────────────────────── */
    if (hall == 0 || hall == 7) {
        /* Nieprawidłowy stan — trzymaj ostatni kąt */
        if ((now - last_hall_tick_) > HALL_TIMEOUT_MS) {
            sinus_stop();
            g_driver_state = STATE_FAULT;
        }
        return;
    }

    /* ── Wykrycie zmiany Halla ────────────────────────── */
    if (hall != last_hall_) {
        uint32_t dt = now - last_hall_tick_;

        if (dt >= 2) {   /* filtr: ignoruj <2 ms (bounce) */
            hall_period_ms_ = dt;
            speed_valid_    = true;

            /* Snap kąta do początku nowego sektora */
            angle_deg_ = hall_angle_[hall];

            last_hall_tick_ = now;
        }
        last_hall_ = hall;

    } else {
        /* ── Interpolacja kąta między Hallami ────────── */
        if (speed_valid_ && hall_period_ms_ > 0) {
            /* Prędkość = 60° / hall_period_ms_ [deg/ms] */
            float deg_per_ms = 60.0f / (float)hall_period_ms_;
            uint32_t elapsed = now - last_hall_tick_;
            float interp = deg_per_ms * (float)elapsed;

            /* Ogranicz interpolację do 60° (nie wybiega poza sektor) */
            if (interp > 59.0f) interp = 59.0f;

            angle_deg_ = hall_angle_[hall] + interp;
        }
        /* Jeśli brak pomiaru prędkości (postój) — kąt stoi na snap.
         * Napięcia sinusoidalne przy stałym kącie tworzą stały wektor
         * → moment na wirnik → wirnik się obraca → Hall się zmienia
         * → sekwencja naturalna jak w BLOCK. */

        /* ── Timeout Halla ───────────────────────────── */
        if ((now - last_hall_tick_) > HALL_TIMEOUT_MS) {
            sinus_stop();
            g_driver_state = STATE_FAULT;
            return;
        }
    }

    /* ── Normalizacja kąta ───────────────────────────── */
    if (angle_deg_ >= 360.0f) angle_deg_ -= 360.0f;
    if (angle_deg_ < 0.0f)    angle_deg_ += 360.0f;

    /* ══════════════════════════════════════════════════════
     * Generacja 3-fazowego sinusa
     *
     * θ_stator = θ_rotor + ADVANCE (60°)
     * v_A = 0.5 + 0.5 * m * sin(θ_stator)
     * v_B = 0.5 + 0.5 * m * sin(θ_stator - 120°)
     * v_C = 0.5 + 0.5 * m * sin(θ_stator - 240°)
     * ══════════════════════════════════════════════════════ */
    float theta = DEG2RAD(angle_deg_ + ADVANCE_DEG);

    float sa = sinf(theta);
    float sb = sinf(theta - DEG2RAD(120.0f));
    float sc = sinf(theta - DEG2RAD(240.0f));

    /* ── Wstrzykiwanie 3-ciej harmonicznej (midpoint) ── */
    float vmin = sa;
    if (sb < vmin) vmin = sb;
    if (sc < vmin) vmin = sc;
    float vmax = sa;
    if (sb > vmax) vmax = sb;
    if (sc > vmax) vmax = sc;
    float offset = (vmin + vmax) * 0.5f;

    sa -= offset;
    sb -= offset;
    sc -= offset;

    /* Po odjęciu offsetu max(|s|) ≈ 0.866; skaluj do [-1, 1] */
    float scale = 1.1547005f;  /* 2/√3 */
    sa *= scale;
    sb *= scale;
    sc *= scale;

    /* ── Duty wokół 50% ─────────────────────────────── */
    float half_m = modulation_ * 0.5f;
    float da = 0.5f + half_m * sa;
    float db = 0.5f + half_m * sb;
    float dc = 0.5f + half_m * sc;

    /* Clamp do [0, 1] — NIE do PWM_MAX_DUTY (to ogranicza modulację) */
    if (da < 0.0f) da = 0.0f;
    if (da > 1.0f) da = 1.0f;
    if (db < 0.0f) db = 0.0f;
    if (db > 1.0f) db = 1.0f;
    if (dc < 0.0f) dc = 0.0f;
    if (dc > 1.0f) dc = 1.0f;

    /* ── Zapis do PWM ────────────────────────────────── */
    pwm_set_duty(PHASE_A, da);
    pwm_set_duty(PHASE_B, db);
    pwm_set_duty(PHASE_C, dc);
}
