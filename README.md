# ECHO

## Where to write code

- **`src/main.cpp`**: `setup()` runs once; `loop()` runs repeatedly.
- **`src/`**: add more `.c` / `.cpp` files. They build automatically.
- **`include/`**: put shared headers here. Include `app.h` to use the STM32 HAL.
- **`ECHO/`**: generated board setup and drivers. Hidden in VS Code; normally leave it alone.

## Capacitance meter

`include/capacitance.h` declares `CapacitanceMeter`; `src/capacitance.cpp`
contains the hardware control, timing algorithm and resistor calibration.
The public operations match the resistance meter:

| Operation | Purpose |
| --- | --- |
| `CapacitanceMeter(ADC_HandleTypeDef &adc)` | Store the ADC reference; no hardware access |
| `activate()` | Configure sense input, ADC channel/calibration, resistor pins and cycle counter |
| `read_measurement()` | Return one capacitance measurement in farads as a float |
| `deactivate()` | Stop ADC conversions and leave all resistor pins high impedance |

```cpp
CapacitanceMeter c(hadc1);
// After CubeMX hardware initialisation:
if (c.activate() == CapacitanceMeter::Status::Ok) {
    float capacitance = c.read_measurement();
    // Use capacitance here; check std::isnan(capacitance) for failure.
}
c.deactivate(); // Before selecting another meter.
```

Readings return `NAN` if inactive, ADC conversion fails, a measurement phase
times out, or capacitance is below 100 pF. Include `<cmath>` to use
`std::isnan`. The former negative error codes and free functions are removed.
Activation returns a status so the application can handle setup failures.

`src/main.cpp` uses object `c` and variable `capacitance`, with no serial
printing, averaging or application delay. Measurement calls are blocking;
individual charge/discharge phases retain the existing 8-second timeout.
The application should schedule calls and handle failed readings as needed.

The existing 20%-to-80% charging estimate selects a resistor; the final
80%-to-20% discharge interval gives `C = time / (R * ln(4))`. Calibration
values remain at the top of `src/capacitance.cpp`:

| Resistor | MCU pin |
| --- | --- |
| 1 kOhm | PA9 |
| 10 kOhm | PA12 |
| 100 kOhm | PB6 |
| 1 MOhm | PA7 |

Sense input remains PB0 (D3), ADC1 channel 15. Deactivate the previous meter
before activating this one when sharing the ADC. The ADC must already be
initialised for single software-triggered 12-bit conversions by CubeMX.
The DWT cycle counter is shared: activation enables it without resetting it,
and deactivation leaves it running. High-impedance idle stops excitation;
it does not guarantee that a connected capacitor is fully discharged.

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
