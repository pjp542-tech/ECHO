# Architecture

This document outlines the structure of the ECHO student project.

## Bird's-eye view

ECHO is a component meter built on a NUCLEO-G431KB. It measures resistance, capacitance and inductance, shows the result on a 20x4 I2C LCD and is controlled with two buttons. It measures unpowered components that are out of circuit.

### Board

The STM32CubeMX-generated setup code and ST's HAL. It starts the board's peripherals, then hands control to the application through `setup()` and `loop()`.

### Meters

One class each for resistance, capacitance and inductance. A meter owns its pins only between `activate()` and `deactivate()`, and reports every outcome as `MeasurementResult`.

### Display and buttons

An I2C LCD driver and two buttons.

## Code map

| File | What it does | Written by |
|---|---|---|
| `src/main.cpp` | TODO | Salah |
| `include/app.h` | TODO | Salah |
| `src/meter_app.cpp`, `include/meter_app.h` | TODO | Salah |
| `src/component_meter.cpp`, `include/component_meter.h` | TODO | Salah |
| `src/resistance.cpp`, `include/resistance.h` | TODO | Salah |
| `src/capacitance.cpp`, `include/capacitance.h` | Code for the capacitance meter. Does one charge then discharge loop, estimating C to find the optimum resistor during the charge and finding a more accurate value on discharge| Jamie |
| `src/inductance.cpp`, `include/inductance.h` | TODO | Treya |
| `src/hardware_manager.cpp`, `include/hardware_manager.h` | TODO | Salah |
| `include/measurement.h` | TODO | Salah |
| `src/buttons.cpp`, `include/buttons.h` | TODO | Salah |
| `src/i2c_lcd.c`, `include/i2c_lcd.h`, `include/lcd.h` | TODO |  |
| `CMakeLists.txt`, `cmake/`, `build.sh`, `build.ps1`, `.vscode/` | TODO | Salah |
| `Core/`, `ECHO.ioc`, `Drivers/` | TODO | Salah |

## Class reference

What each class does.

### MeterApp

- `MeterApp(hardware)` — Connects the measurement controller, buttons and LCD to the shared hardware.
- `begin()` — Initialises shared hardware, LCD and buttons without selecting a meter, returning whether meter setup succeeded.
- `update()` — Runs the blocking menu flow; called repeatedly by the application loop.

### ComponentMeter

- `ComponentMeter(hardware)` — Creates the three meter objects using the same hardware manager.
- `begin(initial_mode = Mode::None)` — Initialises shared hardware with no active meter by default; MeterApp selects a mode after a menu choice.
- `select_mode(mode)` — Releases the previous meter and activates the requested one.
- `stop()` — Deactivates the current meter and clears the latest result while preserving the sequence counter.
- `update()` — Performs a blocking measurement when due and returns true when a new result, including an error, is published.
- `measurement_due()` — Reports whether a meter is active and no measurement has completed yet or 500 ms have passed since the last completion.
- `mode()` — Returns the currently active measurement mode.
- `latest()` — Returns a reference to the latest result, including value, mode, status, error message, sequence and completion time.

### HardwareManager

- `HardwareManager(adc, timer, i2c)` — Stores references to the shared HAL peripheral handles without initialising hardware.
- `begin()` — Sets safe idle pin states, calibrates the ADC and enables the cycle counter, returning whether initialisation succeeded.
- `ready()` — Reports whether shared hardware initialisation has completed successfully.
- `try_acquire(owner)` — Attempts to claim exclusive measurement hardware ownership without waiting, returning whether it succeeded.
- `release(owner)` — Releases measurement hardware ownership only when the supplied owner holds it.
- `adc()` — Returns a reference to the shared ADC handle.
- `timer()` — Returns a reference to the shared timer handle.
- `i2c()` — Returns a reference to the shared I2C handle.
- `configure_adc(sense, channel, sampling_time)` — Sets the sense pin to analogue mode and configures the ADC channel and sampling time, returning success.
- `configure_pin(pin, mode, alternate = 0)` — Configures a GPIO pin's mode and alternate function with no pull resistor and low speed.
- `write(pin, state)` — Writes the requested HIGH or LOW state to a GPIO pin.

### ResistanceMeter

- `ResistanceMeter(hardware)` — Stores references to the hardware manager and shared ADC without accessing hardware.
- `activate()` — Claims shared hardware and configures ADC1 channel 1 and the resistor bank, returning an activation status.
- `read_measurement()` — Scans the four reference resistors and returns the best resistance reading in ohms, or an error result.
- `deactivate()` — Releases the resistor pins to high impedance, stops ADC conversions and releases hardware ownership.

### CapacitanceMeter

- Measures capacitance by timing how long the capacitor takes to discharge using formula `C = dt / (R ln 4)`. Where dt is the time it takes to discharge from 80-20 percent.
- The meter picks from one of 4 resistors with a quick charge on the 1k resistor and estimating the capaitance.
- Results out of range are reported as "too large" or "too small".

### InductanceMeter

- TODO Treya

### Buttons

- `Buttons(modebtn, measurebtn)` — Stores the GPIO pins used by the mode and measure buttons.
- `begin()` — Configures input pull-ups and initialises debounce state so a button held at startup does not generate a press.
- `poll()` — Returns one event per debounced button press after 30 ms of stable input; call repeatedly from the application loop.

### MeasurementResult

- `success(value)` — Creates an `Ok` result containing a base-SI value and an empty error message.
- `failure(status, message)` — Creates a failed result containing NaN, the supplied status and a static error-message string.
