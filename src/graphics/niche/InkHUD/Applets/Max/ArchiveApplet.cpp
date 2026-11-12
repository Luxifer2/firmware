#include "configuration.h"

#if defined(MESHTASTIC_INCLUDE_INKHUD) && defined(T_DECK_MAX)

#include "./ArchiveApplet.h"

#include "./MaxFiles.h"
#include "./MaxHunter.h"
#include "./MaxTrack.h"
#include "./MaxUi.h"
#include "./NoteApplet.h"
#include "./ReaderApplet.h"

using namespace NicheGraphics;

void InkHUD::ArchiveApplet::txt(int x, int y, const char *text, int font, int align, int valign, uint16_t color)
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

void InkHUD::ArchiveApplet::onForeground()
{
    refreshFiles = true; // count the text files again: the card may have changed
}

void InkHUD::ArchiveApplet::onRender(bool full)
{
    (void)full;
    MaxUi::Archive a;
    a.sd = maxSdCardMb(a.sdMb);
    a.notes = maxDiaryCount();
    a.textFiles = maxTextFileCount(refreshFiles);
    refreshFiles = false;
    a.nodesTotal = maxHunterTotal();
    a.nodesNew = maxHunterNew();
    static char track[24];
    maxGpxStatusText(track, sizeof(track), true);
    a.track = track;
    MaxUi::renderArchive(*this, width(), height(), a);
}

bool InkHUD::ArchiveApplet::onTouchPoint(uint16_t x, uint16_t y, bool longPress)
{
    if (longPress)
        return false;

    // The hexagon in the title brings you home
    if (x < 44 && y < 44) {
        inkhud->showApplet(MaxUi::A_HOME);
        return true;
    }

    switch (MaxUi::archiveRowAt(width(), x, y)) {
    case MaxUi::R_NOTE:
        NoteApplet::open(inkhud);
        return true;
    case MaxUi::R_DIARY:
        ReaderApplet::openFile(inkhud, MAX_DIARY_PATH);
        return true;
    case MaxUi::R_FILES:
        ReaderApplet::openList(inkhud);
        return true;
    default:
        return true;
    }
}

#endif
