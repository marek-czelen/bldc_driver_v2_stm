/**
 * BLDC Controller v2 — szkiclet aplikacji (tryb BLOCK)
 *
 * MCU:      STM32F411CEU6 (WeAct Black Pill), 96 MHz
 * Framework: CMSIS (bez HAL)
 * Sterowanie: 6-step trapezoidalne z czujnikami Halla
 *
 * Tryby (do implementacji):
 *  [x] CTRL_MODE_BLOCK  — zaimplementowany
 *  [ ] CTRL_MODE_SINUS  — później
 *  [ ] CTRL_MODE_FOC    — później
 *
 * Budowa:     pio run
 * Wgranie:    pio run --target upload
 */

#include <stm32f4xx.h>
#include "main.h"
#include "config.h"
#include "pwm.h"
#include "adc.h"
#include "gpio.h"
#include "block.h"
#include "sinus.h"
#include "safety.h"
#include "cli.h"

/* ══════════════════════════════════════════════════════════
 * Zmienne globalne
 * ══════════════════════════════════════════════════════════ */

volatile uint32_t sys_tick_ms = 0;      /* licznik milisekund (SysTick) */
control_mode_t    g_control_mode = CTRL_MODE_BLOCK;
driver_state_t    g_driver_state = STATE_INIT;
safety_error_t    g_safety_error = SAFETY_OK;
float             g_bus_voltage  = 0.0f;
float             g_throttle     = 0.0f;
bool              g_cli_control_active = false;
bool              g_watch_active = false;

/* ══════════════════════════════════════════════════════════
 * SysTick — 1 kHz
 * ══════════════════════════════════════════════════════════ */
void SysTick_Handler(void)
{
    sys_tick_ms++;
}

/* ══════════════════════════════════════════════════════════
 * system_clock_init — konfiguracja zegara 96 MHz
 *
 * HSE 25 MHz → PLL (M=25, N=192, P=2, Q=4) = 96 MHz
 * AHB = 96 MHz, APB1 = 48 MHz, APB2 = 96 MHz
 * Flash: 3 wait states, prefetch + cache
 * ══════════════════════════════════════════════════════════ */
static void system_clock_init(void)
{
    /* ── Włącz HSE ──────────────────────────────────── */
    RCC->CR |= RCC_CR_HSEON;
    while (!(RCC->CR & RCC_CR_HSERDY)) {
        __NOP();
    }

    /* ── Flash latency: 3 WS @ 96 MHz + prefetch/cache ─ */
    FLASH->ACR = (3UL << FLASH_ACR_LATENCY_Pos)
               | FLASH_ACR_PRFTEN
               | FLASH_ACR_ICEN
               | FLASH_ACR_DCEN;

    /* ── PLL: HSE, /25, *192, /2 → 96 MHz ───────────── */
    /* PLL_Q = 4 → 48 MHz dla USB                            */
    RCC->PLLCFGR = (25UL  << RCC_PLLCFGR_PLLM_Pos)
                 | (192UL << RCC_PLLCFGR_PLLN_Pos)
                 | (0UL   << RCC_PLLCFGR_PLLP_Pos)   /* P=2 */
                 | (4UL   << RCC_PLLCFGR_PLLQ_Pos)
                 | RCC_PLLCFGR_PLLSRC_HSE;

    /* ── Włącz PLL ──────────────────────────────────── */
    RCC->CR |= RCC_CR_PLLON;
    while (!(RCC->CR & RCC_CR_PLLRDY)) {
        __NOP();
    }

    /* ── Preskalery magistrali ───────────────────────── */
    /* HPRE=0 (/1), PPRE1=4 (/2 → 48 MHz), PPRE2=0 (/1 → 96 MHz) */
    RCC->CFGR = RCC_CFGR_PPRE1_DIV2;   /* APB1 = HCLK/2 */

    /* ── Przełącz na PLL ─────────────────────────────── */
    RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW_Msk) | RCC_CFGR_SW_PLL;
    while ((RCC->CFGR & RCC_CFGR_SWS_Msk) != RCC_CFGR_SWS_PLL) {
        __NOP();
    }

    /* ── Aktualizuj zmienną CMSIS ────────────────────── */
    SystemCoreClock = SYSTEM_CLOCK_HZ;
}

/* ══════════════════════════════════════════════════════════
 * main
 * ══════════════════════════════════════════════════════════ */
