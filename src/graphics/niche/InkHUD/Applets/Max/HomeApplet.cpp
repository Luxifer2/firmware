#include "configuration.h"

#if defined(MESHTASTIC_INCLUDE_INKHUD) && defined(T_DECK_MAX)

#include "./HomeApplet.h"

#include "./LockApplet.h"
#include "./MaxInfo.h"
#include "./MaxUi.h"

using namespace NicheGraphics;

InkHUD::HomeApplet::HomeApplet() : concurrency::OSThread("HomeApplet")
{
    OSThread::setIntervalFromNow(10 * 1000UL);
    OSThread::enabled = true;
}

void InkHUD::HomeApplet::txt(int x, int y, const char *text, int font, int align, int valign, uint16_t color)
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

void InkHUD::HomeApplet::onRender(bool full)
{
    (void)full;
    MaxUi::Info info;
    gatherMaxInfo(info);
    renderedMinute = info.mm;
    renderedUnread = info.unread;
    strncpy(renderedDiag, info.diag ? info.diag : "", sizeof(renderedDiag) - 1);
    MaxUi::renderHome(*this, width(), height(), info);
}

// Touch on a hexagon, or on the menu button
bool InkHUD::HomeApplet::onTouchPoint(uint16_t x, uint16_t y, bool longPress)
{
    if (longPress)
        return false;

    if (MaxUi::homeMenuAt(width(), height(), x, y)) {
        inkhud->openMenu();
        return true;
    }

    switch (MaxUi::homeTileAt(width(), height(), x, y)) {
    case MaxUi::T_MESSAGES:
        tDeckMaxUnread = 0;
        inkhud->showApplet(MaxUi::A_CHANNEL0);
        return true;
    case MaxUi::T_DIRECT:
        tDeckMaxUnread = 0;
        inkhud->showApplet(MaxUi::A_DMS);
        return true;
    case MaxUi::T_NODES:
        inkhud->showApplet(MaxUi::A_HEARD);
        return true;
    case MaxUi::T_MAP:
        inkhud->showApplet(MaxUi::A_POSITIONS);
        return true;
    case MaxUi::T_FAVORITES:
        if (!inkhud->showApplet(MaxUi::A_FAVORITES))
            inkhud->showApplet(MaxUi::A_RECENTS);
        return true;
    case MaxUi::T_OPTIONS:
        inkhud->openMenu();
        return true;
    case MaxUi::T_LOCK:
        LockApplet::lock(inkhud);
        return true;
    default:
        return false;
    }
}

// Keep the clock (and the test line at the bottom) fresh while the home screen is showing
int32_t InkHUD::HomeApplet::runOnce()
{
    // After boot, once the logo / tips / pairing screens are out of the way, start on the home screen
    if (!shownAtBoot) {
        bool busy = false;
        for (SystemApplet *sa : inkhud->systemApplets)
            busy |= (sa->lockRendering || sa->handleInput) && sa->isForeground();
        if (!busy) {
            shownAtBoot = true;
            inkhud->showApplet(MaxUi::A_HOME);
        }
        return 3 * 1000UL;
    }

    if (isForeground()) {
        MaxUi::Info info;
        gatherMaxInfo(info);
        const bool diagChanged = strncmp(renderedDiag, info.diag ? info.diag : "", sizeof(renderedDiag) - 1) != 0;
        if (info.mm != renderedMinute || info.unread != renderedUnread || diagChanged)
            requestUpdate(Drivers::EInk::UpdateTypes::FAST);
    }
    return 5 * 1000UL;
}

#endif
