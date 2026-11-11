#pragma once

#include "configuration.h"

#if defined(MESHTASTIC_INCLUDE_INKHUD) && defined(T_DECK_MAX)

/*

    Boot check of the T-Deck Max: after the logo, the modules are listed one by one with their state.
    All fine: the home screen follows by itself. A module failed: the screen stays until touched
    (or a key under the screen is pressed), so the failure can be read.
    The drawing itself is in MaxUi.h

*/

#include "concurrency/OSThread.h"
#include "graphics/niche/InkHUD/SystemApplet.h"

namespace NicheGraphics::InkHUD
{

class BootApplet : public SystemApplet, public concurrency::OSThread
{
  public:
    BootApplet();

    void onRender(bool full) override;
    void onForeground() override;
    void onBackground() override;
    bool onTouchPoint(uint16_t x, uint16_t y, bool longPress) override;

    // Canvas interface used by the drawing code in MaxUi.h
    using Applet::drawPixel;
    void txt(int x, int y, const char *text, int font, int align, int valign, uint16_t color);

    static void start(InkHUD *hud);
    static bool showing(InkHUD *hud);
    static void userInput(InkHUD *hud); // touch or key: skip the animation, or continue to the home screen

  protected:
    int32_t runOnce() override;

  private:
    void check();  // fill in the module states
    void finish(); // on to the home screen
    uint8_t shown = 0;
    uint32_t doneAtMs = 0;
};

} // namespace NicheGraphics::InkHUD

#endif
