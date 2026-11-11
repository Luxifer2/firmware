#include "configuration.h"

#if defined(MESHTASTIC_INCLUDE_INKHUD) && defined(T_DECK_MAX)

#include "./BootApplet.h"

#include "./MaxUi.h"
#include "GPSStatus.h"
#include "PowerStatus.h"
#include "detect/LoRaRadioType.h"
#include "gps/GPS.h"
#include "gps/RTC.h"
#include "main.h"
#include "mesh/MeshRadio.h"
#include "mesh/Throttle.h"

using namespace NicheGraphics;

static constexpr uint32_t STEP_MS = 500;        // one module revealed every half second
static constexpr uint32_t READY_HOLD_MS = 2500; // "system ready" stays this long

static MaxUi::Boot bootCheck; // module states, filled in by check()

InkHUD::BootApplet::BootApplet() : concurrency::OSThread("BootApplet")
{
    alwaysRender = true;
    OSThread::disable();
}

void InkHUD::BootApplet::txt(int x, int y, const char *text, int font, int align, int valign, uint16_t color)
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

static void addLine(const char *name, const char *value, MaxUi::CheckState state)
{
    if (bootCheck.n >= MaxUi::Boot::MAX_LINES)
        return;
    MaxUi::BootLine &L = bootCheck.lines[bootCheck.n++];
    L.name = name;
    snprintf(L.value, sizeof(L.value), "%s", value);
    L.state = state;
}

void InkHUD::BootApplet::check()
{
    bootCheck.n = 0;
    bootCheck.version = "Meshtastic " xstr(APP_VERSION_SHORT);
    char v[16];

    // Radio: no radio means no mesh at all
    addLine("RADIO LORA", radioType != NO_RADIO ? "OK" : "ASSENTE", radioType != NO_RADIO ? MaxUi::CK_OK : MaxUi::CK_FAIL);

    if (config.lora.region == meshtastic_Config_LoRaConfig_RegionCode_UNSET)
        addLine("REGIONE", "DA IMPOSTARE", MaxUi::CK_WARN);
    else
        addLine("REGIONE", myRegion && myRegion->name ? myRegion->name : "OK", MaxUi::CK_OK);

    addLine("TASTIERA", kb_found ? "OK" : "ASSENTE", kb_found ? MaxUi::CK_OK : MaxUi::CK_FAIL);
    addLine("TOUCH", tDeckMaxTouchOk ? "OK" : "ASSENTE", tDeckMaxTouchOk ? MaxUi::CK_OK : MaxUi::CK_FAIL);
    const bool haptic = tDeckMaxHapticOk();
    addLine("VIBRAZIONE", haptic ? "OK" : "ASSENTE", haptic ? MaxUi::CK_OK : MaxUi::CK_FAIL);

#if !MESHTASTIC_EXCLUDE_GPS && HAS_GPS
    if (config.position.gps_mode != meshtastic_Config_PositionConfig_GpsMode_ENABLED)
        addLine("GPS", "SPENTO", MaxUi::CK_OK);
    else if (!gps)
        addLine("GPS", "ASSENTE", MaxUi::CK_FAIL);
    else if (gpsStatus && gpsStatus->getHasLock()) {
        snprintf(v, sizeof(v), "FIX %u", (unsigned)gpsStatus->getNumSatellites());
        addLine("GPS", v, MaxUi::CK_OK);
    } else
        addLine("GPS", "OK", MaxUi::CK_OK);
#else
    addLine("GPS", "ASSENTE", MaxUi::CK_FAIL);
#endif

    if (powerStatus && powerStatus->getHasBattery()) {
        snprintf(v, sizeof(v), "%d%%", (int)powerStatus->getBatteryChargePercent());
        addLine("BATTERIA", v, MaxUi::CK_OK);
    } else {
        addLine("BATTERIA", "NON LETTA", MaxUi::CK_WARN);
    }

    const uint32_t now = getValidTime(RTCQuality::RTCQualityDevice, true);
    if (now > 0) {
        snprintf(v, sizeof(v), "%02u:%02u", (unsigned)((now / 3600) % 24), (unsigned)((now / 60) % 60));
        addLine("OROLOGIO", v, MaxUi::CK_OK);
    } else {
        addLine("OROLOGIO", "DA GPS", MaxUi::CK_WARN);
    }
}

void InkHUD::BootApplet::onRender(bool full)
{
    (void)full;
    bootCheck.shown = shown;
    MaxUi::renderBoot(*this, width(), height(), boot);
}

bool InkHUD::BootApplet::onTouchPoint(uint16_t x, uint16_t y, bool longPress)
{
    (void)x;
    (void)y;
    (void)longPress;
    userInput(inkhud);
    return true;
}

void InkHUD::BootApplet::onForeground()
{
    SystemApplet::lockRendering = true;
    SystemApplet::lockRequests = true;
    SystemApplet::handleInput = true;
}

void InkHUD::BootApplet::onBackground()
{
    SystemApplet::lockRendering = false;
    SystemApplet::lockRequests = false;
    SystemApplet::handleInput = false;
    OSThread::disable();
}

int32_t InkHUD::BootApplet::runOnce()
{
    if (!isForeground())
        return disable();

    if (shown < bootCheck.n) {
        shown++;
        if (shown == bootCheck.n)
            doneAtMs = millis();
        inkhud->forceUpdate(Drivers::EInk::UpdateTypes::FAST, true);
        return STEP_MS;
    }

    // All checked: with no failure, continue by itself
    if (bootCheck.firstFailure() < 0) {
        if (!Throttle::isWithinTimespanMs(doneAtMs, READY_HOLD_MS)) {
            finish();
            return disable();
        }
        return 250;
    }
    return disable(); // a failure: wait for the user
}

void InkHUD::BootApplet::finish()
{
    sendToBackground();
    inkhud->showApplet(MaxUi::A_HOME);
    inkhud->forceUpdate(Drivers::EInk::UpdateTypes::FULL, true); // clean the e-ink after the fast steps
}

void InkHUD::BootApplet::start(InkHUD *hud)
{
    BootApplet *b = (BootApplet *)hud->getSystemApplet("Boot");
    if (!b) {
        hud->showApplet(MaxUi::A_HOME);
        return;
    }
    b->check();
    b->shown = 0;
    b->bringToForeground();
    b->OSThread::setIntervalFromNow(STEP_MS);
    b->OSThread::enabled = true;
    hud->forceUpdate(Drivers::EInk::UpdateTypes::FAST, true);
}

bool InkHUD::BootApplet::showing(InkHUD *hud)
{
    SystemApplet *b = hud->getSystemApplet("Boot");
    return b && b->isForeground();
}

void InkHUD::BootApplet::userInput(InkHUD *hud)
{
    BootApplet *b = (BootApplet *)hud->getSystemApplet("Boot");
    if (!b || !b->isForeground())
        return;
    if (b->shown < bootCheck.n) { // still checking: show everything at once
        b->shown = bootCheck.n;
        b->doneAtMs = millis();
        hud->forceUpdate(Drivers::EInk::UpdateTypes::FAST, true);
        if (bootCheck.firstFailure() < 0) {
            b->OSThread::setIntervalFromNow(250);
            b->OSThread::enabled = true;
        }
        return;
    }
    b->finish();
}

#endif
