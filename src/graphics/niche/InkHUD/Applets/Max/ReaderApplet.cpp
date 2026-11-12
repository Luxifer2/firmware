#include "configuration.h"

#if defined(MESHTASTIC_INCLUDE_INKHUD) && defined(T_DECK_MAX)

#include "./ReaderApplet.h"

#include "./MaxFiles.h"
#include "./MaxUi.h"
#include "SPILock.h"

#ifdef HAS_SDCARD
#include <SD.h>
#endif

using namespace NicheGraphics;

InkHUD::ReaderApplet *InkHUD::ReaderApplet::instance = nullptr;

static uint8_t pageBuffer[2048];
static constexpr uint8_t FULL_REFRESH_EVERY = 10; // page turns between full refreshes, against e-ink ghosting
static constexpr size_t HISTORY_MAX = 200;

// One character of UTF-8 (or of plain Latin-1, if the file is not UTF-8) as a byte of the font's Windows-1252
// encoding. False if the character is cut by the end of the buffer.
static bool decodeChar(const uint8_t *b, size_t i, size_t n, uint8_t &c, size_t &len)
{
    const uint8_t b0 = b[i];
    if (b0 < 0x80) {
        c = b0;
        len = 1;
        return true;
    }
    const size_t need = (b0 & 0xE0) == 0xC0 ? 2 : ((b0 & 0xF0) == 0xE0 ? 3 : ((b0 & 0xF8) == 0xF0 ? 4 : 0));
    if (need == 0) {
        c = b0; // not UTF-8: take it as Latin-1
        len = 1;
        return true;
    }
    if (i + need > n)
        return false;
    uint32_t cp = b0 & (0x7F >> need);
    for (size_t k = 1; k < need; k++) {
        if ((b[i + k] & 0xC0) != 0x80) {
            c = b0;
            len = 1;
            return true;
        }
        cp = (cp << 6) | (b[i + k] & 0x3F);
    }
    len = need;
    if (cp >= 0xA0 && cp <= 0xFF) {
        c = (uint8_t)cp;
        return true;
    }
    switch (cp) {
    case 0xFEFF: // byte order mark
        c = 0;
        break;
    case 0x20AC:
        c = 0x80;
        break;
    case 0x2026:
        c = 0x85;
        break;
    case 0x2018:
        c = 0x91;
        break;
    case 0x2019:
        c = 0x92;
        break;
    case 0x201C:
        c = 0x93;
        break;
    case 0x201D:
        c = 0x94;
        break;
    case 0x2022:
        c = 0x95;
        break;
    case 0x2013:
        c = 0x96;
        break;
    case 0x2014:
        c = 0x97;
        break;
    default:
        c = '?';
        break;
    }
    return true;
}

InkHUD::ReaderApplet::ReaderApplet()
{
    instance = this;
}

void InkHUD::ReaderApplet::txt(int x, int y, const char *text, int font, int align, int valign, uint16_t color)
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

uint16_t InkHUD::ReaderApplet::advance(uint8_t c)
{
    const GFXfont *f = fontMedium.gfxFont;
    if (!f || c < f->first || c > f->last)
        return 0;
    return f->glyph[c - f->first].xAdvance;
}

// Shorten text (already in the font's encoding) with "..." until it fits
std::string InkHUD::ReaderApplet::fitWidth(const std::string &text, int maxWidth)
{
    if (getTextWidth(text) <= maxWidth)
        return text;
    std::string s = text;
    while (!s.empty() && getTextWidth(s + "...") > maxWidth)
        s.pop_back();
    return s + "...";
}

void InkHUD::ReaderApplet::onForeground()
{
    if (mode == LIST)
        filesLoaded = false; // the card may have new files
}

