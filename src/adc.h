#ifndef ADC_H
#define ADC_H

#include <stdint.h>
#include "main.h"

// ============================================================
//  ADC — ADC1, pomiar prądów, napięć, temperatury
//  Kanały: PA0 (I_A), PA1 (I_B), PA4 (throttle),
//          PA5 (Vbat), PB0 (torque), PB1 (temp)
// ============================================================

void adc_init(void);

// Rozpoczęcie konwersji na wszystkich kanałach
void adc_start_conversion(void);

// Odczyt wyników (wartości surowe 12-bit)
uint16_t adc_get_raw(uint8_t channel);

// Odczyt prądu fazy w amperach
float adc_get_current(phase_t phase);

// Odczyt napięcia baterii
float adc_get_bus_voltage(void);

// Odczyt pozycji manetki (0.0 .. 1.0)
float adc_get_throttle(void);

// Temperatura MOSFET [°C]
float adc_get_temperature(void);

#endif // ADC_H
