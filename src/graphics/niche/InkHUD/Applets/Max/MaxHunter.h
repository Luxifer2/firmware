#pragma once

#include "configuration.h"

#if defined(MESHTASTIC_INCLUDE_INKHUD) && defined(T_DECK_MAX)

#include <stdint.h>

namespace NicheGraphics::InkHUD
{

// Node hunter: every node heard over our own radio is written to /nodi/<date>.csv on the microSD card
// (time, node, signal, hops, where we were, where it was, distance). Nodes never heard before are added
// to the collection in /nodi/visti.bin.

void maxHunterBegin();     // start (once, at setup)
uint32_t maxHunterTotal(); // nodes in the collection
uint32_t maxHunterNew();   // nodes added to the collection since power on

} // namespace NicheGraphics::InkHUD

#endif