void InkHUD::ReaderApplet::header(const char *title, const char *subtitle, const char *button)
{
    const int w = width();
    MaxUi::hex(*this, 22, 22, 13, MaxUi::K);
    fillRect(19, 19, 7, 7, BLACK);
    const int right = button ? w - 72 : w - 12;
    setFont(fontMedium);
    txt(42, 16, fitWidth(parse(title), right - 42).c_str(), MaxUi::MEDIUM, MaxUi::LEFT, MaxUi::MIDDLE, MaxUi::K);
    txt(42, 32, subtitle, MaxUi::SMALL, MaxUi::LEFT, MaxUi::MIDDLE, MaxUi::K);
    if (button) {
        drawRect(w - 66, 8, 58, 28, BLACK);
        drawRect(w - 65, 9, 56, 26, BLACK);
        txt(w - 37, 22, button, MaxUi::SMALL, MaxUi::CENTER, MaxUi::MIDDLE, MaxUi::K);
    }
    MaxUi::divider(*this, 8, width() - 8, 45, MaxUi::K);
}

void InkHUD::ReaderApplet::onRender(bool full)
{
    (void)full;
    fillRect(0, 0, width(), height(), WHITE);
    MaxUi::brackets(*this, 2, 2, width() - 4, height() - 4, 10, MaxUi::K);
    if (mode == READING)
        renderPage();
    else
        renderList();
}

void InkHUD::ReaderApplet::renderList()
{
    const int w = width(), h = height();
    if (!filesLoaded) {
        maxListTextFiles(files);
        filesLoaded = true;
    }
    const int pages = files.empty() ? 1 : (int)((files.size() + LIST_ROWS - 1) / LIST_ROWS);
    if (listPage >= pages)
        listPage = pages - 1;

    char sub[32];
    snprintf(sub, sizeof(sub), "%u FILE .TXT", (unsigned)files.size());
    header("FILE DI TESTO", sub, nullptr);

    if (!maxSdPresent()) {
        txt(w / 2, h / 2 - 10, "MICROSD ASSENTE", MaxUi::MEDIUM, MaxUi::CENTER, MaxUi::MIDDLE, MaxUi::K);
        txt(w / 2, h / 2 + 12, "inseriscila e riaccendi", MaxUi::SMALL, MaxUi::CENTER, MaxUi::MIDDLE, MaxUi::K);
        return;
    }
    if (files.empty()) {
        txt(w / 2, h / 2 - 10, "NESSUN FILE .TXT", MaxUi::MEDIUM, MaxUi::CENTER, MaxUi::MIDDLE, MaxUi::K);
        txt(w / 2, h / 2 + 12, "mettili nella cartella libri", MaxUi::SMALL, MaxUi::CENTER, MaxUi::MIDDLE, MaxUi::K);
        return;
    }

    setFont(fontMedium);
    for (int r = 0; r < LIST_ROWS; r++) {
        const size_t idx = (size_t)listPage * LIST_ROWS + r;
        if (idx >= files.size())
            break;
        const int top = LIST_TOP + r * ROW_H;
        const std::string &p = files[idx];
        const size_t slash = p.find_last_of('/');
        const std::string name = parse(slash == std::string::npos ? p : p.substr(slash + 1));
        setFont(fontMedium);
        txt(12, top + ROW_H / 2, fitWidth(name, w - 44).c_str(), MaxUi::MEDIUM, MaxUi::LEFT, MaxUi::MIDDLE, MaxUi::K);
        txt(w - 16, top + ROW_H / 2, ">", MaxUi::MEDIUM, MaxUi::CENTER, MaxUi::MIDDLE, MaxUi::K);
        drawLine(8, top + ROW_H - 1, w - 8, top + ROW_H - 1, BLACK);
    }

    if (pages > 1) {
        char line[32];
        snprintf(line, sizeof(line), "PAGINA %d DI %d", listPage + 1, pages);
        txt(w / 2, h - 16, line, MaxUi::SMALL, MaxUi::CENTER, MaxUi::MIDDLE, MaxUi::K);
        if (listPage > 0)
            txt(16, h - 16, "<", MaxUi::MEDIUM, MaxUi::LEFT, MaxUi::MIDDLE, MaxUi::K);
        if (listPage + 1 < pages)
            txt(w - 16, h - 16, ">", MaxUi::MEDIUM, MaxUi::RIGHT, MaxUi::MIDDLE, MaxUi::K);
    }
}