int main(void)
{
    /* ── Inicjalizacja sprzętu ──────────────────────────── */
    system_clock_init();
    SysTick_Config(SystemCoreClock / CONTROL_LOOP_HZ);   /* 1 kHz */

    gpio_init();        /* LED, SD, Hall, Brake, Overcurrent */
    pwm_init();         /* TIM1 — 6 wyjść */
    adc_init();         /* ADC1 — prąd, nap., temp. */
    block_init();       /* Tryb BLOCK */
    sinus_init();       /* Tryb SINUS */
    safety_init();      /* Monitor bezpieczeństwa */
    cli_init();         /* UART CLI (PA2/PA3, 115200) */

    /* ── Aktywuj drivery ────────────────────────────────── */
    gpio_set_sd(false);             /* SD = LOW → drivery ON */
    pwm_output_enable(true);        /* MOE = 1 → PWM ON */
    g_driver_state = STATE_IDLE;

    /* ── Zmienne pętli głównej ──────────────────────────── */
    uint32_t last_ctrl_tick = 0;    /* ostatni tick pętli sterowania */
    uint32_t last_led_tick  = 0;    /* ostatni tick LED */
    uint32_t led_pattern_ms = LED_BLINK_MS;
    bool     led_on         = false;

    /* ══════════════════════════════════════════════════════
     * Pętla główna
     * ══════════════════════════════════════════════════════ */
    while (1) {
        uint32_t now = sys_tick_ms;

        /* ── Kontrolka LED ─────────────────────────────── */
        if ((now - last_led_tick) >= led_pattern_ms) {
            last_led_tick = now;

            if (safety_is_faulted()) {
                /* Szybkie miganie przy awarii */
                led_pattern_ms = 100;
                led_on = !led_on;
            } else if (g_driver_state == STATE_RUN) {
                /* Wolne miganie podczas pracy */
                led_pattern_ms = LED_BLINK_MS;
                led_on = !led_on;
            } else {
                /* Pojedyncze krótkie błyski w idle */
                led_pattern_ms = 100;
                uint32_t phase = now % 2000;
                led_on = (phase < 50);
            }

            if (led_on) gpio_led_on();
            else        gpio_led_off();
        }

        /* ── Pętla sterowania (1 kHz) ──────────────────── */
        if ((now - last_ctrl_tick) >= 1) {
            last_ctrl_tick = now;

            /* Odczyt wszystkich wejść analogowych */
            adc_start_conversion();
            g_throttle    = adc_get_throttle();
            g_bus_voltage = adc_get_bus_voltage();

            /* Sprawdzenie bezpieczeństwa */
            safety_check();

            /* ── Maszyna stanów BLOCK ───────────────────── */
            if (!safety_is_faulted()) {

                switch (g_control_mode) {
                case CTRL_MODE_BLOCK:
                    if (g_cli_control_active) {
                        /* Sterowanie z CLI: start/stop + duty niezależnie od manetki */
                        float duty_cmd = block_get_duty();
                        if (duty_cmd > 0.001f) {
                            if (g_driver_state != STATE_RUN) {
                                block_start();
                                g_driver_state = STATE_RUN;
                            }
                            block_set_duty(duty_cmd);
                            block_commutate();
                        } else {
                            /* Brak zadanego duty z CLI */
                            if (g_driver_state == STATE_RUN) {
                                block_stop();
                                g_driver_state = STATE_IDLE;
                            }
                        }
                    } else {
                        /* Sterowanie manetką */
                        if (g_throttle > 0.03f) {
                            if (g_driver_state != STATE_RUN) {
                                block_start();
                                g_driver_state = STATE_RUN;
                            }
                            block_set_duty(g_throttle);
                            block_commutate();
                        } else {
                            if (g_driver_state == STATE_RUN) {
                                block_stop();
                                g_driver_state = STATE_IDLE;
                            }
                        }
                    }
                    break;

                case CTRL_MODE_SINUS:
                    if (g_cli_control_active) {
                        if (g_driver_state != STATE_RUN) {
                            sinus_start();
                            g_driver_state = STATE_RUN;
                        }
                        sinus_update();
                    } else {
                        /* Sterowanie manetką */
                        if (g_throttle > 0.03f) {
                            if (g_driver_state != STATE_RUN) {
                                sinus_start();
                                g_driver_state = STATE_RUN;
                            }
                            sinus_set_duty(g_throttle);
                            sinus_update();
                        } else {
                            if (g_driver_state == STATE_RUN) {
                                sinus_stop();
                                g_driver_state = STATE_IDLE;
                            }
                        }
                    }
                    break;

                case CTRL_MODE_FOC:
                    /* TODO: do implementacji w przyszłości */
                    if (g_driver_state == STATE_RUN) {
                        block_stop();
                        g_driver_state = STATE_IDLE;
                    }
                    break;

                default:
                    break;
                }

            } else {
                /* Awaria — zatrzymaj jeśli nie zatrzymane */
                if (g_driver_state == STATE_RUN) {
                    if (g_control_mode == CTRL_MODE_SINUS)
                        sinus_stop();
                    else
                        block_stop();
                    g_driver_state = STATE_FAULT;
                }
            }
        }

        /* ── Obsługa CLI (UART) ─────────────────────────── */
        cli_process();

        /* ── Podgląd watch (nieblokujący) ────────────────── */
        if (g_watch_active) {
            cli_watch_poll();
        }

        /* ── Uśpienie między cyklami ────────────────────── */
        __WFI();
    }
}
