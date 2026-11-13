# The MCC import's data

The importer (`halo --import-mcc`, `port/linux/game/mcc_import.c`) turns the
Halo 1 maps of a Steam installation of The Master Chief Collection into the
Xbox disc's maps. These files tell it where MCC's maps differ from the disc's.
The game embeds them (`tools/mcc_patches_embed.py`, desktop builds only). A
file of the same name in the data root's `mcc_patches` folder is read in
place of the game's copy, so that a changed file can be tried without a build.

| File | Contents |
| --- | --- |
| `<map>.p2` | The disc's values as operations, one per line (`mcc_import_patch.c` describes each kind): numbers, flags, names, the order of the tags, and the HUD's placements, sizes and the sprite layout of the meters and the zoom reticles. A Halo PC rebuild (beavercreek, bloodgulch, boardingaction, carousel, damnation, hangemhigh, sidewinder) has the tag order and the HUD alone. |
| `ui.p2` | The menus' map's: the tags of MCC's (Halo PC's) the game reaches, in MCC's order, its preload list emptied, and the disc's sizes of its bitmaps. It has no fingerprint: it is never the disc's. |
| `<map>.fp` | The fingerprint of the disc's map: a SHA-256 of each part of its simulation data (each tag, the order, the scenario, each BSP's collision; the first 16 bytes of each, all 32 of the whole map's). An imported map whose fingerprint is the same is the disc's map, and plays with players of the disc; the import's report says which maps are. A few differences are accepted (below). |

What is not here:

- No pixels, sounds, models or collision geometry of the disc's. The few
  tags the disc has and MCC lacks or changed as geometry (the flamethrower,
  `flag_base`, `teleporter_base`, `c_field_generator`, the cyborg's passenger
  animations) are left as MCC has them. In the campaign, those differences
  are accepted (below); in the multiplayer maps they are not yet.

The differences accepted (decided 2026-11-12): parts the campaign's maps have
otherwise in MCC that change nothing the host decides, left out of the
fingerprint (`accepted_tags` and `accepted_parts` in
`mcc_import_fingerprint.c`), so that every campaign level is the disc's:

| What | Levels | Why it is accepted |
| --- | --- | --- |
| An overlay animation's default data | all | The game never reads it (`model_animations.c` reads a base animation's alone). A general rule. |
| The cyborg's `W-passenger rifle idle` animation | all 10 | A still pose in MCC, a slight movement on the disc. A seated unit's root motion is never applied, so it changes only where a seated player's body is drawn, on the machine that draws it: a shot at or from a warthog's passenger can hit a few centimetres differently there. The host decides the damage. |
| `c_field_generator`'s collision geometry | a10, a50, b30, b40, c10, c40, d20, d40 (c20 never places one) | Made again by Halo PC's tools: the same size, health and materials, its surfaces up to 0.07 world units (about 21 cm) apart. A step or a shot beside one can differ on the machine that makes it; the host decides its damage. |
| The flamethrower's collision and first-person animations | every map that has it (c20, d20 and the multiplayer maps) | Never created: no map of the disc's places it, no weapon set gives it (the globals' weapon list names it only to preload it), and in c20 and d20 only two Flood variants carry it, which no squad uses and no script names. |

- No data of MCC's. A tag MCC has only in another map (the warthog, from
  a10; the ghost's gun, from b30) is named by a `donor` line, and the import
  takes it from the user's own MCC map (into the save folder's
  `z/mcc_import`).

How they are made:

- A fingerprint is the game's (`mcc_import_fingerprint.c`), of the disc's
  map. Anyone with the disc can make it again and compare it with the file:

  ```sh
  HALO_DATA_ROOT=<the disc's game data> halo --fingerprint a10 > a10.fp
  ```

  `halo --fingerprint <map>` prints the fingerprint of the data root's
  `maps/<map>.map` (the disc's, or one the import wrote). The fields it
  takes as text or leaves out are `port/linux/game/mcc_fingerprint_fields.h`,
  which `python tools/mcc_fingerprint_fields.py` writes from the game's
  structures (`--check` tells whether it is what it would write).
- The operations come from a comparison of each map's tags with the disc's,
  field by field, made with tools that are not in the repository: what they
  hold is data of the disc's (see the pull request's question on it).