void InkHUD::ReaderApplet::renderPage()
{
    const int w = width(), h = height();
    const size_t slash = path.find_last_of('/');
    const std::string name = slash == std::string::npos ? path : path.substr(slash + 1);
    const std::string folder = slash == std::string::npos || slash == 0 ? "/" : path.substr(0, slash);

    // Read the part of the file this page starts at
    size_t n = 0;
    bool opened = false;
#ifdef HAS_SDCARD
    {
        concurrency::LockGuard guard(spiLock);
        File f = SD.open(path.c_str(), FILE_READ);
        if (f) {
            opened = true;
            fileSize = f.size();
            if (pageStart >= fileSize)
                pageStart = 0;
            if (f.seek(pageStart))
                n = f.read(pageBuffer, sizeof(pageBuffer));
            f.close();
        }
    }
#endif
    if (!opened) {
        header(name.c_str(), folder.c_str(), "ELENCO");
        txt(w / 2, h / 2, "FILE NON LEGGIBILE", MaxUi::MEDIUM, MaxUi::CENTER, MaxUi::MIDDLE, MaxUi::K);
        return;
    }

    // A guessed start (going back past the first page of this session): begin at the next word
    size_t i = 0;
    if (alignStart) {
        while (i < n && (pageBuffer[i] & 0xC0) == 0x80)
            i++;
        size_t j = i;
        while (j < n && j < i + 200 && pageBuffer[j] != ' ' && pageBuffer[j] != '\n')
            j++;
        if (j < n && j < i + 200)
            i = j + 1;
        alignStart = false;
    }
    const uint32_t bufferStart = pageStart;
    pageStart = bufferStart + i;

    // Lay out the lines: wrap at spaces, keep the line breaks of the file
    setFont(fontMedium);
    const int left = 10, maxWidth = w - 20;
    const int lineHeight = fontMedium.lineHeight();
    const int maxLines = (h - 30 - TEXT_TOP) / lineHeight;
    std::vector<std::string> lines;
    std::string line;
    int lineWidth = 0;
    int lastSpace = -1;
    size_t afterSpace = i;
    size_t pageEnd = i;
    bool wrapped = false;
    while (i < n && (int)lines.size() < maxLines) {
        uint8_t c;
        size_t len;
        if (!decodeChar(pageBuffer, i, n, c, len))
            break;
        if (c == '\r' || c == 0) {
            i += len;
            continue;
        }
        if (c == '\n') {
            lines.push_back(line);
            line.clear();
            lineWidth = 0;
            lastSpace = -1;
            wrapped = false;
            i += len;
            pageEnd = i;
            continue;
        }
        if (c == '\t' || c == 0xA0)
            c = ' ';
        if (c == ' ' && line.empty() && wrapped) {
            i += len; // no spaces at the start of a wrapped line
            pageEnd = i;
            continue;
        }
        const int cw = advance(c);
        if (lineWidth + cw > maxWidth && !line.empty()) {
            if (c == ' ') {
                lines.push_back(line);
                line.clear();
                lineWidth = 0;
                lastSpace = -1;
                wrapped = true;
                i += len;
                pageEnd = i;
                continue;
            }
            if (lastSpace >= 0) {
                // Move the last word to the next line
                std::string rest = line.substr(lastSpace + 1);
                line.resize(lastSpace);
                lines.push_back(line);
                pageEnd = afterSpace;
                line = rest;
                lineWidth = 0;
                for (unsigned char r : rest)
                    lineWidth += advance(r);
                lastSpace = -1;
                wrapped = true;
                continue; // c is not used yet
            }
            // A word longer than the line: cut it
            lines.push_back(line);
            line.clear();
            lineWidth = 0;
            wrapped = true;
            pageEnd = i;
            continue;
        }
        if (c == ' ') {
            lastSpace = (int)line.size();
            afterSpace = i + len;
        } else {
            wrapped = false;
        }
        line += (char)c;
        lineWidth += cw;
        i += len;
    }
    if ((int)lines.size() < maxLines && !line.empty()) {
        lines.push_back(line);
        pageEnd = i;
    }
    nextStart = bufferStart + pageEnd;
    if (nextStart <= pageStart)
        nextStart = bufferStart + (n ? n : 1); // never stay on the same page

    char sub[48];
    const unsigned percent = fileSize ? (unsigned)((uint64_t)pageStart * 100 / fileSize) : 0;
    snprintf(sub, sizeof(sub), "%s  -  %u%%", folder.c_str(), percent);
    header(name.c_str(), sub, "ELENCO");

    setFont(fontMedium);
    setTextColor(BLACK);
    for (size_t k = 0; k < lines.size(); k++)
        printAt(left, TEXT_TOP + (int)k * lineHeight, lines[k], LEFT, TOP);

    if (pageStart > 0)
        txt(12, h - 16, "< INDIETRO", MaxUi::SMALL, MaxUi::LEFT, MaxUi::MIDDLE, MaxUi::K);
    txt(w - 12, h - 16, nextStart < fileSize ? "AVANTI >" : "FINE", MaxUi::SMALL, MaxUi::RIGHT, MaxUi::MIDDLE, MaxUi::K);
}

