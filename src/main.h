#ifndef MAIN_H
#define MAIN_H

#include <stdint.h>
#include <stdbool.h>

// ============================================================
//  BLDC Driver v2 — współdzielone definicje
// ============================================================

// ── Tryb sterowania ────────────────────────────────────────
typedef enum {
    CTRL_MODE_BLOCK = 0,
    CTRL_MODE_SINUS,
    CTRL_MODE_FOC,
    CTRL_MODE_COUNT
} control_mode_t;

// ── Faza silnika ───────────────────────────────────────────
typedef enum {
    PHASE_A = 0,
    PHASE_B,
    PHASE_C,
    PHASE_COUNT
} phase_t;

// ── Błędy bezpieczeństwa ───────────────────────────────────
typedef enum {
    SAFETY_OK = 0,
    SAFETY_OVERCURRENT,
    SAFETY_OVERTEMP,
    SAFETY_UNDERVOLTAGE,
    SAFETY_OVERVOLTAGE,
    SAFETY_BRAKE,
    SAFETY_COUNT
} safety_error_t;

// ── Stan sterownika ────────────────────────────────────────
typedef enum {
    STATE_INIT = 0,
    STATE_IDLE,
    STATE_ALIGN,
    STATE_RAMP,
    STATE_RUN,
    STATE_FAULT
} driver_state_t;

// ── Zmienne globalne ───────────────────────────────────────
extern volatile uint32_t sys_tick_ms;
extern control_mode_t    g_control_mode;
extern driver_state_t    g_driver_state;
extern safety_error_t    g_safety_error;
extern float             g_bus_voltage;
extern float             g_throttle;       // 0.0 .. 1.0
extern bool              g_cli_control_active; // true: sterowanie z CLI (start/duty)

#endif // MAIN_H
