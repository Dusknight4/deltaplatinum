#include "follower_mon_talk.h"

#include <nitro.h>
#include <string.h>

#include "constants/battle/condition.h"
#include "constants/field/window.h"
#include "constants/heap.h"
#include "constants/items.h"
#include "constants/map_object.h"
#include "constants/narc.h"
#include "constants/overworld_weather.h"
#include "constants/player_avatar.h"
#include "constants/scrcmd.h"
#include "constants/sound.h"
#include "constants/species.h"
#include "generated/bg_event_types.h"
#include "generated/genders.h"
#include "generated/items.h"
#include "generated/map_headers.h"
#include "generated/movement_types.h"
#include "generated/object_events_gfx.h"
#include "generated/species.h"
#include "generated/text_banks.h"

#include "field/field_system.h"
#include "overlay005/field_effect_manager.h"
#include "overlay005/ov5_021ECC20.h"
#include "overlay005/script_message.h"

#include "bag.h"
#include "bg_window.h"
#include "billboard.h"
#include "field_message.h"
#include "field_overworld_state.h"
#include "field_script_context.h"
#include "follower_mon.h"
#include "heap.h"
#include "item.h"
#include "map_header.h"
#include "map_header_data.h"
#include "map_object.h"
#include "map_tile_behavior.h"
#include "math_util.h"
#include "menu.h"
#include "message.h"
#include "narc.h"
#include "party.h"
#include "player_avatar.h"
#include "pokemon.h"
#include "render_window.h"
#include "save_player.h"
#include "savedata.h"
#include "script_manager.h"
#include "sound_playback.h"
#include "string_template.h"
#include "system.h"
#include "terrain_collision_manager.h"
#include "unk_020298BC.h"
#include "unk_020559DC.h"

// ENTRY P63 (2026-10-03): TALKING TO THE FOLLOWER, PORTED FROM HEARTGOLD.
//
// HeartGold's follower reactions are entirely data-driven. The engine lives in HG's overlay 2
// (asm/overlay_02_02248728.s: Task_FollowMonInteract and ov02_0224EF94..ov02_02250738, never decompiled) and
// reads four data files, which are copied into this ROM byte-for-byte by tools/follower_talk/install_talk_data.py:
//   data/follower_talk_cond.narc    (HG a/2/2/0) condition tables: member 0 = general table (table slots 30..99),
//                                   member mapsec+1 = that location's table (slots 0..29)
//   data/follower_talk_action.narc  (HG a/2/2/1) 1023 actions (up to 5 steps + yes/no + effects)
//   data/follower_talk_anim.narc    (HG a/2/2/2) 108 follower animations
//   data/follower_talk_species.narc (HG a/2/3/1) per-species "category" byte used by one condition
// The 759 messages are HG's msg_0265 in the same order (res/text/follower_talk.json, HG message N = entry N-1)
// plus HG's two Fashion Case messages as entries 759/760. The 14 emote bubbles are HG's fldeff resources,
// appended to this ROM's data/mmodel/fldeff.narc as members 201..216.
//
// What happens on A (all of it read from the HG asm, reproduced here in the same order):
//  1. Script (scripts_unused_0397.s entry 8): PlaySE, LockAll, FacePlayer, FollowerMonTalk, ReleaseAll.
//  2. Snapshot of the lead Pokemon and its surroundings (FollowerTalk_BuildContext).
//  3. Table slots are walked in the order 30-41, location 0-29, 42-99; each slot rolls rand%100 < chance first,
//     then every condition; the first slot that passes picks the action (slot 99 always passes).
//  4. Each of the action's steps: animation (with cry), emote bubble (blocks until it ends), message (A/B).
//     After the last step: mood/friendship deltas, then Yes/No (branches to another action), or an accessory.
//
// Platinum-specific decisions (documented in Changelog ENTRY P63):
//  - Location tables: HG only has tables for 2 Sinnoh map sections (15 Pokemon League, 121 Rotom's Room), used
//    as-is (their 2 HG map-id conditions translated). Every other Sinnoh location borrows the table of a similar
//    HG location (sTemplateMapsec), skipping entries that need an HG map, HG story flag or Shiny Leaf.
//  - HG flag 332 (has the Fashion Case) = ITEM_FASHION_CASE in the bag.
//  - Shiny Leaves and Pokeathlon stats do not exist in Platinum: no leaves owned, no Pokeathlon stat.
//  - Mood is RAM-only, exactly like HG (FieldSystem sub-struct there): reset when the follower changes.

#define TALK_HEAP_ID HEAP_ID_FIELD3

#define TALK_NUM_STEPS        5
#define TALK_NUM_ANIM_FRAMES  10
#define TALK_COND_ENTRY_SIZE  20
#define TALK_NUM_SLOTS        100
#define TALK_NUM_LOCAL_SLOTS  30
#define TALK_ANIM_END         0xFF
#define TALK_STEP_LIST_END    0xFFFF

#define TALK_SOUND_CRY        2379 // HG SEQ_SE_END + 1: normal cry
#define TALK_SOUND_CRY_PINCH  2380 // HG SEQ_SE_END + 2: cry mode 0xB (POKECRY_PINCH_NORMAL)

#define TALK_MSG_ACCESSORY_PUT_AWAY 759 // HG msg_0040 #32
#define TALK_MSG_ACCESSORY_FULL     760 // HG msg_0040 #95

#define HG_FLAG_GOT_FASHION_CASE 332

#define TALK_NUM_EMOTES          14
#define TALK_FLDEFF_EMOTE_MODEL  201
#define TALK_FLDEFF_EMOTE_SEQ    202
#define TALK_FLDEFF_EMOTE_TEX    203
// Field-effect resource slots/ID unused by vanilla (vanilla uses up to 16; 17 is the "free" marker of the
// BillboardResources table in field_effect_manager.c, so never use 17).
#define TALK_EMOTE_SLOT          24
#define TALK_EMOTE_RES_ID        24

#define MON_MOOD_MAX 127
#define MON_MOOD_MIN -127

typedef struct FollowerTalkStep {
    u16 anim;
    u16 msg;
    u16 sound;
    u8 emote;
    u8 wait;
} FollowerTalkStep;

typedef struct FollowerTalkAction {
    FollowerTalkStep steps[TALK_NUM_STEPS];
    u32 yesNo;
    u16 yesAction;
    u16 noAction;
    s8 friendshipDelta;
    s8 moodDelta;
    u8 accessory;
    u8 shinyLeaf;
} FollowerTalkAction;

typedef struct FollowerTalkAnimFrame {
    u8 facing;
    u8 duration;
    s8 dx;
    s8 dHeight;
    s8 dz;
    u8 playSound;
    u8 padding[2];
} FollowerTalkAnimFrame;

// HG's condition snapshot (ov02_0224F058), same value encodings as HG.
typedef struct FollowerTalkContext {
    u8 hasItem;
    u8 itemCategory;
    u8 hpBucket;
    u8 status;
    u8 level;
    u8 type1;
    u8 type2;
    u8 friendship;
    u8 natureCategory;
    u8 gender;
    u8 speciesCategory;
    u8 npcCount;
    u8 hiddenItemCount;
    u8 weather;
    u8 metatile;
    u8 grass;
    u8 timeOfDay;
    u8 pokeathlon;
    u8 facing;
    u8 hasFashionCase;
    s8 mood;
    u16 mapHeaderID;
} FollowerTalkContext;

enum TalkState {
    TALK_STATE_STEPS = 0,
    TALK_STATE_YES_NO,
    TALK_STATE_GIFT,
    TALK_STATE_DONE,
};

enum TalkStepState {
    STEP_STATE_START = 0,
    STEP_STATE_EMOTE,
    STEP_STATE_MESSAGE,
    STEP_STATE_WAIT_START,
    STEP_STATE_NEXT,
    STEP_STATE_ANIM,
    STEP_STATE_PRINTING,
    STEP_STATE_WAIT,
    STEP_STATE_EMOTE_RUNNING,
};

typedef struct FollowerTalkWork {
    FollowerTalkAction action;
    FollowerTalkAnimFrame anim[TALK_NUM_ANIM_FRAMES];
    MessageLoader *loader;
    Menu *menu;
    Billboard *emote;
    MapObject *follower;
    u16 species;
    u8 form;
    u8 state;
    u8 stepIndex;
    u8 stepState;
    u8 numSteps;
    u8 waitCounter;
    u8 animFrame;
    u8 animCounter;
    u8 animRunning;
    u8 savedFacing;
    u8 emotePhase;
    u8 emoteSeqIndex;
    u8 emoteCounter;
    u8 giftWaiting;
    fx32 emoteRise;
    fx32 emoteRiseVelocity;
} FollowerTalkWork;

static FollowerTalkWork *sTalkWork = NULL;
static BOOL sTalkActive = FALSE;
static VecFx32 sTalkOffset = { 0, 0, 0 };

