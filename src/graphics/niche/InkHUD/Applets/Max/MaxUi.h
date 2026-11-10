#pragma once

// Drawing code for the T-Deck Max home screen and lock screen.
// Everything here is plain drawing on a canvas type G, which needs these members:
//   drawPixel, drawLine, drawRect, fillRect, drawCircle, fillCircle, drawCircleHelper, fillTriangle, fillRoundRect,
//   txt(x, y, text, font, align, valign, color)
// so the same code can be tried out on a PC (see tools used during development) and runs unchanged inside InkHUD.

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

namespace MaxUi
{

constexpr uint16_t K = 0; // black
constexpr uint16_t W = 1; // white

// Positions in the list of user applets (the order they are added in the variant's nicheGraphics.h)
enum AppletIndex : uint8_t { A_DMS = 1, A_CHANNEL0 = 2, A_POSITIONS = 4, A_RECENTS = 6, A_HEARD = 7, A_FAVORITES = 8, A_HOME = 9, A_CONTROLS = 10 };

enum Font : uint8_t { SMALL = 0, MEDIUM = 1, LARGE = 2 };
enum Align : uint8_t { LEFT = 0, CENTER = 1, RIGHT = 2 };
enum VAlign : uint8_t { TOP = 0, MIDDLE = 1 };

// Everything the screens show, gathered by the applets
struct Info {
    bool timeValid = false;
    uint8_t hh = 0, mm = 0;
    uint8_t wday = 0; // 0 = Sunday
    uint8_t mday = 1;
    uint8_t mon = 0; // 0 = January
    uint16_t year = 2026;
    bool gpsPresent = false;
    bool gpsLock = false;
    uint8_t sats = 0;
    int32_t lat = 0, lon = 0; // degrees * 1e7
    bool hasBatt = false;
    uint8_t batt = 0; // percent
    bool charging = false;
    uint16_t nodes = 0;  // nodes heard recently
    uint16_t unread = 0; // messages not yet read
    const char *diag = nullptr;
};

// Date from days since 1970-01-01 (civil calendar)
inline void civilFromDays(int32_t z, uint16_t &y, uint8_t &m, uint8_t &d)
{
    z += 719468;
    const int32_t era = (z >= 0 ? z : z - 146096) / 146097;
    const uint32_t doe = (uint32_t)(z - era * 146097);
    const uint32_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    const int32_t yy = (int32_t)yoe + era * 400;
    const uint32_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const uint32_t mp = (5 * doy + 2) / 153;
    d = (uint8_t)(doy - (153 * mp + 2) / 5 + 1);
    m = (uint8_t)(mp < 10 ? mp + 2 : mp - 10); // 0 = January
    y = (uint16_t)(yy + (m <= 1 ? 1 : 0));
}

// Fill the time and date fields from "local" epoch seconds (already shifted to the local time zone)
inline void setTime(Info &i, uint32_t localEpoch)
{
    const uint32_t days = localEpoch / 86400;
    const uint32_t sod = localEpoch % 86400;
    i.hh = sod / 3600;
    i.mm = (sod % 3600) / 60;
    i.wday = (days + 4) % 7; // 1970-01-01 was a Thursday
    civilFromDays((int32_t)days, i.year, i.mon, i.mday);
    i.timeValid = true;
}

inline const char *weekdayName(uint8_t d)
{
    static const char *n[7] = {"DOMENICA", "LUNEDI", "MARTEDI", "MERCOLEDI", "GIOVEDI", "VENERDI", "SABATO"};
    return n[d % 7];
}
inline const char *monthName(uint8_t m)
{
    static const char *n[12] = {"GENNAIO", "FEBBRAIO", "MARZO",   "APRILE",   "MAGGIO",    "GIUGNO",
                                "LUGLIO",  "AGOSTO",   "SETTEMBRE", "OTTOBRE", "NOVEMBRE", "DICEMBRE"};
    return n[m % 12];
}
inline const char *monthShort(uint8_t m)
{
    static const char *n[12] = {"GEN", "FEB", "MAR", "APR", "MAG", "GIU", "LUG", "AGO", "SET", "OTT", "NOV", "DIC"};
    return n[m % 12];
}

// ------------------------------------------------------------------------------------------------
// Segment digits: the clock
// ------------------------------------------------------------------------------------------------

template <class G> void segH(G &g, int x, int y, int len, int th, uint16_t c)
{
    const int h = th / 2;
    g.fillRect(x + h, y, len - 2 * h, th, c);
    g.fillTriangle(x, y + h, x + h, y, x + h, y + th - 1, c);
    g.fillTriangle(x + len - 1, y + h, x + len - 1 - h, y, x + len - 1 - h, y + th - 1, c);
}

template <class G> void segV(G &g, int x, int y, int len, int th, uint16_t c)
{
    const int h = th / 2;
    g.fillRect(x, y + h, th, len - 2 * h, c);
    g.fillTriangle(x + h, y, x, y + h, x + th - 1, y + h, c);
    g.fillTriangle(x + h, y + len - 1, x, y + len - 1 - h, x + th - 1, y + len - 1 - h, c);
}

// One digit, w x h pixels, top left at x, y
template <class G> void digit(G &g, int x, int y, int w, int h, int th, int n, uint16_t c)
{
    // bit0 top, bit1 top right, bit2 bottom right, bit3 bottom, bit4 bottom left, bit5 top left, bit6 middle
    static const uint8_t mask[10] = {0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F};
    const uint8_t m = mask[n % 10];
    const int mid = (h - th) / 2;
    const int vlen = mid + th;
    if (m & 0x01)
        segH(g, x + 1, y, w - 2, th, c);
    if (m & 0x40)
        segH(g, x + 1, y + mid, w - 2, th, c);
    if (m & 0x08)
        segH(g, x + 1, y + h - th, w - 2, th, c);
    if (m & 0x20)
        segV(g, x, y + 1, vlen - 2, th, c);
    if (m & 0x02)
        segV(g, x + w - th, y + 1, vlen - 2, th, c);
    if (m & 0x10)
        segV(g, x, y + mid + 1, vlen - 2, th, c);
    if (m & 0x04)
        segV(g, x + w - th, y + mid + 1, vlen - 2, th, c);
}

inline int clockWidth(int h)
{
    const int w = h * 55 / 100;
    const int th = h / 8 > 2 ? h / 8 : 2;
    const int gap = h / 8 > 3 ? h / 8 : 3;
    return 4 * w + 3 * gap + (th + 2 * gap);
}

// "HH:MM", total width = clockWidth(h). Pass left = x of the left edge.
template <class G> void clockDigits(G &g, int left, int top, int h, int hh, int mm, bool valid, uint16_t c)
{
    const int w = h * 55 / 100;
    const int th = h / 8 > 2 ? h / 8 : 2;
    const int gap = h / 8 > 3 ? h / 8 : 3;
    int x = left;
    if (valid) {
        digit(g, x, top, w, h, th, hh / 10, c);
        x += w + gap;
        digit(g, x, top, w, h, th, hh % 10, c);
        x += w + gap;
    } else {
        // "--:--"
        for (int i = 0; i < 2; i++) {
            g.fillRect(x + 1, top + h / 2 - th / 2, w - 2, th, c);
            x += w + gap;
        }
    }
    // colon
    const int cx = x + gap + th / 2;
    g.fillRect(cx - th / 2, top + h / 3 - th / 2, th, th, c);
    g.fillRect(cx - th / 2, top + 2 * h / 3 - th / 2, th, th, c);
    x += th + 2 * gap;
    if (valid) {
        digit(g, x, top, w, h, th, mm / 10, c);
        x += w + gap;
        digit(g, x, top, w, h, th, mm % 10, c);
    } else {
        for (int i = 0; i < 2; i++) {
            g.fillRect(x + 1, top + h / 2 - th / 2, w - 2, th, c);
            x += w + gap;
        }
    }
}

// ------------------------------------------------------------------------------------------------
// Hexagons
// ------------------------------------------------------------------------------------------------

constexpr float SQRT3 = 1.7320508f;

// Pointy-top hexagon outline
template <class G> void hex(G &g, int cx, int cy, int r, uint16_t c)
{
    const int hw = (int)lroundf(r * SQRT3 / 2.0f);
    const int hr = r / 2;
    g.drawLine(cx, cy - r, cx + hw, cy - hr, c);
    g.drawLine(cx + hw, cy - hr, cx + hw, cy + hr, c);
    g.drawLine(cx + hw, cy + hr, cx, cy + r, c);
    g.drawLine(cx, cy + r, cx - hw, cy + hr, c);
    g.drawLine(cx - hw, cy + hr, cx - hw, cy - hr, c);
    g.drawLine(cx - hw, cy - hr, cx, cy - r, c);
}

template <class G> void hexFilled(G &g, int cx, int cy, int r, uint16_t c)
{
    const int hw = (int)lroundf(r * SQRT3 / 2.0f);
    const int hr = r / 2;
    g.fillTriangle(cx, cy, cx, cy - r, cx + hw, cy - hr, c);
    g.fillTriangle(cx, cy, cx + hw, cy - hr, cx + hw, cy + hr, c);
    g.fillTriangle(cx, cy, cx + hw, cy + hr, cx, cy + r, c);
    g.fillTriangle(cx, cy, cx, cy + r, cx - hw, cy + hr, c);
    g.fillTriangle(cx, cy, cx - hw, cy + hr, cx - hw, cy - hr, c);
    g.fillTriangle(cx, cy, cx - hw, cy - hr, cx, cy - r, c);
}

// Is the point inside the hexagon?
inline bool inHex(int px, int py, int cx, int cy, int r)
{
    const float dx = fabsf((float)(px - cx));
    const float dy = fabsf((float)(py - cy));
    const float hw = r * SQRT3 / 2.0f;
    return dx <= hw && dy <= r - dx / SQRT3;
}

// ------------------------------------------------------------------------------------------------
// Icons. All are drawn around (cx, cy) and fit in roughly 30 x 30 pixels.
// ------------------------------------------------------------------------------------------------

template <class G> void iconMessage(G &g, int cx, int cy, uint16_t c)
{
    const uint16_t bg = c == K ? W : K;
    g.fillRoundRect(cx - 14, cy - 12, 28, 19, 4, c);
    g.fillTriangle(cx - 8, cy + 6, cx - 3, cy + 6, cx - 9, cy + 13, c);
    g.fillRect(cx - 8, cy - 3, 4, 4, bg);
    g.fillRect(cx - 2, cy - 3, 4, 4, bg);
    g.fillRect(cx + 4, cy - 3, 4, 4, bg);
}

template <class G> void iconEnvelope(G &g, int cx, int cy, uint16_t c)
{
    g.drawRect(cx - 14, cy - 9, 28, 19, c);
    g.drawRect(cx - 13, cy - 8, 26, 17, c);
    g.drawLine(cx - 13, cy - 8, cx, cy + 3, c);
    g.drawLine(cx + 13, cy - 8, cx, cy + 3, c);
    g.drawLine(cx - 13, cy - 7, cx, cy + 4, c);
    g.drawLine(cx + 13, cy - 7, cx, cy + 4, c);
    g.drawLine(cx - 13, cy + 9, cx - 5, cy + 1, c);
    g.drawLine(cx + 13, cy + 9, cx + 5, cy + 1, c);
}

template <class G> void iconNodes(G &g, int cx, int cy, uint16_t c)
{
    g.drawLine(cx, cy - 9, cx - 11, cy + 8, c);
    g.drawLine(cx, cy - 9, cx + 11, cy + 8, c);
    g.drawLine(cx - 11, cy + 8, cx + 11, cy + 8, c);
    g.drawLine(cx, cy - 8, cx - 10, cy + 8, c);
    g.drawLine(cx, cy - 8, cx + 10, cy + 8, c);
    g.fillCircle(cx, cy - 9, 5, c);
    g.fillCircle(cx - 11, cy + 8, 5, c);
    g.fillCircle(cx + 11, cy + 8, 5, c);
}

template <class G> void iconPin(G &g, int cx, int cy, uint16_t c)
{
    const uint16_t bg = c == K ? W : K;
    g.fillCircle(cx, cy - 5, 10, c);
    g.fillTriangle(cx - 9, cy - 1, cx + 9, cy - 1, cx, cy + 14, c);
    g.fillCircle(cx, cy - 5, 4, bg);
}

template <class G> void iconHeart(G &g, int cx, int cy, uint16_t c)
{
    g.fillCircle(cx - 6, cy - 5, 7, c);
    g.fillCircle(cx + 6, cy - 5, 7, c);
    g.fillTriangle(cx - 13, cy - 2, cx + 13, cy - 2, cx, cy + 12, c);
    g.fillTriangle(cx - 12, cy - 5, cx + 12, cy - 5, cx, cy + 6, c);
}

template <class G> void iconGear(G &g, int cx, int cy, uint16_t c)
{
    const uint16_t bg = c == K ? W : K;
    for (int i = 0; i < 8; i++) {
        const float a = i * 3.14159265f / 4.0f;
        const int tx = cx + (int)lroundf(cosf(a) * 11.5f);
        const int ty = cy + (int)lroundf(sinf(a) * 11.5f);
        g.fillCircle(tx, ty, 3, c);
    }
    g.fillCircle(cx, cy, 10, c);
    g.fillCircle(cx, cy, 4, bg);
}

template <class G> void iconCrosshair(G &g, int cx, int cy, int r, uint16_t c)
{
    g.drawCircle(cx, cy, r, c);
    g.drawCircle(cx, cy, r - 1, c);
    g.drawLine(cx - r - 3, cy, cx - r + 3, cy, c);
    g.drawLine(cx + r - 3, cy, cx + r + 3, cy, c);
    g.drawLine(cx, cy - r - 3, cx, cy - r + 3, c);
    g.drawLine(cx, cy + r - 3, cx, cy + r + 3, c);
    g.fillCircle(cx, cy, 2, c);
}

template <class G> void iconPadlock(G &g, int cx, int cy, int s, bool locked, uint16_t c)
{
    // s = half width of the body
    const uint16_t bg = c == K ? W : K;
    const int bodyTop = cy - s / 3;
    g.fillRoundRect(cx - s, bodyTop, 2 * s, (int)(1.4f * s), 2, c);
    g.fillCircle(cx, bodyTop + s / 2, s / 5 + 1, bg);
    g.fillRect(cx - 1, bodyTop + s / 2, 2, s / 2, bg);
    const int sr = s * 2 / 3;
    if (locked) {
        g.drawCircleHelper(cx, bodyTop - 1, sr, 0x3, c);
        g.drawCircleHelper(cx, bodyTop - 1, sr - 1, 0x3, c);
        g.drawLine(cx - sr, bodyTop - 1, cx - sr, bodyTop + 1, c);
        g.drawLine(cx + sr, bodyTop - 1, cx + sr, bodyTop + 1, c);
        g.drawLine(cx - sr + 1, bodyTop - 1, cx - sr + 1, bodyTop + 1, c);
        g.drawLine(cx + sr - 1, bodyTop - 1, cx + sr - 1, bodyTop + 1, c);
    } else {
        g.drawCircleHelper(cx + sr, bodyTop - 4, sr, 0x3, c);
        g.drawCircleHelper(cx + sr, bodyTop - 4, sr - 1, 0x3, c);
        g.drawLine(cx, bodyTop - 4, cx, bodyTop + 1, c);
        g.drawLine(cx + 1, bodyTop - 4, cx + 1, bodyTop + 1, c);
    }
}

// Menu: hexagon with three bars
template <class G> void iconMenu(G &g, int cx, int cy, int r, uint16_t c)
{
    hex(g, cx, cy, r, c);
    hex(g, cx, cy, r - 1, c);
    const int hw = r / 2;
    for (int i = -1; i <= 1; i++)
        g.fillRect(cx - hw, cy + i * (r / 3) - 1, 2 * hw, 2, c);
}

template <class G> void iconArrow(G &g, int cx, int cy, uint16_t c)
{
    g.fillRect(cx - 9, cy - 2, 12, 5, c);
    g.fillTriangle(cx + 2, cy - 8, cx + 11, cy, cx + 2, cy + 8, c);
}

// Small versions (about 14 px) for status lines
template <class G> void miniHeart(G &g, int cx, int cy, uint16_t c)
{
    g.fillCircle(cx - 3, cy - 2, 3, c);
    g.fillCircle(cx + 3, cy - 2, 3, c);
    g.fillTriangle(cx - 6, cy - 1, cx + 6, cy - 1, cx, cy + 6, c);
}

template <class G> void miniNodes(G &g, int cx, int cy, uint16_t c)
{
    g.drawLine(cx, cy - 5, cx - 6, cy + 4, c);
    g.drawLine(cx, cy - 5, cx + 6, cy + 4, c);
    g.drawLine(cx - 6, cy + 4, cx + 6, cy + 4, c);
    g.fillCircle(cx, cy - 5, 2, c);
    g.fillCircle(cx - 6, cy + 4, 2, c);
    g.fillCircle(cx + 6, cy + 4, 2, c);
}

template <class G> void miniEnvelope(G &g, int cx, int cy, uint16_t c)
{
    g.drawRect(cx - 7, cy - 5, 14, 10, c);
    g.drawLine(cx - 7, cy - 5, cx, cy + 1, c);
    g.drawLine(cx + 6, cy - 5, cx, cy + 1, c);
}

// Battery: width w, height h, left top at x, y. Level in percent.
template <class G> void battery(G &g, int x, int y, int w, int h, int pct, bool charging, uint16_t c)
{
    const uint16_t bg = c == K ? W : K;
    g.drawRect(x, y, w - 2, h, c);
    g.fillRect(x + w - 2, y + h / 4, 2, h - h / 2, c);
    const int inner = w - 6;
    const int fill = inner * (pct > 100 ? 100 : pct) / 100;
    if (fill > 0)
        g.fillRect(x + 2, y + 2, fill, h - 4, c);
    if (charging) {
        const int bx = x + (w - 2) / 2;
        const int by = y + h / 2;
        g.fillTriangle(bx + 2, by - h / 2 - 1, bx - 4, by + 1, bx + 1, by + 1, bg);
        g.fillTriangle(bx - 2, by + h / 2 + 1, bx + 4, by - 1, bx - 1, by - 1, bg);
        g.drawLine(bx + 2, by - h / 2 - 1, bx - 4, by + 1, c);
        g.drawLine(bx - 2, by + h / 2 + 1, bx + 4, by - 1, c);
    }
}

// Corner brackets around a region
template <class G> void brackets(G &g, int x, int y, int w, int h, int len, uint16_t c)
{
    g.drawLine(x, y, x + len, y, c);
    g.drawLine(x, y, x, y + len, c);
    g.drawLine(x + w - 1, y, x + w - 1 - len, y, c);
    g.drawLine(x + w - 1, y, x + w - 1, y + len, c);
    g.drawLine(x, y + h - 1, x + len, y + h - 1, c);
    g.drawLine(x, y + h - 1, x, y + h - 1 - len, c);
    g.drawLine(x + w - 1, y + h - 1, x + w - 1 - len, y + h - 1, c);
    g.drawLine(x + w - 1, y + h - 1, x + w - 1, y + h - 1 - len, c);
}

// Dotted divider with a small hexagon in the middle
template <class G> void divider(G &g, int x0, int x1, int y, uint16_t c)
{
    const int mid = (x0 + x1) / 2;
    for (int x = x0; x < x1; x += 3)
        if (x < mid - 9 || x > mid + 9)
            g.drawPixel(x, y, c);
    hex(g, mid, y, 5, c);
    g.fillRect(mid - 1, y - 1, 3, 3, c);
}

// ------------------------------------------------------------------------------------------------
// Home screen
// ------------------------------------------------------------------------------------------------

// Eight hexagons in a honeycomb: three, two, three
enum Tile : uint8_t { T_MESSAGES, T_DIRECT, T_NODES, T_MAP, T_FAVORITES, T_CONTROLS, T_OPTIONS, T_LOCK, T_COUNT };

struct HomeLayout {
    int hexR;
    int hexCx[T_COUNT], hexCy[T_COUNT];
    int menuCx, menuCy, menuR;
    int statusY;
};

inline HomeLayout homeLayout(int w, int h)
{
    (void)h;
    HomeLayout L;
    int r = (int)((w - 16) / (3.0f * SQRT3));
    if (r > 42)
        r = 42;
    L.hexR = r;
    const float hexW = r * SQRT3;
    const int step = (int)(1.5f * r) + 2;
    const int top = 72 + r;
    const int mid = w / 2;
    // row 0: tiles 0, 1, 2   row 1: tiles 3, 4   row 2: tiles 5, 6, 7
    L.hexCx[0] = mid - (int)hexW;
    L.hexCx[1] = mid;
    L.hexCx[2] = mid + (int)hexW;
    L.hexCx[3] = mid - (int)(hexW / 2.0f);
    L.hexCx[4] = mid + (int)(hexW / 2.0f);
    L.hexCx[5] = mid - (int)hexW;
    L.hexCx[6] = mid;
    L.hexCx[7] = mid + (int)hexW;
    for (int i = 0; i < T_COUNT; i++)
        L.hexCy[i] = top + (i < 3 ? 0 : (i < 5 ? step : 2 * step));
    L.menuR = 13;
    L.menuCx = w - 58;
    L.menuCy = 17;
    L.statusY = 47;
    return L;
}

// Which tile (if any) is at this point
inline int homeTileAt(int w, int h, int x, int y)
{
    const HomeLayout L = homeLayout(w, h);
    for (int i = 0; i < T_COUNT; i++)
        if (inHex(x, y, L.hexCx[i], L.hexCy[i], L.hexR))
            return i;
    return -1;
}

inline bool homeMenuAt(int w, int h, int x, int y)
{
    const HomeLayout L = homeLayout(w, h);
    return (x - L.menuCx) * (x - L.menuCx) + (y - L.menuCy) * (y - L.menuCy) <= 20 * 20;
}

// Sliders (controls icon): three lines with knobs
template <class G> void iconSliders(G &g, int cx, int cy, uint16_t c)
{
    const int kn[3] = {-5, 4, -2};
    for (int i = 0; i < 3; i++) {
        const int y = cy - 8 + i * 8;
        g.drawLine(cx - 11, y, cx + 11, y, c);
        g.fillCircle(cx + kn[i] * 2, y, 3, c);
    }
}

template <class G> void badge(G &g, int cx, int cy, int n)
{
    char s[12];
    if (n > 99)
        snprintf(s, sizeof(s), "99+");
    else
        snprintf(s, sizeof(s), "%d", n);
    const int w = (int)strlen(s) * 7 + 8;
    g.fillRoundRect(cx - w / 2 - 2, cy - 9, w + 4, 18, 9, W);
    g.fillRoundRect(cx - w / 2, cy - 7, w, 14, 7, K);
    g.txt(cx, cy, s, SMALL, CENTER, MIDDLE, W);
}

template <class G> void renderHome(G &g, int w, int h, const Info &in)
{
    g.fillRect(0, 0, w, h, W);
    const HomeLayout L = homeLayout(w, h);

    // --- top bar: clock, date, menu button (the battery is drawn on top of the top right corner by InkHUD)
    clockDigits(g, 10, 4, 24, in.hh, in.mm, in.timeValid, K);
    if (in.timeValid) {
        char d[24];
        snprintf(d, sizeof(d), "%.3s %u %s", weekdayName(in.wday), in.mday, monthShort(in.mon));
        g.txt(10, 30, d, SMALL, LEFT, TOP, K);
    } else {
        g.txt(10, 30, "ORA NON IMPOSTATA", SMALL, LEFT, TOP, K);
    }
    iconMenu(g, L.menuCx, L.menuCy, L.menuR, K);

    // --- status row: GPS, nodes, messages
    const int sy = L.statusY;
    char s[24];
    iconCrosshair(g, 14, sy + 7, 5, K);
    if (!in.gpsPresent)
        snprintf(s, sizeof(s), "GPS --");
    else if (in.gpsLock)
        snprintf(s, sizeof(s), "FIX %u", in.sats);
    else
        snprintf(s, sizeof(s), "CERCO %u", in.sats);
    g.txt(26, sy, s, SMALL, LEFT, TOP, K);

    miniNodes(g, 104, sy + 7, K);
    snprintf(s, sizeof(s), "NODI %u", in.nodes);
    g.txt(115, sy, s, SMALL, LEFT, TOP, K);

    miniEnvelope(g, 188, sy + 7, K);
    snprintf(s, sizeof(s), "%u", in.unread);
    g.txt(199, sy, s, SMALL, LEFT, TOP, K);

    divider(g, 8, w - 8, sy + 21, K);

    // --- hexagon tiles
    static const char *labels[T_COUNT] = {"CHAT", "POSTA", "NODI", "MAPPA", "AMICI", "CONTROLLI", "OPZIONI", "BLOCCA"};
    for (int i = 0; i < T_COUNT; i++) {
        const int cx = L.hexCx[i], cy = L.hexCy[i];
        hex(g, cx, cy, L.hexR - 2, K);
        hex(g, cx, cy, L.hexR - 3, K);
        const int iy = cy - 9;
        switch (i) {
        case T_MESSAGES:
            iconMessage(g, cx, iy, K);
            break;
        case T_DIRECT:
            iconEnvelope(g, cx, iy, K);
            break;
        case T_NODES:
            iconNodes(g, cx, iy, K);
            break;
        case T_MAP:
            iconPin(g, cx, iy, K);
            break;
        case T_FAVORITES:
            iconHeart(g, cx, iy, K);
            break;
        case T_CONTROLS:
            iconSliders(g, cx, iy + 1, K);
            break;
        case T_OPTIONS:
            iconGear(g, cx, iy, K);
            break;
        case T_LOCK:
            iconPadlock(g, cx, iy + 2, 11, true, K);
            break;
        }
        g.txt(cx, cy + 15, labels[i], SMALL, CENTER, MIDDLE, K);
    }
    if (in.unread > 0)
        badge(g, L.hexCx[T_MESSAGES] + L.hexR / 2 + 4, L.hexCy[T_MESSAGES] - L.hexR / 2 - 3, in.unread);

    brackets(g, 2, 2, w - 4, h - 4, 10, K);
}

// ------------------------------------------------------------------------------------------------
// Lock screen
// ------------------------------------------------------------------------------------------------

template <class G> void renderLock(G &g, int w, int h, const Info &in)
{
    g.fillRect(0, 0, w, h, W);
    brackets(g, 2, 2, w - 4, h - 4, 14, K);

    // padlock in a hexagon
    hex(g, w / 2, 34, 24, K);
    hex(g, w / 2, 34, 22, K);
    iconPadlock(g, w / 2, 36, 9, true, K);

    // big clock
    const int ch = 64;
    const int cw = clockWidth(ch);
    clockDigits(g, (w - cw) / 2, 70, ch, in.hh, in.mm, in.timeValid, K);

    char s[40];
    if (in.timeValid) {
        snprintf(s, sizeof(s), "%s %u %s", weekdayName(in.wday), in.mday, monthName(in.mon));
        g.txt(w / 2, 150, s, MEDIUM, CENTER, TOP, K);
    } else {
        g.txt(w / 2, 150, "ORA NON IMPOSTATA", MEDIUM, CENTER, TOP, K);
    }

    divider(g, 10, w - 10, 178, K);

    // GPS
    int y = 194;
    iconCrosshair(g, 22, y + 10, 8, K);
    if (!in.gpsPresent) {
        g.txt(42, y + 10, "GPS NON DISPONIBILE", MEDIUM, LEFT, MIDDLE, K);
    } else if (in.gpsLock) {
        const double lat = in.lat / 1e7, lon = in.lon / 1e7;
        snprintf(s, sizeof(s), "%.5f %c", fabs(lat), lat >= 0 ? 'N' : 'S');
        g.txt(42, y + 2, s, MEDIUM, LEFT, MIDDLE, K);
        snprintf(s, sizeof(s), "%.5f %c   %u SAT", fabs(lon), lon >= 0 ? 'E' : 'W', in.sats);
        g.txt(42, y + 19, s, MEDIUM, LEFT, MIDDLE, K);
    } else {
        g.txt(42, y + 2, "RICERCA SATELLITI...", MEDIUM, LEFT, MIDDLE, K);
        snprintf(s, sizeof(s), "%u SATELLITI VISTI", in.sats);
        g.txt(42, y + 19, s, SMALL, LEFT, MIDDLE, K);
    }

    // battery and nodes
    y = 238;
    battery(g, 12, y + 2, 32, 16, in.hasBatt ? in.batt : 0, in.charging, K);
    if (in.hasBatt)
        snprintf(s, sizeof(s), "%u%%", in.batt);
    else
        snprintf(s, sizeof(s), "USB");
    g.txt(52, y + 10, s, MEDIUM, LEFT, MIDDLE, K);
    miniNodes(g, w / 2 + 20, y + 10, K);
    snprintf(s, sizeof(s), "%u NODI", in.nodes);
    g.txt(w / 2 + 34, y + 10, s, MEDIUM, LEFT, MIDDLE, K);

    // messages
    y = 262;
    iconEnvelope(g, 28, y + 10, K);
    if (in.unread == 0)
        snprintf(s, sizeof(s), "NESSUN MESSAGGIO NUOVO");
    else
        snprintf(s, sizeof(s), "%u MESSAGGI NUOVI", in.unread);
    g.txt(52, y + 10, s, MEDIUM, LEFT, MIDDLE, K);

    // unlock hint
    for (int x = 8; x < w - 8; x += 3)
        g.drawPixel(x, h - 36, K);
    miniHeart(g, 18, h - 24, K);
    g.txt(30, h - 24, "TIENI PREMUTO PER SBLOCCARE", SMALL, LEFT, MIDDLE, K);
    g.txt(30, h - 11, "oppure tocco lungo sullo schermo", SMALL, LEFT, MIDDLE, K);
}


// ------------------------------------------------------------------------------------------------
// Controls screen: square tiles with quick switches
// ------------------------------------------------------------------------------------------------

enum Control : uint8_t { C_SCREEN_LIGHT, C_KEY_LIGHT, C_VIBRATION, C_GPS, C_BLUETOOTH, C_WIFI, C_LOCK, C_HOME, C_COUNT };

struct Controls {
    bool screenLight = false;
    bool keyLight = false;
    bool vibration = false;
    bool gps = false;
    bool bluetooth = false;
    bool wifi = false;
    int8_t confirm = -1; // control waiting for a second tap (restarts the device)
};

struct ControlRect {
    int x, y, w, h;
};

inline ControlRect controlRect(int w, int i)
{
    const int gap = 8, tw = (w - 3 * gap) / 2, th = 52, top = 50;
    ControlRect r;
    r.w = tw;
    r.h = th;
    r.x = gap + (i % 2) * (tw + gap);
    r.y = top + (i / 2) * (th + gap);
    return r;
}

inline int controlAt(int w, int x, int y)
{
    for (int i = 0; i < C_COUNT; i++) {
        const ControlRect r = controlRect(w, i);
        if (x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h)
            return i;
    }
    return -1;
}

template <class G> void iconSun(G &g, int cx, int cy, uint16_t c)
{
    g.fillCircle(cx, cy, 4, c);
    for (int i = 0; i < 8; i++) {
        const float a = i * 0.7853982f;
        g.drawLine(cx + (int)(7 * cosf(a)), cy + (int)(7 * sinf(a)), cx + (int)(10 * cosf(a)), cy + (int)(10 * sinf(a)), c);
    }
}

template <class G> void iconKeys(G &g, int cx, int cy, uint16_t c)
{
    for (int r = 0; r < 2; r++)
        for (int k = 0; k < 4; k++)
            g.drawRect(cx - 11 + k * 6, cy - 6 + r * 7, 5, 6, c);
    g.drawRect(cx - 8, cy + 8, 16, 4, c);
}

template <class G> void iconVibration(G &g, int cx, int cy, uint16_t c)
{
    g.drawRect(cx - 4, cy - 8, 9, 16, c);
    g.drawLine(cx - 8, cy - 5, cx - 8, cy + 5, c);
    g.drawLine(cx + 8, cy - 5, cx + 8, cy + 5, c);
    g.drawLine(cx - 11, cy - 3, cx - 11, cy + 3, c);
    g.drawLine(cx + 11, cy - 3, cx + 11, cy + 3, c);
}

template <class G> void iconWifi(G &g, int cx, int cy, uint16_t c)
{
    g.fillCircle(cx, cy + 6, 2, c);
    g.drawCircleHelper(cx, cy + 6, 6, 3, c);
    g.drawCircleHelper(cx, cy + 6, 10, 3, c);
    g.drawCircleHelper(cx, cy + 6, 14, 3, c);
}

template <class G> void renderControls(G &g, int w, int h, const Controls &cs)
{
    g.fillRect(0, 0, w, h, W);
    brackets(g, 2, 2, w - 4, h - 4, 10, K);

    // title, with a hexagon that brings you home, and the menu button
    hex(g, 22, 22, 13, K);
    g.fillRect(19, 19, 7, 7, K);
    g.txt(42, 22, "CONTROLLI", MEDIUM, LEFT, MIDDLE, K);
    iconMenu(g, w - 58, 17, 13, K);
    divider(g, 8, w - 8, 41, K);

    static const char *names[C_COUNT] = {"SCHERMO", "TASTIERA", "VIBRA", "GPS", "BLUETOOTH", "WIFI", "BLOCCA", "HOME"};
    const bool on[C_COUNT] = {cs.screenLight, cs.keyLight, cs.vibration, cs.gps, cs.bluetooth, cs.wifi, false, false};

    for (int i = 0; i < C_COUNT; i++) {
        const ControlRect r = controlRect(w, i);
        const bool toggle = i < C_LOCK;
        const bool filled = toggle && on[i];
        const uint16_t fg = filled ? W : K;
        if (filled) {
            g.fillRoundRect(r.x, r.y, r.w, r.h, 6, K);
        } else {
            g.drawRect(r.x, r.y, r.w, r.h, K);
            g.drawRect(r.x + 1, r.y + 1, r.w - 2, r.h - 2, K);
        }
        const int ix = r.x + 17, iy = r.y + r.h / 2 - 2;
        switch (i) {
        case C_SCREEN_LIGHT:
            iconSun(g, ix, iy, fg);
            break;
        case C_KEY_LIGHT:
            iconKeys(g, ix, iy, fg);
            break;
        case C_VIBRATION:
            iconVibration(g, ix, iy, fg);
            break;
        case C_GPS:
            iconCrosshair(g, ix, iy, 9, fg);
            break;
        case C_BLUETOOTH:
            g.txt(ix, iy, "BT", MEDIUM, CENTER, MIDDLE, fg);
            break;
        case C_WIFI:
            iconWifi(g, ix, iy - 2, fg);
            break;
        case C_LOCK:
            iconPadlock(g, ix, iy + 2, 9, true, fg);
            break;
        case C_HOME:
            hex(g, ix, iy, 11, fg);
            g.fillRect(ix - 3, iy - 3, 7, 7, fg);
            break;
        }
        const int tx = r.x + 34;
        if (cs.confirm == i) {
            g.txt(tx, r.y + 17, "RIAVVIA?", SMALL, LEFT, MIDDLE, fg);
            g.txt(tx, r.y + 35, "RITOCCA", SMALL, LEFT, MIDDLE, fg);
        } else if (i <= C_KEY_LIGHT) {
            g.txt(tx, r.y + 11, "LUCE", SMALL, LEFT, MIDDLE, fg);
            g.txt(tx, r.y + 26, names[i], SMALL, LEFT, MIDDLE, fg);
            g.txt(tx, r.y + 41, on[i] ? "ACCESA" : "SPENTA", SMALL, LEFT, MIDDLE, fg);
        } else {
            g.txt(tx, r.y + (toggle ? 17 : 26), names[i], SMALL, LEFT, MIDDLE, fg);
            if (toggle)
                g.txt(tx, r.y + 35, on[i] ? "ACCESO" : "SPENTO", SMALL, LEFT, MIDDLE, fg);
        }
    }
    g.txt(w / 2, h - 22, "Bluetooth e WiFi riavviano", SMALL, CENTER, MIDDLE, K);
    g.txt(w / 2, h - 9, "il dispositivo", SMALL, CENTER, MIDDLE, K);
}

} // namespace MaxUi
