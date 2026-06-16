/**
 * cli.c — Interfejs tekstowy przez UART (USART2: PA2-TX, PA3-RX)
 *
 * Funkcje:
 *  - Odbiór znaków przerwaniem (USART2_IRQ)
 *  - Bufor linii, przetwarzanie po Enter
 *  - Komendy: help, start, stop, duty, speed, mode, status
 *  - Echo + prompt "bldc> "
 */

#include <stm32f4xx.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "cli.h"
#include "main.h"
#include "config.h"
#include "block.h"
#include "safety.h"
#include "adc.h"
#include "gpio.h"

/* ── Stałe UART ──────────────────────────────────────────── */
#define UART_BAUD           115200
#define CLI_LINE_BUF_SIZE   64
#define CLI_MAX_ARGS        4

/* ── Bufor odbioru ───────────────────────────────────────── */
static char     rx_line_buf_[CLI_LINE_BUF_SIZE];
static uint8_t  rx_idx_      = 0;
static bool     rx_complete_ = false;

/* ── Forward declarations ────────────────────────────────── */
static void cmd_help(int argc, char *argv[]);
static void cmd_start(int argc, char *argv[]);
static void cmd_stop(int argc, char *argv[]);
static void cmd_duty(int argc, char *argv[]);
static void cmd_speed(int argc, char *argv[]);
static void cmd_mode(int argc, char *argv[]);
static void cmd_status(int argc, char *argv[]);
static void cmd_fault(int argc, char *argv[]);
static void cmd_vbat(int argc, char *argv[]);

/* ── Tablica komend ──────────────────────────────────────── */
typedef struct {
    const char *name;
    const char *help;
    void (*handler)(int argc, char *argv[]);
} cli_cmd_t;

static const cli_cmd_t cmd_table_[] = {
    { "help",   "help                         - lista komend",          cmd_help   },
    { "?",      "?                            - skrot do help",          cmd_help   },
    { "start",  "start                        - uruchom silnik",         cmd_start  },
    { "stop",   "stop                         - zatrzymaj silnik",       cmd_stop   },
    { "duty",   "duty <0-100>                 - wypelnienie PWM [%]",    cmd_duty   },
    { "speed",  "speed <rpm>                  - zadana predkosc",        cmd_speed  },
    { "mode",   "mode <block|sinus|foc>       - tryb sterowania",        cmd_mode   },
    { "status", "status                       - stan systemu",           cmd_status },
    { "fault",  "fault                        - stan bledow / reset",    cmd_fault  },
    { "vbat",   "vbat                         - odczyt napiecia baterii",cmd_vbat   },
};

#define CMD_COUNT (sizeof(cmd_table_) / sizeof(cmd_table_[0]))

/* ─────────────────────────────────────────────────────────
 * cli_init — inicjalizacja USART2 + CLI
 * ──────────────────────────────────────────────────────── */
