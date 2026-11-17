# MUX resistance meter (NUCLEO-G431KB)

The MUX resistance meter uses a modular `ResistanceMeter` class with continuous
measurements. Serial printing is currently disabled. Averaging has been removed. Remove the direct GPIO-to-resistor
connections and restore the MUX circuit before flashing.

## Control wiring

| MUX control | Board pin | GPIO |
| --- | --- | --- |
| Address A0 (least significant bit) | D12 | PB4 |
| Address A1 | D9 | PA8 |
| Address A2 | D10 | PA11 |
| EN | D11 | PB5 |

MUX address A0 is NOT the board's analog A0 pin. Board A0 / PA0 is the
ADC input. This restores the original firmware's active-HIGH enable and
address codes 000..011 for S1..S4; use the original compatible MUX.
Physical IC pin numbers and supply connections depend on the MUX model.

## Resistor wiring

| MUX channel | Address A2 A1 A0 | Reference resistor |
| --- | --- | --- |
| S1 | 000 | 1 kOhm |
| S2 | 001 | 10 kOhm |
| S3 | 010 | 100 kOhm |
| S4 | 011 | 1 MOhm |

Connect MUX common to 3V3. Each channel connects through its reference
resistor to the common divider junction at board A0 / PA0. Connect the
unknown resistor from that junction to board GND. Restore the MUX's
original power wiring and share GND with the board. Measure unpowered,
isolated resistors only.

```text
3V3 ---- MUX common
          S1 ---- 1 kOhm ----+
          S2 --- 10 kOhm ----+
          S3 -- 100 kOhm ----+---- A0 / PA0 ---- unknown ---- GND
          S4 ---- 1 MOhm ----+
```

## Measurement

- `include/resistance.h`: public `ResistanceMeter` class, activation status type.
- `src/resistance.cpp`: MUX control, ADC calibration/configuration, automatic range selection and resistance conversion.
- `src/main.cpp`: creates the meter, calls `activate()` in `setup()`, and calls `read_measurement()` in `loop()`, then waits 500 ms. Serial printing is currently disabled.

The class accepts an ADC handle. Calibration constants are in `src/resistance.cpp`. Its constructor
does not touch hardware; call `activate()` after CubeMX initialises the ADC.
It owns ADC channel 1 and MUX control pins D9-D12 while active.
Call `deactivate()` before selecting another meter: it disables the MUX,
sets address outputs LOW and stops ADC conversions. Read attempts while
inactive return `NAN` without touching hardware. Call `activate()` again to
restore the sense pin and ADC channel. The class does not print or impose
a delay between measurement attempts.

`read_measurement()` scans all four ranges with 10 ms settling per range,
uses the successful scan sample closest to half scale and leaves its range
selected. A normal call blocks for approximately 50 ms. ADC timeouts can
make it longer. It returns resistance in ohms as a `float`, preserving
fractional ohms. Inactive, failed-scan and saturated readings return `NAN`;
a failed scan also disables the MUX. Use `std::isnan(R)` from `<cmath>` to
check for an invalid result before displaying or using it.

```cpp
ResistanceMeter meter(hadc1);
// After hardware initialisation:
if (meter.activate() == ResistanceMeter::Status::Ok) {
    float R = meter.read_measurement(); // Ohms; NAN indicates no valid reading.
    // Use R here.
}
meter.deactivate(); // When leaving this meter.
```

`R_unknown = (reference_ohms[range] + mux_ohms) * ADC / (4095 - ADC)`

Edit `reference_ohms` and `mux_ohms` at the top of `src/resistance.cpp`
to supply calibration values.
Defaults retain the existing nominal references and estimated 170 ohm MUX
correction. If calibrated reference values already include MUX resistance,
set `mux_ohms` to zero to avoid counting it twice.

## Build and serial testing

Run `./build.ps1` to build. Rewire first, then use
`./build.ps1 -Action Flash` to build and flash through SWD.

Serial printing is currently disabled in `src/main.cpp`. The loop still
calls `read_measurement()` continuously, with a 500 ms delay between calls.
There is no averaging mode.
