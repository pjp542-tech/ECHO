#ifndef ECHO_BUTTONS_H
#define ECHO_BUTTONS_H

#include "hardware_manager.h"

// Two normally-open switches to GND, using the MCU's internal pull-ups.
// Poll from the application loop, not from an interrupt. One event per press.
class Buttons {
public:
    struct Events { bool modebtn_pressed = false; bool measurebtn_pressed = false; };
    Buttons(Pin modebtn, Pin measurebtn) : pins_{modebtn, measurebtn} {}
    void begin();
    Events poll();

private:
    struct State {
        bool candidate = false;
        bool stable = false;
        uint32_t changed_ms = 0;
    };
    bool update(unsigned index, uint32_t now);
    Pin pins_[2];
    State states_[2];
    bool ready_ = false;
    static constexpr uint32_t debounce_ms = 30;
};

#endif
