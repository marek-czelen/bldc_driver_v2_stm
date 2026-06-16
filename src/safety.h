#ifndef SAFETY_H
#define SAFETY_H

#include <stdint.h>
#include <stdbool.h>
#include "main.h"

// ============================================================
//  SAFETY — monitorowanie bezpieczeństwa
//  Overcurrent, overtemperature, under/overvoltage, brake
// ============================================================

void safety_init(void);

// Sprawdzenie wszystkich warunków bezpieczeństwa
// Wywoływane cyklicznie w pętli głównej
safety_error_t safety_check(void);

// Awaryjne zatrzymanie — wyłączenie PWM + SD
void safety_emergency_stop(safety_error_t error);

// Reset stanu awaryjnego
void safety_clear_fault(void);

// Czy system jest w stanie awarii?
bool safety_is_faulted(void);

// Tekstowy opis błędu
const char* safety_error_string(safety_error_t error);

#endif // SAFETY_H
