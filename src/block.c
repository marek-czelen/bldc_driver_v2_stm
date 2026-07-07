/**
 * block.c — Sterowanie BLOCK (6-step / trapezoidalne)
 *
 * Komutacja na podstawie czujników Halla (PB5, PB8, PB9).
 *
 * Tablica komutacji (standardowa dla czujników 120°):
 *   Hall CBA | Krok | Fazy
 *   ─────────┼──────┼───────
 *    0 0 1   │  0   │ A+ B-
 *    0 1 1   │  1   │ A+ C-
 *    0 1 0   │  2   │ B+ C-
 *    1 1 0   │  3   │ B+ A-
 *    1 0 0   │  4   │ C+ A-
 *    1 0 1   │  5   │ C+ B-
 *
 * High-side: PWM (TIM1_CHx), Low-side: PWM komplementarne (TIM1_CHxN)
 */

#include <stm32f4xx.h>
#include <stddef.h>
#include "block.h"
#include "pwm.h"
#include "gpio.h"
#include "config.h"

/* ── Struktura opisująca stan wyjść dla jednego kroku ───── */
typedef struct {
    phase_t hs_phase;       /* która faza high-side PWM */
    phase_t ls_phase;       /* która faza low-side ON */
} block_step_t;

/* Tablica komutacji indeksowana stanem Halla (0..7) */
/* Wartość 0xFF = stan nieprawidłowy */
static const int8_t hall_to_step_[8] = {
    -1,     /* 000 — nieprawidłowy */
     0,     /* 001 — A+ B- */
     2,     /* 010 — B+ C- */
     1,     /* 011 — A+ C- */
     4,     /* 100 — C+ A- */
     5,     /* 101 — C+ B- */
     3,     /* 110 — B+ A- */
    -1      /* 111 — nieprawidłowy */
};

/* Tablica 6 kroków komutacji */
static const block_step_t step_table_[6] = {
    { PHASE_A, PHASE_B },   /* krok 0: A+ B- */
    { PHASE_A, PHASE_C },   /* krok 1: A+ C- */
    { PHASE_B, PHASE_C },   /* krok 2: B+ C- */
    { PHASE_B, PHASE_A },   /* krok 3: B+ A- */
    { PHASE_C, PHASE_A },   /* krok 4: C+ A- */
    { PHASE_C, PHASE_B },   /* krok 5: C+ B- */
};

/* ── Zmienne lokalne ───────────────────────────────────── */
static float   block_duty_     = 0.0f;
static int8_t  block_step_     = -1;   /* aktualny krok (-1 = brak) */
static int8_t  block_step_prev_ = -1;
static bool    block_running_  = false;

/* ─────────────────────────────────────────────────────────
 * block_init — inicjalizacja trybu BLOCK
 * ──────────────────────────────────────────────────────── */
void block_init(void)
{
    block_duty_     = 0.0f;
    block_step_     = -1;
    block_step_prev_ = -1;
    block_running_  = false;
}

/* ─────────────────────────────────────────────────────────
 * block_set_duty
 * ──────────────────────────────────────────────────────── */
void block_set_duty(float duty)
{
    if (duty < 0.0f) duty = 0.0f;
    if (duty > PWM_MAX_DUTY) duty = PWM_MAX_DUTY;
    block_duty_ = duty;
}

/* ─────────────────────────────────────────────────────────
 * block_set_speed — placeholder (wymaga PID)
 * ──────────────────────────────────────────────────────── */
void block_set_speed(float rpm)
{
    /* TODO: regulacja prędkości z PID */
    (void)rpm;
}

/* ─────────────────────────────────────────────────────────
 * block_commutate — wykonaj krok komutacji
 *
 * Wywoływana cyklicznie. Odczytuje stan Halla, wyznacza
 * krok i jeśli się zmienił — przełącza wyjścia.
 *
 * Zwraca true jeśli nastąpiła zmiana kroku.
 * ──────────────────────────────────────────────────────── */
