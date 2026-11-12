#include "configuration.h"

#if defined(MESHTASTIC_INCLUDE_INKHUD) && defined(T_DECK_MAX)

#include "./NoteApplet.h"

#include "./MaxFiles.h"
#include "./MaxInfo.h"
#include "./MaxUi.h"

using namespace NicheGraphics;

static constexpr uint32_t RESULT_SHOWN_MS = 1500;

InkHUD::NoteApplet::NoteApplet() : concurrency::OSThread("NoteApplet")
{
    OSThread::enabled = false;
}

void InkHUD::NoteApplet::txt(int x, int y, const char *text, int font, int align, int valign, uint16_t color)
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

void InkHUD::NoteApplet::onRender(bool full)
{
    (void)full;
    const int w = width(), h = height();
    fillRect(0, 0, w, h, WHITE);
    MaxUi::brackets(*this, 2, 2, w - 4, h - 4, 10, MaxUi::K);

    MaxUi::hex(*this, 22, 22, 13, MaxUi::K);
    MaxUi::iconPencil(*this, 22, 22, MaxUi::K);
    txt(42, 16, "NUOVA NOTA", MaxUi::MEDIUM, MaxUi::LEFT, MaxUi::MIDDLE, MaxUi::K);
    txt(42, 32, info, MaxUi::SMALL, MaxUi::LEFT, MaxUi::MIDDLE, MaxUi::K);
    MaxUi::divider(*this, 8, w - 8, 45, MaxUi::K);

    const int buttonsTop = h - 44;
    if (state == SAVED || state == FAILED) {
        txt(w / 2, h / 2 - 12, state == SAVED ? "NOTA SALVATA" : "NON SALVATA", MaxUi::MEDIUM, MaxUi::CENTER, MaxUi::MIDDLE,
            MaxUi::K);
        txt(w / 2, h / 2 + 10, state == SAVED ? "nel diario sulla microSD" : "controlla la microSD", MaxUi::SMALL, MaxUi::CENTER,
            MaxUi::MIDDLE, MaxUi::K);
        return;
    }

    // The note so far, with a cursor
    setFont(fontMedium);
    setTextColor(BLACK);
    if (!sd)
        txt(12, 54, "MICROSD ASSENTE: NON SI PUO' SALVARE", MaxUi::SMALL, MaxUi::LEFT, MaxUi::TOP, MaxUi::K);
    setFont(fontMedium);
    printWrapped(12, sd ? 54 : 70, w - 24, parse(text) + "_");

    char count[16];
    snprintf(count, sizeof(count), "%u/%u", (unsigned)text.size(), (unsigned)MAX_CHARS);
    txt(w - 12, buttonsTop - 10, count, MaxUi::SMALL, MaxUi::RIGHT, MaxUi::MIDDLE, MaxUi::K);

    // Buttons: cancel (heart) on the left, save (speech bubble) on the right
    const int bw = (w - 30) / 2;
    drawRect(10, buttonsTop, bw, 32, BLACK);
    drawRect(11, buttonsTop + 1, bw - 2, 30, BLACK);
    txt(10 + bw / 2, buttonsTop + 16, "ANNULLA", MaxUi::SMALL, MaxUi::CENTER, MaxUi::MIDDLE, MaxUi::K);
    fillRoundRect(w - 10 - bw, buttonsTop, bw, 32, 6, BLACK);
    txt(w - 10 - bw / 2, buttonsTop + 16, "SALVA", MaxUi::SMALL, MaxUi::CENTER, MaxUi::MIDDLE, MaxUi::W);
}

void InkHUD::NoteApplet::onForeground()
{
    SystemApplet::lockRendering = true;
    SystemApplet::lockRequests = true;
    SystemApplet::handleInput = true;
    SystemApplet::handleFreeText = true;
    state = WRITING;
    text.clear();
    sd = maxSdPresent();

    // When and where the note was started, shown in the title
    MaxUi::Info in;
    gatherMaxInfo(in);
    int32_t lat, lon;
    char where[24] = "senza posizione";
    if (maxOwnPosition(lat, lon))
        snprintf(where, sizeof(where), "%.4f %.4f", lat * 1e-7, lon * 1e-7);
    if (in.timeValid)
        snprintf(info, sizeof(info), "%02u:%02u  %s", (unsigned)in.hh, (unsigned)in.mm, where);
    else
        snprintf(info, sizeof(info), "%s", where);
}

void InkHUD::NoteApplet::onBackground()
{
    SystemApplet::lockRendering = false;
    SystemApplet::lockRequests = false;
    SystemApplet::handleInput = false;
    SystemApplet::handleFreeText = false;
    OSThread::enabled = false;
}

bool InkHUD::NoteApplet::onTouchPoint(uint16_t x, uint16_t y, bool longPress)
{
    if (longPress)
        return true;
    if (state != WRITING) {
        close();
        return true;
    }
    if (y >= height() - 44) {
        if (x < width() / 2)
            onFreeTextCancel();
        else
            onFreeTextDone();
    }
    return true;
}

void InkHUD::NoteApplet::onFreeText(char c)
{
    if (state != WRITING)
        return;
    if (c == '\b') {
        if (!text.empty())
            text.pop_back();
    } else if (text.size() < MAX_CHARS) {
        text += c;
    }
    requestUpdate(Drivers::EInk::UpdateTypes::FAST);
}

void InkHUD::NoteApplet::onFreeTextDone()
{
    if (state != WRITING || text.empty()) {
        close();
        return;
    }
    save();
}

void InkHUD::NoteApplet::onFreeTextCancel()
{
    close();
}

void InkHUD::NoteApplet::save()
{
    state = maxDiaryAppend(text) ? SAVED : FAILED;
    requestUpdate(Drivers::EInk::UpdateTypes::FAST);
    // Show the result for a moment, then go back
    OSThread::setIntervalFromNow(RESULT_SHOWN_MS);
    OSThread::enabled = true;
}

int32_t InkHUD::NoteApplet::runOnce()
{
    if (isForeground() && state != WRITING)
        close();
    return OSThread::disable();
}

void InkHUD::NoteApplet::close()
{
    if (!isForeground())
        return;
    sendToBackground();
    inkhud->forceUpdate(Drivers::EInk::UpdateTypes::FAST, true);
}

void InkHUD::NoteApplet::open(InkHUD *hud)
{
    SystemApplet *lock = hud->getSystemApplet("Lock");
    if (lock && lock->isForeground())
        return;
    SystemApplet *menu = hud->getSystemApplet("Menu");
    if (menu && menu->isForeground())
        menu->sendToBackground();
    SystemApplet *note = hud->getSystemApplet("Note");
    if (note && !note->isForeground()) {
        note->bringToForeground();
        hud->forceUpdate(Drivers::EInk::UpdateTypes::FAST, true);
    }
}

#endif
