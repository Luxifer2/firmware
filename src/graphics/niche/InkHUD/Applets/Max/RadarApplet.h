#pragma once

#include "configuration.h"

#if defined(MESHTASTIC_INCLUDE_INKHUD) && defined(T_DECK_MAX)

/*

    Radar screen of the T-Deck Max: nodes with a position around us (north up),
    our own track since boot, and GPS data. Tap the radar to pick the next node.
    The drawing itself is in MaxUi.h

*/

#include "concurrency/OSThread.h"
#include "graphics/niche/InkHUD/Applet.h"

namespace NicheGraphics::InkHUD
{

class RadarApplet : public Applet, public concurrency::OSThread
{
  public:
    RadarApplet();
    void onRender(bool full) override;
    bool onTouchPoint(uint16_t x, uint16_t y, bool longPress) override;

    // Canvas interface used by the drawing code in MaxUi.h
    using Applet::drawPixel;
    void txt(int x, int y, const char *text, int font, int align, int valign, uint16_t color);

  protected:
    int32_t runOnce() override;

  private:
    static constexpr int TRACK_MAX = 120;
    int32_t trackLat[TRACK_MAX] = {}, trackLon[TRACK_MAX] = {};
    int trackCount = 0, trackHead = 0; // ring buffer
    NodeNum selectedNode = 0;
    uint32_t lastRenderMs = 0;
    int32_t renderedLat = 0, renderedLon = 0;
    size_t renderedNodes = 0;

    bool ourPosition(int32_t &lat, int32_t &lon);
};

} // namespace NicheGraphics::InkHUD

#endif
