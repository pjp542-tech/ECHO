# ECHO

Component meter firmware for the STM32 NUCLEO-G431KB, with resistance,
capacitance and inductance measurements and a 20x4 I2C LCD.

Application code is in `src/` and `include/`. STM32 setup and drivers are in
`Core/` and `Drivers/`. The menu and button actions still need to be added.

Install the STM32Cube GNU tools, CMake and Ninja, then build on Windows:

```powershell
./build.ps1 -Action Build
```

Use `-Action Flash` to program the board or `-Action Doctor` to check the tools.
