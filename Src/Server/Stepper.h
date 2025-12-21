#pragma once
#include "GPIO.h"
#include <iostream>
#include <chrono>
#include <thread>
#include <vector>
#include <numeric>

/**
 * @brief Stepper motor driver class using pigpio GPIO control.
 *
 * Supports:
 *  - Direction control
 *  - Microstepping (Full → 1/16)
 *  - Shared microstepping mode between motors
 *  - Angle-based rotation
 *  - Basic acceleration profile (handled in .cpp)
 */
class Stepper {
public:

    /**
     * @brief Microstepping mode for the stepper motor.
     *
     * FULL       = 1 step per electrical pulse  
     * HALF       = 2 microsteps  
     * QUARTER    = 4 microsteps  
     * EIGHTH     = 8 microsteps  
     * SIXTEENTH  = 16 microsteps  
     */
    enum StepMode {
        FULL,
        HALF,
        QUARTER,
        EIGHTH,
        SIXTEENTH
    };

    /**
     * @brief Direction of rotation.
     */
    enum StepDirection {
        CLOCKWISE = true,
        COUNTER_CLOCKWISE = false
    };

    /**
     * @brief Constructor for a stepper driver.
     *
     * @param enable_pin   ENABLE pin (usually ENA)
     * @param direction_pin DIR pin
     * @param step_pin     STEP pin
     * @param MS1,MS2,MS3  Microstepping control pins
     * @param sharedMode   Pointer to shared step mode (optional)
     * @param direction    Initial rotation direction
     */
    Stepper(int enable_pin, int direction_pin, int step_pin,
            int MS1, int MS2, int MS3,
            StepMode* sharedMode = nullptr,
            StepDirection direction = CLOCKWISE);

    /**
     * @brief Destructor – nothing special (GPIO cleanup is handled globally).
     */
    ~Stepper();

    /**
     * @brief Perform raw step pulses with ramping profile.
     *
     * @param steps Number of step pulses to send.
     * @param direction CLOCKWISE / COUNTER_CLOCKWISE.
     * @param freq Step frequency (Hz).
     */
    void step(int steps, StepDirection direction, int freq = 800);

    /**
     * @brief Set microstepping mode.
     *
     * If shared_mode_ptr exists, it updates the shared mode instead.
     */
    void setMode(StepMode mode);

    /**
     * @brief Returns the currently active step mode.
     */
    StepMode getMode() const;

    /**
     * @brief Returns current rotation direction.
     */
    StepDirection getDirection() const;

    /**
     * @brief Rotate by an angle in degrees.
     *
     * Converts degrees → steps based on:
     *  - 200 steps per revolution
     *  - Microstepping multiplier
     */
    void rotate(float angle, StepDirection direction, int freq = 800);

    /**
     * @brief Enable the motor driver (usually ENA = LOW).
     */
    void enable();

    /**
     * @brief Disable the motor driver (usually ENA = HIGH).
     */
    void disable();

private:
    GPIO& gpio;              ///< Reference to global GPIO singleton

    int direction_pin;       ///< DIR pin
    int step_pin;            ///< STEP pin
    int MS1, MS2, MS3;       ///< Microstepping select pins
    int enable_pin;          ///< Driver enable pin

    float position;          ///< Logical position in degrees
    StepDirection direction; ///< Current direction

    StepMode* shared_mode_ptr; ///< Optional pointer shared with other steppers
    StepMode local_mode;       ///< Local step mode if no shared mode

    /**
     * @brief Write the microstepping mode to MS1/MS2/MS3.
     */
    void applyMode();
};