#pragma once

#include "configuration.h"

#if defined(MESHTASTIC_INCLUDE_INKHUD) && defined(T_DECK_MAX)

/*

    Quick diary note of the T-Deck Max: write a line with the keyboard, and it is added to the diary
    on the microSD card with date, time, position and altitude. Opened by holding the speech bubble key,
    or from the archive screen. Bubble (or enter) saves, heart cancels.

*/

#include "concurrency/OSThread.h"
#include "graphics/niche/InkHUD/SystemApplet.h"

#include <string>

namespace NicheGraphics::InkHUD
{

class NoteApplet : public SystemApplet, public concurrency::OSThread
{
  public:
    NoteApplet();

    void onRender(bool full) override;
    void onForeground() override;
    void onBackground() override;
    bool onTouchPoint(uint16_t x, uint16_t y, bool longPress) override;
    void onFreeText(char c) override;
    void onFreeTextDone() override;
    void onFreeTextCancel() override;

    // Canvas interface used by the drawing code in MaxUi.h
    using Applet::drawPixel;
    void txt(int x, int y, const char *text, int font, int align, int valign, uint16_t color);

    static void open(InkHUD *hud);

  protected:
    int32_t runOnce() override;

  private:
    static constexpr size_t MAX_CHARS = 400;
    enum State : uint8_t { WRITING, SAVED, FAILED };
    State state = WRITING;
    std::string text;
    char info[48] = "";
    bool sd = false;

    void save();
    void close();
};

} // namespace NicheGraphics::InkHUD

#endif