static s8 sFollowerMood = 0;
static BOOL sFollowerMoodValid = FALSE;
static u16 sFollowerMoodSpecies = 0;
static u32 sFollowerMoodPersonality = 0;

// ov02_02253AC0: nature -> HG "nature category"
static const u8 sNatureCategory[25] = { 4, 5, 4, 4, 1, 4, 3, 2, 1, 2, 5, 6, 3, 1, 1, 3, 6, 3, 5, 6, 2, 2, 1, 3, 6 };

// ov02_0224F79C: type -> HG talk type code (type 9, the "???" type, maps to 0)
static const u8 sTypeCode[18] = { 1, 7, 10, 8, 9, 13, 12, 14, 17, 0, 2, 3, 5, 4, 11, 6, 15, 16 };

// ov02_0224F820: bag pocket -> HG item category (key items assert in HG and fall back to 8)
static const u8 sPocketCategory[7] = { 4, 2, 1, 7, 6, 5, 3 };

// Emote bubble frame/texture sequence (HG fldeff 150; all 14 HG emote sequences are identical): duration per
// sequence index {0,4,8,12} (HG ov01_02203DF8 uses the startFrame table as durations), texture {0,1,0,1}.
// The billboard picks texture N at frameNum 4*N via the same sequence data (fldeff member 202).
static const u8 sEmoteSeqDuration[4] = { 0, 4, 8, 12 };
static const u8 sEmoteSeqTexture[4] = { 0, 1, 0, 1 };

static const BillboardAnim sEmoteAnims[] = {
    { 0, 15, BILLBOARD_ANIM_TYPE_LOOP },
    { 0, 0, BILLBOARD_ANIM_TYPE_TABLE_END },
};

// Platinum map section (mapLabelTextID, 0..125) -> HG map section whose location table is borrowed.
// 0 = no location table (general table only). 15 and 121 are HG's own tables for those exact places.
#define HG_NEW_BARK        126
#define HG_CHERRYGROVE     127
#define HG_CIANWOOD        130
#define HG_MAHOGANY        134
#define HG_LAKE_OF_RAGE    135
#define HG_MT_SILVER       137
#define HG_ROUTE_29        177
#define HG_ROUTE_40        188
#define HG_POWER_PLANT     201
#define HG_SAFARI_ZONE     202
#define HG_SEAFOAM         203
#define HG_RADIO_TOWER     208
#define HG_RUINS_OF_ALPH   209
#define HG_UNION_CAVE      210
#define HG_ROCKET_HQ       213
#define HG_ILEX_FOREST     214
#define HG_MT_MORTAR       216
#define HG_WHIRL_ISLANDS   218
#define HG_MT_SILVER_CAVE  219
#define HG_DARK_CAVE       220
#define HG_VICTORY_ROAD    221
#define HG_FRONTIER_ACCESS 229
#define HG_SINJOH_RUINS    231
#define HG_EMBEDDED_TOWER  232

#define FOLLOWER_TALK_NUM_PLAT_MAPSECS 126

static const u8 sTemplateMapsec[FOLLOWER_TALK_NUM_PLAT_MAPSECS] = {
    [1] = HG_NEW_BARK, // Twinleaf Town
    [2] = HG_CHERRYGROVE, // Sandgem Town
    [3] = HG_CHERRYGROVE, // Floaroma Town
    [4] = HG_CHERRYGROVE, // Solaceon Town
    [5] = HG_CHERRYGROVE, // Celestic Town
    [6] = HG_CHERRYGROVE, // Jubilife City
    [7] = HG_CIANWOOD, // Canalave City
    [8] = HG_CHERRYGROVE, // Oreburgh City
    [9] = HG_CHERRYGROVE, // Eterna City
    [10] = HG_CHERRYGROVE, // Hearthome City
    [11] = HG_CIANWOOD, // Pastoria City
    [12] = HG_CHERRYGROVE, // Veilstone City
    [13] = HG_CIANWOOD, // Sunyshore City
    [14] = HG_MAHOGANY, // Snowpoint City
    [15] = 15, // Pokemon League (HG's own table)
    [16] = HG_ROUTE_29, // Route 201
    [17] = HG_ROUTE_29, // Route 202
    [18] = HG_ROUTE_29, // Route 203
    [19] = HG_ROUTE_29, // Route 204
    [20] = HG_ROUTE_29, // Route 205
    [21] = HG_ROUTE_29, // Route 206
    [22] = HG_ROUTE_29, // Route 207
    [23] = HG_ROUTE_29, // Route 208
    [24] = HG_ROUTE_29, // Route 209
    [25] = HG_ROUTE_29, // Route 210
    [26] = HG_ROUTE_29, // Route 211
    [27] = HG_ROUTE_29, // Route 212
    [28] = HG_ROUTE_40, // Route 213 (beach)
    [29] = HG_ROUTE_29, // Route 214
    [30] = HG_ROUTE_29, // Route 215
    [31] = HG_SEAFOAM, // Route 216 (snow)
    [32] = HG_SEAFOAM, // Route 217 (snow)
    [33] = HG_ROUTE_40, // Route 218 (sea)
    [34] = HG_ROUTE_40, // Route 219 (sea)
    [35] = HG_ROUTE_40, // Route 220 (sea)
    [36] = HG_ROUTE_29, // Route 221
    [37] = HG_ROUTE_40, // Route 222 (beach)
    [38] = HG_ROUTE_40, // Route 223 (sea)
    [39] = HG_ROUTE_40, // Route 224 (beach)
    [40] = HG_ROUTE_29, // Route 225
    [41] = HG_ROUTE_40, // Route 226 (sea/beach)
    [42] = HG_ROUTE_29, // Route 227
    [43] = HG_ROUTE_40, // Route 228 (sand)
    [44] = HG_ROUTE_29, // Route 229
    [45] = HG_ROUTE_40, // Route 230 (sea)
    [46] = HG_UNION_CAVE, // Oreburgh Mine
    [47] = HG_POWER_PLANT, // Valley Windworks
    [48] = HG_ILEX_FOREST, // Eterna Forest
    [49] = HG_POWER_PLANT, // Fuego Ironworks
    [50] = HG_MT_MORTAR, // Mt. Coronet
    [51] = HG_SINJOH_RUINS, // Spear Pillar
    [52] = HG_SAFARI_ZONE, // Great Marsh
    [53] = HG_RUINS_OF_ALPH, // Solaceon Ruins
    [54] = HG_VICTORY_ROAD, // Victory Road
    [55] = HG_SAFARI_ZONE, // Pal Park
    [57] = HG_DARK_CAVE, // Ravaged Path
    [58] = HG_ROUTE_29, // Floaroma Meadow
    [59] = HG_DARK_CAVE, // Oreburgh Gate
    [60] = HG_ILEX_FOREST, // Fullmoon Island
    [61] = HG_MT_SILVER, // Sendoff Spring
    [62] = HG_MT_SILVER_CAVE, // Turnback Cave
    [63] = HG_ILEX_FOREST, // Flower Paradise
    [64] = HG_EMBEDDED_TOWER, // Snowpoint Temple
    [65] = HG_DARK_CAVE, // Wayward Cave
    [66] = HG_UNION_CAVE, // Ruin Maniac Cave
    [67] = HG_UNION_CAVE, // Maniac Tunnel
    [68] = HG_ROUTE_29, // Trophy Garden
    [69] = HG_UNION_CAVE, // Iron Island
    [71] = HG_ROCKET_HQ, // Galactic HQ
    [72] = HG_ROUTE_29, // Verity Lakefront
    [73] = HG_ROUTE_29, // Valor Lakefront
    [74] = HG_SEAFOAM, // Acuity Lakefront
    [75] = HG_ROUTE_29, // Spring Path
    [76] = HG_LAKE_OF_RAGE, // Lake Verity
    [77] = HG_LAKE_OF_RAGE, // Lake Valor
    [78] = HG_LAKE_OF_RAGE, // Lake Acuity
    [79] = HG_ILEX_FOREST, // Newmoon Island
    [80] = HG_FRONTIER_ACCESS, // Battle Tower
    [81] = HG_FRONTIER_ACCESS, // Fight Area
    [82] = HG_FRONTIER_ACCESS, // Survival Area
    [83] = HG_CHERRYGROVE, // Resort Area
    [84] = HG_MT_MORTAR, // Stark Mountain
    [85] = HG_ROUTE_40, // Seabreak Path
    [86] = HG_SINJOH_RUINS, // Hall of Origin
    [87] = HG_WHIRL_ISLANDS, // Verity Cavern
    [88] = HG_WHIRL_ISLANDS, // Valor Cavern
    [89] = HG_WHIRL_ISLANDS, // Acuity Cavern
    [90] = HG_RADIO_TOWER, // Jubilife TV
    [111] = HG_FRONTIER_ACCESS, // Battle Park
    [112] = HG_FRONTIER_ACCESS, // Battle Frontier
    [113] = HG_FRONTIER_ACCESS, // Battle Factory
    [114] = HG_FRONTIER_ACCESS, // Battle Castle
    [115] = HG_FRONTIER_ACCESS, // Battle Arcade
    [116] = HG_FRONTIER_ACCESS, // Battle Hall
    [117] = HG_EMBEDDED_TOWER, // Distortion World
    [120] = HG_FRONTIER_ACCESS, // Battleground
    [121] = 121, // Rotom's Room (HG's own table)
    [122] = HG_ROCKET_HQ, // T.G. Eterna Bldg
    [123] = HG_EMBEDDED_TOWER, // Iron Ruins
    [124] = HG_EMBEDDED_TOWER, // Iceberg Ruins
    [125] = HG_EMBEDDED_TOWER, // Rock Peak Ruins
};

