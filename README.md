# ECHO

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

- `CapacitanceMeter(hardware)` — Stores references to the hardware manager and shared ADC without accessing hardware.
- `activate()` — Claims shared hardware and configures ADC1 channel 1 for capacitance measurements, returning an activation status.
- `read_measurement()` — Performs a blocking charge/discharge measurement and returns capacitance in farads, or an error result.
- `deactivate()` — Releases the resistor pins to high impedance, stops ADC conversions and releases hardware ownership.

### InductanceMeter

- `InductanceMeter(hardware)` — Stores references to the hardware manager and shared timer without accessing hardware.
- `activate()` — Claims shared hardware and configures TIM2 channel 2, PA1 capture and PA4 excitation, returning an activation status.
- `read_measurement()` — Excites the circuit and measures its oscillation period to return inductance in henries, or an error result.
- `deactivate()` — Disconnects excitation, stops capture, returns PA1 to high impedance and releases hardware ownership.

`HardwareManager`, `ResistanceMeter`, `CapacitanceMeter` and `InductanceMeter` each delete their copy constructor and copy assignment operator to prevent copying hardware owners.

### Buttons

- `Buttons(modebtn, measurebtn)` — Stores the GPIO pins used by the mode and measure buttons.
- `begin()` — Configures input pull-ups and initialises debounce state so a button held at startup does not generate a press.
- `poll()` — Returns one event per debounced button press after 30 ms of stable input; call repeatedly from the application loop.

### MeasurementResult

- `success(value)` — Creates an `Ok` result containing a base-SI value and an empty error message.
- `failure(status, message)` — Creates a failed result containing NaN, the supplied status and a static error-message string.
