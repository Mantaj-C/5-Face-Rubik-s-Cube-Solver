#include "GPIO.h"
#include <iostream>

/* ------------------------------------------------------------
   Constructor
   - Initializes the pigpio library
   - Must be called before using any GPIO functions
   ------------------------------------------------------------ */
GPIO::GPIO() {
    gpioInitialise();
}

/* ------------------------------------------------------------
   Destructor
   - Shuts down pigpio when the program exits
   ------------------------------------------------------------ */
GPIO::~GPIO() {
    gpioTerminate();
}

/* ------------------------------------------------------------
   getInstance()
   - Singleton pattern
   - Ensures only ONE GPIO object is ever created
   - Prevents multiple pigpioInitialise() calls
   ------------------------------------------------------------ */
GPIO& GPIO::getInstance() {
    static GPIO instance;  // Created once on first use
    return instance;
}

/* ------------------------------------------------------------
   write_digi(pin, state)
   - Digital write
   - Automatically configures pin as OUTPUT on first use
   - Stores configuration in pinRegistry
   - state = true → HIGH, state = false → LOW
   ------------------------------------------------------------ */
void GPIO::write_digi(int pin, bool state) {
    // First-time configuration
    if (pinRegistry.find(pin) == pinRegistry.end()) {
        pinRegistry[pin] = OUTPUT;
        gpioSetMode(pin, PI_OUTPUT);
    }

    // Write HIGH / LOW
    gpioWrite(pin, state ? 1 : 0);
}

/* ------------------------------------------------------------
   read_digi(pin)
   - Digital read
   - Automatically configures pin as INPUT on first use
   - Returns current state (0 or 1)
   ------------------------------------------------------------ */
bool GPIO::read_digi(int pin) {
    // First-time configuration
    if (pinRegistry.find(pin) == pinRegistry.end()) {
        pinRegistry[pin] = INPUT;
        gpioSetMode(pin, PI_INPUT);
    }

    // Read and return pin state
    return gpioRead(pin);
}

/* ------------------------------------------------------------
   write_servo(pin, PulseWidth)
   - Sends a servo control pulse using pigpio’s gpioServo()
   - PulseWidth: 500–2500 µs typically
   - Automatically configures pin for SERVO on first use
   ------------------------------------------------------------ */
void GPIO::write_servo(int pin, float PulseWidth) {
    if (pinRegistry.find(pin) == pinRegistry.end()) {
        pinRegistry[pin] = SERVO;
        gpioSetMode(pin, PI_OUTPUT);
    }

    // pigpio expects servo pulse width in microseconds (int)
    gpioServo(pin, static_cast<int>(PulseWidth));
}

/* ------------------------------------------------------------
   write_PWM(pin, PulseWidth, Frequency)
   - Sets hardware PWM on supported pins
   - PulseWidth: duty cycle (0–255)
   - Frequency: PWM frequency in Hz
   - Automatically configures pin for PWM on first use
   ------------------------------------------------------------ */
void GPIO::write_PWM(int pin, float PulseWidth, float Frequency) {
    if (pinRegistry.find(pin) == pinRegistry.end()) {
        pinRegistry[pin] = PWM;
        gpioSetMode(pin, PI_OUTPUT);
    }

    gpioSetPWMfrequency(pin, Frequency);
    gpioPWM(pin, static_cast<int>(PulseWidth));
}