bool InkHUD::ReaderApplet::onTouchPoint(uint16_t x, uint16_t y, bool longPress)
{
    if (longPress)
        return false;
    const int w = width(), h = height();

    // The hexagon in the title brings you home
    if (x < 44 && y < 44) {
        inkhud->showApplet(MaxUi::A_HOME);
        return true;
    }

    if (mode == READING) {
        if (y < 44) {
            if (x >= w - 72) {
                mode = LIST;
                filesLoaded = false;
                requestUpdate(Drivers::EInk::UpdateTypes::FAST);
            }
            return true;
        }
        if (x < w / 3)
            previousPage();
        else
            nextPage();
        return true;
    }

    // List of files
    if (y >= h - 32) {
        const int pages = files.empty() ? 1 : (int)((files.size() + LIST_ROWS - 1) / LIST_ROWS);
        if (x < w / 2 && listPage > 0)
            listPage--;
        else if (x >= w / 2 && listPage + 1 < pages)
            listPage++;
        requestUpdate(Drivers::EInk::UpdateTypes::FAST);
        return true;
    }
    if (y >= LIST_TOP) {
        const int r = (y - LIST_TOP) / ROW_H;
        const size_t idx = (size_t)listPage * LIST_ROWS + r;
        if (r < LIST_ROWS && idx < files.size()) {
            startReading(files[idx]);
            requestUpdate(Drivers::EInk::UpdateTypes::FULL);
        }
    }
    return true;
}

void InkHUD::ReaderApplet::startReading(const std::string &newPath)
{
    path = newPath;
    mode = READING;
    pageStart = maxReaderPosition(path);
    nextStart = pageStart;
    fileSize = 0;
    alignStart = false;
    history.clear();
    turns = 0;
}

void InkHUD::ReaderApplet::nextPage()
{
    if (nextStart >= fileSize)
        return;
    history.push_back(pageStart);
    if (history.size() > HISTORY_MAX)
        history.erase(history.begin());
    pageStart = nextStart;
    maxReaderSetPosition(path, pageStart);
    turns++;
    requestUpdate(turns % FULL_REFRESH_EVERY ? Drivers::EInk::UpdateTypes::FAST : Drivers::EInk::UpdateTypes::FULL);
}

void InkHUD::ReaderApplet::previousPage()
{
    if (!history.empty()) {
        pageStart = history.back();
        history.pop_back();
    } else if (pageStart > 0) {
        // Before the first page of this session: go back by about one page
        const uint32_t span = nextStart > pageStart ? nextStart - pageStart : 800;
        pageStart = pageStart > span ? pageStart - span : 0;
        alignStart = pageStart > 0;
    } else {
        return;
    }
    maxReaderSetPosition(path, pageStart);
    turns++;
    requestUpdate(turns % FULL_REFRESH_EVERY ? Drivers::EInk::UpdateTypes::FAST : Drivers::EInk::UpdateTypes::FULL);
}

void InkHUD::ReaderApplet::openList(InkHUD *hud)
{
    if (instance) {
        instance->mode = LIST;
        instance->filesLoaded = false;
    }
    hud->showApplet(MaxUi::A_READER);
}

void InkHUD::ReaderApplet::openFile(InkHUD *hud, const std::string &path)
{
    if (instance)
        instance->startReading(path);
    hud->showApplet(MaxUi::A_READER);
}

#endif
