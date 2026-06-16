#ifndef CLI_H
#define CLI_H

#include <stdint.h>
#include <stdbool.h>

// ============================================================
//  CLI — interfejs tekstowy przez UART (PA2/PA3, USART2)
//  Baud: 115200, 8N1
// ============================================================

// Inicjalizacja USART2 + CLI
void cli_init(void);

// Przetwarzanie odebranych znaków — wywołuj w pętli głównej
void cli_process(void);

// Wysłanie pojedynczego znaku (blokujące)
void cli_putc(char c);

// Wysłanie łańcucha
void cli_puts(const char *s);

// Wysłanie łańcucha z nową linią (CR+LF)
void cli_println(const char *s);

// Wypisanie statusu systemu
void cli_print_status(void);

#endif // CLI_H
