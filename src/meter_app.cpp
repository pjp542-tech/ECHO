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
    const bool meter_ready = component_meter_.begin(ComponentMeter::Mode::None);
    lcd_init(&lcd_); // Initialise and clear the 20x4 screen; leave it blank.
    buttons_.begin(); // Configure pull-ups and initialise debounce state.
    // The existing LCD driver has no status return; this reports meter setup only.
    return meter_ready;
}

float MeterApp::avg_measurement() {
    const int num_samples = 10; // Number of samples to average
    ComponentMeter::Result result = component_meter_.latest();
    double sum = 0.0;
    for (int i = 0; i < num_samples; ++i) {
        // update() returns false during the interval between fresh readings.
        while (!component_meter_.update()) {
            HAL_Delay(1);
        }
        result = component_meter_.latest();
        if (result.status != ComponentMeter::Status::Ok)
            return result.value;
        sum += result.value;
    }
    // Keep the mode and final sample's sequence/completion time with the mean.
    result.value = static_cast<float>(sum / num_samples);
    return result.value;
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

    std::snprintf(text, sizeof(text), "%.2f%s", scaled, prefixes[prefix]);
    return text;
}

void MeterApp::display_result(const char* Value) {
    lcd_clear(&lcd_);
    ComponentMeter::Result result = component_meter_.latest();
    if (result.status == ComponentMeter::Status::Ok) {
        // Display the measured value with appropriate units
        switch (result.mode) {

            case ComponentMeter::Mode::Resistance:
                lcd_print(&lcd_, 0, "Resistance");
                {
                    char final_result[48];
                    std::snprintf(final_result, sizeof(final_result), "%s ohms", Value);
                    lcd_print(&lcd_, 1, final_result);
                }
                break;

            case ComponentMeter::Mode::Capacitance:
                lcd_print(&lcd_, 0, "Capacitance");
                {
                    char final_result[48];
                    std::snprintf(final_result, sizeof(final_result), "%s F", Value);
                    lcd_print(&lcd_, 1, final_result);
                }
                break;

            case ComponentMeter::Mode::Inductance:
                lcd_print(&lcd_, 0, "Inductance");
                {
                    char final_result[48];
                    std::snprintf(final_result, sizeof(final_result), "%s H", Value);
                    lcd_print(&lcd_, 1, final_result);
                }
                break;
        }

        lcd_print(&lcd_, 2, "Mode: Menu");
        lcd_print(&lcd_, 3, "Measure: Repeat");
    }
}

void MeterApp::get_measurement()
{
    lcd_print(&lcd_, 1, "                    ");
    lcd_print(&lcd_, 2, "                    ");
    lcd_print(&lcd_, 1, "Measuring...");
    float result = avg_measurement();
    const char* result_text = sanatise_result(result);
    display_result(result_text);
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
    component_meter_.select_mode(mode);
    
    lcd_print(&lcd_, 1, "Connect Component");
    lcd_print(&lcd_, 2, "Press Measure");
    while (true) {
        Buttons::Events events = buttons_.poll();
        if (events.measurebtn_pressed) {
            // Measure the component
            get_measurement();
        } else if (events.modebtn_pressed) {
            // Return to menu
            break;
        }
    }
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
    int current_row = 0;
    lcd_clear(&lcd_);
    lcd_print(&lcd_, 0, "Select Meter:");
    lcd_print(&lcd_, 1, "  1. Resistance");
    lcd_print(&lcd_, 2, "  2. Capacitance");
    lcd_print(&lcd_, 3, "  3. Inductance");
    change_row(current_row);

    while (true) {
        Buttons::Events events = buttons_.poll();
        if (events.modebtn_pressed) {
            current_row = (current_row + 1) % 3; // Move to the next row
            change_row(current_row);
        } else if (events.measurebtn_pressed) {
            selected_meter(current_row);
            break; // Exit the menu after selection
        }
    }

}

void MeterApp::update()
{
    menu(); // Call the menu to select a meter mode
}
