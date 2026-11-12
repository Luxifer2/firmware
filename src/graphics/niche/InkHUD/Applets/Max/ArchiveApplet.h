#pragma once

#include "configuration.h"

#if defined(MESHTASTIC_INCLUDE_INKHUD) && defined(T_DECK_MAX)

/*

    Archive screen of the T-Deck Max: what is on the microSD card. New diary note, diary and text files
    (opened in the reader), node hunter and GPX track. The drawing itself is in MaxUi.h

*/

#include "graphics/niche/InkHUD/Applet.h"

namespace NicheGraphics::InkHUD
{

class ArchiveApplet : public Applet
{
  public:
    void onRender(bool full) override;
    void onForeground() override;
    bool onTouchPoint(uint16_t x, uint16_t y, bool longPress) override;

    // Canvas interface used by the drawing code in MaxUi.h
    using Applet::drawPixel;
    void txt(int x, int y, const char *text, int font, int align, int valign, uint16_t color);

  private:
    bool refreshFiles = true;
};

} // namespace NicheGraphics::InkHUD

#endif
