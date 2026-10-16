# ECHO

## Where to write code

- **`src/main.c`**: `setup()` runs once; `loop()` runs repeatedly.
- **`src/`**: add more `.c` / `.cpp` files. They build automatically.
- **`include/`**: put shared headers here. Include `app.h` to use the STM32 HAL.
- **`ECHO/`**: generated board setup and drivers. Hidden in VS Code; normally leave it alone.

The starter application is empty. The existing Nucleo pin/peripheral configuration is retained.

## Build and run (Windows or Mac)

1. Open the **ECHO repository root** in VS Code (the folder containing this README, not the generated `ECHO/` subfolder) and install its recommended extensions.
2. In STM32Cube's bundle manager, install **GNU Tools for STM32, CMake and Ninja**.
   For the board, also install **STM32CubeProgrammer, GNU GDB for STM32 and ST-Link GDB Server**.
3. Run **Terminal > Run Task > ECHO: Check tools** once.
4. Build: **Ctrl+Shift+B** on Windows / **Cmd+Shift+B** on Mac.
5. With the Nucleo connected, run **ECHO: Flash**, or **F5** to debug at `setup()`.

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
