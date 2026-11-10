#pragma once

#include "configuration.h"

#if defined(MESHTASTIC_INCLUDE_INKHUD) && defined(T_DECK_MAX)

/*

    Home screen of the T-Deck Max: clock, status line and seven hexagon buttons.
    The drawing itself is in MaxUi.h

*/

#include "concurrency/OSThread.h"
#include "graphics/niche/InkHUD/Applet.h"

namespace NicheGraphics::InkHUD
{

class HomeApplet : public Applet, public concurrency::OSThread
{
  public:
    HomeApplet();

    void onRender(bool full) override;
    bool onTouchPoint(uint16_t x, uint16_t y, bool longPress) override;

    // Canvas interface used by the drawing code in MaxUi.h
    using Applet::drawPixel;
    void txt(int x, int y, const char *text, int font, int align, int valign, uint16_t color);

  protected:
    int32_t runOnce() override;

  private:
    bool shownAtBoot = false;
    uint8_t renderedMinute = 255;
    char renderedDiag[48] = {0};
    uint16_t renderedUnread = 0;
};

} // namespace NicheGraphics::InkHUD

#endif
