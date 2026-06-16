# Tryby sterowania silnikiem BLDC

## 1. BLOCK (6-step / Trapezoidal)

### Zasada działania

Komutacja 6-stopniowa — w każdej chwili zasilane są 2 z 3 faz (jedna do +BUS, druga do GND), trzecia faza jest nieaktywna (odcięta lub pomiar BEMF).

```
Krok 1: A+  B-  C_open
Krok 2: A+  C-  B_open
Krok 3: B+  C-  A_open
Krok 4: B+  A-  C_open
Krok 5: C+  A-  B_open
Krok 6: C+  B-  A_open
```

### Sygnały PWM

- **PWM tylko na górnych tranzystorach** (chopping) — dolne włączone na stało w danym kroku
- lub **PWM na dolnych** (albo synchroniczny PWM na obu)
- Stałe wypełnienie (duty cycle) w ramach jednego kroku

### Detekcja pozycji wirnika

#### Z czujnikami Halla
- 3 czujniki halla → 6 stanów → bezpośrednia komutacja
- W tym projekcie odczyt Halla jest realizowany przez GPIO / EXTI na **PB5, PB8, PB9**

#### Sensorless (BEMF)
- Pomiar napięcia na nieaktywnej fazie (back-EMF)
- Komparator lub ADC + detekcja przejścia przez zero (ZCD)
- Start: ramping (forced commutation) do uzyskania mierzalnego BEMF

### Zalety / Wady

| ✅ Zalety | ❌ Wady |
|-----------|---------|
| Prosty algorytm | Tętnienia momentu (torque ripple) |
| Niskie wymagania obliczeniowe | Hałas akustyczny |
| Dobry do wysokich prędkości | Gorsza efektywność przy niskich obrotach |
| Sensorless łatwy do implementacji | |

---

## 2. SINUS (Sinusoidal PWM)

### Zasada działania

Napięcia na fazach są modulowane sinusoidalnie z przesunięciem 120°:

$$
V_A = V_{DC} \cdot \frac{1 + m \cdot \sin(\theta)}{2}
$$
$$
V_B = V_{DC} \cdot \frac{1 + m \cdot \sin(\theta + 120°)}{2}
$$
$$
V_C = V_{DC} \cdot \frac{1 + m \cdot \sin(\theta + 240°)}{2}
$$

gdzie:
- $m$ — współczynnik modulacji (0..1)
- $\theta$ — kąt elektryczny wirnika
- $V_{DC}$ — napięcie szyny

### Modulacja

- **SPWM** (Sinusoidal PWM) — klasyczna, wykorzystuje ~86% napięcia DC
- **SVPWM** (Space Vector PWM) — lepsze wykorzystanie napięcia (~100%), mniejsze zniekształcenia

### Detekcja pozycji

- **Hall + interpolacja** — czujniki halla dają 6 pozycji na obrót, między nimi interpolacja (liniowa lub z filtrem)
- **Enkoder** — dokładna pozycja absolutna/inkrementalna
- **Sensorless + obserwator** — obserwator strumienia lub BEMF

### Zalety / Wady

| ✅ Zalety | ❌ Wady |
|-----------|---------|
| Gładki moment obrotowy | Wymaga dokładnej pozycji wirnika |
| Niski hałas akustyczny | Wyższe wymagania obliczeniowe |
| Lepsza efektywność niż BLOCK | Wymaga enkodera lub dobrego obserwatora |

---

## 3. FOC (Field Oriented Control)

### Zasada działania

FOC przekształca prądy fazowe $I_A, I_B, I_C$ do układu wirującego $d-q$:

1. **Clarke** — $I_A, I_B, I_C \rightarrow I_\alpha, I_\beta$ (układ stacjonarny)
2. **Park** — $I_\alpha, I_\beta \rightarrow I_d, I_q$ (układ wirujący z wirnikiem)

W układzie $dq$:
- **$I_q$** — prąd generujący moment obrotowy (quadrature)
- **$I_d$** — prąd wzbudzenia (direct) — dla SPMSM $I_d^* = 0$, dla IPMSM $I_d^* < 0$ (MTPA)

### Pętle regulacji

```
                        ┌──────────────────────────────────────────────────┐
                        │                  FOC                             │
ω_ref ──┤PI├──► I_q_ref ──┤PI├──► V_q_ref ──┤Inverse│     │            │
                        │              │        │ Park  │     │  SVPWM   │──► INVERTER
                        │    I_d_ref ──┤PI├──► V_d_ref ──┤ &     │     │            │
                        │              │                 │ Clarke│     │            │
                        │    I_d ◄────┤   I_q ◄─────────┤       │     │            │
                        │              │                 └───────┘     │            │
                        │    ┌─────────┴──────────┐                    │            │
                        │    │  Clarke + Park      │                    └────────────┘
                        │    │  I_α,I_β → I_d,I_q  │                          ▲
                        │    └─────────┬──────────┘                          │
                        │              │                               ┌─────┴─────┐
                        │         I_α, I_β                             │  PWM gen  │
                        │              │                               │  TIM1 6ch │
                        │    ┌─────────┴──────────┐                    └─────┬─────┘
                        │    │  Clarke (3→2)      │                          │
                        │    │  I_A,I_B,I_C → I_α,I_β                      │
                        │    └─────────┬──────────┘                          │
                        │              │                               ┌─────┴─────┐
                        │         I_A, I_B, I_C                        │  EG2113D  │
                        │              │                               │  ×6 HY4008 │
                        │    ┌─────────┴──────────┐                    └─────┬─────┘
                        │    │  ADC (cur sensing) │                          │
                        │    └────────────────────┘                          │
                        │                                               SILNIK BLDC
                        └──────────────────────────────────────────────────────┘
```

