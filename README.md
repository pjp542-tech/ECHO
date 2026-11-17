# ECHO

## Where to write code

- **`src/main.cpp`**: `setup()` runs once; `loop()` runs repeatedly.
- **`src/`**: add more `.c` / `.cpp` files. They build automatically.
- **`include/`**: put shared headers here. Include `app.h` to use the STM32 HAL.
- **`ECHO/`**: generated board setup and drivers. Hidden in VS Code; normally leave it alone.

## Inductance meter

`include/inductance.h` declares `InductanceMeter`. Hardware control and
calibration constants live in `src/inductance.cpp`. The public operations are:

| Operation | Purpose |
| --- | --- |
| Constructor | Store the TIM2 handle without touching hardware |
| `activate()` | Configure PA4 excitation and PA1 input capture, start channel 2 |
| `read_measurement()` | Return one measurement in henries as a float |
| `deactivate()` | Disconnect PA4, stop channel 2 capture and leave PA1 high impedance |

```cpp
InductanceMeter l(htim2);
// After CubeMX initialises TIM2:
if (l.activate() == InductanceMeter::Status::Ok) {
    float inductance = l.read_measurement();
    // Use inductance here. std::isnan(inductance) indicates failure.
}
l.deactivate(); // Before switching meters.
```

Include `<cmath>` for `std::isnan`. Inactive reads, capture timeouts,
overcapture, pulse timing failure and out-of-window periods return `NAN`.
There are no serial prints or averaging. `main.cpp` keeps the existing
500 ms interval between attempts. Each read is blocking, with a 5 ms timeout
per edge. Activation preserves the original 20 ms startup delay.

The original LC method and calibration are retained: a nominal 2 us pulse,
a 10 nF capacitor, period multiplied by 1.1, and a corrected-period
acceptance window of 5-100 us. These are adjustable constants at the top of
`src/inductance.cpp`. Inductance is calculated as `1 / (4*pi*pi*f*f*C)`.

The class requires TIM2 configured by CubeMX as a 10 MHz, 32-bit up-counter
(prescaler 16 for the existing 170 MHz timer clock, period 0xFFFFFFFF).
Activation restores rising-edge direct input capture on TIM2 channel 2,
PA1 / AF1, without changing the timer base configuration. Do not repurpose
the timer base while this meter is active. Deactivate the previous owner
before switching meters, and restore that base configuration if another
meter changed it. PA4 remains the excitation pin; PA1 receives the external
comparator output. Deactivation releases the circuit; it does not force it
to discharge. Constructor and inactive calls do not alter hardware.

## Build and run (Windows or Mac)

1. Open the **ECHO repository root** in VS Code (the folder containing this README, not the generated `ECHO/` subfolder) and install its recommended extensions.
2. In STM32Cube's bundle manager, install **GNU Tools for STM32, CMake and Ninja**.
   For the board, also install **STM32CubeProgrammer, GNU GDB for STM32 and ST-Link GDB Server**.
3. Run **Terminal > Run Task > ECHO: Check tools** once.
4. Build: **Ctrl+Shift+B** on Windows / **Cmd+Shift+B** on Mac.
5. With the Nucleo connected, run **ECHO: Flash**, or **F5** to debug at `setup()`.

After `git switch`, build as usual; branches can reuse the same build folder.
The build tasks reconfigure before building and automatically reset a CMake cache
that still points to a previous project location after a folder move or rename.

Mac uses the same tasks with a Bash launcher; no PowerShell is needed.
Tools are found in ST's default bundle folder or on PATH. Custom location: set
`STM32CUBE_BUNDLE_ROOT`. [ST's installation guide](https://dev.st.com/stm32cube-docs/stm32cubeide-vscode/latest/en/docs/markup/getting_started/installation.html).

## Regeneration (only when changing pins/peripherals)

1. Commit your work. Open `ECHO/ECHO.ioc` in CubeMX (use File > Open File if hidden).
2. Change the hardware settings. Keep **CMake**, **Keep User Code when re-generating**
   and the existing `ECHO/` output directory; leave Nucleo demo code disabled.
3. Generate, then build from the repository root as usual. Review and commit the generated changes.

Your `src/` and `include/` files stay separate from generation. CubeMX preserves the
connection in `ECHO/Core/Src/main.c` inside its **USER CODE** blocks:

- **Includes**: `#include "app.h"`
- **WHILE**: `setup();` just before `while (1)`
- **3**: `loop();` inside the loop

The build reports an error if a connection disappears. Add any peripheral handles
you need to `include/app.h`, e.g. `extern ADC_HandleTypeDef hadc1;` for ADC1.

Windows builds are verified. The Mac launcher is checked on Windows, but still needs
one real Mac/board check; live debugging and CubeMX regeneration are also untested.