static BOOL FollowerTalk_UpdateScript(ScriptContext *ctx);
static void FollowerTalk_ForceRenderUpdate(MapObject *follower);

static inline u16 ReadU16(const u8 *p)
{
    return p[0] | (p[1] << 8);
}

static Pokemon *FollowerTalk_GetLeadMon(FieldSystem *fieldSystem)
{
    Party *party = SaveData_GetParty(fieldSystem->saveData);
    int count = Party_GetCurrentCount(party);

    for (int i = 0; i < count; i++) {
        Pokemon *mon = Party_GetPokemonBySlotIndex(party, i);

        if (Pokemon_GetValue(mon, MON_DATA_SPECIES_OR_EGG, NULL) == SPECIES_EGG) {
            continue;
        }
        if (Pokemon_GetValue(mon, MON_DATA_HP, NULL) == 0) {
            continue;
        }
        return mon;
    }

    return NULL;
}

static MapObject *FollowerTalk_GetFollower(FieldSystem *fieldSystem)
{
    return FollowerMon_GetMapObject(fieldSystem); // ENTRY P87: never a partner trainer
}

// HG resets the mood whenever the follower becomes a different Pokemon (species or personality change).
static void FollowerTalk_SyncMood(Pokemon *mon)
{
    u16 species = Pokemon_GetValue(mon, MON_DATA_SPECIES, NULL);
    u32 personality = Pokemon_GetValue(mon, MON_DATA_PERSONALITY, NULL);

    if (sFollowerMoodValid == FALSE || species != sFollowerMoodSpecies || personality != sFollowerMoodPersonality) {
        sFollowerMoodValid = TRUE;
        sFollowerMoodSpecies = species;
        sFollowerMoodPersonality = personality;
        sFollowerMood = 0;
    }
}

void FollowerMonTalk_OnPlayerStep(FieldSystem *fieldSystem)
{
    if (FollowerTalk_GetFollower(fieldSystem) == NULL) {
        return;
    }

    Pokemon *mon = FollowerTalk_GetLeadMon(fieldSystem);

    if (mon == NULL) {
        return;
    }

    FollowerTalk_SyncMood(mon);

    if (sFollowerMood > 0) {
        sFollowerMood--;
    } else if (sFollowerMood < 0) {
        sFollowerMood++;
    }
}

BOOL FollowerMonTalk_GetRenderOffset(VecFx32 *offset)
{
    if (sTalkActive == FALSE) {
        return FALSE;
    }

    *offset = sTalkOffset;
    return TRUE;
}

// ---------------------------------------------------------------------------------------------------------------
// ENTRY P64: the other HG mood sources (HG src/pokemon_mood.c, src/follow_mon.c FieldSystem_UnkSub108_AddMonMood
// and its callers). HG stores mood in the Pokemon but resets it whenever a Pokemon becomes the follower, so only
// changes to the CURRENT follower ever matter - here that is the Pokemon the RAM mood belongs to.
// ---------------------------------------------------------------------------------------------------------------

static BOOL sFieldMoveAnimPaused = FALSE;

static void FollowerTalk_AddMood(int delta)
{
    int mood = sFollowerMood + delta;

    if (mood > MON_MOOD_MAX) {
        mood = MON_MOOD_MAX;
    } else if (mood < MON_MOOD_MIN) {
        mood = MON_MOOD_MIN;
    }

    sFollowerMood = mood;
}

static BOOL FollowerTalk_IsMoodOwner(Pokemon *mon)
{
    if (mon == NULL || sFollowerMoodValid == FALSE) {
        return FALSE;
    }

    return Pokemon_GetValue(mon, MON_DATA_SPECIES, NULL) == sFollowerMoodSpecies
        && Pokemon_GetValue(mon, MON_DATA_PERSONALITY, NULL) == sFollowerMoodPersonality;
}

void FollowerMonTalk_AddMoodForMon(Pokemon *mon, int delta)
{
    if (FollowerTalk_IsMoodOwner(mon)) {
        FollowerTalk_AddMood(delta);
    }
}

// HG ItemIdGetMoodEffect (src/pokemon_mood.c), applied where HG applies it: an item that changes friendship
// (field/menu use) or any successful battle item use.
void FollowerMonTalk_ApplyItemMood(Pokemon *mon, u16 item)
{
    int delta = 0;

    switch (item) {
    case ITEM_RARE_CANDY:
        delta = 40;
        break;
    case ITEM_ENERGYPOWDER:
    case ITEM_HEAL_POWDER:
        delta = -20;
        break;
    case ITEM_ENERGY_ROOT:
        delta = -30;
        break;
    case ITEM_REVIVAL_HERB:
        delta = -40;
        break;
    case ITEM_HP_UP:
    case ITEM_PROTEIN:
    case ITEM_IRON:
    case ITEM_CARBOS:
    case ITEM_CALCIUM:
    case ITEM_ZINC:
        delta = 8;
        break;
    case ITEM_GUARD_SPEC:
    case ITEM_DIRE_HIT:
    case ITEM_X_ATTACK:
    case ITEM_X_DEFENSE:
    case ITEM_X_SPEED:
    case ITEM_X_ACCURACY:
    case ITEM_X_SPECIAL:
    case ITEM_X_SP_DEF:
        delta = 10;
        break;
    }

    if (delta != 0) {
        FollowerMonTalk_AddMoodForMon(mon, delta);
    }
}

static BOOL FollowerTalk_LeadHasType(Pokemon *mon, int type)
{
    return Pokemon_GetValue(mon, MON_DATA_TYPE_1, NULL) == type || Pokemon_GetValue(mon, MON_DATA_TYPE_2, NULL) == type;
}

// HG Surf/Rock Climb/Sweet Scent/Dig/Teleport/Fly (overlay 1/2 tasks): +1 if the lead has the move's type and the
// follower is out.
void FollowerMonTalk_FieldMoveTypeMood(FieldSystem *fieldSystem, int type)
{
    Pokemon *mon = FollowerTalk_GetLeadMon(fieldSystem);

    if (mon == NULL || FollowerTalk_GetFollower(fieldSystem) == NULL) {
        return;
    }

    FollowerTalk_SyncMood(mon);

    if (FollowerTalk_LeadHasType(mon, type)) {
        FollowerTalk_AddMood(1);
    }
}

BOOL FollowerMonTalk_IsAnimationPaused(void)
{
    return sFieldMoveAnimPaused;
}

// HG scr_seq_0146 (Cut, Rock Smash, Headbutt, Strength, Flash): when the Pokemon using the move is the follower
// itself (player walking, follower out), the follower does the move instead of the HM cut-in: mood + amount, then
// ScrCmd_734 - the follower hops on the spot twice if it has the move's type (and mood +1), once otherwise
// (movement script ov02_02253A70: PauseAnimation, JumpOnSpotFast<facing>, Delay4, ResumeAnimation; skipped for
// Diglett/Dugtrio) - then its cry plays and the script waits for it.
#define REACTION_HOP_FRAMES 8
#define REACTION_DELAY_FRAMES 4

// Platinum's own JumpOnSpotFast curve (sJumpHeights_High read every other entry, 8 frames, src/unk_020655F4.c).
static const u8 sReactionHopHeights[REACTION_HOP_FRAMES] = { 8, 11, 12, 11, 9, 6, 0, 0 };

typedef struct FollowerFieldMoveReaction {
    MapObject *follower;
    u16 species;
    u8 form;
    u8 hopsLeft;
    u8 frame;
    u8 waitingForCry;
} FollowerFieldMoveReaction;

static FollowerFieldMoveReaction sReaction;