### Wymagania sprzętowe dla FOC

| Wymaganie | Uwagi |
|-----------|-------|
| Pomiar prądu min. 2 faz | Shunt + ADC (**PA0**, **PA1**), trzecia faza wyliczana |
| Synchronizacja ADC z PWM | Próbkowanie w środku okresu PWM |
| Szybka pętla prądowa | 10-20 kHz |
| Powolna pętla prędkości | 1-5 kHz |
| FPU | STM32F411 ma FPU — bez problemu |

### Sensorless FOC

Gdy brak enkodera/czujników:
- **Obserwator Luenbergera** — na podstawie modelu silnika
- **Sliding Mode Observer (SMO)** — odporny na zakłócenia
- **Obserwator strumienia** — całkowanie BEMF (z kompensacją dryftu)

### Metody startu dla sensorless FOC

1. **Open-loop start** — najpierw wirowanie bez sprzężenia (VF), potem przejście do FOC
2. **HFI (High Frequency Injection)** — dla silników z wydatnością biegunową (IPMSM)
3. **Align & ramp** — ustawienie wirnika w znanej pozycji, potem przyspieszenie

### Zalety / Wady

| ✅ Zalety | ❌ Wady |
|-----------|---------|
| Najwyższa efektywność | Wymaga precyzyjnego pomiaru prądu |
| Najgładszy moment (zero ripple) | Złożony algorytm (matematyka) |
| Pełna kontrola nad momentem i strumieniem | Kalibracja i tuningu |
| Praca w całym zakresie prędkości | Wyższe wymagania obliczeniowe |
| Możliwość regulacji w słabym polu (FW) | |

---

## Porównanie trybów

| Parametr | BLOCK | SINUS | FOC |
|----------|-------|-------|-----|
| Złożoność obliczeniowa | Niska | Średnia | Wysoka |
| Jakość momentu | Niska | Dobra | Bardzo dobra |
| Hałas akustyczny | Wysoki | Niski | Bardzo niski |
| Efektywność przy niskich RPM | Niska | Średnia | Wysoka |
| Efektywność przy wysokich RPM | Wysoka | Wysoka | Wysoka |
| Wymagany czujnik | Hall/BEMF | Hall/Encoder | Encoder/sensorless |
| Pomiar prądu | Opcjonalny | Opcjonalny | Wymagany |
| Maks. prędkość (elektryczna) | Bardzo wysoka | Wysoka | Średnia/wysoka |
| Łatwość tuningu | Łatwy | Średni | Trudny |

---

## Implementacja w projekcie

Planowana jest obsługa wszystkich 3 trybów z możliwością przełączania w locie:

```c
typedef enum {
    CTRL_MODE_BLOCK = 0,
    CTRL_MODE_SINUS,
    CTRL_MODE_FOC
} control_mode_t;
```

### Tryb startu
1. **Align** — ustawienie wirnika (dla wszystkich trybów sensorless)
2. **Open-loop ramp** — rozpędzenie do minimalnej prędkości pomiarowej
3. **Przejście do wybranego trybu** — BLOCK / SINUS / FOC

### Przełączanie trybów
- Przez CLI (UART przez TXB0102 lub USB-C)
- Przez przycisk (GPIO)
- Automatycznie w zależności od prędkości (np. BLOCK przy starcie → FOC po osiągnięciu RPM)

---

## Integracja ze sprzętem projektu

### INA240A2D — pomiar prądu dla FOC i zabezpieczeń

W projekcie zastosowano **3× INA240A2D** (jeden na fazę) — wzmacniacz różnicowy z wzmocnieniem 50V/V.

- **Dla FOC:** Pomiar prądu w 2 fazach (A i B), trzeci wyliczany ($I_C = -I_A - I_B$)
- **Dla BLOCK:** Pomiar prądu fazowego jest opcjonalny i może służyć do ograniczania prądu lub diagnostyki
- **Dla zabezpieczeń:** Wyjście INA240 → LM393 (komparator) → OVERCURRENT → SD (shutdown) EG2113D

**Konwersja ADC → prąd [A]:**
$$I = \frac{V_{OUT} - V_{REF}}{G \cdot R_{SHUNT}}$$
gdzie:
- $V_{OUT}$ — napięcie wyjściowe INA240 (0..5V)
- $V_{REF}$ — napięcie referencyjne (2.5V = VCC/2)
- $G = 50$ (wzmocnienie INA240A2D)
- $R_{SHUNT}$ — rezystancja shunta

### EG2113D — shutdown (SD) dla bezpieczeństwa

Wszystkie 3 drivery **EG2113D** mają połączony pin **SD** (shutdown) sterowany z **PB4** STM32.

| Stan SD | Efekt |
|---------|-------|
| LOW (0) | Normalna praca — PWM aktywne |
| HIGH (1) | Wszystkie MOSFETy wyłączone (Hi-Z) — tryb awaryjny |

SD jest aktywowany przez:
1. **LM393** (sprzętowo) — przekroczenie progu prądowego
2. **STM32** (programowo) — overcurrent, overtemperature, error state

### Synchronizacja ADC z PWM

Próbkowanie prądu odbywa się w **środku okresu PWM** (dla FOC i SINUS), gdy oba tranzystory half-bridge są w stanie przewodzenia (unikanie szumu przełączania).

1. PWM timer generuje trigger na środku okresu
2. ADC1 wykonuje konwersję automatycznie
3. Przerwanie PWM_UP odczytuje wyniki ADC
