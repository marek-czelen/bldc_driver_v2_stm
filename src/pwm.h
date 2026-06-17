#ifndef PWM_H
#define PWM_H

#include <stdint.h>
#include <stdbool.h>
#include "main.h"

// ============================================================
//  PWM — TIM1, 6 kanałów komplementarnych (3 pary high/low)
//  High-side: PA8/PA9/PA10  (TIM1_CH1/CH2/CH3,    AF1)
//  Low-side:  PB13/PB14/PB15 (TIM1_CH1N/CH2N/CH3N, AF1)
//  BKIN:      PB12           (TIM1_BKIN,            AF1)
//
//  Low-side sterowany sprzętowo:
//   LS ON:  CCxE=1 + CCxNE=1, CCRx=0 → OCx=LOW, OCxN=HIGH
//   LS OFF: CCxE=0 + CCxNE=0 → idle LOW (OISx=OISxN=0)
//  Wszystkie 6 wyjść na jednym timerze = synchronizacja
//  sprzętowa + dead-time + break input.
// ============================================================

void pwm_init(void);

// Ustawienie wypełnienia PWM dla high-side fazy (0.0 .. 1.0)
void pwm_set_duty(phase_t phase, float duty);

// Włączenie/wyłączenie konkretnego high-side PWM
void pwm_hs_enable(phase_t phase, bool enable);

// Włączenie/wyłączenie konkretnego low-side (GPIO)
void pwm_ls_enable(phase_t phase, bool enable);

// Globalne włączenie/wyłączenie wszystkich wyjść PWM (MOE)
void pwm_output_enable(bool enable);

// Ustawienie wszystkich 6 wyjść w stan bezpieczny (OFF)
void pwm_all_off(void);

// Wypełnienie w % dla debugu
float pwm_get_duty(phase_t phase);

#endif // PWM_H
