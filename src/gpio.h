#ifndef GPIO_H
#define GPIO_H

#include <stdint.h>
#include <stdbool.h>
#include "main.h"

// ============================================================
//  GPIO — obsługa wejść/wyjść cyfrowych
//  LED (PC13), SD (PB4), Hall (PB5/8/9),
//  Brake (PB10), Overcurrent (PB12)
// ============================================================

void gpio_init(void);

// LED
void gpio_led_on(void);
void gpio_led_off(void);
void gpio_led_toggle(void);

// Shutdown driverów (PB4)
void gpio_set_sd(bool enable);          // true = shutdown (SD=high)
bool gpio_get_sd(void);

// Odczyt czujników Halla — zwraca 3-bitową maskę (bit0=A, bit1=B, bit2=C)
uint8_t gpio_get_hall_state(void);

// Hamulec (PB10)
bool gpio_get_brake(void);

// Overcurrent z LM393 (PB12)
bool gpio_get_overcurrent(void);

#endif // GPIO_H
