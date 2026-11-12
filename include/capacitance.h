#ifndef ECHO_CAPACITANCE_H
#define ECHO_CAPACITANCE_H

// error codes returned by capacintance measure
enum cap_meter_error_codes{
    cap_ok = 0,
    cap_too_large = -1,
    cap_too_small = -2,
 };

// once to initiate at setup
void capacitance_init(void);

// measures capacitance in farads, returns error codes if out of range 
float capacitance_measure(void);

#endif
