/**
 * pid.c — Regulator PID z anti-windup
 *
 * Do wykorzystania w trybach SINUS i FOC.
 * Uproszczona implementacja — tylko człon P i PI.
 */

#include "pid.h"
#include <string.h>

/* ─────────────────────────────────────────────────────────
 * pid_init
 * ──────────────────────────────────────────────────────── */
void pid_init(pid_t *pid, float kp, float ki, float kd,
              float output_limit, float dt)
{
    memset(pid, 0, sizeof(*pid));
    pid->kp            = kp;
    pid->ki            = ki;
    pid->kd            = kd;
    pid->output_limit  = output_limit;
    pid->integral_limit = output_limit * 0.8f;  /* 80% wyjścia */
    pid->dt            = dt;
}

/* ─────────────────────────────────────────────────────────
 * pid_update
 * ──────────────────────────────────────────────────────── */
float pid_update(pid_t *pid, float setpoint, float measurement)
{
    float error = setpoint - measurement;

    /* Człon proporcjonalny */
    float p_term = pid->kp * error;

    /* Człon całkujący z anti-windup (clamping) */
    float i_term = 0.0f;
    if (pid->ki > 0.0f) {
        pid->integral += pid->ki * error * pid->dt;

        /* Ograniczenie całki */
        if (pid->integral >  pid->integral_limit) pid->integral =  pid->integral_limit;
        if (pid->integral < -pid->integral_limit) pid->integral = -pid->integral_limit;

        i_term = pid->integral;
    }

    /* Człon różniczkujący */
    float d_term = 0.0f;
    if (pid->kd > 0.0f && pid->dt > 0.0f) {
        d_term = pid->kd * (error - pid->prev_error) / pid->dt;
    }
    pid->prev_error = error;

    /* Sumowanie */
    float output = p_term + i_term + d_term;

    /* Ograniczenie wyjścia + anti-windup */
    if (output > pid->output_limit) {
        output = pid->output_limit;
        /* Cofnij całkę jeśli nasycone */
        if (error > 0) pid->integral -= pid->ki * error * pid->dt;
    } else if (output < -pid->output_limit) {
        output = -pid->output_limit;
        if (error < 0) pid->integral -= pid->ki * error * pid->dt;
    }

    pid->output = output;
    return output;
}

/* ─────────────────────────────────────────────────────────
 * pid_reset
 * ──────────────────────────────────────────────────────── */
void pid_reset(pid_t *pid)
{
    pid->integral   = 0.0f;
    pid->prev_error = 0.0f;
    pid->output     = 0.0f;
}

/* ─────────────────────────────────────────────────────────
 * pid_set_gains
 * ──────────────────────────────────────────────────────── */
void pid_set_gains(pid_t *pid, float kp, float ki, float kd)
{
    pid->kp = kp;
    pid->ki = ki;
    pid->kd = kd;
    pid_reset(pid);   /* reset przy zmianie nastaw */
}