void cli_init(void)
{
    /* ── Zegary ──────────────────────────────────────── */
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    RCC->APB1ENR |= RCC_APB1ENR_USART2EN;
    __DSB();

    /* ── PA2: USART2_TX (AF7), push-pull ──────────────── */
    GPIOA->MODER   &= ~GPIO_MODER_MODER2_Msk;
    GPIOA->MODER   |=  (2UL << GPIO_MODER_MODER2_Pos);  /* AF */
    GPIOA->OSPEEDR |=  (3UL << GPIO_OSPEEDR_OSPEED2_Pos); /* very high */
    GPIOA->AFR[0] &= ~(0xFUL << 8);                      /* clear AF for PA2 */
    GPIOA->AFR[0] |=  (7UL << 8);                        /* AF7 = USART2 */

    /* ── PA3: USART2_RX (AF7), input ─────────────────── */
    GPIOA->MODER   &= ~GPIO_MODER_MODER3_Msk;
    GPIOA->MODER   |=  (2UL << GPIO_MODER_MODER3_Pos);  /* AF */
    GPIOA->PUPDR   &= ~GPIO_PUPDR_PUPD3_Msk;
    GPIOA->PUPDR   |=  (1UL << GPIO_PUPDR_PUPD3_Pos);   /* pull-up */
    GPIOA->AFR[0] &= ~(0xFUL << 12);
    GPIOA->AFR[0] |=  (7UL << 12);                       /* AF7 = USART2 */

    /* ── USART2: 115200, 8N1 ─────────────────────────── */
    /* USARTDIV = APB1_CLK / (16 * baud) = 48e6 / (16*115200) = 26.0417
     * Mantissa = 26, Fraction = 1 (0.0417 * 16 = 0.667 → 1) */
    USART2->BRR  = (26UL << 4) | 1UL;

    USART2->CR1  = USART_CR1_RE          /* RX enable */
                 | USART_CR1_TE          /* TX enable */
                 | USART_CR1_RXNEIE;     /* RX interrupt enable */

    USART2->CR2  = 0;
    USART2->CR3  = 0;

    USART2->CR1 |= USART_CR1_UE;         /* USART enable */

    /* ── NVIC: USART2 IRQ ─────────────────────────────── */
    NVIC_SetPriority(USART2_IRQn, 3);
    NVIC_EnableIRQ(USART2_IRQn);

    /* ── Wyślij banner ────────────────────────────────── */
    cli_println("");
    cli_println("BLDC Controller CLI v2.0");
    cli_println("Wpisz 'help' aby zobaczyc liste komend.");
    cli_puts("bldc> ");
}

/* ─────────────────────────────────────────────────────────
 * USART2_IRQHandler — odbiór znaku
 * ──────────────────────────────────────────────────────── */
void USART2_IRQHandler(void)
{
    if (USART2->SR & USART_SR_RXNE) {
        char c = (char)(USART2->DR & 0xFF);

        if (c == '\r' || c == '\n') {
            /* Koniec linii */
            if (rx_idx_ > 0) {
                rx_line_buf_[rx_idx_] = '\0';
                rx_complete_ = true;
            }
            /* Echo nowej linii */
            while (!(USART2->SR & USART_SR_TXE)) { __NOP(); }
            USART2->DR = '\r';
            while (!(USART2->SR & USART_SR_TXE)) { __NOP(); }
            USART2->DR = '\n';
        } else if (c == '\b' || c == 0x7F) {
            /* Backspace */
            if (rx_idx_ > 0) {
                rx_idx_--;
                /* Echo backspace */
                while (!(USART2->SR & USART_SR_TXE)) { __NOP(); }
                USART2->DR = '\b';
                while (!(USART2->SR & USART_SR_TXE)) { __NOP(); }
                USART2->DR = ' ';
                while (!(USART2->SR & USART_SR_TXE)) { __NOP(); }
                USART2->DR = '\b';
            }
        } else if (c >= ' ' && c <= '~') {
            /* Drukwalny znak */
            if (rx_idx_ < CLI_LINE_BUF_SIZE - 1) {
                rx_line_buf_[rx_idx_++] = c;
                /* Echo */
                while (!(USART2->SR & USART_SR_TXE)) { __NOP(); }
                USART2->DR = c;
            }
        }
        /* Inne znaki sterujące — ignoruj */
    }
}

/* ─────────────────────────────────────────────────────────
 * cli_putc — wysłanie pojedynczego znaku (blokujące)
 * ──────────────────────────────────────────────────────── */
void cli_putc(char c)
{
    while (!(USART2->SR & USART_SR_TXE)) {
        __NOP();
    }
    USART2->DR = c;
}

/* ─────────────────────────────────────────────────────────
 * cli_puts — wysłanie łańcucha
 * ──────────────────────────────────────────────────────── */
void cli_puts(const char *s)
{
    while (*s) {
        cli_putc(*s++);
    }
}

