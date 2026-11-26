# Wiring 

How to setup the hardware for the ECHO project on a NUCLEO-G431KB. 

## Pin table

| Board pin | Used for |
|---|---|
| A0 (PA0) | shared ADC sensor |
| D1 (PA9) | 1 kΩ reference resistor |
| D2 (PA12) | 10 kΩ reference resistor |
| D6 (PB6) | 100 kΩ reference resistor |
| D0 (PA10) | 1 MΩ reference resistor |

TODO Salah, fill in which pins you use.

## Capacitance meter

Pick one breadboard row as the test node and wire it to `A0`. Connect one end of each reference resistor to the test node and the other end to its pin: the 1 kΩ to `D1`, the 10 kΩ to `D2`, the 100 kΩ to `D6` and the 1 MΩ to `D0`. The capacitor goes between the test node and GND.

Each resistor must be on its own pin, as the code uses their exact values. Connect electrolytic capacitors with + to the test node, and discharge them before connecting.

## Resistance meter 

TODO Salah

## Buttons

TODO Salah

## LCD

TODO Salah

## Inductance

TODO Treya

## Troubleshooting

| Symptom | Likely cause |
|---|---|
| Capacitance always "too large" | A0 is not connected to the test node |
| Readings jump around randomly | The sense wire is on the wrong pin |
| One range reads wrong | A resistor is on the wrong pin |
