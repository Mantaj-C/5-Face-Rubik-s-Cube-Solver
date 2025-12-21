#pragma once
#include <pigpio.h>
#include <map>
#include <stdexcept>

/* ------------------------------------------------------------
   PinMode enumeration
   - Tracks what mode each pin is configured as
   - Used internally to prevent re-initializing pins incorrectly
   ------------------------------------------------------------ */
enum PinMode { INPUT, OUTPUT, PWM, SERVO };

/* ------------------------------------------------------------
   GPIO Class (Singleton)
   - Wraps all pigpio functions with safe C++ methods
   - Ensures pigpio is initialized once globally
   - Tracks pin modes to prevent accidental misuse
   ------------------------------------------------------------ */
class GPIO
{
private:
    /* --------------------------------------------------------
       Constructor / Destructor
       - Private because this class uses the Singleton pattern
       - Constructor initializes pigpio
       - Destructor shuts it down
       -------------------------------------------------------- */
    GPIO();
    ~GPIO();

    /* --------------------------------------------------------
       pinRegistry
       - Maps pin number → PinMode enum
       - Used to avoid reconfiguring a pin incorrectly
       -------------------------------------------------------- */
    std::map<int, PinMode> pinRegistry;

    /* --------------------------------------------------------
       Disable copy operations
       - Prevents accidental duplication of the GPIO instance
       -------------------------------------------------------- */
    GPIO(const GPIO&) = delete;
    GPIO& operator=(const GPIO&) = delete;

public:
    /* --------------------------------------------------------
       getInstance()
       - Access the single global GPIO instance
       - Created on first use, destroyed automatically at exit
       -------------------------------------------------------- */
    static GPIO& getInstance();

    /* --------------------------------------------------------
       write_digi(pin, state)
       - Digital write (HIGH or LOW)
       - Automatically configures pin as OUTPUT on first use
       -------------------------------------------------------- */
    void write_digi(int pin, bool state);

    /* --------------------------------------------------------
       read_digi(pin)
       - Digital read (0 or 1)
       - Automatically configures pin as INPUT on first use
       -------------------------------------------------------- */
    bool read_digi(int pin);

    /* --------------------------------------------------------
       write_PWM(pin, PulseWidth, Frequency)
       - Hardware PWM output
       - PulseWidth: duty cycle (0–255)
       - Frequency: Hz
       - Automatically configures pin as PWM on first use
       -------------------------------------------------------- */
    void write_PWM(int pin, float PulseWidth, float Frequency);

    /* --------------------------------------------------------
       write_servo(pin, PulseWidth)
       - Servo pulse control using pigpio servo output
       - PulseWidth: 500–2500 µs typical
       - Automatically configures pin as SERVO on first use
       -------------------------------------------------------- */
    void write_servo(int pin, float PulseWidth);
};