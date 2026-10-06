#ifndef POKEPLATINUM_FOLLOWER_MON_TALK_H
#define POKEPLATINUM_FOLLOWER_MON_TALK_H

#include "field/field_system.h"
#include "field_script_context.h"
#include "map_object.h"
#include "pokemon.h"
#include "script_manager.h"

// ENTRY P63: talking to the follower Pokemon (press A while facing it), ported from HeartGold's data-driven
// engine (HG overlay 2, Task_FollowMonInteract). See src/follower_mon_talk.c for the full design and
// docs/follower_talk/HG_FOLLOWER_TALK_REFERENCE.txt for every message and the conditions it needs.

// The follower's talk script: entry 8 of res/field/scripts/scripts_unused_0397.s (9th ScriptEntry).
#define FOLLOWER_MON_TALK_SCRIPT_ID (SCRIPT_ID_OFFSET_UNUSED_0397 + 8)

// Script command SCRCMD_FOLLOWERMONTALK: picks and runs one reaction, returns when it is finished.
BOOL ScrCmd_FollowerMonTalk(ScriptContext *ctx);

// Called once per player step (src/overlay005/field_control.c Field_ProcessStep): HG's mood decay.
void FollowerMonTalk_OnPlayerStep(FieldSystem *fieldSystem);

// For the follower renderer (src/overlay005/ov5_021EB1A0.c). Returns TRUE while a talk is in progress, with
// the current talk animation's offset: x/z = HG's per-frame position moves, y = HG's model height.
BOOL FollowerMonTalk_GetRenderOffset(VecFx32 *offset);

// ENTRY P64: HG's other mood sources. All only affect the Pokemon that is currently the follower (HG resets mood
// whenever a Pokemon becomes the follower, so changes to anyone else never matter there either).
void FollowerMonTalk_AddMoodForMon(Pokemon *mon, int delta);
void FollowerMonTalk_ApplyItemMood(Pokemon *mon, u16 item);
void FollowerMonTalk_FieldMoveTypeMood(FieldSystem *fieldSystem, int type);

// Field-move reaction (HG scr_seq_0146): TRUE = the follower is the move user and reacted (the script is paused
// until it finishes); FALSE = not applicable, the caller plays the normal HM cut-in instead.
BOOL FollowerMonTalk_StartFieldMoveReaction(ScriptContext *ctx, u16 partySlot, u8 moodAmount, u8 type);

// TRUE while the field-move reaction holds the follower's walk animation (HG PauseAnimation).
BOOL FollowerMonTalk_IsAnimationPaused(void);

// HG mood modifiers (HG include/pokemon_mood.h sMoodModifiers)
#define FOLLOWER_MOOD_LEVEL_UP_IN_BATTLE 40
#define FOLLOWER_MOOD_LEARN_TMHM         40
#define FOLLOWER_MOOD_BATTLE_FAINT       (-80)
#define FOLLOWER_MOOD_BATTLE_FAINT_HIGH  (-100)
#define FOLLOWER_MOOD_SURVIVED_POISON    (-50)

#endif // POKEPLATINUM_FOLLOWER_MON_TALK_H
