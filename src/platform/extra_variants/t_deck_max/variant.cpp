#include "Arduino.h"
#include "configuration.h"

#ifdef T_DECK_MAX

#include <Wire.h>

#ifdef MESHTASTIC_INCLUDE_INKHUD
uint16_t tDeckMaxUnread = 0;
char tDeckMaxDiag[48] = "";
#endif

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
static bool screenLightEnabled = false;
static bool keyLightEnabled = false;
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
    const bool allowed = !sleeping && !NicheGraphics::InkHUD::InkHUD::getInstance()->isTouchTimedOut();
    digitalWrite(KB_BL_PIN, keyLightEnabled && allowed);
    analogWrite(PIN_EINK_FRONTLIGHT, (screenLightEnabled && allowed) ? 64 : 0);
}

// Both lights together (the old single switch): if either is on they go off, otherwise both come on
void tDeckProToggleBacklights()
{
    const bool anyOn = screenLightEnabled || keyLightEnabled;
    screenLightEnabled = keyLightEnabled = !anyOn;
    tDeckProUpdateBacklights();
}

// which: 0 = screen light, 1 = keyboard light
bool tDeckMaxGetLight(uint8_t which)
{
    return which == 0 ? screenLightEnabled : keyLightEnabled;
}

void tDeckMaxSetLight(uint8_t which, bool on)
{
    if (which == 0)
        screenLightEnabled = on;
    else
        keyLightEnabled = on;
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

// Why did we start? Shown on the controls screen, so restarts can be diagnosed without a computer.
// The counter lives in RTC memory: it survives crashes and watchdog resets, and starts from zero at power-on.
#include <esp_attr.h>
#include <esp_system.h>
RTC_NOINIT_ATTR static uint32_t bootMagic;
RTC_NOINIT_ATTR static uint32_t bootCrashes;

const char *tDeckMaxBootReason(uint32_t *crashesSincePowerOn)
{
    static const char *text = nullptr;
    static uint32_t crashes = 0;
    if (!text) {
        const esp_reset_reason_t r = esp_reset_reason();
        if (bootMagic != 0x7DEC4A11 || r == ESP_RST_POWERON) {
            bootMagic = 0x7DEC4A11;
            bootCrashes = 0;
        }
        switch (r) {
        case ESP_RST_POWERON:
            text = "ACCENSIONE";
            break;
        case ESP_RST_SW:
            text = "RIAVVIO NORMALE";
            break;
        case ESP_RST_PANIC:
            text = "ERRORE SOFTWARE";
            bootCrashes++;
            break;
        case ESP_RST_INT_WDT:
        case ESP_RST_TASK_WDT:
        case ESP_RST_WDT:
            text = "BLOCCO (WATCHDOG)";
            bootCrashes++;
            break;
        case ESP_RST_BROWNOUT:
            text = "CALO DI TENSIONE";
            bootCrashes++;
            break;
        case ESP_RST_DEEPSLEEP:
            text = "RISVEGLIO DAL SONNO";
            break;
        case ESP_RST_EXT:
            text = "TASTO RESET";
            break;
        default:
            text = "ALTRO";
            break;
        }
        crashes = bootCrashes;
    }
    if (crashesSincePowerOn)
        *crashesSincePowerOn = crashes;
    return text;
}

#endif // T_DECK_MAX
