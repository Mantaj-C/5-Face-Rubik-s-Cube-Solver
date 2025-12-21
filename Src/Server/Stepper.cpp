#include "Stepper.h"

/* ------------------------------------------------------------
   Constructor
   - Initializes a stepper motor with:
       - Enable pin
       - Direction pin
       - Step pin
       - Microstepping control pins (MS1, MS2, MS3)
   - Can share a microstepping mode via sharedMode pointer
   - Sets initial direction and enables the driver
   ------------------------------------------------------------ */
Stepper::Stepper(int enable_pin, int dir, int step,
                 int MS1, int MS2, int MS3,
                 StepMode* sharedMode,
                 StepDirection direction)
    : gpio(GPIO::getInstance()),
      enable_pin(enable_pin),
      direction_pin(dir),
      step_pin(step),
      MS1(MS1),
      MS2(MS2),
      MS3(MS3),
      shared_mode_ptr(sharedMode),
      local_mode(FULL),
      direction(direction),
      position(0.0f)
{
    // Enable the stepper driver by default
    enable();

    // Set initial direction pin
    gpio.write_digi(direction_pin, direction);

    // Apply microstepping mode (either shared or local)
    applyMode();
}

Stepper::~Stepper() {}

/* ------------------------------------------------------------
   setMode(mode)
   - Sets the step mode for this motor
   - If a shared_mode_ptr is provided, it writes into the shared
     mode (used by multiple steppers).
   - Otherwise, it uses this motor's local_mode.
   - Then calls applyMode() to update MS1/MS2/MS3.
   ------------------------------------------------------------ */
void Stepper::setMode(StepMode mode) {
    if (shared_mode_ptr) {
        *shared_mode_ptr = mode;
    } else {
        local_mode = mode;
    }
    applyMode();
}

/* ------------------------------------------------------------
   getMode()
   - Returns the currently active step mode:
       - Shared mode if shared_mode_ptr is set
       - Local mode otherwise
   ------------------------------------------------------------ */
Stepper::StepMode Stepper::getMode() const {
    return shared_mode_ptr ? *shared_mode_ptr : local_mode;
}

/* ------------------------------------------------------------
   applyMode()
   - Actually applies the microstepping mode to the MS1/MS2/MS3
     pins based on the current mode (FULL, HALF, etc).
   - Uses a lookup table to avoid repeated if/switch logic.
   ------------------------------------------------------------ */
void Stepper::applyMode() {
    StepMode mode = shared_mode_ptr ? *shared_mode_ptr : local_mode;

    // Lookup table: MS1, MS2, MS3 for each mode
    static const bool microstep_pins[][3] = {
        /* FULL      */ {false, false, false},
        /* HALF      */ {true,  false, false},
        /* QUARTER   */ {false, true,  false},
        /* EIGHTH    */ {true,  true,  false},
        /* SIXTEENTH */ {true,  true,  true}
    };

    const bool* config = microstep_pins[mode];

    // Write microstepping configuration to pins
    gpio.write_digi(MS1, config[0]);
    gpio.write_digi(MS2, config[1]);
    gpio.write_digi(MS3, config[2]);
}

/* ------------------------------------------------------------
   step(steps, direction, freq)
   - Low-level stepping function:
       - Handles direction changes
       - Applies a basic acceleration / deceleration profile
       - Tries to keep total move time ≈ (steps / freq) seconds
   - steps: number of step pulses
   - direction: CLOCKWISE or COUNTER_CLOCKWISE
   - freq: nominal step frequency (Hz)
   ------------------------------------------------------------ */
void Stepper::step(int steps, StepDirection direction, int freq) {
    // If direction changed (and this isn't the special 180 case),
    // update the direction pin and add a short settling delay.
    if (direction != this->direction && steps != 180) {
        gpio.write_digi(direction_pin, direction);
        this->direction = direction;
        gpioDelay(10000 * freq / 800);  // small dir-change delay
    }

    // Total allowed movement time in microseconds
    int total_time_us = 1e6 * steps / freq;

    // Simple acceleration profile:
    // 20% ramp-up + 60% cruise + 20% ramp-down
    int ramp_steps   = steps * 0.2;
    int cruise_steps = steps - 2 * ramp_steps;

    // Base delay between steps for target frequency
    double base_delay = 1e6 / freq;

    // Precompute all delays (per-step)
    std::vector<int> delays;
    delays.reserve(steps);

    // Acceleration: start slower, then speed up
    for (int i = 0; i < ramp_steps; ++i) {
        double scale = 1.0 - static_cast<double>(i) / ramp_steps; // 1 → 0
        delays.push_back(static_cast<int>(base_delay * (1.0 + scale * 0.5)));
    }

    // Cruise: constant speed
    for (int i = 0; i < cruise_steps; ++i) {
        delays.push_back(static_cast<int>(base_delay));
    }

    // Deceleration: slow down again
    for (int i = 0; i < ramp_steps; ++i) {
        double scale = static_cast<double>(i) / ramp_steps; // 0 → 1
        delays.push_back(static_cast<int>(base_delay * (1.0 + scale * 0.5)));
    }

    // Adjust delays to match desired total time as closely as possible
    int sum_delays = std::accumulate(delays.begin(), delays.end(), 0);
    double adjust_factor = static_cast<double>(total_time_us) / sum_delays;
    for (int& d : delays) {
        d = static_cast<int>(d * adjust_factor);
    }

    // Perform actual stepping based on computed delays
    for (int i = 0; i < steps; ++i) {
        gpio.write_digi(step_pin, true);
        gpioDelay(delays[i] / 2);
        gpio.write_digi(step_pin, false);
        gpioDelay(delays[i] / 2);
    }

    std::cout << "Done stepping " << steps << " steps" << std::endl;
}

/* ------------------------------------------------------------
   rotate(angle, direction, freq)
   - High-level function:
       - Converts a requested rotation angle into steps based on:
            - 200 steps per revolution (base)
            - Current microstepping mode
       - Delegates to step()
       - Tracks the logical "position" in degrees
   ------------------------------------------------------------ */
void Stepper::rotate(float angle, StepDirection direction, int freq) {
    int base_steps_per_rev = 200;  // standard 1.8°/step motor

    StepMode mode = shared_mode_ptr ? *shared_mode_ptr : local_mode;

    // Microstepping multiplier
    int multiplier = 1;
    switch (mode) {
        case FULL:      multiplier = 1;  break;
        case HALF:      multiplier = 2;  break;
        case QUARTER:   multiplier = 4;  break;
        case EIGHTH:    multiplier = 8;  break;
        case SIXTEENTH: multiplier = 16; break;
    }

    // Convert angle → steps
    float steps = (angle / 360.0f) * base_steps_per_rev * multiplier;

    // Execute motion
    step(static_cast<int>(steps), direction, freq);

    // Track logical angle (can be used for soft-limits or debugging)
    position += angle;
}

/* ------------------------------------------------------------
   enable()
   - Enables the stepper driver (usually active low).
   - Implementation assumes LOW = enabled for the driver.
   ------------------------------------------------------------ */
void Stepper::enable() {
    gpio.write_digi(enable_pin, false);
}

/* ------------------------------------------------------------
   disable()
   - Disables the stepper driver (to reduce heat/hold torque).
   - Implementation assumes HIGH = disabled for the driver.
   ------------------------------------------------------------ */
void Stepper::disable() {
    gpio.write_digi(enable_pin, true);
}

/* ------------------------------------------------------------
   getDirection()
   - Returns the current stored direction of this stepper.
   ------------------------------------------------------------ */
Stepper::StepDirection Stepper::getDirection() const {
    return direction;
}