#ifndef ECHO_MEASUREMENT_H
#define ECHO_MEASUREMENT_H

#include <limits>

enum class MeasurementStatus {
    NoReading,
    Ok,
    Inactive,
    Timeout,
    AdcError,
    OutOfRange,
    TimerError,
    CaptureOverrun,
    HardwareUnavailable,
    InitializationFailed,
    ActivationFailed,
    InvalidReading
};

struct MeasurementResult {
    // Base SI units: ohms, farads or henries. Only valid when status == Ok.
    float value = std::numeric_limits<float>::quiet_NaN();
    MeasurementStatus status = MeasurementStatus::NoReading;
    // Static string literals only: safe to copy results, no heap allocation.
    // Empty for a valid reading or when no reading has been attempted.
    const char* error_message = "";

    static MeasurementResult success(float value)
    {
        return {value, MeasurementStatus::Ok, ""};
    }

    static MeasurementResult failure(MeasurementStatus status, const char* message)
    {
        return {std::numeric_limits<float>::quiet_NaN(), status, message};
    }
};

#endif