/* ─────────────────────────────────────────────────────────
 * cli_println — łańcuch + CR+LF
 * ──────────────────────────────────────────────────────── */
void cli_println(const char *s)
{
    cli_puts(s);
    cli_puts("\r\n");
}

/* ─────────────────────────────────────────────────────────
 * cli_process — przetwarzanie odebranej linii
 * ──────────────────────────────────────────────────────── */
void cli_process(void)
{
    if (!rx_complete_) return;
    rx_complete_ = false;

    char *line = rx_line_buf_;
    rx_idx_ = 0;

    /* ── Parsuj argumenty: "cmd arg1 arg2" ────────────── */
    int    argc = 0;
    char  *argv[CLI_MAX_ARGS];
    char  *token = strtok(line, " \t");

    while (token && argc < CLI_MAX_ARGS) {
        argv[argc++] = token;
        token = strtok(NULL, " \t");
    }

    if (argc == 0) {
        /* Pusta linia */
        cli_puts("bldc> ");
        return;
    }

    /* ── Szukaj komendy ───────────────────────────────── */
    bool found = false;
    for (size_t i = 0; i < CMD_COUNT; i++) {
        if (strcmp(argv[0], cmd_table_[i].name) == 0) {
            cmd_table_[i].handler(argc, argv);
            found = true;
            break;
        }
    }

    if (!found) {
        cli_println("Nieznana komenda. Wpisz 'help'.");
    }

    cli_puts("bldc> ");
}

/* ══════════════════════════════════════════════════════════
 * Handlery komend
 * ══════════════════════════════════════════════════════════ */

static void cmd_help(int argc, char *argv[])
{
    (void)argc; (void)argv;
    for (size_t i = 0; i < CMD_COUNT; i++) {
        cli_println(cmd_table_[i].help);
    }
}

static void cmd_start(int argc, char *argv[])
{
    (void)argc; (void)argv;
    if (safety_is_faulted()) {
        cli_println("BLAD: system w stanie awarii. Wpisz 'fault'.");
        return;
    }
    if (g_driver_state == STATE_RUN) {
        cli_println("Silnik juz pracuje.");
        return;
    }
    /* Sprawdź napięcie baterii przed startem */
    adc_start_conversion();
    float vbat = adc_get_bus_voltage();
    if (vbat < UNDERVOLTAGE_THRESHOLD) {
        char buf[64];
        snprintf(buf, sizeof(buf), "BLAD: napiecie baterii %.1f V < %.0f V",
                 (double)vbat, (double)UNDERVOLTAGE_THRESHOLD);
        cli_println(buf);
        return;
    }
    if (vbat > OVERVOLTAGE_THRESHOLD) {
        char buf[64];
        snprintf(buf, sizeof(buf), "BLAD: napiecie baterii %.1f V > %.0f V",
                 (double)vbat, (double)OVERVOLTAGE_THRESHOLD);
        cli_println(buf);
        return;
    }
    g_bus_voltage = vbat;
    g_cli_control_active = true;
    g_driver_state = STATE_RUN;
    block_start();
    cli_println("Silnik uruchomiony.");
}

static void cmd_stop(int argc, char *argv[])
{
    (void)argc; (void)argv;
    if (g_driver_state != STATE_RUN) {
        g_cli_control_active = false;
        cli_println("Silnik juz zatrzymany.");
        return;
    }
    g_cli_control_active = false;
    block_stop();
    g_driver_state = STATE_IDLE;
    cli_println("Silnik zatrzymany.");
}

