#ifdef MESHTASTIC_INCLUDE_INKHUD

/*

    Shows the Meshtastic logo fullscreen, with accompanying text
    Used for boot and shutdown

*/

#pragma once

#include "configuration.h"

#include "concurrency/OSThread.h"
#include "graphics/niche/InkHUD/SystemApplet.h"

namespace NicheGraphics::InkHUD
{

class LogoApplet : public SystemApplet, public concurrency::OSThread
{
  public:
    LogoApplet();
    void onRender(bool full) override;
    void onForeground() override;
    void onBackground() override;
    void onShutdown() override;
    void onReboot() override;
    void onApplyingChanges();

#ifdef T_DECK_MAX
    // Canvas interface used by the drawing code in Max/MaxUi.h (the badge shown once switched off)
    using Applet::drawPixel;
    void txt(int x, int y, const char *text, int font, int align, int valign, uint16_t color);
#endif

  protected:
    int32_t runOnce() override;

    std::string textLeft;
    std::string textRight;
    std::string textTitle;
    AppletFont fontTitle;
    bool inverted = false; // Invert colors. Used during shutdown, to restore display health.
#ifdef T_DECK_MAX
    bool badge = false; // switched off: draw the T-Deck Max badge instead of the logo
#endif
};

} // namespace NicheGraphics::InkHUD

#endif