static BOOL FollowerTalk_UpdateFieldMoveReaction(ScriptContext *ctx)
{
    FollowerFieldMoveReaction *r = &sReaction;

    if (r->waitingForCry) {
        return Sound_IsPokemonCryPlaying() == FALSE;
    }

    if (r->hopsLeft > 0) {
        if (r->frame == 0) {
            Sound_PlayEffect(SEQ_SE_DP_DANSA_sseq);
        }

        sTalkOffset.y = r->frame < REACTION_HOP_FRAMES ? sReactionHopHeights[r->frame] * FX32_ONE : 0;
        FollowerTalk_ForceRenderUpdate(r->follower);

        if (++r->frame >= REACTION_HOP_FRAMES + REACTION_DELAY_FRAMES) {
            r->frame = 0;
            r->hopsLeft--;
        }

        return FALSE;
    }

    sTalkOffset.y = 0;
    sFieldMoveAnimPaused = FALSE;
    FollowerTalk_ForceRenderUpdate(r->follower);
    sTalkActive = FALSE;

    Sound_PlayPokemonCry(r->species, r->form);
    r->waitingForCry = TRUE;
    return FALSE;
}

BOOL FollowerMonTalk_StartFieldMoveReaction(ScriptContext *ctx, u16 partySlot, u8 moodAmount, u8 type)
{
    FieldSystem *fieldSystem = ctx->fieldSystem;
    MapObject *follower = FollowerTalk_GetFollower(fieldSystem);
    Party *party = SaveData_GetParty(fieldSystem->saveData);
    Pokemon *lead = FollowerTalk_GetLeadMon(fieldSystem);

    if (follower == NULL || lead == NULL || sTalkWork != NULL || sTalkActive) {
        return FALSE;
    }

    if (PlayerAvatar_GetPlayerState(fieldSystem->playerAvatar) != PLAYER_AVATAR_WALKING) {
        return FALSE;
    }

    if (partySlot >= Party_GetCurrentCount(party) || Party_GetPokemonBySlotIndex(party, partySlot) != lead) {
        return FALSE;
    }

    FollowerTalk_SyncMood(lead);
    FollowerTalk_AddMood(moodAmount);

    u8 hops = 1;

    if (FollowerTalk_LeadHasType(lead, type)) {
        hops = 2;
        FollowerTalk_AddMood(1);
    }

    u16 species = Pokemon_GetValue(lead, MON_DATA_SPECIES, NULL);

    if (species == SPECIES_DIGLETT || species == SPECIES_DUGTRIO) {
        hops = 0;
    }

    sReaction.follower = follower;
    sReaction.species = species;
    sReaction.form = Pokemon_GetValue(lead, MON_DATA_FORM, NULL);
    sReaction.hopsLeft = hops;
    sReaction.frame = 0;
    sReaction.waitingForCry = FALSE;

    sTalkOffset.x = 0;
    sTalkOffset.y = 0;
    sTalkOffset.z = 0;
    sTalkActive = TRUE;
    sFieldMoveAnimPaused = TRUE;

    ScriptContext_Pause(ctx, FollowerTalk_UpdateFieldMoveReaction);
    return TRUE;
}

// ---------------------------------------------------------------------------------------------------------------
// Condition snapshot (HG ov02_0224F058 and its helpers)
// ---------------------------------------------------------------------------------------------------------------

static u8 FollowerTalk_GetStatusCode(u32 status)
{
    if (status & (MON_CONDITION_POISON | MON_CONDITION_TOXIC)) {
        return 5;
    }
    if (status & (MON_CONDITION_SLEEP_0 | MON_CONDITION_SLEEP_1 | MON_CONDITION_SLEEP_2)) {
        return 8;
    }
    if (status & MON_CONDITION_BURN) {
        return 2;
    }
    if (status & MON_CONDITION_FREEZE) {
        return 3;
    }
    if (status & MON_CONDITION_PARALYSIS) {
        return 4;
    }
    return 1;
}

static u8 FollowerTalk_GetTypeCode(u32 type)
{
    if (type < NELEMS(sTypeCode)) {
        return sTypeCode[type];
    }
    return 0;
}

static u8 FollowerTalk_CountNearbyPeople(FieldSystem *fieldSystem)
{
    int playerX = PlayerAvatar_GetXPos(fieldSystem->playerAvatar);
    int playerZ = PlayerAvatar_GetZPos(fieldSystem->playerAvatar);
    MapObject *mapObj;
    int index = 0;
    u8 count = 0;

    while (MapObjectMan_FindObjectWithStatus(fieldSystem->mapObjMan, &mapObj, &index, MAP_OBJ_STATUS_0) == TRUE) {
        int graphicsID = MapObject_GetGraphicsID(mapObj);

        // HG skips its boulder/breakable-rock/cut-tree sprites (it only records that they exist).
        if (graphicsID == OBJ_EVENT_GFX_STRENGTH_BOULDER || graphicsID == OBJ_EVENT_GFX_ROCK_SMASH || graphicsID == OBJ_EVENT_GFX_CUT_TREE) {
            continue;
        }

        int dx = playerX - MapObject_GetX(mapObj);
        int dz = playerZ - MapObject_GetZ(mapObj);

        if (dx < -1 || dx > 1 || dz < -1 || dz > 1) {
            continue;
        }

        if (MapObject_GetLocalID(mapObj) == LOCALID_PLAYER || MapObject_GetMovementType(mapObj) == MOVEMENT_TYPE_FOLLOW_PLAYER) {
            continue;
        }

        count++;
    }

    return count;
}

static u8 FollowerTalk_CountHiddenItems(FieldSystem *fieldSystem)
{
    const BgEvent *bgEvents = MapHeaderData_GetBgEvents(fieldSystem);
    int numBgEvents = MapHeaderData_GetNumBgEvents(fieldSystem);
    u8 count = 0;

    if (bgEvents == NULL) {
        return 0;
    }

    for (int i = 0; i < numBgEvents; i++) {
        if (bgEvents[i].type == BG_EVENT_TYPE_HIDDEN_ITEM && FieldSystem_CheckFlag(fieldSystem, Script_GetHiddenItemFlag(bgEvents[i].script)) == FALSE) {
            count++;
        }
    }

    return count;
}

static u8 FollowerTalk_GetSpeciesCategory(u16 species)
{
    u8 category = 0;

    if (species >= 1 && species <= NATIONAL_DEX_COUNT) {
        NARC_ReadFromMemberByIndexPair(&category, NARC_INDEX_DATA__FOLLOWER_TALK_SPECIES, 0, species - 1, 1);
    }

    return category;
}

static void FollowerTalk_BuildContext(FieldSystem *fieldSystem, Pokemon *mon, MapObject *follower, FollowerTalkContext *ctx)
{
    memset(ctx, 0, sizeof(FollowerTalkContext));

    u16 item = Pokemon_GetValue(mon, MON_DATA_HELD_ITEM, NULL);

    if (item != ITEM_NONE) {
        u32 pocket = Item_LoadParam(item, ITEM_PARAM_FIELD_POCKET, TALK_HEAP_ID);

        ctx->hasItem = TRUE;
        ctx->itemCategory = pocket < NELEMS(sPocketCategory) ? sPocketCategory[pocket] : 8;
    } else {
        ctx->hasItem = FALSE;
        ctx->itemCategory = 8;
    }

    u32 hp = Pokemon_GetValue(mon, MON_DATA_HP, NULL);
    u32 maxHP = Pokemon_GetValue(mon, MON_DATA_MAX_HP, NULL);
    u32 hpPercent = maxHP != 0 ? (hp * 100) / maxHP : 0;

    if (hpPercent == 100) {
        ctx->hpBucket = 1;
    } else if (hpPercent >= 75) {
        ctx->hpBucket = 2;
    } else if (hpPercent >= 50) {
        ctx->hpBucket = 3;
    } else if (hpPercent >= 25) {
        ctx->hpBucket = 4;
    } else {
        ctx->hpBucket = 5;
    }

    ctx->status = FollowerTalk_GetStatusCode(Pokemon_GetValue(mon, MON_DATA_STATUS, NULL));

    int level = Pokemon_GetValue(mon, MON_DATA_LEVEL, NULL);

    if (level + 2 >= 50) {
        ctx->level = 4;
    } else if (level - 2 <= 50) {
        ctx->level = 6;
    } else {
        ctx->level = 5;
    }

    ctx->type1 = FollowerTalk_GetTypeCode(Pokemon_GetValue(mon, MON_DATA_TYPE_1, NULL));
    ctx->type2 = FollowerTalk_GetTypeCode(Pokemon_GetValue(mon, MON_DATA_TYPE_2, NULL));
    ctx->friendship = Pokemon_GetValue(mon, MON_DATA_FRIENDSHIP, NULL);

    u8 nature = Pokemon_GetNature(mon);
    ctx->natureCategory = nature < NELEMS(sNatureCategory) ? sNatureCategory[nature] : 0;
    ctx->gender = Pokemon_GetGender(mon) == GENDER_MALE ? 1 : 2;
    ctx->speciesCategory = FollowerTalk_GetSpeciesCategory(Pokemon_GetValue(mon, MON_DATA_SPECIES, NULL));

    ctx->npcCount = FollowerTalk_CountNearbyPeople(fieldSystem);
    ctx->hiddenItemCount = FollowerTalk_CountHiddenItems(fieldSystem);

    u16 weather = FieldOverworldState_GetWeather(SaveData_GetFieldOverworldState(fieldSystem->saveData));

    if (weather == OVERWORLD_WEATHER_CLEAR) {
        ctx->weather = 1;
    } else if (weather == OVERWORLD_WEATHER_RAINING) {
        ctx->weather = 3;
    } else {
        ctx->weather = 0;
    }

    ctx->metatile = TerrainCollisionManager_GetTileBehavior(fieldSystem, MapObject_GetX(follower), MapObject_GetZ(follower));
    ctx->grass = TileBehavior_HasEncounters(ctx->metatile) ? 1 : 2;
    ctx->timeOfDay = FieldSystem_GetTimeOfDay(fieldSystem) + 1;

    FollowerTalk_SyncMood(mon);
    ctx->mood = sFollowerMood;

    ctx->pokeathlon = 0; // no Pokeathlon in Platinum: entries needing a Pokeathlon stat never pass

    switch (MapObject_GetFacingDir(follower)) {
    case DIR_NORTH:
        ctx->facing = 3;
        break;
    case DIR_SOUTH:
        ctx->facing = 4;
        break;
    case DIR_WEST:
        ctx->facing = 2;
        break;
    case DIR_EAST:
        ctx->facing = 1;
        break;
    default:
        ctx->facing = 0;
        break;
    }

    ctx->mapHeaderID = fieldSystem->location->mapHeaderID;
    ctx->hasFashionCase = Bag_GetItemQuantity(SaveData_GetBag(fieldSystem->saveData), ITEM_FASHION_CASE, TALK_HEAP_ID) > 0;
}

