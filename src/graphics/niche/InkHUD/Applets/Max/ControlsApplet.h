#pragma once

#include "configuration.h"

#if defined(MESHTASTIC_INCLUDE_INKHUD) && defined(T_DECK_MAX)

/*

    Controls screen of the T-Deck Max: square tiles with quick switches
    (screen light, keyboard light, vibration, GPS, Bluetooth, WiFi), plus lock and home.
    The drawing itself is in MaxUi.h

*/

#include "graphics/niche/InkHUD/Applet.h"

namespace NicheGraphics::InkHUD
{

class ControlsApplet : public Applet
{
  public:
    void onRender(bool full) override;
    bool onTouchPoint(uint16_t x, uint16_t y, bool longPress) override;

    // Canvas interface used by the drawing code in MaxUi.h
    using Applet::drawPixel;
    void txt(int x, int y, const char *text, int font, int align, int valign, uint16_t color);

  private:
    int8_t confirm = -1;       // control waiting for a second tap
    uint32_t confirmSince = 0; // millis() when the first tap happened
};

} // namespace NicheGraphics::InkHUD

#endif
