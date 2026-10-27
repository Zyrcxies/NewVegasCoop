#pragma once

// The Escape menu keeps the world running in co-op (Alex, October 2026: the host's pause froze everyone's NPCs). Real
// Time Menus does this for the Pip-Boy, containers, dialogue and the like but not the Escape menu (it saves and loads
// there); a save made with the world running is what a quicksave does anyway.

namespace game
{
    // Once, after every plugin's DeferredInit (Real Time Menus takes the same checks there and ignores what was
    // before it; ours keeps its answers and only changes "paused" to "running" for the Escape menu).
    void KeepWorldRunningInEscapeMenu();
    void EscapeMenuLive(bool live);  // on in co-op
}
