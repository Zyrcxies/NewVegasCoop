#pragma once

// Borderless window instead of fullscreen (Alex, October 2026: alt-tab crashed a friend's game; the community's fix,
// "Tabbed Out" and OneTweak, is the same: Direct3D 9 loses its device when an exclusive fullscreen game is switched
// away from). The game asks Windows for its INI's "bFull Screen" (GetPrivateProfileStringA/IntA, its imports): we
// answer 0, and once its window is up, a player who chose fullscreen gets it borderless over the whole screen. Their
// INI keeps their own choice (the game writing it back is given theirs).

namespace display
{
    void Install();  // at plugin load, before the game reads its INI files
    void Update();   // every frame until the window is made borderless (or isn't to be)
}
