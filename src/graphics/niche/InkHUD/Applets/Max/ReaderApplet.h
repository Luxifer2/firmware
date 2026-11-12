#pragma once

#include "configuration.h"

#if defined(MESHTASTIC_INCLUDE_INKHUD) && defined(T_DECK_MAX)

/*

    Text file reader of the T-Deck Max: lists the .txt files on the microSD card (in /, /libri and /diario)
    and shows one a page at a time. Tap the right of the screen for the next page, the left for the previous one.
    The page reached in each file is remembered.

*/

#include "graphics/niche/InkHUD/Applet.h"

#include <string>
#include <vector>

namespace NicheGraphics::InkHUD
{

class ReaderApplet : public Applet
{
  public:
    ReaderApplet();

    void onRender(bool full) override;
    void onForeground() override;
    bool onTouchPoint(uint16_t x, uint16_t y, bool longPress) override;

    // Canvas interface used by the drawing code in MaxUi.h
    using Applet::drawPixel;
    void txt(int x, int y, const char *text, int font, int align, int valign, uint16_t color);

    static void openList(InkHUD *hud);
    static void openFile(InkHUD *hud, const std::string &path);

  private:
    static ReaderApplet *instance;
    static constexpr int LIST_ROWS = 8;
    static constexpr int ROW_H = 28;
    static constexpr int LIST_TOP = 50;
    static constexpr int TEXT_TOP = 52;
    static constexpr size_t PAGE_BUFFER = 2048;

    enum Mode : uint8_t { LIST, READING };
    Mode mode = LIST;
    std::vector<std::string> files;
    bool filesLoaded = false;
    uint8_t listPage = 0;

    std::string path;
    uint32_t fileSize = 0;
    uint32_t pageStart = 0;
    uint32_t nextStart = 0;
    bool alignStart = false;       // pageStart was guessed: move it to the start of a word
    std::vector<uint32_t> history; // starts of the pages before this one, in this reading session
    uint8_t turns = 0;             // page turns since the last full refresh

    void renderList();
    void renderPage();
    void header(const char *title, const char *subtitle, const char *button);
    void startReading(const std::string &newPath);
    void nextPage();
    void previousPage();
    uint16_t advance(uint8_t c);
    std::string fitWidth(const std::string &text, int maxWidth);
};

} // namespace NicheGraphics::InkHUD

#endif
