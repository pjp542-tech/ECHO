#ifndef ECHO_CAPACITANCE_H
#define ECHO_CAPACITANCE_H

#include "hardware_manager.h"
#include "measurement.h"

// error codes returned by capacintance measure
enum cap_meter_error_codes{
    cap_ok = 0,
    cap_too_large = -1,
    cap_too_small = -2,
    cap_adc_error = -3,
    cap_channel_config_failed = -4,
    cap_not_initialised = -5,
 };


 class CapacitanceMeter
{
public:
    enum class Status
    {
        Ok,
        HardwareUnavailable,
        ChannelConfigurationFailed
    };

    explicit CapacitanceMeter(HardwareManager &hardware);
    CapacitanceMeter(const CapacitanceMeter&) = delete;
    CapacitanceMeter& operator=(const CapacitanceMeter&) = delete;
    // Call after HardwareManager::begin().
    Status activate();
    MeasurementResult read_measurement();
    void deactivate();

private:
    HardwareManager &hardware_;
    bool active_ = false;
};

#endif