bool block_commutate(void)
{
    if (!block_running_) return false;

    uint8_t hall = gpio_get_hall_state();
    int8_t new_step = hall_to_step_[hall];

    /* Nieprawidłowy stan Halla — nie zmieniaj wyjść */
    if (new_step < 0) return false;

    /* Ten sam krok → tylko aktualizuj duty */
    if (new_step == block_step_) {
        /* Aktualizacja PWM dla aktywnej fazy high-side */
        if (block_step_ >= 0) {
            pwm_set_duty(step_table_[block_step_].hs_phase, block_duty_);
        }
        return false;
    }

    /* ── Zmiana kroku! ───────────────────────────────── */
    block_step_prev_ = block_step_;
    block_step_      = new_step;

    const block_step_t *step = &step_table_[new_step];
    const block_step_t *prev = (block_step_prev_ >= 0)
                               ? &step_table_[block_step_prev_]
                               : NULL;

    /* ── Sekwencja przełączania ──────────────────────────
     * KLUCZOWE: przełączaj TYLKO fazy, które faktycznie się zmieniają.
     * Przy każdej komutacji 6-step jedna faza pozostaje tą samą stroną
     * (np. HS stały, zmienia się tylko LS — i odwrotnie). Tej fazy NIE
     * wolno wyłączać i od razu ponownie załączać: przenosi ona prąd
     * silnika, więc jej chwilowe wyłączenie wymusza przewodzenie diody
     * body przeciwnego FET-a, a ponowne załączenie daje twarde
     * odzyskiwanie tej diody (reverse recovery) = impuls prądowy typu
     * shoot-through na TEJ SAMEJ nodze. Impuls rośnie z wypełnieniem
     * i prędkością komutacji → niszczy FET-y bez ich nagrzewania. */

    /* 1. Wyłącz stary HS tylko jeśli nowy HS jest na innej fazie */
    if (prev && prev->hs_phase != step->hs_phase) {
        pwm_set_duty(prev->hs_phase, 0.0f);
        pwm_hs_enable(prev->hs_phase, false);
    }

    /* 2. Wyłącz stary LS tylko jeśli nowy LS jest na innej fazie */
    if (prev && prev->ls_phase != step->ls_phase) {
        pwm_ls_enable(prev->ls_phase, false);
    }

    /* 3. Nowy HS: zawsze zaktualizuj wypełnienie; załącz tylko przy zmianie fazy */
    pwm_set_duty(step->hs_phase, block_duty_);
    if (!prev || prev->hs_phase != step->hs_phase) {
        pwm_hs_enable(step->hs_phase, true);
    }

    /* 4. Nowy LS: załącz tylko przy zmianie fazy */
    if (!prev || prev->ls_phase != step->ls_phase) {
        pwm_ls_enable(step->ls_phase, true);
    }

    return true;
}

/* ─────────────────────────────────────────────────────────
 * block_stop — zatrzymanie silnika
 * ──────────────────────────────────────────────────────── */
void block_stop(void)
{
    block_running_ = false;

    /* Wyłącz wszystkie LS */
    pwm_ls_enable(PHASE_A, false);
    pwm_ls_enable(PHASE_B, false);
    pwm_ls_enable(PHASE_C, false);

    /* Wyłącz wszystkie HS */
    pwm_set_duty(PHASE_A, 0.0f);
    pwm_set_duty(PHASE_B, 0.0f);
    pwm_set_duty(PHASE_C, 0.0f);
    pwm_hs_enable(PHASE_A, false);
    pwm_hs_enable(PHASE_B, false);
    pwm_hs_enable(PHASE_C, false);

    block_step_      = -1;
    block_step_prev_ = -1;
}

/* ─────────────────────────────────────────────────────────
 * block_start — uruchomienie silnika
 * ──────────────────────────────────────────────────────── */
void block_start(void)
{
    block_running_  = true;
    block_step_     = -1;
    block_step_prev_ = -1;

    /* Pierwsza komutacja nastąpi w następnym wywołaniu
       block_commutate() */
}

/* ─────────────────────────────────────────────────────────
 * block_get_step
 * ──────────────────────────────────────────────────────── */
uint8_t block_get_step(void)
{
    return (block_step_ >= 0) ? (uint8_t)block_step_ : 0;
}

/* ─────────────────────────────────────────────────────────
 * block_get_duty
 * ──────────────────────────────────────────────────────── */
float block_get_duty(void)
{
    return block_duty_;
}
