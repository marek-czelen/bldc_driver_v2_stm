#ifndef PID_H
#define PID_H

#include <stdint.h>
#include <stdbool.h>

// ============================================================
//  PID — regulator PI/D z anti-windup
//  Do wykorzystania przy trybach SINUS i FOC
// ============================================================

typedef struct {
    float kp;               // Wzmocnienie proporcjonalne
    float ki;               // Wzmocnienie całkujące
    float kd;               // Wzmocnienie różniczkujące
    float integral;         // Skumulowany błąd
    float prev_error;       // Poprzedni błąd (dla członu D)
    float output_limit;     // Ograniczenie wyjścia (abs)
    float integral_limit;   // Ograniczenie całki (abs)
    float dt;               // Krok czasowy [s]
    float output;           // Ostatnie wyjście
} pid_t;

// Inicjalizacja regulatora
void pid_init(pid_t *pid, float kp, float ki, float kd,
              float output_limit, float dt);

// Obliczenie kroku regulatora
float pid_update(pid_t *pid, float setpoint, float measurement);

// Reset stanu regulatora
void pid_reset(pid_t *pid);

// Ustawienie nowych nastaw
void pid_set_gains(pid_t *pid, float kp, float ki, float kd);

#endif // PID_H
