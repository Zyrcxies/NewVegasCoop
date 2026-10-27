#pragma once

// Developer console commands (DESIGN "Prove it in a lab first"): type "coop help" in the game's console (~).
// Each one says in the console and the log whether it worked.

struct NVSEInterface;
namespace game { class Inspector; class Mirror; }

namespace dev
{
    // Registers the "coop" command, in the opcode range xNVSE sets aside for testing (0x2000). A released
    // version needs its own range from the xNVSE team.
    bool RegisterCommands(NVSEInterface* nvse, game::Inspector* inspector, game::Mirror* mirror);
    void EveryFrame();  // lab switches that act each frame (menus too)
}