// ---------------------------------------------------------------------------------------------------------------
// Condition checks (HG ov02_0224F108 and its range helpers)
// ---------------------------------------------------------------------------------------------------------------

static BOOL FollowerTalk_MoodInRange(u8 range, s8 mood)
{
    switch (range) {
    case 1:
        return mood == 127;
    case 2:
        return mood >= 100 && mood < 127;
    case 3:
        return mood >= 50 && mood < 100;
    case 4:
        return mood >= 30 && mood < 50;
    case 5:
        return mood > -30 && mood < 30;
    case 6:
        return mood > -50 && mood <= -30;
    case 7:
        return mood > -127 && mood <= -50;
    case 8:
        return mood == -127;
    case 9:
        return mood >= 0;
    case 10:
        return mood <= -1;
    }

    return FALSE;
}

static BOOL FollowerTalk_FriendshipInRange(u8 range, u8 friendship)
{
    switch (range) {
    case 1:
        return friendship == 255;
    case 2:
        return friendship >= 200 && friendship < 255;
    case 3:
        return friendship >= 150 && friendship < 200;
    case 4:
        return friendship >= 90 && friendship < 150;
    case 5:
        return friendship >= 60 && friendship < 90;
    case 6:
        return friendship >= 30 && friendship < 60;
    case 7:
        return friendship >= 1 && friendship < 30;
    case 8:
        return friendship == 0;
    case 9:
        return friendship >= 90;
    case 10:
        return friendship < 60;
    }

    return FALSE;
}

static BOOL FollowerTalk_SpeciesCategoryMatches(u8 wanted, u8 category)
{
    switch (wanted) {
    case 0xFA:
        return category <= 19;
    case 0xFB:
        return category <= 130;
    case 0xFC:
        return category >= 140 && category <= 149;
    case 0xFD:
        return category >= 160;
    case 0xFE:
        return category >= 220;
    }

    return wanted == category;
}

// The only two HG map-id conditions in the two Sinnoh tables HG has (Pokemon League, Rotom's Room).
static BOOL FollowerTalk_MapMatches(u16 hgMapID, u16 mapHeaderID)
{
    switch (hgMapID) {
    case 58: // MAP_INDIGO_PLATEAU, the outdoor League map
        return mapHeaderID == MAP_HEADER_POKEMON_LEAGUE;
    case 403: // MAP_SAFFRON_SILPH_CO_ROTOM_ROOM
        return mapHeaderID == MAP_HEADER_ROTOMS_ROOM;
    }

    return FALSE;
}

static BOOL FollowerTalk_EntryPasses(const u8 *entry, const FollowerTalkContext *ctx, BOOL borrowedTable)
{
    u16 flagsWord = ReadU16(entry + 0x8);
    u16 miscWord = ReadU16(entry + 0xA);
    u16 hgMapID = ReadU16(entry + 0xC);
    u16 metatile = ReadU16(entry + 0xE);
    u16 flag = ReadU16(entry + 0x12);
    u8 value;

    if ((LCRNG_Next() % 100) >= entry[0x11]) {
        return FALSE;
    }

    if (flag != 0) {
        if (flag != HG_FLAG_GOT_FASHION_CASE || ctx->hasFashionCase == FALSE) {
            return FALSE;
        }
    }

    value = entry[3] & 0x1F;
    if (value != 0) {
        if (value == 9) {
            if (ctx->hasItem == FALSE) {
                return FALSE;
            }
        } else if (value != ctx->itemCategory) {
            return FALSE;
        }
    }

    if (entry[0] != 0 && entry[0] != ctx->hpBucket) {
        return FALSE;
    }

    value = entry[2] >> 5;
    if (value != 0) {
        if (value == 7) {
            if (ctx->status != 2 && ctx->status != 3 && ctx->status != 4 && ctx->status != 5 && ctx->status != 8) {
                return FALSE;
            }
        } else if (value != ctx->status) {
            return FALSE;
        }
    }

    value = miscWord & 7;
    if (value != 0) {
        if (value == 5) {
            if (ctx->npcCount < 5) {
                return FALSE;
            }
        } else if (value != ctx->npcCount) {
            return FALSE;
        }
    }

    value = entry[0x10] >> 5;
    if (value != 0) {
        if (value == 4) {
            if (ctx->hiddenItemCount < 4) {
                return FALSE;
            }
        } else if (value != ctx->hiddenItemCount) {
            return FALSE;
        }
    }

    value = (entry[0x10] >> 1) & 3;
    if (value != 0 && value != ctx->level) { // HG bug kept: this can never match
        return FALSE;
    }

    if (((entry[0x10] >> 3) & 3) != 0) { // HG bug kept: the NPC-sprite condition always fails
        return FALSE;
    }

    value = entry[4] & 0x1F;
    if (value != 0 && value != ctx->type1 && value != ctx->type2) {
        return FALSE;
    }

    value = flagsWord & 7;
    if (value != 0 && value != ctx->weather) {
        return FALSE;
    }

    if (metatile != 0 && metatile != ctx->metatile) {
        return FALSE;
    }

    if (entry[5] != 0 && entry[5] != ctx->grass) {
        return FALSE;
    }

    if (hgMapID != 0) {
        if (borrowedTable || FollowerTalk_MapMatches(hgMapID - 1, ctx->mapHeaderID) == FALSE) {
            return FALSE;
        }
    }

    value = (miscWord >> 3) & 7;
    if (value != 0 && value != ctx->timeOfDay) {
        return FALSE;
    }

    value = entry[1] & 0xF;
    if (value != 0 && FollowerTalk_MoodInRange(value, ctx->mood) == FALSE) {
        return FALSE;
    }

    value = entry[4] >> 5;
    if (value != 0 && value != ctx->pokeathlon) {
        return FALSE;
    }

    value = entry[1] >> 4;
    if (value != 0 && FollowerTalk_FriendshipInRange(value, ctx->friendship) == FALSE) {
        return FALSE;
    }

    value = entry[2] & 7;
    if (value != 0 && value != ctx->natureCategory) {
        return FALSE;
    }

    if (entry[6] != 0 && FollowerTalk_SpeciesCategoryMatches(entry[6], ctx->speciesCategory) == FALSE) {
        return FALSE;
    }

    // Shiny Leaf "not obtained yet": no leaves exist in Platinum so HG's check would always pass, but those
    // entries only exist in HG route tables and lead to leaf-giving actions, so borrowed ones are skipped.
    if (entry[7] != 0 && borrowedTable) {
        return FALSE;
    }

    value = (entry[2] >> 3) & 3;
    if (value != 0 && value != ctx->gender) {
        return FALSE;
    }

    value = flagsWord >> 13;
    if (value != 0 && value != ctx->facing) {
        return FALSE;
    }

    return TRUE;
}

