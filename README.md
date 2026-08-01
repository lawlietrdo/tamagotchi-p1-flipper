# Tamagotchi P1 Emulator for Flipper Zero — Improved Edition

Emulator for the **original Tamagotchi P1 (1996)** running natively on the
Flipper Zero. This is an improved fork of
[GMMan's port](https://github.com/GMMan/flipperzero-tamagotch-p1), built on
[TamaLIB](https://github.com/jcrona/tamalib/) by Jean-Christophe Rona, with
modern-firmware compatibility fixes and new features (auto-save, real-time
catch-up, turbo mode and more).

Built and tested against official firmware **1.4.3 (API 87.1)**.

## ⚠️ About the ROM (read this first)

This emulator requires the Tamagotchi P1 ROM, which is **owned by Bandai and
NOT included in this repository** (and must never be committed — it is
`.gitignore`d on purpose). To get it:

1. Find the dump known as `tama.b` (12,288 bytes, the same set MAME uses as
   "Bandai Tamagotchi", `tama.zip`).
2. Rename it to `rom.bin`.
3. Copy it to the Flipper's microSD under the `tama_p1/` folder
   (final path: `SD:/tama_p1/rom.bin`).

The strictly legal route is dumping the ROM from an original device you own.
This project does not distribute or link to copyrighted material.

## Installation

1. Copy `dist/tamagotchi_p1.fap` to the microSD under `apps/Games/` (via
   qFlipper, or directly with `ufbt launch` if building from source).
2. Place the ROM as described above.
3. On the Flipper: `Apps → Games → Tamagotchi`.

## Controls

| Button | Function |
|---|---|
| Left | A button (move selection) |
| OK | B button (confirm) |
| Right | C button (cancel) |
| Up (short) | Turbo on/off (max-speed emulation) |
| Up (long) | Vibration on/off for beeps |
| Down (short) | Volume: high → low → mute |
| Down (long) | **Reset pet** (deletes save, fresh egg) |
| Back (long) | Exit (saves automatically) |

The status line under the screen shows turbo (`>>`), volume and vibration,
or catch-up progress (`Catching up... N%`).

## How to play

This is the real 1996 Tamagotchi, so it plays exactly like the original
keychain. On first launch (or after a reset) you set the clock — **A**
changes the value, **B** confirms — and an egg hatches about 5 minutes
later (tip: turbo makes the wait instant).

All menus follow the same rhythm: **A** moves / changes option, **B**
confirms, **C** cancels.

The 8 icons are laid out in two columns to the right of the LCD.
Inner column, top to bottom:

| Icon | What it does |
|---|---|
| **Food** (fork & knife) | Feed: **A** toggles rice (fills Hunger hearts) / snack (raises Happy but adds weight), **B** feeds, **C** exits. |
| **Light** | Turn the light off when your pet falls asleep, back on when it wakes. Leaving it on while sleeping leads to sickness. |
| **Game** (ball) | Mini-game, 5 rounds: guess which way the pet will look — **A** = left, **B** = right. Win 3+ rounds to raise Happy hearts (and lose weight). **C** quits. |
| **Medicine** (syringe) | Use when the skull icon appears. Sometimes takes 2–3 doses. |

Outer column, top to bottom:

| Icon | What it does |
|---|---|
| **Bathroom** (duck) | Clean up droppings. Leaving them around causes sickness. |
| **Meter** (face) | Status pages (**B** to flip): age/weight, discipline, Hunger hearts, Happy hearts. |
| **Discipline** (megaphone) | Scold when the pet beeps for attention while needing nothing (all hearts full). Builds the discipline bar. |
| **Attention** (crying face) | Lights up when the pet needs something — check the meter to see what. |

**The care loop:** when it beeps, check what it needs (hunger, happiness,
droppings, light, sickness) and take care of it. How well you care —
hearts kept full, quick cleanups, proper discipline — decides which
character it evolves into. Neglect it for too long and it dies, exactly
like in 1996; with real-time catch-up enabled, time passes even while the
app is closed, so don't disappear for a week.

### From GMMan's original emulator
- Full E0C6S46 emulation via TamaLIB: the real 1996 game with its sprites,
  evolutions, care mechanics and deaths.
- Sound with the original frequencies.

### Added in this fork
- **Auto-save** — on exit and every ~2 minutes, the full emulator state
  (CPU, timers, interrupts, memory) is persisted to `SD:/tama_p1/save.bin`
  and restored on launch. Versioned v2 format with magic header;
  backward-compatible with v1 saves.
- **Real-time catch-up** — the save records the RTC time; on relaunch, the
  time that passed while the app was closed is emulated at max speed
  (capped at 6 h per launch). Your pet "lives" even when you're not
  watching, like the original keychain. Note: the Flipper does not run apps
  in the background — while closed there are no alerts; needs pile up and
  you find them when you return.
- **Turbo** — max-speed emulation at the press of a button (no more waiting
  for hatching or evolutions).
- **In-app reset** — no file juggling needed.
- **3 volume levels + mute** and **optional vibration** on beeps.
- **Bigger display** — LCD at 3x scale (96x48) with the 8 icons in two
  columns on the right and a status line.

### Modern firmware compatibility fixes
The original 2022 code does not work on current firmware. This fork fixes:
- **TIM2 without clock**: modern firmware gates peripheral clocks by
  default; without `furi_hal_bus_enable(FuriHalBusTIM2)` the emulation
  freezes and the screen stays blank.
- **Speaker crash**: calling `furi_hal_speaker_start` without acquiring the
  speaker first triggers `furi_check failed` on the first beep on modern
  firmware.
- API migration: `m-string` → `FuriString`, callbacks with `void*`
  signatures.
- Emulation thread stack increased from 1 KB to 4 KB.

## Building from source

```
pip install ufbt
ufbt update --channel=release   # SDK for the current release firmware
ufbt                            # produces dist/tamagotchi_p1.fap
ufbt launch                     # build, install over USB and launch
```

## Credits

- **[Jean-Christophe Rona (jcrona)](https://github.com/jcrona/tamalib/)** —
  author of TamaLIB, the E0C6S46 emulation library that makes all of this
  possible, and of MCUGotchi.
- **[GMMan](https://github.com/GMMan/flipperzero-tamagotch-p1)** — author of
  the original Flipper Zero port this fork is based on.
- The **Flipper Devices** team — for the SDK and ufbt.
- Improvements in this fork: luigi.

## License

This project is free software under **GPL-2.0** (see [LICENSE](LICENSE)),
the same license as TamaLIB, whose terms require derivative works to remain
under the GPL. The TamaLIB code included in `lib/tamalib/` keeps its
original copyright © Jean-Christophe Rona.

The Tamagotchi ROM is © Bandai and is expressly outside this license and
this repository.
