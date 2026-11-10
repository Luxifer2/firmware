#include "configuration.h"

#if defined(MESHTASTIC_INCLUDE_INKHUD) && defined(T_DECK_MAX)

#include "./LockApplet.h"

#include "./MaxInfo.h"
#include "./MaxUi.h"

using namespace NicheGraphics;

InkHUD::LockApplet::LockApplet() : concurrency::OSThread("LockApplet")
{
    alwaysRender = true;
    OSThread::setIntervalFromNow(15 * 1000UL);
    OSThread::enabled = true;
}

void InkHUD::LockApplet::txt(int x, int y, const char *text, int font, int align, int valign, uint16_t color)
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

void InkHUD::LockApplet::onRender(bool full)
{
    (void)full;
    MaxUi::Info info;
    gatherMaxInfo(info);
    renderedMinute = info.mm;
    MaxUi::renderLock(*this, width(), height(), info);
}

// While locked, ignore everything. A long touch anywhere unlocks.
bool InkHUD::LockApplet::onTouchPoint(uint16_t x, uint16_t y, bool longPress)
{
    (void)x;
    (void)y;
    if (longPress)
        unlock(inkhud);
    return true;
}

void InkHUD::LockApplet::onForeground()
{
    SystemApplet::lockRendering = true;
    SystemApplet::lockRequests = true;
    SystemApplet::handleInput = true; // blocks input from reaching other applets
}

void InkHUD::LockApplet::onBackground()
{
    SystemApplet::lockRendering = false;
    SystemApplet::lockRequests = false;
    SystemApplet::handleInput = false;
}

// Keep the clock on the lock screen up to date
int32_t InkHUD::LockApplet::runOnce()
{
    if (isForeground()) {
        MaxUi::Info info;
        gatherMaxInfo(info);
        if (info.mm != renderedMinute)
            inkhud->forceUpdate(Drivers::EInk::UpdateTypes::FAST, true);
    }
    return 15 * 1000UL;
}

void InkHUD::LockApplet::lock(InkHUD *hud)
{
    SystemApplet *menu = hud->getSystemApplet("Menu");
    if (menu && menu->isForeground())
        menu->sendToBackground();
    SystemApplet *lockApplet = hud->getSystemApplet("Lock");
    if (lockApplet && !lockApplet->isForeground()) {
        lockApplet->bringToForeground();
        hud->forceUpdate(Drivers::EInk::UpdateTypes::FAST, true);
    }
}

void InkHUD::LockApplet::unlock(InkHUD *hud)
{
    SystemApplet *lockApplet = hud->getSystemApplet("Lock");
    if (lockApplet && lockApplet->isForeground()) {
        lockApplet->sendToBackground();
        hud->showApplet(MaxUi::A_HOME); // draws the screen as well
        hud->forceUpdate(Drivers::EInk::UpdateTypes::FAST, true);
    }
}

void InkHUD::LockApplet::toggle(InkHUD *hud)
{
    SystemApplet *lockApplet = hud->getSystemApplet("Lock");
    if (lockApplet && lockApplet->isForeground())
        unlock(hud);
    else
        lock(hud);
}

#endif