static u16 FollowerTalk_ChooseAction(FieldSystem *fieldSystem, const FollowerTalkContext *ctx)
{
    u8 *table = Heap_AllocAtEnd(TALK_HEAP_ID, TALK_NUM_SLOTS * TALK_COND_ENTRY_SIZE);
    u32 mapsec = MapHeader_GetMapLabelTextID(fieldSystem->location->mapHeaderID);
    u32 hgMapsec = mapsec < FOLLOWER_TALK_NUM_PLAT_MAPSECS ? sTemplateMapsec[mapsec] : 0;
    BOOL borrowedTable = (hgMapsec != mapsec);
    u16 actionID = 0;
    int i;

    memset(table, 0, TALK_NUM_SLOTS * TALK_COND_ENTRY_SIZE);

    if (hgMapsec != 0 && NARC_GetMemberSizeByIndexPair(NARC_INDEX_DATA__FOLLOWER_TALK_COND, hgMapsec + 1) >= TALK_NUM_LOCAL_SLOTS * TALK_COND_ENTRY_SIZE) {
        NARC_ReadFromMemberByIndexPair(table, NARC_INDEX_DATA__FOLLOWER_TALK_COND, hgMapsec + 1, 0, TALK_NUM_LOCAL_SLOTS * TALK_COND_ENTRY_SIZE);
    }

    NARC_ReadFromMemberByIndexPair(table + TALK_NUM_LOCAL_SLOTS * TALK_COND_ENTRY_SIZE, NARC_INDEX_DATA__FOLLOWER_TALK_COND, 0, 0, (TALK_NUM_SLOTS - TALK_NUM_LOCAL_SLOTS) * TALK_COND_ENTRY_SIZE);

    // HG's slot order: general 30..41, location 0..29, general 42..99.
    for (i = 0; i < TALK_NUM_SLOTS && actionID == 0; i++) {
        int slot;
        BOOL borrowedEntry;

        if (i < 12) {
            slot = 30 + i;
        } else if (i < 42) {
            slot = i - 12;
        } else {
            slot = i;
        }

        borrowedEntry = (slot < TALK_NUM_LOCAL_SLOTS) && borrowedTable;

        const u8 *entry = table + slot * TALK_COND_ENTRY_SIZE;
        u16 entryAction = ReadU16(entry + 0xA) >> 6;

        if (entryAction == 0) {
            continue;
        }

        if (FollowerTalk_EntryPasses(entry, ctx, borrowedEntry)) {
            actionID = entryAction;
        }
    }

    Heap_Free(table);

    // General slot 99 always passes in HG, so this never happens with HG's data.
    if (actionID == 0) {
        actionID = 1;
    }

    return actionID;
}

// ---------------------------------------------------------------------------------------------------------------
// Running the action (HG ov02_0224F8FC, Task_FollowMonInteract)
// ---------------------------------------------------------------------------------------------------------------

static void FollowerTalk_LoadAction(FollowerTalkWork *work, u16 actionID)
{
    u8 raw[52];

    NARC_ReadWholeMemberByIndexPair(raw, NARC_INDEX_DATA__FOLLOWER_TALK_ACTION, actionID - 1);

    for (int i = 0; i < TALK_NUM_STEPS; i++) {
        const u8 *step = raw + i * 8;

        work->action.steps[i].anim = ReadU16(step);
        work->action.steps[i].msg = ReadU16(step + 2);
        work->action.steps[i].sound = ReadU16(step + 4);
        work->action.steps[i].emote = step[6];
        work->action.steps[i].wait = step[7];
    }

    work->action.yesNo = raw[0x28] | (raw[0x29] << 8) | (raw[0x2A] << 16) | (raw[0x2B] << 24);
    work->action.yesAction = ReadU16(raw + 0x2C);
    work->action.noAction = ReadU16(raw + 0x2E);
    work->action.friendshipDelta = (s8)raw[0x30];
    work->action.moodDelta = (s8)raw[0x31];
    work->action.accessory = raw[0x32];
    work->action.shinyLeaf = raw[0x33];

    work->numSteps = 0;

    while (work->numSteps < TALK_NUM_STEPS && work->action.steps[work->numSteps].anim != TALK_STEP_LIST_END) {
        work->numSteps++;
    }

    work->stepIndex = 0;
    work->stepState = STEP_STATE_START;
    work->waitCounter = 0;
}

// While a script holds LockAll the follower's movement is paused, and MapObject_Draw (ov5_021ECC20.c) then skips
// its renderer update - HG unpauses its follower during animations/emotes for the same reason. Forcing a single
// renderer update per frame here (only while it is paused, so it never runs twice) is what makes facing changes
// and talk offsets visible, without letting the follower's movement task run.
static void FollowerTalk_ForceRenderUpdate(MapObject *follower)
{
    if (MapObject_IsMovementPaused(follower) == TRUE && sub_02062F7C(follower) == FALSE && sub_02062D4C(follower) == TRUE) {
        sub_02062B68(follower);
    }
}

static void FollowerTalk_SetPrinterTemplate(FieldSystem *fieldSystem, Pokemon *mon)
{
    StringTemplate *template = *(StringTemplate **)FieldSystem_GetScriptMemberPtr(fieldSystem, SCRIPT_MANAGER_STR_TEMPLATE);
    BoxPokemon *boxMon = Pokemon_GetBoxPokemon(mon);

    StringTemplate_SetNickname(template, 0, boxMon);
    StringTemplate_SetSpeciesName(template, 1, boxMon);
    StringTemplate_SetPlayerName(template, 2, SaveData_GetTrainerInfo(fieldSystem->saveData));
    StringTemplate_SetLocationName(template, 3, MapHeader_GetMapLabelTextID(fieldSystem->location->mapHeaderID));
    StringTemplate_SetItemName(template, 4, Pokemon_GetValue(mon, MON_DATA_HELD_ITEM, NULL));
}

static void FollowerTalk_CloseMessage(FieldSystem *fieldSystem)
{
    Window *window = FieldSystem_GetScriptMemberPtr(fieldSystem, SCRIPT_MANAGER_WINDOW);
    u8 *isMsgBoxOpen = FieldSystem_GetScriptMemberPtr(fieldSystem, SCRIPT_MANAGER_IS_MSG_BOX_OPEN);

    if (*isMsgBoxOpen) {
        Window_EraseMessageBox(window, FALSE);
        Window_Remove(window);
        *isMsgBoxOpen = FALSE;
    }
}

static BOOL FollowerTalk_MessageFinished(FieldSystem *fieldSystem)
{
    u8 *printerID = FieldSystem_GetScriptMemberPtr(fieldSystem, SCRIPT_MANAGER_MESSAGE_ID);

    return FieldMessage_FinishedPrinting(*printerID);
}

static BOOL FollowerTalk_ABPressed(void)
{
    return (gSystem.pressedKeys & (PAD_BUTTON_A | PAD_BUTTON_B)) != 0;
}

static void FollowerTalk_PlayFrameSound(FollowerTalkWork *work, u16 sound)
{
    if (sound == TALK_SOUND_CRY) {
        Sound_PlayPokemonCryEx(POKECRY_NORMAL, work->species, 0, 127, TALK_HEAP_ID, work->form);
    } else if (sound == TALK_SOUND_CRY_PINCH) {
        Sound_PlayPokemonCryEx(POKECRY_PINCH_NORMAL, work->species, 0, 127, TALK_HEAP_ID, work->form);
    }
    // HG's data uses no other sounds.
}

static void FollowerTalk_StartAnim(FollowerTalkWork *work, u16 animID)
{
    u8 raw[TALK_NUM_ANIM_FRAMES * 8];

    NARC_ReadWholeMemberByIndexPair(raw, NARC_INDEX_DATA__FOLLOWER_TALK_ANIM, animID - 1);

    for (int i = 0; i < TALK_NUM_ANIM_FRAMES; i++) {
        work->anim[i].facing = raw[i * 8];
        work->anim[i].duration = raw[i * 8 + 1];
        work->anim[i].dx = (s8)raw[i * 8 + 2];
        work->anim[i].dHeight = (s8)raw[i * 8 + 3];
        work->anim[i].dz = (s8)raw[i * 8 + 4];
        work->anim[i].playSound = raw[i * 8 + 5];
    }

    work->savedFacing = MapObject_GetFacingDir(work->follower);
    work->animFrame = 0;
    work->animCounter = 0;
    work->animRunning = TRUE;
    sTalkOffset.x = 0;
    sTalkOffset.y = 0;
    sTalkOffset.z = 0;
}

