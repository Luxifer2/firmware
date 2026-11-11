#pragma once

#include "configuration.h"

#if defined(MESHTASTIC_INCLUDE_INKHUD) && defined(T_DECK_MAX)

#include "./MaxUi.h"

namespace NicheGraphics::InkHUD
{

// Collect time, GPS, battery, nodes, messages, base and sun for the T-Deck Max screens
void gatherMaxInfo(MaxUi::Info &info);

// Where we are: from the GPS, otherwise what our node knows (the phone can provide it). Degrees * 1e7.
bool maxOwnPosition(int32_t &lat, int32_t &lon);

// The base: one saved position, kept in flash across restarts
bool maxBaseGet(int32_t &lat, int32_t &lon);
bool maxBaseSetHere(); // false if we do not know where we are

} // namespace NicheGraphics::InkHUD

#endif
