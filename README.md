<p align="center"><img src="hunterce/logo.png" width="140" alt="A Hunter of Halo: Combat Evolved, drawn simply"></p>

<h1 align="center">HunterCE</h1>

<p align="center"><i>Hunters always come in pairs. So should Halo players.</i></p>

HunterCE lets players without the Xbox disc play
[OpenCE](https://github.com/OpenCommunityEdition/OpenCE) with players who have it. They bring their
Steam copy of Halo: The Master Chief Collection (MCC) instead, and
[Hunter Importer](https://github.com/GDAndres98/hunter-importer) turns its Halo 1 maps into the
disc's maps.

- **HunterCE is not OpenCE**, and OpenCE's team does not make it. Report HunterCE's problems here,
  not to OpenCE.
- **Everything else is OpenCE's:** the game, its features, its settings, building it, internet
  play. Read [OpenCE's README](https://github.com/OpenCommunityEdition/OpenCE#readme) for those.
  This fork does not repeat it.

## How it works

| Piece | What it does |
| --- | --- |
| **Hunter Importer** (its own repository, private for now) | Reads an MCC install and writes a `maps` folder of Xbox maps. It checks each map against the disc's: a map that matches goes to `maps`; a map that cannot match is written as MCC's own, in `mcc_maps`. |
| **The import's code** (this fork's `mcc-extractor` branch) | The conversion itself, OpenCE's own code run as `halo --import-mcc`. Hunter Importer builds it from this branch; it is never a pull request. |
| **HunterCE** (this branch, `mcc-testing`) | OpenCE with the few changes it needs to play those maps, plus fixes found on the way. |
| **The fork's branches** | One change each, made from OpenCE's `main` and offered to OpenCE as its own small pull request. `mcc-testing` merges them all; `main` stays equal to OpenCE's. |

**When a map is "the disc's":** the importer computes a fingerprint of each map: a hash of
everything the game simulates (each tag, the tag order, the scenario, each BSP's collision). It is
compared with the fingerprint of the disc's map. If they are equal, the map plays with disc
players. A map that does not match never goes to `maps`, so it never plays with disc players.

**What the repositories hold:** of the disc, only values (numbers, names, orders and hashes),
never pictures, sounds, models or geometry. Of MCC, nothing: the importer reads the player's own
install.

## Playing

You need MCC on Steam, and Hunter Importer. On Windows, `python make_hunterce.py` (in Hunter
Importer) builds the importer and HunterCE, imports every map, and makes a folder to play from:
start `halo.exe` there. It needs what OpenCE's Windows build needs (Visual Studio Build Tools,
LLVM, Python and ninja; see [port/windows/README.md](port/windows/README.md)).

## The plan

**The goal:** imported maps that play with disc players. The campaign first (co-op with disc
players), then the multiplayer maps. Halo PC's 7 rebuilt multiplayer maps can never match the
disc's: they play only with other MCC players.

### The importer's steps

| Step | What | Status |
| --- | --- | --- |
| M0 | The importer: imports every map at once, safely (no `maps` folder until every map is written), and reports each map | done |
| M1 | Every campaign BSP loads (MCC's extra pause-menu screens left out, so each level fits in memory) | done |
| M2 | The campaign matches the disc: all 10 levels are the disc's (with the accepted differences below) | done |
| M3 | The HUD at the disc's sizes and layout, with no marks in the maps | done |
| M3b | The multiplayer bases: MCC's flag and teleporter bases have no collision; the import builds it from MCC's own model | done |
| M4 | The menus' map: its pictures in the disc's order, and the campaign's pause menu as the disc's | done |
| M5 | Maps that cannot match are written as MCC's own, for OpenCE's MCC maps | done |
| M5b | The import's data (the disc's values) lives in Hunter Importer alone | done |
| M6 | The importer as its own program, without the game; releases for Windows and Linux | later |
| M7 | Clean-up and the remaining review fixes | ongoing |

### The changes in HunterCE

| Branch | Change | Offered to OpenCE |
| --- | --- | --- |
| `menus-missing-tags` | The menus work over a menus map that lacks some of the Xbox's tags (a missing picture is drawn empty and logged) | not yet |
| `hires-hud-layout` | A setting, off by default: the high-res HUD also over a HUD bitmap with the disc's layout but other pixels | not yet |
| `missing-mp-maps` | Create Game works when the maps folder lacks the map it would start on | not yet |
| `data-root-maps-folder` | Only a folder named `maps` is taken as game data | not yet |
| `fix-widget-pointer-targets` | Forget the pointer's menu targets when a widget is deleted | not yet |
| `fix-vibrate-alias` | A Halo PC map's call of `player_effect_set_max_vibrate` works (Custom Edition maps too) | being prepared (`fix-ce-max-vibrate`) |
| `fix-contrail-drawing` | Two contrail drawing crashes of debug builds | yes, waiting for review |
| `test-input-buttons` | The test input presses the sticks and holds buttons | not yet |

## The checks

### Each map

| Maps | Imported as | Played |
| --- | --- | --- |
| The campaign: a10, a30, a50, b30, b40, c10, c20, c40, d20, d40 | the disc's | alone, on HunterCE; b30 as a Warthog passenger (2026-11-18). **Not yet with a disc player.** |
| chillout, longest, prisoner, putput, ratrace, wizard | the disc's | chillout with two local players; prisoner and wizard over the internet with OpenCE players on the disc's maps (2026-11-18): the bases stop players |
| Halo PC's rebuilds: beavercreek, bloodgulch, boardingaction, carousel, damnation, hangemhigh, sidewinder | MCC's own (`mcc_maps`) | with MCC players only |
| MCC's Halo PC maps: dangercanyon, deathisland, gephyrophobia, icefields, infinity, timberland | MCC's own (`mcc_maps`) | with MCC players only |
| `ui` (the menus) | MCC's own | never played with others: it holds the menus only |

### The differences accepted

A few parts of MCC's maps differ from the disc's, but change nothing the host decides, or very
little. The fingerprint leaves them out, so the maps still count as the disc's. Hunter Importer's
`data/README.md` gives the reason for each one.

| What | Maps | Why it is accepted |
| --- | --- | --- |
| An overlay animation's default data | all | The game never reads it |
| The Chief's "Warthog passenger with a rifle" idle pose | all | MCC has one still seated pose (the disc's first frame), the disc a slight movement. Only where the body is drawn |
| The field generator's collision | most campaign levels | Made again by Halo PC's tools, up to about 21 cm apart. The host decides its damage |
| The flag bases' and chillout's teleporter bases' collision | the 6 multiplayer maps above | MCC has none; the import builds it from MCC's model, within a few centimetres of the disc's |
| The flamethrower's collision and first-person animations | where it is | No map of the disc's ever creates it |

### Tests

- **Every import** reports, for each map, whether its fingerprint is the disc's and whether every
  BSP fits in memory.
- **The disc's fingerprints can be made again** by anyone with the disc, with the import's game
  (`halo --mcc-data <Hunter Importer's data> --fingerprint <map>`), and compared with the
  importer's.
- **The harness tests of the import's code** (on `mcc-extractor`): 204 pass. One fails,
  `test_mcc_damage[threshold]`; it already failed before the latest changes and is being looked
  at.

### Known problems and what is not tested

- **Campaign co-op with a disc player:** not tested yet.
- **Shots and grenades against the rebuilt bases:** not tested yet. Players walking into them are
  stopped.
- **Teleporters** do not work in single player on MCC's multiplayer maps.
- **Fixed on 2026-11-18:** the Chief stood up as a Warthog passenger holding an assault rifle or a
  plasma rifle, in every imported map.
