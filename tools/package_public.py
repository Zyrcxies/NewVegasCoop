# Packs the public release (Nexus, itch.io) into build/: only our own files, laid out from the game's Data folder
# (NVSE/Plugins, MCM) so mod managers install it as is. Build the Release preset first.
import os, zipfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
version = next(l.split('"')[1] for l in open(os.path.join(ROOT, 'src', 'version.h'))
               if 'NVCOOP_VERSION_STRING' in l)
ZIP = os.path.join(ROOT, 'build', f'New Vegas Co-op {version}.zip')

INI = '''[Coop]
; menu: Host Co-op and Join Co-op on the main menu
Mode=menu
Port=27960
; When hosting: quest mods whose quests your game runs for everyone (file names, commas between)
HostQuestMods=
; 1 keeps Discord's overlay out of the game (it crashes New Vegas, even when turned off in Discord)
BlockDiscordOverlay=1
'''

README = f'''New Vegas Co-op {version}
==========================

Play Fallout: New Vegas with 1-3 friends. One player hosts their world; friends join with their own characters.

Requirements (everyone)
- Fallout: New Vegas, Steam version, with all the DLC (Ultimate Edition)
- xNVSE
- JIP LN NVSE
- kNVSE Animation Plugin
- JohnnyGuitar NVSE
- Real Time Menus - NVSE (with its Fixes plugin)
- FNV 4GB Patcher
Recommended: The Mod Configuration Menu (+ MCM Extender) for the in-game settings page; LOD Flicker Fix - NVSE
(with Shader Loader - NVSE) for the game's own flicker of distant hills.
Everyone needs the same mods in the same load order; the game checks and says what differs.

Install
Mod manager: install this archive like any other mod. By hand: copy the NVSE and MCM folders into
Fallout New Vegas\\Data (merge folders).
Start the game with nvse_loader.exe (or your mod manager), with Steam open.

How to start a game
Host: main menu > Host Co-op > load a save, or start a new game (pick Quick start). A new save asks for its mode
once (Normal or Co-op balanced, friendly fire or not). Friends can join once you're outside Doc Mitchell's house.
Friends: main menu > Join Co-op > the host's name (a Steam friend); the host lets you in. The first time, pick the
character to bring (a save, or a new game). After that, Join loads the right save by itself.
Later sessions: the host loads the group's last save with Host Co-op; friends just click Join Co-op.

Playing
Main menu: Host Co-op, or Join Co-op and pick a friend's game (the host is asked to let you in).
Co-op starts once you're out of Doc Mitchell's house.
Only join games from people you trust.

Settings: Data\\NVSE\\Plugins\\NVCoop.ini, or the MCM page.

Best practices
- Skip Doc Mitchell's house: starting a new game, pick Quick start. You go straight outside, finish your character
  there, and the game saves it. Co-op starts once you're outside.
- Everyone uses the same mods in the same load order.
- Install the 4GB Patcher, and play in borderless window mode if you alt-tab.
- Stay together: doors and fast travel pull the group along.
- Save often; if something goes wrong, the host loads the last save and everyone goes back together.
- Don't use the console in co-op: it changes only your game.
- Turn off Discord's in-game overlay.

Early release: please report bugs!
This is the first public version, so expect some. On the Nexus page's Bugs tab, say what happened and what you
were doing, and attach NVCoop.log and NVCoop.previous.log from the game's folder (from everyone who played if you
can), plus any NVCoop-crash-....dmp file.
'''

files = {
    'NVSE/Plugins/NVCoop.dll': open(os.path.join(ROOT, 'build', 'Release', 'NVCoop.dll'), 'rb').read(),
    'NVSE/Plugins/NVCoop.ini': INI.replace('\n', '\r\n').encode(),
    'MCM/NVCoop.json': open(os.path.join(ROOT, 'data', 'MCM', 'NVCoop.json'), 'rb').read(),
    'README - New Vegas Co-op.txt': README.replace('\n', '\r\n').encode('utf-8'),
}
with zipfile.ZipFile(ZIP, 'w', zipfile.ZIP_DEFLATED) as z:
    for name, data in files.items():
        z.writestr(name, data)
print(ZIP, round(os.path.getsize(ZIP) / 1024), 'KB')
