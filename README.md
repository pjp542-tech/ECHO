# Resistance meter functions

| Function | What it does |
| --- | --- |
| `select_range(range)` | Switches the MUX to the selected known resistor. |
| `initialise_mux()` | Configures the MUX control pins and selects the first resistor. |
| `initialise_adc()` | Calibrates the ADC and configures the divider voltage input. |
| `read_adc(reading)` | Takes one ADC reading. Returns whether it succeeded. |
| `find_optimal_known_resistor()` | Takes one reading per known resistor and selects the one closest to half the ADC range. Returns the selected range, or `-1` if all readings fail. |
| `calculate_resistance(range, reading, resistance)` | Converts an ADC reading to resistance, including MUX resistance compensation. Rejects invalid ranges and saturated readings. |
| `continuous_reading_loop()` | Selects the best resistor, stores one valid measurement in `resistance_ohms` and waits 500 ms. Called repeatedly for continuous readings. |
| `read_average_20_seconds(average_ohms)` | Selects the best resistor once, then averages valid resistance readings spaced roughly 500 ms apart over 20 seconds. Stores the result in `average_ohms` and `resistance_ohms`; returns false if no valid readings are available. |
| `setup()` | Initialises the MUX and ADC. |
| `loop()` | Repeatedly calls continuous reading or 20-second averaging, depending on `use_20_second_average`. |