// HG ov02_02250004 / ov02_0224FF5C: each frame's moves are applied once, when the frame starts; position, height
// and facing are restored when the animation ends. Returns TRUE when finished.
static BOOL FollowerTalk_UpdateAnim(FollowerTalkWork *work)
{
    if (work->animFrame >= TALK_NUM_ANIM_FRAMES || work->anim[work->animFrame].facing == TALK_ANIM_END) {
        sTalkOffset.x = 0;
        sTalkOffset.y = 0;
        sTalkOffset.z = 0;
        MapObject_Face(work->follower, work->savedFacing);
        FollowerTalk_ForceRenderUpdate(work->follower);
        work->animRunning = FALSE;
        return TRUE;
    }

    const FollowerTalkAnimFrame *frame = &work->anim[work->animFrame];

    if (work->animCounter == 0) {
        sTalkOffset.x += frame->dx * FX32_ONE;
        sTalkOffset.z += frame->dz * FX32_ONE;

        // HG never lifts Diglett/Dugtrio (they stay in the ground).
        if (frame->dHeight != 0 && work->species != SPECIES_DIGLETT && work->species != SPECIES_DUGTRIO) {
            sTalkOffset.y += frame->dHeight * FX32_ONE;
        }

        if (frame->playSound) {
            FollowerTalk_PlayFrameSound(work, work->action.steps[work->stepIndex].sound);
        }

        if (frame->facing != 0) {
            MapObject_Face(work->follower, frame->facing - 1);
        }
    }

    FollowerTalk_ForceRenderUpdate(work->follower);

    work->animCounter++;

    if (work->animCounter >= frame->duration) {
        work->animFrame++;
        work->animCounter = 0;
    }

    return FALSE;
}

static BOOL FollowerTalk_EmoteAllowed(FieldSystem *fieldSystem, MapObject *follower)
{
    // HG sub_0205BB04: no emote bubble on tile behaviors 113, 114 and 46.
    u8 behavior = TerrainCollisionManager_GetTileBehavior(fieldSystem, MapObject_GetX(follower), MapObject_GetZ(follower));

    return behavior != 113 && behavior != 114 && behavior != 46;
}

// ENTRY P91: HG puts the bubble a fixed 32 units above every follower (ov01_02203CB8) - it has no tall-specific code;
// under HG's flatter camera that clears its 64x64 "too tall" sprites. Platinum's camera is much steeper (~59 deg), so
// 32 world units of height are only ~16 pixels on screen and the bubble sat inside the big sprites. For the 64x64
// sprites (FOLLOWER_TALL_TABLE) the bubble is raised by the extra 32 pixels of sprite height, along the camera's own
// up axis (world-space column 1 of the camera matrix): straight up on screen, same depth, so it can never sink behind
// the sprite. 32x32 followers are unchanged.
// ENTRY P92: 32 was about 3x too high in-game (user) - 11 units.
#define FOLLOWER_TALK_TALL_EMOTE_LIFT (FX32_ONE * 11)

static void FollowerTalk_GetEmotePos(FollowerTalkWork *work, VecFx32 *pos)
{
    ov5_021ECDA0(work->follower, pos);
    pos->y += FX32_ONE * 32 + work->emoteRise;
    pos->z += FX32_ONE * 1;

    if (FollowerMon_IsTallGraphicsID(MapObject_GetGraphicsID(work->follower))) {
        const MtxFx43 *camera = NNS_G3dGlbGetCameraMtx();

        pos->x += FX_Mul(camera->_01, FOLLOWER_TALK_TALL_EMOTE_LIFT);
        pos->y += FX_Mul(camera->_11, FOLLOWER_TALK_TALL_EMOTE_LIFT);
        pos->z += FX_Mul(camera->_21, FOLLOWER_TALK_TALL_EMOTE_LIFT);
    }
}

static void FollowerTalk_StartEmote(FieldSystem *fieldSystem, FollowerTalkWork *work, u8 emote)
{
    FieldEffectManager *fieldEffMan = fieldSystem->fieldEffMan;
    VecFx32 pos;

    ov5_021DF9E0(fieldEffMan, TALK_EMOTE_SLOT, TALK_FLDEFF_EMOTE_MODEL);
    ov5_021DFA14(fieldEffMan, TALK_EMOTE_SLOT, TALK_FLDEFF_EMOTE_SEQ);
    ov5_021DFA3C(fieldEffMan, TALK_EMOTE_SLOT, TALK_FLDEFF_EMOTE_TEX + emote - 1, 1);
    ov5_021DF864(fieldEffMan, TALK_EMOTE_RES_ID, TALK_EMOTE_SLOT, TALK_EMOTE_SLOT, TALK_EMOTE_SLOT, 0, sEmoteAnims);

    work->emoteRise = 0;
    work->emoteRiseVelocity = FX32_ONE * 6;
    work->emotePhase = 0;
    work->emoteSeqIndex = 0;
    work->emoteCounter = 0;

    FollowerTalk_GetEmotePos(work, &pos);
    work->emote = ov5_021DF84C(fieldEffMan, TALK_EMOTE_RES_ID, &pos);

    if (work->emote != NULL) {
        Billboard_SetFrameNum(work->emote, 0);
    }

    Sound_PlayEffect(SEQ_SE_DP_DECIDE_sseq);
}

static void FollowerTalk_FreeEmote(FieldSystem *fieldSystem, FollowerTalkWork *work)
{
    FieldEffectManager *fieldEffMan = fieldSystem->fieldEffMan;

    if (work->emote != NULL) {
        Billboard_Delete(work->emote);
        work->emote = NULL;
    }

    ov5_021DFA08(fieldEffMan, TALK_EMOTE_SLOT);
    ov5_021DFA30(fieldEffMan, TALK_EMOTE_SLOT);
    ov5_021DFA7C(fieldEffMan, TALK_EMOTE_SLOT);
    ov5_021DF9D4(fieldEffMan, TALK_EMOTE_RES_ID);
}

// HG ov01_02203CB8: rise (velocity 6, -2 per frame), then the 2-frame texture sequence, then 2 more frames.
// Returns TRUE when the bubble is gone.
static BOOL FollowerTalk_UpdateEmote(FieldSystem *fieldSystem, FollowerTalkWork *work)
{
    switch (work->emotePhase) {
    case 0:
        work->emoteRise += work->emoteRiseVelocity;

        if (work->emoteRise > 0) {
            work->emoteRiseVelocity -= FX32_ONE * 2;
        } else {
            work->emoteRise = 0;
            work->emoteRiseVelocity = 0;
            work->emotePhase++;
        }
        break;
    case 1:
        work->emoteCounter++;

        if (work->emoteCounter >= sEmoteSeqDuration[work->emoteSeqIndex]) {
            work->emoteSeqIndex++;
            work->emoteCounter = 0;

            if (work->emoteSeqIndex >= NELEMS(sEmoteSeqDuration)) {
                work->emotePhase++;
            } else if (work->emote != NULL) {
                Billboard_SetFrameNum(work->emote, sEmoteSeqTexture[work->emoteSeqIndex] * FX32_CONST(4));
            }
        }
        break;
    case 2:
        work->emoteCounter++;

        if (work->emoteCounter >= 2) {
            work->emotePhase++;
        }
        break;
    default:
        FollowerTalk_FreeEmote(fieldSystem, work);
        return TRUE;
    }

    if (work->emote != NULL) {
        VecFx32 pos;

        FollowerTalk_GetEmotePos(work, &pos);
        Billboard_SetPos(work->emote, &pos);
    }

    return FALSE;
}

static void FollowerTalk_ApplyEffects(FieldSystem *fieldSystem, FollowerTalkWork *work)
{
    Pokemon *mon = FollowerTalk_GetLeadMon(fieldSystem);
    int mood = sFollowerMood + work->action.moodDelta;

    if (mood > MON_MOOD_MAX) {
        mood = MON_MOOD_MAX;
    } else if (mood < MON_MOOD_MIN) {
        mood = MON_MOOD_MIN;
    }

    sFollowerMood = mood;

    if (mon != NULL && work->action.friendshipDelta != 0) {
        int friendship = Pokemon_GetValue(mon, MON_DATA_FRIENDSHIP, NULL) + work->action.friendshipDelta;
        u8 value;

        if (friendship > 255) {
            friendship = 255;
        } else if (friendship < 0) {
            friendship = 0;
        }

        value = friendship;
        Pokemon_SetValue(mon, MON_DATA_FRIENDSHIP, &value);
    }
}

// HG Task_FollowMonInteract state 3: give the accessory (or say the case is full), with the HG messages.
static void FollowerTalk_StartGift(ScriptContext *ctx, FollowerTalkWork *work)
{
    FieldSystem *fieldSystem = ctx->fieldSystem;
    StringTemplate *template = *(StringTemplate **)FieldSystem_GetScriptMemberPtr(fieldSystem, SCRIPT_MANAGER_STR_TEMPLATE);
    FashionCase *fashionCase = ImageClips_GetFashionCase(SaveData_GetImageClips(fieldSystem->saveData));
    u32 accessory = work->action.accessory - 1;

    StringTemplate_SetPlayerName(template, 0, SaveData_GetTrainerInfo(fieldSystem->saveData));

    if (FashionCase_CanFitAccessoryCount(fashionCase, accessory, 1) == TRUE) {
        FashionCase_AddAccessory(fashionCase, accessory, 1);
        StringTemplate_SetContestAccessoryName(template, 1, accessory);
        ScriptMessage_Show(ctx, work->loader, TALK_MSG_ACCESSORY_PUT_AWAY, TRUE, NULL);
        Sound_PlayFanfare(SEQ_ACCE_sseq);
    } else {
        StringTemplate_SetContestAccessoryNameWithArticle(template, 1, accessory);
        ScriptMessage_Show(ctx, work->loader, TALK_MSG_ACCESSORY_FULL, TRUE, NULL);
    }
}