static void cmd_duty(int argc, char *argv[])
{
    if (argc < 2) {
        cli_println("Uzycie: duty <0-100>");
        char buf[32];
        snprintf(buf, sizeof(buf), "Aktualne: %.1f %%", (double)(block_get_duty() * 100.0f));
        cli_println(buf);
        return;
    }
    float pct = (float)atof(argv[1]);
    if (pct < 0.0f) pct = 0.0f;
    float max_pct = PWM_MAX_DUTY * 100.0f;
    if (pct > max_pct) pct = max_pct;

    float duty = pct / 100.0f;
    block_set_duty(duty);

    char buf[32];
    snprintf(buf, sizeof(buf), "Duty ustawione na %.1f %%", (double)pct);
    cli_println(buf);
}

static void cmd_speed(int argc, char *argv[])
{
    if (argc < 2) {
        cli_println("Uzycie: speed <rpm>   (funkcja w budowie)");
        return;
    }
    float rpm = (float)atof(argv[1]);
    block_set_speed(rpm);
    char buf[64];
    snprintf(buf, sizeof(buf), "Predkosc zadana: %.0f RPM (TODO: PID)", (double)rpm);
    cli_println(buf);
}

static void cmd_mode(int argc, char *argv[])
{
    if (argc < 2) {
        const char *mode_str = "???";
        switch (g_control_mode) {
        case CTRL_MODE_BLOCK: mode_str = "BLOCK"; break;
        case CTRL_MODE_SINUS: mode_str = "SINUS"; break;
        case CTRL_MODE_FOC:   mode_str = "FOC";   break;
        default: break;
        }
        char buf[64];
        snprintf(buf, sizeof(buf), "Tryb: %s. Uzycie: mode <block|sinus|foc>", mode_str);
        cli_println(buf);
        return;
    }

    if (strcmp(argv[1], "block") == 0) {
        g_control_mode = CTRL_MODE_BLOCK;
        cli_println("Tryb: BLOCK");
    } else if (strcmp(argv[1], "sinus") == 0) {
        g_control_mode = CTRL_MODE_SINUS;
        cli_println("Tryb: SINUS (niezaimplementowany)");
    } else if (strcmp(argv[1], "foc") == 0) {
        g_control_mode = CTRL_MODE_FOC;
        cli_println("Tryb: FOC (niezaimplementowany)");
    } else {
        cli_println("Nieznany tryb. Dostepne: block, sinus, foc");
    }
}

static void cmd_status(int argc, char *argv[])
{
    (void)argc; (void)argv;
    cli_print_status();
}

static void cmd_fault(int argc, char *argv[])
{
    (void)argc; (void)argv;
    if (safety_is_faulted()) {
        cli_println("AWARIA!");
        char buf[64];
        snprintf(buf, sizeof(buf), "  Blad: %s", safety_error_string(g_safety_error));
        cli_println(buf);
        cli_println("Wpisz 'fault clear' aby skasowac.");
    } else if (argc >= 2 && strcmp(argv[1], "clear") == 0) {
        safety_clear_fault();
        cli_println("Awaria skasowana.");
    } else {
        cli_println("Brak awarii.");
    }
}

static void cmd_vbat(int argc, char *argv[])
{
    (void)argc; (void)argv;

    /* Szybki odczyt samego VBAT dla diagnostyki */
    adc_start_conversion();
    uint16_t raw  = adc_get_raw(ADC_CH_BATTERY_V);
    float    vadc = ((float)raw * ADC_VREF) / ADC_RESOLUTION;
    float    vbat = vadc * VBAT_DIVIDER;

    char buf[64];
    cli_println("─── VBAT DIAG ──────────────────────");
    snprintf(buf, sizeof(buf), "  Raw ADC:    %u", (unsigned)raw);
    cli_println(buf);
    snprintf(buf, sizeof(buf), "  V(ADC pin): %.3f V", (double)vadc);
    cli_println(buf);
    snprintf(buf, sizeof(buf), "  Mnożnik:    x%.1f", (double)VBAT_DIVIDER);
    cli_println(buf);
    snprintf(buf, sizeof(buf), "  Vbat:       %.2f V", (double)vbat);
    cli_println(buf);
    cli_println("───────────────────────────────────");

    g_bus_voltage = vbat;
}

