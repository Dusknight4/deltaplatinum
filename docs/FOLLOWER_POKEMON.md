# Follower Pokémon (HeartGold-style) for Pokémon Platinum

The first Delta Platinum commit adds HeartGold/SoulSilver-style follower Pokémon to otherwise unmodified Platinum. Your
party leader (the first Pokémon that is not an egg and has HP left) walks behind you on the overworld.

## What it does

- **Sprites:** every HeartGold follower sprite is included, all 566 rows of HG's `tp_param.csv`. That covers 493 species,
  with female sprites, all 28 Unown letters, and the Deoxys, Burmy, Wormadam, Shellos, Gastrodon, Rotom, Giratina,
  Shaymin and Arceus forms. The sprites use HG's own 8-frame textures and normal/shiny palettes, and they update the
  moment the party leader changes.
- **Large Pokémon:** HG's 31 "too tall" species (Lugia, Ho-Oh, Rayquaza, Steelix, ...) use the 64×64 model, keep
  HG's extra spacing, stay out of buildings like in HG, and get a depth pass so they don't clip into signs or scenery.
- **Movement:** walking and running follow, with ledge leaps and seamless route transitions. The follower stays in its
  Poké Ball after a door or warp, then comes out (HG's ball effect) once you take your first step. It also keeps
  following during cutscenes that walk the player, and catches up before a trainer battle starts.
- **Talking:** press A while facing your follower to get HeartGold's reactions: mood, emote bubbles, the animations
  and HG's message set. Mood changes from battles, items, level-ups, TMs, field moves and poison, as in HG.
- **Pokémon Center:** if healing revives the Pokémon that should lead, the follower is swapped while the screen is
  black, and you get *"<mon> is feeling better, so you decided to take <mon> with you!"*.
- **Partner trainers:** while Cheryl, Mira, Riley, Marley or Buck walk with you, your Pokémon stays in its ball.

This commit has **no** colour rotation and **no** gameplay changes. Followers show HG's normal or shiny palette.

## Files

| Area | Files |
|---|---|
| Core | `src/follower_mon.c`, `include/follower_mon.h` (spawn/refresh, species/form/gender to sprite tables, palettes, Poké Ball release, heal script commands) |
| Talking | `src/follower_mon_talk.c`, `include/follower_mon_talk.h`, `res/text/follower_talk.json`, `res/prebuilt/data/follower_talk/*`, `res/field/scripts/scripts_unused_0397.s` |
| Rendering | `src/overlay005/ov5_021EB1A0.c` (render offsets, bounce, depth pass, ball effect), `src/billboard.c`, `include/billboard.h`, `src/overlay005/ov5_021F134C.c` (shadow), `src/overlay005/object_event_gfx_data.c`, `res/graphics/field_sprites/**` |
| Movement | `src/unk_02069BE0.c` (follow + ledges), `src/unk_020655F4.c`, `src/unk_020EDBAC.c`, `generated/movement_actions.txt` |
| Hooks | `src/overlay005/fieldmap.c`, `src/field_map_change.c`, `src/overlay005/field_control.c`, `src/scrcmd.c`, `res/field/scripts/scripts_common.s`, `res/field/scripts/scripts_field_moves.s`, mood hooks in battle/item/party code |
| Effects | `res/prebuilt/data/mmodel/fldeff.narc` (+3 members from HG: ball, burst, burst animation), `src/overlay005/field_effect_renderer.c` |
| Tools | `tools/follower_import/` (sprite import + verifier), `tools/follower_talk/` (talk data extraction) |

All sprites, talk data and effect models come from files that pret/pokeheartgold already publishes.