// Called once all steps of the current action have run (HG ov02_02250504 + the return code of ov02_0224F8FC).
static void FollowerTalk_FinishSteps(ScriptContext *ctx, FollowerTalkWork *work)
{
    FieldSystem *fieldSystem = ctx->fieldSystem;

    FollowerTalk_ApplyEffects(fieldSystem, work);

    if (work->action.yesNo) {
        Menu **menu = FieldSystem_GetScriptMemberPtr(fieldSystem, SCRIPT_MANAGER_UI_CONTROL);
        static const WindowTemplate sYesNoTemplate = {
            .bgLayer = BG_LAYER_MAIN_3,
            .tilemapLeft = 25,
            .tilemapTop = 13,
            .width = 6,
            .height = 4,
            .palette = FIELD_MESSAGE_PALETTE_INDEX,
            .baseTile = 0x21F,
        };

        LoadStandardWindowGraphics(fieldSystem->bgConfig, BG_LAYER_MAIN_3, 1024 - (18 + 12) - 9, 11, STANDARD_WINDOW_SYSTEM, HEAP_ID_FIELD1);
        *menu = Menu_MakeYesNoChoice(fieldSystem->bgConfig, &sYesNoTemplate, 1024 - (18 + 12) - 9, 11, HEAP_ID_FIELD1);
        work->menu = *menu;
        work->state = TALK_STATE_YES_NO;
    } else if (work->action.accessory != 0 && work->action.accessory <= 100) {
        FollowerTalk_StartGift(ctx, work);
        work->state = TALK_STATE_GIFT;
    } else {
        // Shiny Leaf actions (HG state 4) are unreachable here: their entries are only in HG route tables, which
        // are skipped when borrowed.
        work->state = TALK_STATE_DONE;
    }
}

static void FollowerTalk_UpdateSteps(ScriptContext *ctx, FollowerTalkWork *work)
{
    FieldSystem *fieldSystem = ctx->fieldSystem;

    if (work->stepIndex >= work->numSteps) {
        FollowerTalk_FinishSteps(ctx, work);
        return;
    }

    const FollowerTalkStep *step = &work->action.steps[work->stepIndex];

    switch (work->stepState) {
    case STEP_STATE_START:
        if (step->anim != 0) {
            FollowerTalk_StartAnim(work, step->anim);
            work->stepState = STEP_STATE_ANIM;
            break;
        }
        // fall through
    case STEP_STATE_EMOTE:
        if (step->emote >= 1 && step->emote <= TALK_NUM_EMOTES && FollowerTalk_EmoteAllowed(fieldSystem, work->follower)) {
            FollowerTalk_StartEmote(fieldSystem, work, step->emote);
            work->stepState = STEP_STATE_EMOTE_RUNNING;
            break;
        }
        // fall through
    case STEP_STATE_MESSAGE:
        if (step->msg != 0) {
            ScriptMessage_Show(ctx, work->loader, step->msg - 1, TRUE, NULL);
            work->stepState = STEP_STATE_PRINTING;
            break;
        }
        // fall through
    case STEP_STATE_WAIT_START:
        work->waitCounter = 0;

        if (step->wait != 0) {
            work->stepState = STEP_STATE_WAIT;
            break;
        }
        // fall through
    case STEP_STATE_NEXT:
        work->stepIndex++;
        work->stepState = STEP_STATE_START;
        break;
    case STEP_STATE_ANIM:
        if (FollowerTalk_UpdateAnim(work)) {
            work->stepState = STEP_STATE_EMOTE;
        }
        break;
    case STEP_STATE_EMOTE_RUNNING:
        if (FollowerTalk_UpdateEmote(fieldSystem, work)) {
            work->stepState = STEP_STATE_MESSAGE;
        }
        break;
    case STEP_STATE_PRINTING:
        if (FollowerTalk_MessageFinished(fieldSystem) == FALSE) {
            break;
        }

        // The last step's message stays up for the Yes/No menu; every other message waits for A/B.
        if (work->stepIndex + 1 >= work->numSteps && work->action.yesNo) {
            work->stepState = STEP_STATE_WAIT_START;
        } else if (FollowerTalk_ABPressed()) {
            FollowerTalk_CloseMessage(fieldSystem);
            work->stepState = STEP_STATE_WAIT_START;
        }
        break;
    case STEP_STATE_WAIT:
        if (work->waitCounter >= step->wait) {
            work->stepIndex++;
            work->stepState = STEP_STATE_START;
        } else {
            work->waitCounter++;
        }
        break;
    }
}

static void FollowerTalk_UpdateYesNo(ScriptContext *ctx, FollowerTalkWork *work)
{
    u32 result = Menu_ProcessInputAndHandleExit(work->menu, HEAP_ID_FIELD1);

    if (result == MENU_NOTHING_CHOSEN) {
        return;
    }

    work->menu = NULL;
    FollowerTalk_CloseMessage(ctx->fieldSystem);

    u16 next = (result == MENU_YES) ? work->action.yesAction : work->action.noAction;

    if (next == 0) {
        work->state = TALK_STATE_DONE;
        return;
    }

    FollowerTalk_LoadAction(work, next);
    work->state = TALK_STATE_STEPS;
}

static void FollowerTalk_UpdateGift(ScriptContext *ctx, FollowerTalkWork *work)
{
    if (FollowerTalk_MessageFinished(ctx->fieldSystem) && Sound_IsBGMPausedByFanfare() == FALSE && FollowerTalk_ABPressed()) {
        FollowerTalk_CloseMessage(ctx->fieldSystem);
        work->state = TALK_STATE_DONE;
    }
}

static void FollowerTalk_End(FieldSystem *fieldSystem)
{
    FollowerTalkWork *work = sTalkWork;

    if (work != NULL) {
        if (work->emote != NULL) {
            FollowerTalk_FreeEmote(fieldSystem, work);
        }

        if (work->animRunning) {
            MapObject_Face(work->follower, work->savedFacing);
        }

        FollowerTalk_CloseMessage(fieldSystem);
        MessageLoader_Free(work->loader);
        Heap_Free(work);
        sTalkWork = NULL;
    }

    sTalkOffset.x = 0;
    sTalkOffset.y = 0;
    sTalkOffset.z = 0;
    sTalkActive = FALSE;
}

static BOOL FollowerTalk_UpdateScript(ScriptContext *ctx)
{
    FollowerTalkWork *work = sTalkWork;

    if (work == NULL) {
        return TRUE;
    }

    switch (work->state) {
    case TALK_STATE_STEPS:
        FollowerTalk_UpdateSteps(ctx, work);
        break;
    case TALK_STATE_YES_NO:
        FollowerTalk_UpdateYesNo(ctx, work);
        break;
    case TALK_STATE_GIFT:
        FollowerTalk_UpdateGift(ctx, work);
        break;
    }

    if (work->state == TALK_STATE_DONE) {
        FollowerTalk_End(ctx->fieldSystem);
        return TRUE;
    }

    return FALSE;
}

BOOL ScrCmd_FollowerMonTalk(ScriptContext *ctx)
{
    FieldSystem *fieldSystem = ctx->fieldSystem;
    MapObject *follower = FollowerTalk_GetFollower(fieldSystem);
    Pokemon *mon = FollowerTalk_GetLeadMon(fieldSystem);
    FollowerTalkContext talkCtx;

    if (follower == NULL || mon == NULL || sTalkWork != NULL) {
        return FALSE;
    }

    FollowerTalk_BuildContext(fieldSystem, mon, follower, &talkCtx);
    u16 actionID = FollowerTalk_ChooseAction(fieldSystem, &talkCtx);

    FollowerTalkWork *work = Heap_Alloc(TALK_HEAP_ID, sizeof(FollowerTalkWork));
    memset(work, 0, sizeof(FollowerTalkWork));

    work->follower = follower;
    work->species = Pokemon_GetValue(mon, MON_DATA_SPECIES, NULL);
    work->form = Pokemon_GetValue(mon, MON_DATA_FORM, NULL);
    work->loader = MessageLoader_Init(MSG_LOADER_LOAD_ON_DEMAND, NARC_INDEX_MSGDATA__PL_MSG, TEXT_BANK_FOLLOWER_TALK, TALK_HEAP_ID);
    work->state = TALK_STATE_STEPS;

    FollowerTalk_SetPrinterTemplate(fieldSystem, mon);
    FollowerTalk_LoadAction(work, actionID);

    sTalkWork = work;
    sTalkActive = TRUE;
    sTalkOffset.x = 0;
    sTalkOffset.y = 0;
    sTalkOffset.z = 0;

    ScriptContext_Pause(ctx, FollowerTalk_UpdateScript);
    return TRUE;
}
