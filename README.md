# Úkoly:

## 1. Vliv zpoždění na rychlost přenosu pro Stop-and-Wait
- Odvodit

time per packet = 2 * delay + const
transfer time = time per packet * number of packets + const * number of packets

Pro nízká zpoždění víceméně konstantní, pro velká zpoždění víceméně lineární.

- Ověřit

Platí víceméně přesně.

## 2. Min. velikost vysílacího okna pro maximální rychlost přenosu (vztah k zpoždění)
- Odvodit vztah

Záleží, jak rychle jde vysílat packety.
dt = minimální doba mezi odeslanými packety
t0 = zpoždění tam a zpět
N = t0 / dt

- Ověřit vztah

Jak ověřit?


# TODO: 
- Zlepšit kdy a jak se posílají ACKs ať se nestává, že jeden program zůstane otevřený
  - Něco jsem tam asi rozbil...