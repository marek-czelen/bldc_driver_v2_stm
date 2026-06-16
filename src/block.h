#ifndef BLOCK_H
#define BLOCK_H

#include <stdint.h>
#include <stdbool.h>
#include "main.h"

// ============================================================
//  BLOCK — sterowanie 6-step (trapezoidalne)
//  Komutacja na podstawie czujników Halla
// ============================================================

// Inicjalizacja trybu BLOCK
void block_init(void);

// Ustawienie docelowego wypełnienia (0.0 .. 1.0)
void block_set_duty(float duty);

// Ustawienie docelowej prędkości RPM (wymaga PID)
void block_set_speed(float rpm);

// Wykonaj jeden krok komutacji na podstawie stanu Halla
// Zwraca true jeśli nastąpiła zmiana kroku
bool block_commutate(void);

// Zatrzymanie silnika (wszystkie wyjścia OFF)
void block_stop(void);

// Uruchomienie silnika
void block_start(void);

// Pobranie aktualnego kroku komutacji (0..5)
uint8_t block_get_step(void);

// Pobranie docelowego wypełnienia
float block_get_duty(void);

#endif // BLOCK_H
