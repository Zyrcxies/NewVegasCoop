# Changelog

## 0.1.3
- Fast travel points unlock for everyone: a place any player discovers can be fast travelled to by the whole group (and it stays in everyone's save).
- A Co-op Teleporter in everyone's Aid items while playing together: use it to go to a friend or bring a friend to you (for when someone gets stuck). It can't be dropped or sold, and it's taken away when co-op ends.
- Friends show on your Pip-Boy map (with their character's name), and as a tick on the compass when they're near.
- Friends sitting in chairs show sitting in your game too (and stand up when they do).
- Friends' characters no longer push you or each other when everyone arrives at the same spot (they stand just beside you instead), and they're hidden during cutscenes so they don't block the view.
- Join by IP address, for port forwarding or a free VPN (Tailscale, ZeroTier, Radmin VPN), all from the main menu: Host Co-op asks who can join (Steam friends, or by IP too), and Join Co-op asks how (a Steam friend's game, or by IP: type the address, shown as stars and remembered for next time). Addresses are never shown on their own or written to the logs (safe for streaming): the host finds theirs in the pause menu's Co-op Address. The host still lets everyone in.
- Works with animation mods that use kNVSE (tested with Enhanced Movement's sprint): your friends see the animations those mods give your character, at your real speed (sprinting included). Everyone needs the same animation mods installed.
- Friends' characters follow their players much more closely: about half the delay (around 300 ms down to 140 ms in our tests), no more flicking between walking and running, and fewer snap-backs.
- Downed players lie on the ground in pain instead of ragdolling (the ragdoll kept glitching). A downed friend gets a red "DOWNED" marker over them with the seconds left before they bleed out, and holding E to revive shows a progress bar under the crosshair.
- If JIP LN NVSE, kNVSE or Real Time Menus is missing (or didn't load), the game now says so on screen and won't start co-op, instead of freezing later (saving or loading from the menu froze without Real Time Menus).
- Everyone needs 0.1.3 to play together.

## 0.1.2
- Boxes and bodies no longer change their items right after a friend opens them (a friend's game first showed its own random loot, or the 500 "show" bullets on a dead enemy, then switched to the host's). They're now matched as soon as the friend aims at them.
- A friend's own reputation changes (killing a townsperson, say) now count for everyone, instead of being undone by the host's a few seconds later. Everyone else also gets the "Infamy gained" / "Fame gained" message.
- Everyone needs 0.1.2 to play together.

## 0.1.1
- Level-ups wait out fights for everyone: the level-up screen no longer pops up for friends while enemies are still attacking them.
- Starting a new game with Join Co-op now waits until Quick Start is completely done (character finished and saved) before joining, instead of pulling you in with the "finish your character" screen still open.
- Friends' characters no longer cower ("hide") when enemies are near.
- If the game freezes for 8+ seconds during co-op, NVCoop.log now records where it got stuck, so freeze reports can be fixed.
- Everyone needs 0.1.1 to play together (0.1.0 and 0.1.1 can't connect).

## 0.1.0
- First release.
