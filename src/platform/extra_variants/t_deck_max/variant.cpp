#include "Arduino.h"
#include "configuration.h"

#ifdef T_DECK_MAX

#include <Wire.h>

// ---------------------------------------------------------------------------------------------
// XL9555 IO expander (address 0x20). On the T-Deck Max it switches power and reset lines:
//   port 0: IO00 modem power, IO01 LoRa power, IO02 GPS power, IO03 1.8V rail (gyroscope),
//           IO04 LoRa antenna (HIGH internal / LOW external), IO05 haptic motor power,
//           IO06 speaker amplifier, IO07 touch reset (active LOW)
//   port 1: IO10 modem power key, IO11 keyboard reset (active LOW), IO12 audio route
// ---------------------------------------------------------------------------------------------
static constexpr uint8_t XL9555_ADDR = 0x20;
static constexpr uint8_t XL9555_REG_OUT0 = 0x02;
static constexpr uint8_t XL9555_REG_OUT1 = 0x03;
static constexpr uint8_t XL9555_REG_CFG0 = 0x06;
static constexpr uint8_t XL9555_REG_CFG1 = 0x07;

// Output levels: LoRa, GPS, 1.8V rail, internal antenna, haptic motor and touch run; the 4G modem and amplifier stay off.
static constexpr uint8_t XL9555_PORT0_RUN = 0xBE;
static constexpr uint8_t XL9555_TOUCH_RST_BIT = 0x80;
static constexpr uint8_t XL9555_PORT1_RUN = 0x02; // keyboard out of reset, audio routed to the ES8311

static void xl9555Write(uint8_t reg, uint8_t value)
{
    Wire.beginTransmission(XL9555_ADDR);
    Wire.write(reg);
    Wire.write(value);
    Wire.endTransmission();
}

#ifdef MESHTASTIC_INCLUDE_INKHUD
#include "NodeDB.h"
#include "concurrency/Periodic.h"
#include "graphics/niche/InkHUD/InkHUD.h"
#include "modules/ExternalNotificationModule.h"
#include <Adafruit_DRV2605.h>

static Adafruit_DRV2605 haptic;
static bool hapticReady = false;
static bool backlightsEnabled = false;
static concurrency::Periodic *touchFeedbackTimer = nullptr;

static int32_t stopTouchFeedback()
{
    if (hapticReady)
        haptic.stop();
    return touchFeedbackTimer->disable();
}

// Name kept from the T-Deck Pro code that shares the touch and keyboard drivers.
// The Max has the haptic driver (DRV2605), so this brings it up once and reports "newer hardware".
bool tDeckProIsV11()
{
    static const bool ready = []() {
        hapticReady = haptic.begin();
        if (hapticReady) {
            haptic.selectLibrary(1);
            haptic.setMode(DRV2605_MODE_INTTRIG);
            haptic.setWaveform(0, 16);
            haptic.setWaveform(1, 0);
        } else {
            LOG_WARN("DRV2605 initialization failed");
        }
        return true;
    }();
    return ready;
}

void tDeckProUpdateBacklights(bool sleeping)
{
    const bool on = backlightsEnabled && !sleeping && !NicheGraphics::InkHUD::InkHUD::getInstance()->isTouchTimedOut();
    digitalWrite(KB_BL_PIN, on);
    analogWrite(PIN_EINK_FRONTLIGHT, on ? 64 : 0);
}

void tDeckProToggleBacklights()
{
    backlightsEnabled = !backlightsEnabled;
    tDeckProUpdateBacklights();
}

void tDeckProSetVibration(int pin, bool on)
{
    (void)pin;
    if (touchFeedbackTimer)
        touchFeedbackTimer->disable();
    if (!hapticReady)
        return;
    if (on) {
        haptic.setWaveform(0, 16);
        haptic.go();
    } else {
        haptic.stop();
    }
}

void tDeckProTouchFeedback()
{
    const auto &settings = moduleConfig.external_notification;
    if (!settings.enabled || !(settings.alert_message_vibra || settings.alert_bell_vibra))
        return;
    if (externalNotificationModule && (externalNotificationModule->getMute() || externalNotificationModule->nagging() ||
                                       externalNotificationModule->getExternal(1)))
        return;
    if (!hapticReady)
        return;
    if (!touchFeedbackTimer)
        touchFeedbackTimer = new concurrency::Periodic("TouchHaptic", stopTouchFeedback);

    haptic.setWaveform(0, 1);
    haptic.go();
    touchFeedbackTimer->setIntervalFromNow(10);
    touchFeedbackTimer->enabled = true;
}
#endif

// Runs at the very start of setup(), before the radio, GPS, touch and keyboard are touched. No LOG_* here.
void earlyInitVariant()
{
    Wire.begin(I2C_SDA, I2C_SCL);

    // Levels first, then direction, so nothing glitches when the pins become outputs.
    xl9555Write(XL9555_REG_OUT0, XL9555_PORT0_RUN);
    xl9555Write(XL9555_REG_OUT1, XL9555_PORT1_RUN);
    xl9555Write(XL9555_REG_CFG0, 0x00); // all of port 0 are outputs
    xl9555Write(XL9555_REG_CFG1, 0xF8); // IO10, IO11, IO12 are outputs, the rest stay inputs
    delay(10);

    // Reset the touch controller
    xl9555Write(XL9555_REG_OUT0, XL9555_PORT0_RUN & ~XL9555_TOUCH_RST_BIT);
    delay(20);
    xl9555Write(XL9555_REG_OUT0, XL9555_PORT0_RUN);
    delay(60);

    pinMode(LORA_CS, OUTPUT);
    digitalWrite(LORA_CS, HIGH);
    pinMode(SDCARD_CS, OUTPUT);
    digitalWrite(SDCARD_CS, HIGH);
    pinMode(PIN_EINK_CS, OUTPUT);
    digitalWrite(PIN_EINK_CS, HIGH);
    pinMode(PIN_EINK_FRONTLIGHT, OUTPUT);
    digitalWrite(PIN_EINK_FRONTLIGHT, LOW);
}

#endif // T_DECK_MAX
