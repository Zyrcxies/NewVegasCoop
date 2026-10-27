#pragma once

// Developer-only (AllowSecondCopy=1 in NVCoop.ini): lets a second copy of the game run on
// the same PC for testing. Must be called while plugins load, before the game's own check.
// waitForDebugger: the second copy shows a message and waits, so a debugger (Visual Studio) can be attached.
void AllowSecondCopy(bool waitForDebugger);
