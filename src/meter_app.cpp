#include "meter_app.h"
#include <cmath>
#include <cstdio>

MeterApp::MeterApp(HardwareManager& hardware)
    : component_meter_(hardware), buttons_(BoardPins::modebtn, BoardPins::measurebtn),
      lcd_{&hardware.i2c(), 0x27 << 1}
{
}

bool MeterApp::begin()
{
    // Initialises shared hardware (including ADC calibration) through the controller.
    const bool meter_ready = component_meter_.begin();
    lcd_init(&lcd_); // Initialise and clear the 20x4 screen; leave it blank.
    buttons_.begin(); // Configure pull-ups and initialise debounce state.
    if (meter_ready) menu();
    // The existing LCD driver has no status return; this reports meter setup only.
    return meter_ready;
}

void MeterApp::avg_measurement() {
    // update() returns false between acquisitions: never reuse latest() then.
    if (!component_meter_.update()) return;
    const ComponentMeter::Result result = component_meter_.latest();
    if (result.status != ComponentMeter::Status::Ok) {
        state_ = State::Ready;
        display_error(result);
        return;
    }
    sample_sum_ += result.value;
    if (++sample_count_ == num_samples) {
        state_ = State::Ready;
        display_result(sanatise_result(static_cast<float>(sample_sum_ / num_samples)));
    }
}

const char* MeterApp::sanatise_result(float result)
{
    static char text[48];
    static const char* const prefixes[] = {"p", "n", "u", "m", "", "k", "M"};
    double scaled = result;
    int prefix = 4; // Start in base units.
    if (std::isfinite(scaled) && scaled != 0.0) {
        while (std::fabs(scaled) < 1.0 && prefix > 0) {
            scaled *= 1000.0;
            --prefix;
        }
        while (std::fabs(scaled) >= 1000.0 && prefix < 6) {
            scaled /= 1000.0;
            ++prefix;
        }
        // If rounding would show 1000.00, move to the next available prefix.
        if (std::round(std::fabs(scaled) * 100.0) >= 100000.0 && prefix < 6) {
            scaled /= 1000.0;
            ++prefix;
        }
    }
    if (scaled == 0.0) scaled = 0.0; // Avoid displaying negative zero.

    std::snprintf(text, sizeof(text), "%.2f %s", scaled, prefixes[prefix]);
    return text;
}

void MeterApp::display_result(const char* Value) {
    lcd_clear(&lcd_);
        // Display the measured value with appropriate units
    switch (component_meter_.mode()) {

        case ComponentMeter::Mode::Resistance:
            lcd_print(&lcd_, 0, "Resistance");
            {
                char final_result[48];
                std::snprintf(final_result, sizeof(final_result), "%sohms", Value);
                lcd_print(&lcd_, 1, final_result);
            }
            break;

        case ComponentMeter::Mode::Capacitance:
            lcd_print(&lcd_, 0, "Capacitance");
            {
                char final_result[48];
                std::snprintf(final_result, sizeof(final_result), "%sF", Value);
                lcd_print(&lcd_, 1, final_result);
            }
            break;

        case ComponentMeter::Mode::Inductance:
            lcd_print(&lcd_, 0, "Inductance");
            {
                char final_result[48];
                std::snprintf(final_result, sizeof(final_result), "%sH", Value);
                lcd_print(&lcd_, 1, final_result);
            }
            break;
        case ComponentMeter::Mode::None:
            break;
    }

    lcd_print(&lcd_, 2, "Mode: Menu");
    lcd_print(&lcd_, 3, "Measure: Repeat");
}

void MeterApp::display_error(ComponentMeter::Result result) {
    const char* message = result.error_message;
    if (message == nullptr || *message == '\0')
        message = "No reading available";
    lcd_clear(&lcd_);
    lcd_print(&lcd_, 0, "       Error");
    for (uint8_t row = 1; row < 4 && *message; ++row) {
        while (*message == ' ' || *message == '\r') ++message;
        char line[21] = {};
        unsigned length = 0;
        unsigned last_space = 0;
        while (length < 20 && message[length] && message[length] != '\n') {
            if (message[length] == ' ') last_space = length;
            ++length;
        }
        // Keep whole words together; split only words longer than a row.
        if (length == 20 && message[length] && message[length] != ' ' &&
            message[length] != '\n' && last_space != 0)
            length = last_space;
        for (unsigned i = 0; i < length; ++i) line[i] = message[i];
        message += length;
        while (*message == ' ' || *message == '\r') ++message;
        if (*message == '\n') ++message;
        lcd_print(&lcd_, row, line);
    }
}
void MeterApp::get_measurement()
{
    lcd_clear(&lcd_);
    lcd_print(&lcd_, 1, "Measuring...");
    lcd_print(&lcd_, 3, "Mode: Cancel");
    sample_count_ = 0;
    sample_sum_ = 0.0;
    state_ = State::Measuring;
}



void MeterApp::selected_meter(int choice)
{
    lcd_clear(&lcd_);
    ComponentMeter::Mode mode;
    switch (choice) {
        case 0: mode = ComponentMeter::Mode::Resistance; lcd_print(&lcd_, 0, "Resistance"); break;
        case 1: mode = ComponentMeter::Mode::Capacitance; lcd_print(&lcd_, 0, "Capacitance"); break;
        case 2: mode = ComponentMeter::Mode::Inductance; lcd_print(&lcd_, 0, "Inductance"); break;
        default: return; // Invalid choice, do nothing
    }
    if (!component_meter_.select_mode(mode)) {
        state_ = State::Error;
        display_error(component_meter_.latest());
        return;
    }
    state_ = State::Ready;
    
    lcd_print(&lcd_, 1, "Connect Component");
    lcd_print(&lcd_, 2, "Press Measure");
}
void MeterApp::change_row(int row)
{
    if (row < 0 || row >= 3) return;

    // Menu choices 0..2 occupy LCD rows 1..3. Only redraw the markers.
    for (int choice = 0; choice < 3; ++choice) {
        lcd_gotoxy(&lcd_, 0, choice + 1);
        lcd_putchar(&lcd_, choice == row ? '>' : ' ');
        lcd_gotoxy(&lcd_, 19, choice + 1);
        lcd_putchar(&lcd_, choice == row ? '<' : ' ');
    }
}

void MeterApp::menu(){
    component_meter_.stop();
    state_ = State::Menu;
    current_row_ = 0;
    lcd_clear(&lcd_);
    lcd_print(&lcd_, 0, "Select Meter:");
    lcd_print(&lcd_, 1, "  1. Resistance");
    lcd_print(&lcd_, 2, "  2. Capacitance");
    lcd_print(&lcd_, 3, "  3. Inductance");
    change_row(current_row_);

}

void MeterApp::update()
{
    const Buttons::Events events = buttons_.poll();
    if (state_ == State::Menu) {
        if (events.modebtn_pressed) {
            current_row_ = (current_row_ + 1) % 3;
            change_row(current_row_);
        } else if (events.measurebtn_pressed) {
            selected_meter(current_row_);
        }
        return;
    }
    if (events.modebtn_pressed) {
        menu(); // Also cancels an unfinished average and releases the hardware.
        return;
    }
    if (state_ == State::Error) {
        if (events.measurebtn_pressed) selected_meter(current_row_);
        return;
    }
    if (state_ == State::Ready && events.measurebtn_pressed) get_measurement();
    if (state_ == State::Measuring) avg_measurement();
}
