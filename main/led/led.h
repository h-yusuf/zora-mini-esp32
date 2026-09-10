#ifndef _LED_H_
#define _LED_H_

class Led {
public:
    virtual ~Led() = default;
    // Set the led state based on the device state
    virtual void OnStateChanged() = 0;
    // Cycle through colors once so the wiring can be verified. Blocks for
    // roughly a second. No-op unless the driver implements it.
    virtual void SelfTest() {}
};


class NoLed : public Led {
public:
    virtual void OnStateChanged() override {}
};

#endif // _LED_H_
