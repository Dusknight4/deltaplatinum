HeartGold follower sprite import tools
======================================

These scripts generated the follower sprites under res/graphics/field_sprites/pokemon_hg/ and the matching
tables in src/follower_mon.c and src/overlay005/object_event_gfx_data.c. You only need them to re-import or to
add sprites; the results are already committed.

  python3 import_followers.py --dry-run   # show the plan, write nothing
  python3 import_followers.py             # import every HeartGold follower row not imported yet (runs verify_followers.py)
  python3 import_followers.py --tall      # also allow HG's "too_tall" rows (64x64 model, 16,832-byte textures)
  python3 verify_followers.py             # wiring check - run before any build that changes follower species

Inputs:
  - a pret/pokeheartgold checkout CONTAINING this repository (the tools look for ../files/data/mmodel/mmodel.narc and
    ../files/fielddata/tsurepoke/tp_param.csv relative to the pokeplatinum root) with mmodel.narc already built
  - build/tools/nitroarc and build/tools/nitrobtx (a normal build of this repository builds them)

Extracted NARC members and palette dumps are cached in .cache/ next to the scripts (safe to delete).
After a run that changes res/graphics/field_sprites/meson.build or field_sprites.order, reconfigure before building.

Rules the tool enforces:
  - palette tables live inside src/follower_mon.c (never in a separate .c file)
  - every follower row uses BILLBOARD_FRAME_SEQ_HG_TSURE_POKE / sHGTsurePokeBillboardAnims (8-frame textures)
  - native OBJ_EVENT_GFX_<NAME> entries are never edited (colliding HG names get an _HG suffix)
  - "too_tall" species use BILLBOARD_MODEL_GENERIC_64x64 and are listed in FOLLOWER_TALL_TABLE
  - form / gender variants (Unown letters, Deoxys/Burmy/Wormadam/Shellos/Gastrodon/Rotom/Giratina/Shaymin/Arceus
    forms, female sprites) go into FOLLOWER_FORM_TABLE (FORM_ROWS in import_followers.py)
