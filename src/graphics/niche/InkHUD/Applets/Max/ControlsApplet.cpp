#include "configuration.h"

#if defined(MESHTASTIC_INCLUDE_INKHUD) && defined(T_DECK_MAX)

#include "./ControlsApplet.h"

#include "./LockApplet.h"
#include "./MaxUi.h"
#include "MeshService.h"
#include "NodeDB.h"
#include "UptimeClock.h"
#include "main.h"
#include "mesh/Throttle.h"

using namespace NicheGraphics;

static constexpr uint32_t CONFIRM_WINDOW_MS = 6000;

void InkHUD::ControlsApplet::txt(int x, int y, const char *text, int font, int align, int valign, uint16_t color)
{
    setFont(font == MaxUi::SMALL ? fontSmall : (font == MaxUi::MEDIUM ? fontMedium : fontLarge));
    setTextColor(color);
    HorizontalAlignment ha = LEFT;
    if (align == MaxUi::CENTER)
        ha = CENTER;
    else if (align == MaxUi::RIGHT)
        ha = RIGHT;
    printAt(x, y, text, ha, valign == MaxUi::MIDDLE ? MIDDLE : TOP);
    setTextColor(BLACK);
}

static bool vibrationIsOn()
{
    return moduleConfig.external_notification.enabled && moduleConfig.external_notification.alert_message_vibra;
}

void InkHUD::ControlsApplet::onRender(bool full)
{
    (void)full;
    if (confirm >= 0 && Throttle::isWithinTimespanMs(confirmSince, CONFIRM_WINDOW_MS) == false)
        confirm = -1;

    MaxUi::Controls cs;
    cs.screenLight = tDeckMaxGetLight(0);
    cs.keyLight = tDeckMaxGetLight(1);
    cs.vibration = vibrationIsOn();
#if !MESHTASTIC_EXCLUDE_GPS && HAS_GPS
    cs.gps = config.position.gps_mode == meshtastic_Config_PositionConfig_GpsMode_ENABLED;
#endif
    cs.bluetooth = config.bluetooth.enabled;
    cs.wifi = config.network.wifi_enabled;
    cs.confirm = confirm;
    cs.bootReason = tDeckMaxBootReason(&cs.crashes);
    cs.lastThread = tDeckMaxLastThread();
    MaxUi::renderControls(*this, width(), height(), cs);
}

bool InkHUD::ControlsApplet::onTouchPoint(uint16_t x, uint16_t y, bool longPress)
{
    if (longPress)
        return false;

    // Menu button, top right
    if (MaxUi::homeMenuAt(width(), height(), x, y)) {
        inkhud->openMenu();
        return true;
    }

    // Hexagon top left: back to the home screen
    if ((x - 22) * (x - 22) + (y - 22) * (y - 22) <= 20 * 20) {
        inkhud->showApplet(MaxUi::A_HOME);
        return true;
    }

    const int which = MaxUi::controlAt(width(), x, y);
    if (which < 0)
        return false;

    // Switches that restart the device need a second tap
    if (which == MaxUi::C_BLUETOOTH || which == MaxUi::C_WIFI) {
        if (confirm != which) {
            confirm = which;
            confirmSince = millis();
            requestUpdate(Drivers::EInk::UpdateTypes::FAST);
            return true;
        }
        confirm = -1;
    } else {
        confirm = -1;
    }

    switch (which) {
    case MaxUi::C_SCREEN_LIGHT:
        tDeckMaxSetLight(0, !tDeckMaxGetLight(0));
        break;

    case MaxUi::C_KEY_LIGHT:
        tDeckMaxSetLight(1, !tDeckMaxGetLight(1));
        break;

    case MaxUi::C_VIBRATION: {
        auto &ext = moduleConfig.external_notification;
        const bool wasEnabled = ext.enabled;
        if (vibrationIsOn()) {
            ext.alert_message_vibra = false;
            ext.alert_bell_vibra = false;
        } else {
            ext.enabled = true;
            ext.alert_message_vibra = true;
            ext.alert_bell_vibra = true;
        }
        nodeDB->saveToDisk(SEGMENT_MODULECONFIG);
        service->reloadConfig(SEGMENT_MODULECONFIG);
        if (ext.enabled != wasEnabled) {
            // The notification module only starts at boot
            inkhud->notifyApplyingChanges();
            rebootAtMsec = Time::timerEndsAtMillis(DEFAULT_REBOOT_SECONDS * 1000);
        }
        break;
    }

    case MaxUi::C_GPS:
#if !MESHTASTIC_EXCLUDE_GPS && HAS_GPS
        if (config.position.gps_mode == meshtastic_Config_PositionConfig_GpsMode_DISABLED)
            config.position.gps_mode = meshtastic_Config_PositionConfig_GpsMode_ENABLED;
        else if (config.position.gps_mode == meshtastic_Config_PositionConfig_GpsMode_ENABLED)
            config.position.gps_mode = meshtastic_Config_PositionConfig_GpsMode_DISABLED;
        else
            break;
        nodeDB->saveToDisk(SEGMENT_CONFIG);
        service->reloadConfig(SEGMENT_CONFIG);
#endif
        break;

    case MaxUi::C_BLUETOOTH:
        config.bluetooth.enabled = !config.bluetooth.enabled;
        if (config.bluetooth.enabled)
            config.network.wifi_enabled = false; // same rule as the menu: Bluetooth and WiFi exclude each other
        nodeDB->saveToDisk(SEGMENT_CONFIG);
        inkhud->notifyApplyingChanges();
        rebootAtMsec = Time::timerEndsAtMillis(DEFAULT_REBOOT_SECONDS * 1000);
        return true;

    case MaxUi::C_WIFI:
        config.network.wifi_enabled = !config.network.wifi_enabled;
        if (config.network.wifi_enabled)
            config.bluetooth.enabled = false;
        nodeDB->saveToDisk(SEGMENT_CONFIG);
        inkhud->notifyApplyingChanges();
        rebootAtMsec = Time::timerEndsAtMillis(DEFAULT_REBOOT_SECONDS * 1000);
        return true;

    case MaxUi::C_LOCK:
        LockApplet::lock(inkhud);
        return true;

    case MaxUi::C_HOME:
        inkhud->showApplet(MaxUi::A_HOME);
        return true;
    }

    requestUpdate(Drivers::EInk::UpdateTypes::FAST);
    return true;
}

#endif
