#pragma once

#include "configuration.h"

#if defined(MESHTASTIC_INCLUDE_INKHUD) && defined(T_DECK_MAX)

#include <stddef.h>
#include <stdint.h>

namespace NicheGraphics::InkHUD
{

// GPX track on the microSD card: one file per session in /tracce, written while the GPS has a fix.
// Every point is written with the closing tags after it, so the file stays valid if power is cut.
enum class GpxState : uint8_t { OFF, NO_CARD, WAITING_FIX, RECORDING, CARD_ERROR };

void maxGpxBegin(); // start recording (once, at setup)
bool maxGpxEnabled();
void maxGpxSetEnabled(bool enabled); // saved in flash; on by default
GpxState maxGpxState();
uint32_t maxGpxPoints(); // points in the current file
void maxGpxStatusText(char *buf, size_t n, bool longForm); // "REC 12" (long: "REC 12 PUNTI"), "CERCO GPS", "NO SD"...

bool maxSdCardMb(uint32_t &mb); // size of the microSD card; false if there is none

} // namespace NicheGraphics::InkHUD

#endif
