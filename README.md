# Tamagotchi P1 Emulator para Flipper Zero — Edición Mejorada

Emulador del **Tamagotchi P1 original (1996)** corriendo de forma nativa en el
Flipper Zero. Este es un fork mejorado del proyecto de
[GMMan](https://github.com/GMMan/flipperzero-tamagotch-p1), construido sobre
[TamaLIB](https://github.com/jcrona/tamalib/) de Jean-Christophe Rona, con
arreglos de compatibilidad para firmware moderno y funcionalidades nuevas
(guardado, paso del tiempo real, turbo y más).

Compilado y probado contra firmware oficial **1.4.3 (API 87.1)**.

## ⚠️ Sobre la ROM (léelo primero)

Este emulador necesita la ROM del Tamagotchi P1, que es **propiedad de Bandai
y NO se incluye en este repositorio** (ni debe subirse nunca: está en el
`.gitignore` a propósito). Para conseguirla:

1. Busca el dump conocido como `tama.b` (12.288 bytes, el mismo set que usa
   MAME como "Bandai Tamagotchi", `tama.zip`).
2. Renómbralo a `rom.bin`.
3. Cópialo a la microSD del Flipper en la carpeta `tama_p1/`
   (ruta final: `SD:/tama_p1/rom.bin`).

La vía estrictamente legal es dumpear la ROM de un dispositivo original que
poseas. Este proyecto no distribuye ni enlaza material con copyright.

## Instalación

1. Copia `dist/tamagotchi_p1.fap` a la microSD en `apps/Games/` (con qFlipper,
   o directamente con `ufbt launch` si compilas desde código).
2. Coloca la ROM como se explica arriba.
3. En el Flipper: `Apps → Games → Tamagotchi`.

## Controles

| Botón | Función |
|---|---|
| Izquierda | Botón A (mover selección) |
| OK | Botón B (confirmar) |
| Derecha | Botón C (cancelar) |
| Arriba (corto) | Turbo on/off (emulación a máxima velocidad) |
| Arriba (largo) | Vibración on/off en los pitidos |
| Abajo (corto) | Volumen: alto → bajo → silencio |
| Abajo (largo) | **Reiniciar la mascota** (borra la partida, huevo nuevo) |
| Atrás (largo) | Salir (guarda automáticamente) |

La línea de estado bajo la pantalla muestra turbo (`>>`), volumen y vibración,
o el progreso de la puesta al día (`Al dia... N%`).

## Funcionalidades

### Del emulador original de GMMan
- Emulación completa del E0C6S46 vía TamaLIB: el juego real de 1996 con sus
  sprites, evoluciones, cuidados y muertes.
- Sonido con las frecuencias originales.

### Añadidas en este fork
- **Guardado automático** — al salir y cada ~2 minutos se persiste el estado
  completo del emulador (CPU, timers, interrupciones, memoria) en
  `SD:/tama_p1/save.bin` y se restaura al abrir. Formato v2 con
  magic/versión; retrocompatible con saves v1.
- **Paso del tiempo real (catch-up)** — el save registra la hora del RTC; al
  reabrir, el tiempo transcurrido con la app cerrada se emula a máxima
  velocidad (limitado a 6 h por apertura). Tu mascota "vive" aunque no la
  mires, como el llavero original. Nota: el Flipper no ejecuta apps en
  segundo plano — con la app cerrada no hay avisos; las necesidades se
  acumulan y las encuentras al volver.
- **Turbo** — emulación a máxima velocidad con un botón (eclosiones y
  evoluciones sin esperas).
- **Reinicio desde la app** — sin tocar archivos.
- **Volumen en 3 niveles + silencio** y **vibración opcional** en los pitidos.
- **Pantalla ampliada** — LCD a escala 3x (96x48) con los 8 iconos en dos
  columnas a la derecha y línea de estado.

### Arreglos de compatibilidad con firmware moderno
El código original (2022) no funciona en firmwares actuales. Este fork corrige:
- **TIM2 sin reloj**: los firmwares modernos apagan los periféricos por
  defecto; sin `furi_hal_bus_enable(FuriHalBusTIM2)` la emulación queda
  congelada y la pantalla en blanco.
- **Crash del altavoz**: `furi_hal_speaker_start` sin `acquire` previo provoca
  `furi_check failed` al primer pitido en firmware moderno.
- Migración de API: `m-string` → `FuriString`, callbacks con firma `void*`.
- Pila del hilo de emulación ampliada de 1 KB a 4 KB.

## Compilar desde código

```
pip install ufbt
ufbt update --channel=release   # SDK del firmware release actual
ufbt                            # genera dist/tamagotchi_p1.fap
ufbt launch                     # compila, instala por USB y lanza
```

## Créditos

- **[Jean-Christophe Rona (jcrona)](https://github.com/jcrona/tamalib/)** —
  autor de TamaLIB, la librería de emulación del E0C6S46 que hace posible
  todo esto, y de MCUGotchi.
- **[GMMan](https://github.com/GMMan/flipperzero-tamagotch-p1)** — autor del
  port original a Flipper Zero del que parte este fork.
- Equipo de **Flipper Devices** — por el SDK y ufbt.
- Mejoras de este fork: luigi.

## Licencia

Este proyecto es software libre bajo **GPL-2.0** (ver [LICENSE](LICENSE)),
la misma licencia de TamaLIB, cuyos términos obligan a mantener las obras
derivadas bajo GPL. El código de TamaLIB incluido en `lib/tamalib/` conserva
su copyright original © Jean-Christophe Rona.

La ROM del Tamagotchi es © Bandai y queda expresamente fuera de esta licencia
y de este repositorio.
