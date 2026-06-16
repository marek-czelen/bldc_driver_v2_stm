# Plan rozwoju (Roadmap)

## Faza 1 — Podstawy sprzętowe i uruchomienie (MVP)

### Cel: Uruchomienie silnika w trybie BLOCK (6-step)

- [x] Określenie komponentów (STM32F411CEU6 Black Pill, 3× EG2113D, 6× HY4008, 3× INA240A2D)
- [x] Dokumentacja pinoutu i architektury
- [x] Przygotowanie schematu KiCad (wersja robocza)
- [ ] Wykonanie PCB (prototyp)
- [ ] Montaż i wstępne testy zasilania
- [ ] Konfiguracja TIM1 — 6 kanałów PWM z dead-time
- [ ] Konfiguracja ADC — pomiar prądu, temperatury, napięcia
- [ ] Implementacja BLOCK (6-step) z czujnikami Halla
- [ ] Uruchomienie silnika w trybie BLOCK
- [ ] CLI przez UART (podstawowe komendy)

### Kryteria akceptacji
- Silnik obraca się płynnie w trybie BLOCK
- Możliwość zmiany prędkości przez CLI
- Pomiar prądu i temperatury

---

## Faza 2 — Tryb SINUS

- [ ] Implementacja generowania sinusoidalnego PWM (SPWM/SVPWM)
- [ ] Obsługa enkodera (TIM2) lub interpolacja Halla
- [ ] Płynna zmiana prędkości w trybie SINUS
- [ ] Przełączanie BLOCK ↔ SINUS w locie

### Kryteria akceptacji
- Silnik pracuje cicho w trybie SINUS
- Niższe tętnięcia prądu niż BLOCK

---

## Faza 3 — FOC (Field Oriented Control)

- [ ] Implementacja transformacji Clarke i Park
- [ ] Pętla regulacji prądu (PI) w układzie dq
- [ ] SVPWM dla FOC
- [ ] Kalibracja offsetów ADC i przesunięcia fazowego
- [ ] Sensorless FOC (obserwator Luenbergera lub SMO)
- [ ] Pętla prędkości (kaskadowa: prędkość → prąd)

### Kryteria akceptacji
- Silnik pracuje w FOC z enkoderem
- Płynny start (open-loop → closed-loop)
- Sensorless FOC do 1000 RPM (początkowo)

---

## Faza 4 — Udoskonalenia

- [ ] Regulacja PID z autotuningiem
- [ ] Zabezpieczenia: overcurrent (szybkie), overtemperature, UVLO
- [ ] Obsługa błędów i recovery
- [ ] OLED — wyświetlanie parametrów (RPM, prąd, temp, tryb)
- [ ] MTPA (Maximum Torque Per Ampere) dla IPMSM
- [ ] Field Weakening (słabienie pola) dla wysokich RPM

---

## Faza 5 — Zaawansowane funkcje

- [ ] CAN bus (komunikacja z nadrzędnym sterownikiem)
- [ ] Logowanie danych (SD card lub przez UART do PC)
- [ ] Graficzny interfejs monitorujący (Python + PyQt/qtplot)
- [ ] Autotuning PID (step response + Ziegler-Nichols)
- [ ] Automatyczne wykrywanie parametrów silnika (R, L, Ke)
- [ ] Obsługa silników IPMSM (z wydatnością biegunową)

---

## Faza 6 — Produkcja i optymalizacja

- [ ] Optymalizacja pamięci (Flash/RAM)
- [ ] Testy na różnych silnikach
- [ ] Dokumentacja użytkownika (PDF)
- [ ] Przygotowanie do produkcji (BOM, gerbery, testy EMC)
- [ ] Wdrożenie bootloadera (DFU lub CAN)

---

## Oś czasu (szacunkowa)

| Faza | Czas | Status |
|------|------|--------|
| Faza 1 — BLOCK MVP | 4-6 tygodni | 🔜 Planowanie |
| Faza 2 — SINUS | 2-3 tygodnie | ⏳ |
| Faza 3 — FOC | 4-6 tygodni | ⏳ |
| Faza 4 — Udoskonalenia | 2-4 tygodnie | ⏳ |
| Faza 5 — Zaawansowane | 4-8 tygodni | ⏳ |
| Faza 6 — Produkcja | 4-6 tygodni | ⏳ |
