#ifndef POKEPLATINUM_FOLLOWER_MON_H
#define POKEPLATINUM_FOLLOWER_MON_H

#include "struct_decls/map_object.h"

#include "field/field_system.h"

#include "field_script_context.h"

// FOLLOWER MON (new feature, requested alongside the color mod port): an overworld companion Pokemon that
// walks behind the player, showing the current party leader's species. See src/follower_mon.c's own header
// comment for the full design, scope and known limitations - this is a v0/first pass, built on Platinum's
// own existing MOVEMENT_TYPE_FOLLOW_PLAYER engine support (already used for partner trainers and the Amity
// Square "walk with your Pokemon" feature), NOT a port of HeartGold/SoulSilver's 3D-model-based follower
// (Platinum's overworld has no 3D Pokemon models or assets to port - see the design comment for why).

/**
 * @brief Spawn, update, or remove the follower object for the CURRENT map, based on the current party
 * leader (first non-egg, non-fainted party Pokemon). Call once after a map's objects have been created
 * (fieldSystem->mapObjMan and fieldSystem->playerAvatar must already be valid), and again whenever the
 * party leader might have changed while already on a map (catching/depositing/reordering party, fainting
 * in battle, etc.) if a same-map refresh is wanted.
 */
void FollowerMon_Refresh(FieldSystem *fieldSystem);

/**
 * @brief Remove the follower object from the current map, if one exists. Safe to call even if there is
 * none (e.g. the party leader's species has no follower graphic - see FOLLOWER_GRAPHICS_TABLE).
 */
void FollowerMon_Despawn(FieldSystem *fieldSystem);

/**
 * @brief Re-apply the follower's normal or shiny palette (matching the party leader) to its overworld billboard sprite.
 * Safe to call every field frame (it no-ops quickly if there is no follower or its billboard isn't ready
 * yet). MUST only be called from code that is itself guaranteed to run while overlay5 (the field overlay)
 * is loaded - e.g. FieldMap_Main in src/overlay005/fieldmap.c - since this reaches into overlay5-resident
 * rendering code (ov5_021EB1A0) to find the follower's Billboard.
 */
void FollowerMon_RefreshPalette(FieldSystem *fieldSystem);

/**
 * @brief Returns the hardcoded, verified-correct BGR555 palette (see sFollowerHGPristinePalettes in
 * src/follower_mon.c for the full derivation) for the given billboard graphics ID, or NULL if it isn't one
 * of the ENTRY P17 HG-sourced species. Doubles as the "is this an HG-sourced follower species" check used
 * by ENTRY P29's idle-animation code in src/overlay005/ov5_021EB1A0.c - exposed here rather than
 * duplicating a second copy of the same 9-species list in that file.
 */
const u16 *FollowerMon_GetHGPristinePalette(u16 graphicsID);

/**
 * @brief ENTRY P64: TRUE if the graphics ID is one of the 64x64 "too tall" follower sprites (FOLLOWER_TALL_TABLE,
 * ENTRY P61). Used by ov5_021EB1A0.c to keep large followers (and their shadows) a little farther back.
 */
BOOL FollowerMon_IsTallGraphicsID(u16 graphicsID);

// ENTRY P87: the follower Pokemon's MapObject (never a partner trainer, which shares MOVEMENT_TYPE_FOLLOW_PLAYER), or NULL.
MapObject *FollowerMon_GetMapObject(FieldSystem *fieldSystem);
BOOL FollowerMon_IsFollowerMonObject(const MapObject *mapObj);

// ENTRY P87: lets a follower that is waiting in its Poke Ball (created on the player's tile after a map change) come
// out once the player has left that tile. Call every field frame from overlay-5 code only (FieldMap_Main).
void FollowerMon_UpdateRelease(FieldSystem *fieldSystem);

// ENTRY P87: Pokemon Center heal script commands (see src/follower_mon.c).
BOOL ScrCmd_FollowerMonHealBegin(ScriptContext *ctx);
BOOL ScrCmd_FollowerMonHealCheck(ScriptContext *ctx);
BOOL ScrCmd_FollowerMonHealRespawn(ScriptContext *ctx);

#endif // POKEPLATINUM_FOLLOWER_MON_H
