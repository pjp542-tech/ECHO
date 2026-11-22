#ifndef ECHO_COMPONENT_METER_H
#define ECHO_COMPONENT_METER_H

#include "resistance.h"
#include "capacitance.h"
#include "inductance.h"
#include <limits>

class ComponentMeter {
public:
    enum class Mode { None, Resistance, Capacitance, Inductance };
    using Status = MeasurementStatus;
    struct Result {
        Mode mode = Mode::None;
        float value = std::numeric_limits<float>::quiet_NaN(); // ohms / farads / henries
        Status status = Status::NoReading;
        const char* error_message = ""; // Static text, empty when no error.
    };

    explicit ComponentMeter(HardwareManager& hardware);
    bool begin(Mode initial_mode = Mode::None);
    bool select_mode(Mode mode);
    void stop();
    // Call repeatedly from loop() (or ONE future worker thread).
    // Returns true only when a new reading is published. Acquisition is blocking.
    bool update();
    bool measurement_due() const;
    Mode mode() const { return mode_; }
    const Result& latest() const { return result_; }

private:
    HardwareManager& hardware_;
    ResistanceMeter resistance_;
    CapacitanceMeter capacitance_;
    InductanceMeter inductance_;
    Mode mode_ = Mode::None;
    Result result_;
    uint32_t last_completed_ms_ = 0;
    bool measured_ = false;
    static constexpr uint32_t interval_ms = 500;
};

#endif
