# Bidirectional diode meter (NUCLEO-G431KB)

The `diode-meter` branch tests a diode in both directions, reports its orientation,
forward voltage and estimated forward current, and classifies open circuits,
short circuits and other responses. It uses two GPIO drive pins, two ADC inputs
and two 1 kOhm resistors. This circuit replaces the previous MUX resistance-meter
wiring; no MUX is used by `DiodeMeter`.

## Current source status

The diode-meter application builds successfully with `.\build.ps1` (Debug).
The ADC member declaration matches the HAL type and implementation. The
unfinished LCD integration example is preserved in
`docs/diode-lcd-example.cpp.txt`, outside the compiled source tree; it needs
an actual `LcdUi` class and helpers before it can be integrated.
The firmware has not been flashed or tested on hardware as part of this check.

## Wiring

**Correction:** Terminal A sensing uses A1 / PA1, not D6. The earlier generated
schematic image labels this connection incorrectly and must not be followed for
that wire. Move the sense wire to A1 and flash the updated firmware together.


These board labels and GPIO assignments are those specified in `src/diode.cpp`.
Use the NUCLEO-G431KB pin labels; board **A4** is a drive output here, while
**Terminal A** is a test node, not a board pin label.

| Connection | Board pin | MCU pin / function |
| --- | --- | --- |
| Drive A, through R1 to Terminal A | A4 | PA5, push-pull output |
| Sense Terminal A, directly | A1 | PA1, ADC1 channel 2 |
| Drive B, through R2 to Terminal B | D13 | PB3, push-pull output |
| Sense Terminal B, directly | A0 | PA0, ADC1 channel 1 |

```text
A4 / PA5 ---- R1: 1 kOhm ----+---- Terminal A
                            |         |
A1 / PA1 -------------------+     diode under test
                                      |
A0 / PA0 -------------------+---- Terminal B
                            |
D13 / PB3 --- R2: 1 kOhm ----+
```

1. Disconnect power while changing the wiring. Remove the previous MUX and
   resistance-divider connections from the shared sense pin A0 / PA0.
2. Connect A4 / PA5 through a 1 kOhm resistor to Terminal A.
3. Connect A1 / PA1 directly to Terminal A, on the diode side of that resistor.
4. Connect D13 / PB3 through another 1 kOhm resistor to Terminal B.
5. Connect A0 / PA0 directly to Terminal B, on the diode side of that resistor.
6. Connect the isolated diode between Terminal A and Terminal B in either
   orientation, then power the board through USB.

Neither test terminal connects directly to GND or 3V3: the firmware reverses
which drive output is HIGH and which is LOW. Both series resistors must remain
in circuit. At the nominal 3.30 V supply, a short gives approximately
`3.30 V / 2000 ohms = 1.65 mA`. Test only unpowered, isolated components; do not
apply an external voltage to the test terminals.

PB3 is also labelled `T_SWO` in the generated `main.h`; it is repurposed as a
GPIO by this meter, so it cannot simultaneously be used for SWO trace.

## Code structure and lifecycle

| File | Responsibility |
| --- | --- |
| `include/diode.h` | `DiodeMeter` interface, status/result enums and measurement fields |
| `src/diode.cpp` | GPIO setup, ADC sampling, polarity reversal and classification |
| `src/main.cpp` | Application `setup()` / `loop()` and serial output |
| `include/app.h` | C-compatible declarations of the application hooks |
| `ECHO/Core/Src/main.c` | Generated HAL, clock, ADC and board initialization; calls the hooks |

The application creates `DiodeMeter diode_meter(hadc1)`. Its constructor only
stores an ADC reference; CubeMX must initialize that ADC and its clocks before
`setup()` calls `activate()`. The generated configuration uses a 12-bit ADC,
one conversion at a time, with continuous conversion disabled.

`activate()` configures PA1 and PA0 as analog inputs, sets PA5 and PB3 as
push-pull outputs initially LOW, and calibrates the ADC in single-ended mode.
On successful calibration it marks the meter active and returns `Status::Ok`.
The application calls `Error_Handler()` if activation fails.
`ChannelConfigurationFailed` is declared in the interface but is currently
unused: channel configuration failures during a measurement become `ReadError`.

`deactivate()` stops ADC conversions and sets both drive pins to high-impedance
analog mode when the meter is active. Repeated calls do nothing. An inactive
read returns `Inactive` without accessing hardware. Note that calibration
failure leaves the drive pins configured LOW with `active_` still false, so
`deactivate()` does not release them in that failure case.

