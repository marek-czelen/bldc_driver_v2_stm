#ifndef SINUS_H
#define SINUS_H

#include <stdint.h>
#include <stdbool.h>
#include "main.h"

// ============================================================
//  SINUS — sterowanie sinusoidalne (SPWM)
//
//  Kąt elektryczny estymowany z czujników Halla z interpolacją.
//  Mapowanie kąta wyprowadzone z działającej komutacji BLOCK.
//  FPU Cortex-M4F użyte do sinf().
// ============================================================

void sinus_init(void);
void sinus_start(void);
void sinus_stop(void);
void sinus_set_duty(float duty);
float sinus_get_duty(void);

// Główna pętla sterowania — wywoływana co 1 ms
void sinus_update(void);

#endif // SINUS_H
