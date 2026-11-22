#include "component_meter.h"
#include <cmath>

namespace {
template<class ActivationStatus>
MeasurementResult activation_result(ActivationStatus status, const char* configuration_error,
                                    const char* other_error = "Meter activation failed")
{
    if (status == ActivationStatus::Ok)
        return MeasurementResult::success(0.0f);
    if (status == ActivationStatus::HardwareUnavailable)
        return MeasurementResult::failure(MeasurementStatus::HardwareUnavailable,
                                           "Measurement hardware busy or not ready");
    return MeasurementResult::failure(MeasurementStatus::ActivationFailed,
        status == ActivationStatus::ChannelConfigurationFailed ? configuration_error : other_error);
}
}

ComponentMeter::ComponentMeter(HardwareManager& hardware)
    : hardware_(hardware), resistance_(hardware), capacitance_(hardware), inductance_(hardware)
{
}

bool ComponentMeter::begin(Mode initial_mode)
{
    if (!hardware_.begin()) {
        result_ = {};
        result_.mode = initial_mode;
        result_.status = Status::InitializationFailed;
        result_.error_message = "Shared hardware initialization failed";
        return false;
    }
    return select_mode(initial_mode);
}

void ComponentMeter::stop()
{
    switch (mode_) {
    case Mode::Resistance: resistance_.deactivate(); break;
    case Mode::Capacitance: capacitance_.deactivate(); break;
    case Mode::Inductance: inductance_.deactivate(); break;
    case Mode::None: break;
    }
    mode_ = Mode::None;
    result_ = {};
}

bool ComponentMeter::select_mode(Mode mode)
{
    if (!hardware_.ready()) {
        result_ = {};
        result_.mode = mode;
        result_.status = Status::HardwareUnavailable;
        result_.error_message = "HardwareManager not initialized";
        return false;
    }
    if (mode == Mode::None) {
        stop();
        return true;
    }
    if (mode == mode_)
        return true;
    stop(); // Release old hardware before configuring the next meter.
    MeasurementResult activation = MeasurementResult::failure(
        Status::ActivationFailed, "Unknown meter mode");
    switch (mode) {
    case Mode::Resistance:
        activation = activation_result(resistance_.activate(), "Resistance ADC setup failed"); break;
    case Mode::Capacitance:
        activation = activation_result(capacitance_.activate(), "Capacitance ADC setup failed"); break;
    case Mode::Inductance:
        activation = activation_result(inductance_.activate(), "Capture channel setup failed",
                                       "Inductance timer start failed"); break;
    case Mode::None: return true;
    }
    result_.mode = mode;
    if (activation.status != Status::Ok) {
        result_.status = activation.status;
        result_.error_message = activation.error_message;
        return false;
    }
    mode_ = mode;
    return true;
}

bool ComponentMeter::measurement_due() const
{
    // Unsigned subtraction handles the millisecond counter wrapping.
    return mode_ != Mode::None &&
           (!measured_ || HAL_GetTick() - last_completed_ms_ >= interval_ms);
}

bool ComponentMeter::update()
{
    if (!measurement_due())
        return false;
    MeasurementResult measurement;
    switch (mode_) {
    case Mode::Resistance: measurement = resistance_.read_measurement(); break;
    case Mode::Capacitance: measurement = capacitance_.read_measurement(); break;
    case Mode::Inductance: measurement = inductance_.read_measurement(); break;
    case Mode::None: return false;
    }
    last_completed_ms_ = HAL_GetTick();
    measured_ = true;
    if (measurement.status == Status::Ok && !std::isfinite(measurement.value))
        measurement = MeasurementResult::failure(Status::InvalidReading, "Non-finite measurement");
    result_ = {mode_, measurement.value, measurement.status, measurement.error_message};
    return true;
}