## How a measurement works

Each call to `read_measurement()` performs the following sequence:

1. Drive A HIGH and B LOW, then wait 5 ms for settling.
2. Read Terminal A on ADC channel 2 and Terminal B on channel 1. Each terminal
   voltage is the average of eight conversions, using a 640.5-cycle sampling
   time and a 100 ms conversion timeout.
3. Set both outputs LOW and calculate `v_ab = max(Va - Vb, 0)`.
4. Drive A LOW and B HIGH and repeat the sampling, then set both outputs LOW
   and calculate `v_ba = max(Vb - Va, 0)`.
5. Classify the two voltage drops using the table below.

A successful full measurement uses 32 ADC conversions and 10 ms of deliberate
settling, plus conversion and software overhead. ADC failures return
`ReadError`; the direction routine resets both outputs LOW after its reads.
The application prints the result and waits 500 ms before the next test, so
its update period is the measurement time plus 500 ms.

ADC counts are converted using the assumed supply/reference voltage:

```text
terminal_voltage_V = average_ADC_counts / 4095 * 3.30
forward_current_mA = (3.30 - forward_voltage_V) / (1000 + 1000) * 1000
```

Current is estimated from the resistor values and nominal drive voltage; it
is not measured independently and is not held constant. For example, a 0.70 V
forward drop gives approximately 1.30 mA. The constants `vcc_volts`, `r1_ohms`
and `r2_ohms` in `src/diode.cpp` can be adjusted to measured values. GPIO voltage
drop and ADC/reference errors still affect accuracy.

## Understanding results

For each direction, a drop below 0.10 V counts as short, a drop from 0.10 V
through 2.95 V inclusive counts as conducting, and a drop above 2.95 V counts
as open.

| Result state | Condition | Meaning / serial output |
| --- | --- | --- |
| `GoodForward` | A-to-B conducts; B-to-A open | `Diode OK (A->B)`: anode at A, cathode at B |
| `GoodReverse` | B-to-A conducts; A-to-B open | `Diode OK (B->A)`: anode at B, cathode at A |
| `OpenCircuit` | Both directions open | `Open circuit / No diode` |
| `ShortCircuit` | Both directions short | `Shorted diode / Wire` |
| `FaultyLeaky` | Any other combination | `Faulty / Leaky / Resistor` |
| `Inactive` | Meter not activated | Application prints `Diode read error` |
| `ReadError` | ADC configuration/start/poll/stop failure | Application prints `Diode read error` |

`Measurement` returns `state`, `forward_voltage_v` in volts and
`forward_current_ma` in milliamps. For a good diode, the application converts
these values to integer mV and uA for printing. `FaultyLeaky` stores the smaller
of the two measured drops as its voltage and leaves current at zero. Other
non-good results leave both numeric fields at their default zero values.
Always interpret the state before using the numeric fields.

These are threshold-based classifications, not a full diode characterization.
`FaultyLeaky` also includes mixed short/conducting/open combinations; it does
not prove leakage specifically. A component requiring more than the available
test voltage, including some LEDs, may appear open. This circuit does not
measure reverse breakdown voltage.

The unfinished LCD example labels a good result below 0.45 V as `Schottky`,
from 0.45 V to below 1.40 V as `Silicon`, and at least 1.40 V as `LED`. These are
display heuristics, not part of `DiodeMeter`'s result classification or reliable
component identification. The example's D0/A5 footer does not implement buttons.

## Building and viewing serial output

Run these commands from the repository root (the directory containing this
README). The build requires CMake, Ninja and the Arm GNU toolchain; flashing
also requires STM32CubeProgrammer. `build.ps1` searches PATH and STM32Cube
bundle folders, including `STM32CUBE_BUNDLE_ROOT` when set.

```powershell
.\build.ps1 -Action Doctor   # Check tool discovery
.\build.ps1                 # Build Debug firmware
.\build.ps1 -Action Flash   # Build, program through SWD, verify and reset
```

Build
outputs are written under `build/Debug/` as `ECHO.elf`, `ECHO.hex` and `ECHO.bin`.
CMake discovers application `.c` and `.cpp` files under `src/` automatically.

The generated application initializes COM1 at 115200 baud. The intended serial
loop prints the diode result repeatedly; use the board's serial connection
with the COM1 settings in `ECHO/Core/Src/main.c` (115200, 8 data bits, no parity,
1 stop bit). Serial output is enabled in the current diode application.