/* ─────────────────────────────────────────────────────────
 * cli_print_status — wypisanie pełnego statusu
 * ──────────────────────────────────────────────────────── */
void cli_print_status(void)
{
    char buf[64];

    cli_println("─── STATUS ───────────────────────");

    /* Tryb */
    const char *mode_str = "???";
    switch (g_control_mode) {
    case CTRL_MODE_BLOCK: mode_str = "BLOCK"; break;
    case CTRL_MODE_SINUS: mode_str = "SINUS"; break;
    case CTRL_MODE_FOC:   mode_str = "FOC";   break;
    default: break;
    }
    snprintf(buf, sizeof(buf), "  Tryb:       %s", mode_str);
    cli_println(buf);

    /* Stan */
    const char *state_str = "???";
    switch (g_driver_state) {
    case STATE_INIT:  state_str = "INIT";  break;
    case STATE_IDLE:  state_str = "IDLE";  break;
    case STATE_ALIGN: state_str = "ALIGN"; break;
    case STATE_RAMP:  state_str = "RAMP";  break;
    case STATE_RUN:   state_str = "RUN";   break;
    case STATE_FAULT: state_str = "FAULT"; break;
    default: break;
    }
    snprintf(buf, sizeof(buf), "  Stan:       %s", state_str);
    cli_println(buf);

    /* Wypełnienie */
    snprintf(buf, sizeof(buf), "  Duty:       %.1f %%", (double)(block_get_duty() * 100.0f));
    cli_println(buf);

    /* Krok komutacji */
    snprintf(buf, sizeof(buf), "  Krok:       %d/6", block_get_step());
    cli_println(buf);

    /* Hall */
    uint8_t hall = gpio_get_hall_state();
    snprintf(buf, sizeof(buf), "  Hall:       %c%c%c (0x%X)",
             (hall & 1) ? '1' : '0',
             (hall & 2) ? '1' : '0',
             (hall & 4) ? '1' : '0',
             hall);
    cli_println(buf);

    /* Napięcie baterii (świeży odczyt) */
    adc_start_conversion();
    uint16_t raw_vbat = adc_get_raw(ADC_CH_BATTERY_V);
    float    vadc     = ((float)raw_vbat * ADC_VREF) / ADC_RESOLUTION;
    float    vbat_now = vadc * VBAT_DIVIDER;
    g_bus_voltage     = vbat_now;

    snprintf(buf, sizeof(buf), "  Vbat ADC:   %.3f V", (double)vadc);
    cli_println(buf);
    snprintf(buf, sizeof(buf), "  Vbat:       %.2f V  (x%.1f)", (double)vbat_now, (double)VBAT_DIVIDER);
    cli_println(buf);
    snprintf(buf, sizeof(buf), "  Vbat raw:   %u", (unsigned)raw_vbat);
    cli_println(buf);

    /* Prądy */
    float ia = adc_get_current(PHASE_A);
    float ib = adc_get_current(PHASE_B);
    snprintf(buf, sizeof(buf), "  I(A):       %.2f A", (double)ia);
    cli_println(buf);
    snprintf(buf, sizeof(buf), "  I(B):       %.2f A", (double)ib);
    cli_println(buf);

    /* Temperatura */
    float temp = adc_get_temperature();
    snprintf(buf, sizeof(buf), "  Temp FET:   %.1f C", (double)temp);
    cli_println(buf);

    /* Manetka */
    snprintf(buf, sizeof(buf), "  Throttle:   %.1f %%", (double)(g_throttle * 100.0f));
    cli_println(buf);

    /* Bezpieczeństwo */
    snprintf(buf, sizeof(buf), "  Safety:     %s",
             safety_is_faulted() ? safety_error_string(g_safety_error) : "OK");
    cli_println(buf);

    cli_println("───────────────────────────────────");
}
