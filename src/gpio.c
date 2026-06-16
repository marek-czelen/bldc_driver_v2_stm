/**
 * gpio.c — Obsługa GPIO
 *
 * Wejścia:  Hall A/B/C (PB5/8/9), Brake (PB10), Overcurrent (PB12)
 * Wyjścia:  LED (PC13, active-low), SD (PB4, shutdown driverów)
 */

#include <stm32f4xx.h>
#include "gpio.h"
#include "config.h"

/* ─────────────────────────────────────────────────────────
 * gpio_init — inicjalizacja wszystkich GPIO
 * ──────────────────────────────────────────────────────── */
void gpio_init(void)
{
    /* ── Zegary ──────────────────────────────────────── */
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOCEN
                  | RCC_AHB1ENR_GPIOBEN;
    __DSB();

    /* ── PC13: LED (output, push-pull) ───────────────── */
    GPIOC->MODER   &= ~GPIO_MODER_MODER13_Msk;
    GPIOC->MODER   |=  (1UL << GPIO_MODER_MODER13_Pos);  /* output */
    GPIOC->OTYPER  &= ~GPIO_OTYPER_OT13;                  /* push-pull */
    GPIOC->OSPEEDR |=  (2UL << GPIO_OSPEEDR_OSPEED13_Pos); /* medium */
    GPIOC->BSRR     =  GPIO_BSRR_BS13;                     /* LED OFF (active-low) */

    /* ── PB4: SD (output, push-pull) ─────────────────── */
    GPIOB->MODER   &= ~GPIO_MODER_MODER4_Msk;
    GPIOB->MODER   |=  (1UL << GPIO_MODER_MODER4_Pos);   /* output */
    GPIOB->OSPEEDR |=  (2UL << GPIO_OSPEEDR_OSPEED4_Pos);
    GPIOB->BSRR     =  GPIO_BSRR_BR4;                     /* SD = LOW (drivery aktywne) */

    /* ── PB5, PB8, PB9: Hall A/B/C (input, pull-up) ──── */
    GPIOB->MODER   &= ~(GPIO_MODER_MODER5_Msk
                      | GPIO_MODER_MODER8_Msk
                      | GPIO_MODER_MODER9_Msk);
    /* input (00) — już wyzerowane */
    GPIOB->PUPDR   &= ~(GPIO_PUPDR_PUPD5_Msk
                      | GPIO_PUPDR_PUPD8_Msk
                      | GPIO_PUPDR_PUPD9_Msk);
    GPIOB->PUPDR   |=  (1UL << GPIO_PUPDR_PUPD5_Pos)    /* pull-up */
                     | (1UL << GPIO_PUPDR_PUPD8_Pos)
                     | (1UL << GPIO_PUPDR_PUPD9_Pos);

    /* ── PB10: Brake (input, pull-up) ────────────────── */
    GPIOB->MODER   &= ~GPIO_MODER_MODER10_Msk;
    GPIOB->PUPDR   &= ~GPIO_PUPDR_PUPD10_Msk;
    GPIOB->PUPDR   |=  (1UL << GPIO_PUPDR_PUPD10_Pos);   /* pull-up */

    /* ── PB12: Overcurrent (input, pull-up) ───────────── */
    GPIOB->MODER   &= ~GPIO_MODER_MODER12_Msk;
    GPIOB->PUPDR   &= ~GPIO_PUPDR_PUPD12_Msk;
    GPIOB->PUPDR   |=  (1UL << GPIO_PUPDR_PUPD12_Pos);   /* pull-up */
}

/* ── LED ───────────────────────────────────────────────── */
void gpio_led_on(void)  { GPIOC->BSRR = GPIO_BSRR_BR13; }  /* active-low → BR = ON */
void gpio_led_off(void) { GPIOC->BSRR = GPIO_BSRR_BS13; }
void gpio_led_toggle(void)
{
    if (GPIOC->ODR & GPIO_ODR_OD13_Msk)
        GPIOC->BSRR = GPIO_BSRR_BR13;   /* ON */
    else
        GPIOC->BSRR = GPIO_BSRR_BS13;   /* OFF */
}

/* ── SD (Shutdown) ─────────────────────────────────────── */
void gpio_set_sd(bool enable)
{
    if (enable) {
        GPIOB->BSRR = GPIO_BSRR_BS4;    /* SD = HIGH → shutdown */
    } else {
        GPIOB->BSRR = GPIO_BSRR_BR4;    /* SD = LOW  → drivery aktywne */
    }
}

bool gpio_get_sd(void)
{
    return (GPIOB->ODR & GPIO_ODR_OD4_Msk) ? true : false;
}

/* ── Hall ──────────────────────────────────────────────── */
uint8_t gpio_get_hall_state(void)
{
    uint32_t idr = GPIOB->IDR;
    uint8_t state = 0;

    if (idr & (1UL << HALL_A_PIN)) state |= (1 << 0);
    if (idr & (1UL << HALL_B_PIN)) state |= (1 << 1);
    if (idr & (1UL << HALL_C_PIN)) state |= (1 << 2);

    return state;
}

/* ── Brake ─────────────────────────────────────────────── */
bool gpio_get_brake(void)
{
    /* Aktywny low (zwarcie do GND = hamulec) */
    return (GPIOB->IDR & GPIO_IDR_ID10_Msk) ? false : true;
}

/* ── Overcurrent ───────────────────────────────────────── */
bool gpio_get_overcurrent(void)
{
    /* LM393 daje stan niski przy przekroczeniu progu */
    return (GPIOB->IDR & GPIO_IDR_ID12_Msk) ? false : true;
}
