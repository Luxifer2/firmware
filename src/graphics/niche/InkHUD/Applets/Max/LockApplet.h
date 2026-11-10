#pragma once

#include "configuration.h"

#if defined(MESHTASTIC_INCLUDE_INKHUD) && defined(T_DECK_MAX)

/*

    Lock screen of the T-Deck Max: shows time, position, battery and messages, and ignores all input
    until it is unlocked (hold the heart key, or long touch on the screen).

*/

#include "concurrency/OSThread.h"
#include "graphics/niche/InkHUD/SystemApplet.h"

namespace NicheGraphics::InkHUD
{

class LockApplet : public SystemApplet, public concurrency::OSThread
{
  public:
    LockApplet();

    void onRender(bool full) override;
    void onForeground() override;
    void onBackground() override;
    bool onTouchPoint(uint16_t x, uint16_t y, bool longPress) override;

    // Canvas interface used by the drawing code in MaxUi.h
    using Applet::drawPixel;
    void txt(int x, int y, const char *text, int font, int align, int valign, uint16_t color);

    static void lock(InkHUD *hud);
    static void unlock(InkHUD *hud);
    static void toggle(InkHUD *hud);

  protected:
    int32_t runOnce() override;

  private:
    uint8_t renderedMinute = 255;
};

} // namespace NicheGraphics::InkHUD

#endif
