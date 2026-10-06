#include "unk_02069BE0.h"

#include <nitro.h>
#include <string.h>

#include "generated/movement_actions.h"
#include "generated/movement_types.h"

#include "struct_decls/map_object.h"
#include "struct_decls/map_object_manager.h"

#include "field/field_system.h"
#include "overlay005/field_effect_manager.h"
#include "overlay005/ov5_021F3D00.h"

#include "follower_mon_talk.h"
#include "map_object.h"
#include "map_object_move.h"
#include "map_tile_behavior.h"
#include "overworld_anim_manager.h"
#include "player_avatar.h"
#include "terrain_collision_manager.h"
#include "trainer_encounter.h"
#include "unk_020655F4.h"

typedef struct {
    u8 unk_00;
    u8 unk_01;
    s16 unk_02;
    s16 unk_04;
    u16 unk_06;
    // ENTRY P64/P65: ledge following for the follower Pokemon (FollowerMon_TryLedgeFollow). The movement data
    // buffer is 16 bytes (sub_02062A54); this struct uses 14.
    u8 ledgePending;
    u8 ledgeDir;
    s16 ledgeStartX;
    s16 ledgeStartZ;
} UnkStruct_02069CA8;

typedef struct {
    u8 unk_00;
    u8 unk_01;
    s16 unk_02;
    s16 unk_04;
    u16 unk_06;
    MapObject *unk_08;
} UnkStruct_02069F48;

typedef struct {
    u8 unk_00;
    u8 unk_01;
    u8 unk_02;
    u8 unk_03;
    OverworldAnimManager *unk_04;
} UnkStruct_0206A0BC;

typedef struct {
    u8 unk_00;
    s8 unk_01;
    u8 unk_02;
    u8 unk_03;
} UnkStruct_0206A23C;

typedef struct {
    u32 unk_00;
    int unk_04;
    int unk_08;
} UnkStruct_0206A47C;

static int sub_02069CA8(MapObject *mapObj, UnkStruct_02069CA8 *param1);
static void sub_02069CD4(MapObject *mapObj, UnkStruct_02069CA8 *param1);
static int sub_02069CFC(MapObject *mapObj, UnkStruct_02069CA8 *param1);
static void sub_02069D30(MapObject *mapObj, UnkStruct_02069CA8 *param1);
static u32 sub_02069D50(MapObject *mapObj);
static int sub_02069D8C(MapObject *mapObj);
static int FollowerMon_Follow(MapObject *mapObj, UnkStruct_02069CA8 *param1);
static int sub_02069F48(MapObject *mapObj, UnkStruct_02069F48 *param1);
static void sub_02069FC4(MapObject *mapObj, UnkStruct_02069F48 *param1, MapObject *param2);
static int sub_02069FE8(MapObject *mapObj, UnkStruct_02069F48 *param1);
static int sub_0206A034(MapObject *mapObj, UnkStruct_02069F48 *param1);
static int sub_0206A630(MapObject *mapObj, UnkStruct_0206A47C *param1, int param2);

int (*const Unk_020EF8D4[])(MapObject *, UnkStruct_02069CA8 *);
int (*const Unk_020EF8EC[])(MapObject *, UnkStruct_02069F48 *);
int (*const Unk_020EF8DC[])(MapObject *, UnkStruct_0206A0BC *);

void sub_02069BE0(MapObject *mapObj)
{
    UnkStruct_02069CA8 *v0 = sub_02062A54(mapObj, (sizeof(UnkStruct_02069CA8)));

    sub_02069CA8(mapObj, v0);
    sub_02062A0C(mapObj, 0);
    sub_02062D10(mapObj);
    sub_02062D80(mapObj, 0);
}

void sub_02069C0C(MapObject *mapObj)
{
    UnkStruct_02069CA8 *v0 = sub_02062A78(mapObj);

    if (sub_02069CA8(mapObj, v0) == 0) {
        return;
    }

    sub_02062D80(mapObj, 0);

    while (Unk_020EF8D4[v0->unk_00](mapObj, v0) == 1) {
        (void)0;
    }
}

void sub_02069C44(MapObject *mapObj)
{
    return;
}

static int sub_02069C48(MapObject *mapObj, UnkStruct_02069CA8 *param1)
{
    sub_02062D10(mapObj);
    MapObject_SetEndMovementOff(mapObj);

    if (sub_02069CFC(mapObj, param1) == 1) {
        sub_02069D30(mapObj, param1);

        if (FollowerMon_Follow(mapObj, param1) == 1) {
            sub_02062D04(mapObj);
            param1->unk_00++;
            return TRUE;
        }
    }

    return FALSE;
}

static int sub_02069C8C(MapObject *mapObj, UnkStruct_02069CA8 *param1)
{
    if (sub_020658DC(mapObj) == 1) {
        sub_02062D10(mapObj);
        param1->unk_00 = 0;
    }

    return 0;
}

static int (*const Unk_020EF8D4[])(MapObject *, UnkStruct_02069CA8 *) = {
    sub_02069C48,
    sub_02069C8C
};

static int sub_02069CA8(MapObject *mapObj, UnkStruct_02069CA8 *param1)
{
    const MapObjectManager *v0 = MapObject_MapObjectManager(mapObj);
    MapObject *v1 = MapObjectMan_GetPlayerMapObject(v0);

    if (v1 == NULL) {
        param1->unk_01 = 0;
        return 0;
    }

    if (param1->unk_01 == 0) {
        sub_02069CD4(mapObj, param1);
    }

    return 1;
}

static void sub_02069CD4(MapObject *mapObj, UnkStruct_02069CA8 *param1)
{
    FieldSystem *fieldSystem = MapObject_FieldSystem(mapObj);
    PlayerAvatar *playerAvatar = FieldSystem_GetPlayerAvatar(fieldSystem);

    param1->unk_01 = 1;
    param1->unk_02 = PlayerAvatar_GetXPos(playerAvatar);
    param1->unk_04 = PlayerAvatar_GetZPos(playerAvatar);
    param1->unk_06 = 0xff;
}

static int sub_02069CFC(MapObject *mapObj, UnkStruct_02069CA8 *param1)
{
    FieldSystem *fieldSystem = MapObject_FieldSystem(mapObj);
    PlayerAvatar *playerAvatar = FieldSystem_GetPlayerAvatar(fieldSystem);

    if (playerAvatar != NULL) {
        int v2 = PlayerAvatar_GetXPos(playerAvatar);
        int v3 = PlayerAvatar_GetZPos(playerAvatar);

        if ((v2 != param1->unk_02) || (v3 != param1->unk_04)) {
            return 1;
        }
    }

    return 0;
}

static void sub_02069D30(MapObject *mapObj, UnkStruct_02069CA8 *param1)
{
    FieldSystem *fieldSystem = MapObject_FieldSystem(mapObj);
    PlayerAvatar *playerAvatar = FieldSystem_GetPlayerAvatar(fieldSystem);

    param1->unk_02 = PlayerAvatar_GetXPos(playerAvatar);
    param1->unk_04 = PlayerAvatar_GetZPos(playerAvatar);
}

static u32 sub_02069D50(MapObject *mapObj)
{
    u32 v0;
    FieldSystem *fieldSystem = MapObject_FieldSystem(mapObj);
    PlayerAvatar *playerAvatar = FieldSystem_GetPlayerAvatar(fieldSystem);

    v0 = PlayerAvatar_GetMovementAction(playerAvatar);

    switch (v0) {
    case 0x58:
        v0 = 0x10;
        break;
    case 0x59:
        v0 = 0x11;
        break;
    case 0x5a:
        v0 = 0x12;
        break;
    case 0x5b:
        v0 = 0x13;
        break;
    }

    return v0;
}

// ENTRY P26/P27 (2026-10-01) added speed-dependent targeting here (first a single extra lookback step,
// then a multi-entry position-history ring buffer) to make the follower's ACTUAL logical tile-by-tile
// pathing lag further behind while the player runs. REVERTED in ENTRY P28 - the user reported it
// overshot badly ("the mon now runs an entire square backward") and suggested a purely visual solution
// instead of manipulating real pathing; see FollowerMon_RefreshVisualLag (src/follower_mon.c) for the
// replacement, which offsets only the RENDERED position via MapObject_SetSpriteJumpOffset and never
// touches this movement/pathing logic at all. This function is back to its original, unmodified form -
// always targets PlayerAvatar_XPosPrev/ZPosPrev, a fixed 1-tile lag regardless of player speed.
// ENTRY P87: the step the player is taking right now, read from the player's own MapObject. Scripted movement
// (ApplyMovement LOCALID_PLAYER in cutscenes) sets only that, not PlayerAvatar's recorded action, which still holds
// whatever the player last did with the D-pad. For D-pad movement both are the same value (PlayerAvatar_SetMovement).
static u32 FollowerMon_GetPlayerStepAction(PlayerAvatar *playerAvatar, u32 fallback)
{
    u32 action = MapObject_GetMovementAction(PlayerAvatar_GetMapObject(playerAvatar));

    if ((action >= MOVEMENT_ACTION_WALK_SLOWER_NORTH && action <= MOVEMENT_ACTION_WALK_FASTER_EAST)
        || (action >= MOVEMENT_ACTION_WALK_FASTEST_NORTH && action <= MOVEMENT_ACTION_WALK_FASTEST_EAST)) {
        return action;
    }

    if (action >= MOVEMENT_ACTION_RUN_NORTH && action <= MOVEMENT_ACTION_RUN_EAST) {
        return action - MOVEMENT_ACTION_RUN_NORTH + MOVEMENT_ACTION_WALK_FAST_NORTH; // same mapping as sub_02069D50
    }

    return fallback;
}

static int sub_02069D8C(MapObject *mapObj)
{
    FieldSystem *fieldSystem = MapObject_FieldSystem(mapObj);
    PlayerAvatar *playerAvatar = FieldSystem_GetPlayerAvatar(fieldSystem);
    int xCur = MapObject_GetX(mapObj);
    int zCur = MapObject_GetZ(mapObj);
    int xPrev = PlayerAvatar_XPosPrev(playerAvatar);
    int zPrev = PlayerAvatar_ZPosPrev(playerAvatar);

    if (xCur != xPrev || zCur != zPrev) {
        u32 v6 = sub_02069D50(mapObj);

        // ENTRY P87, follower Pokemon only (partner trainers keep vanilla logic):
        if (MapObject_GetScript(mapObj) == FOLLOWER_MON_TALK_SCRIPT_ID) {
            int dx = xPrev - xCur;
            int dz = zPrev - zCur;

            // More than one tile from where it should be (the player was moved while it was paused, or teleported
            // within the map): walking there would cut through walls one step per player step, so it goes there.
            if ((dx < 0 ? -dx : dx) + (dz < 0 ? -dz : dz) > 1) {
                MapObject *playerObj = PlayerAvatar_GetMapObject(playerAvatar);

                MapObject_SetPosDirFromCoords(mapObj, xPrev, MapObject_GetY(playerObj), zPrev, PlayerAvatar_GetFacingDir(playerAvatar));
                return 0;
            }

            // Cutscenes: step at the speed the player is actually being moved at.
            v6 = FollowerMon_GetPlayerStepAction(playerAvatar, v6);
        }

        int dir = GetDirectionBetweenPoints(xCur, zCur, xPrev, zPrev);

        v6 = MovementAction_TurnActionTowardsDir(dir, v6);
        sub_02065668(mapObj, v6);

        return 1;
    }

    return 0;
}

// ENTRY P64/P65: LEDGES. Vanilla Platinum (sub_02069D8C) gives the follower the player's own movement action aimed
// at the player's previous tile, so a ledge jump (MOVEMENT_ACTION_JUMP_FAR_*, 2 tiles) was copied immediately: the
// follower jumped 2 tiles from behind the player over flat ground and landed on the ledge (or 2 tiles sideways if
// it was not straight behind), and its pathing stayed off afterwards.
// Now (ENTRY P65, the behavior the user described): when the player jumps, the follower walks one tile (WALK_SLOW,
// the 16 frames the jump takes, as HG's follower does) to the tile the player jumped from and waits at the top of
// the ledge. When the player steps off the landing tile, the follower leaps the ledge itself (JUMP_FAR) onto that
// landing tile - the tile the player just left - so it ends up one tile behind the player again. If the player's
// next move is another ledge jump from the landing tile, the follower leaps onto the landing tile and waits at the
// top of the new ledge.
// ENTRY P64 got this wrong in two ways: it decided from the player's PREVIOUS tile, which MovementAction_Jump_Step1
// resets to the landing tile when the jump ends (MapObject_UpdateCoords), so the catch-up fired as soon as the
// player landed and sent the follower onto the player's own tile; and it crossed the ledge by walking.
// Only for the follower Pokemon (identified by its talk script, ENTRY P63) - partner trainers keep vanilla logic.
#define FOLLOWER_LEDGE_NOT_HANDLED -1

static BOOL FollowerMon_IsActionInRow(u32 action, u32 northAction)
{
    return action >= northAction && action <= northAction + 3;
}

static BOOL FollowerMon_IsLedgeTile(FieldSystem *fieldSystem, int x, int z, int dir)
{
    u8 behavior = TerrainCollisionManager_GetTileBehavior(fieldSystem, x, z);

    switch (dir) {
    case DIR_NORTH:
        return TileBehavior_IsJumpNorth(behavior);
    case DIR_SOUTH:
        return TileBehavior_IsJumpSouth(behavior);
    case DIR_WEST:
        return TileBehavior_IsJumpWest(behavior);
    case DIR_EAST:
        return TileBehavior_IsJumpEast(behavior);
    }

    return FALSE;
}

// ENTRY P66: HeartGold's follower leaps fast enough to be one tile behind the player as soon as the player is one
// tile past the landing spot, walking or running. Pick the 2-tile leap that lasts exactly one player step.
static u32 FollowerMon_GetLeapAction(u32 playerAction, int dir)
{
    u32 northAction;

    if (FollowerMon_IsActionInRow(playerAction, MOVEMENT_ACTION_WALK_SLOWER_NORTH) || FollowerMon_IsActionInRow(playerAction, MOVEMENT_ACTION_WALK_SLOW_NORTH)) {
        northAction = MOVEMENT_ACTION_JUMP_FAR_NORTH; // 16 frames, a slow step
    } else if (FollowerMon_IsActionInRow(playerAction, MOVEMENT_ACTION_WALK_NORMAL_NORTH)) {
        northAction = MOVEMENT_ACTION_FOLLOWER_LEAP_NORMAL_NORTH; // 8 frames
    } else if (FollowerMon_IsActionInRow(playerAction, MOVEMENT_ACTION_WALK_FASTER_NORTH)) {
        northAction = MOVEMENT_ACTION_FOLLOWER_LEAP_FASTER_NORTH; // 2 frames (cycling)
    } else {
        northAction = MOVEMENT_ACTION_FOLLOWER_LEAP_FAST_NORTH; // 4 frames (running; also any other step)
    }

    return northAction + dir;
}

static void FollowerMon_SetLedgePending(UnkStruct_02069CA8 *data, int dir, int startX, int startZ)
{
    data->ledgePending = TRUE;
    data->ledgeDir = dir;
    data->ledgeStartX = startX;
    data->ledgeStartZ = startZ;
}

static int FollowerMon_TryLedgeFollow(MapObject *mapObj, UnkStruct_02069CA8 *data)
{
    if (MapObject_GetScript(mapObj) != FOLLOWER_MON_TALK_SCRIPT_ID) {
        return FOLLOWER_LEDGE_NOT_HANDLED;
    }

    FieldSystem *fieldSystem = MapObject_FieldSystem(mapObj);
    PlayerAvatar *playerAvatar = FieldSystem_GetPlayerAvatar(fieldSystem);
    int xCur = MapObject_GetX(mapObj);
    int zCur = MapObject_GetZ(mapObj);
    int playerX = PlayerAvatar_GetXPos(playerAvatar);
    int playerZ = PlayerAvatar_GetZPos(playerAvatar);
    int xPrev = PlayerAvatar_XPosPrev(playerAvatar);
    int zPrev = PlayerAvatar_ZPosPrev(playerAvatar);
    u32 playerAction = sub_02069D50(mapObj);
    BOOL playerJumping = FollowerMon_IsActionInRow(playerAction, MOVEMENT_ACTION_JUMP_FAR_NORTH);

    if (data->ledgePending) {
        int dir = data->ledgeDir;
        int landX = data->ledgeStartX + MapObject_GetDxFromDir(dir) * 2;
        int landZ = data->ledgeStartZ + MapObject_GetDzFromDir(dir) * 2;

        // The player is still on (or just arriving at, via the jump's midpoint coordinate change) the landing
        // tile: keep waiting at the top of the ledge.
        if (playerX == landX && playerZ == landZ) {
            return 0;
        }

        if (xCur == data->ledgeStartX && zCur == data->ledgeStartZ) {
            sub_02065668(mapObj, FollowerMon_GetLeapAction(playerAction, dir));

            // The player's next move was another ledge jump, starting from the landing tile: wait at its top.
            if (playerJumping && xPrev == landX && zPrev == landZ) {
                FollowerMon_SetLedgePending(data, PlayerAvatar_GetFacingDir(playerAvatar), landX, landZ);
            } else {
                data->ledgePending = FALSE;
            }

            return 1;
        }

        // Anything else (warp, scripted movement...) - drop the pending jump and follow normally.
        data->ledgePending = FALSE;
    }

    if (playerJumping) {
        int dir = PlayerAvatar_GetFacingDir(playerAvatar);
        int startX, startZ;

        // Find the tile the jump started from using the ledge tile, which works at every point of the jump: the
        // player is on the ledge tile during the first half, past it (landing tile) from the midpoint on.
        if (FollowerMon_IsLedgeTile(fieldSystem, playerX, playerZ, dir)) {
            startX = playerX - MapObject_GetDxFromDir(dir);
            startZ = playerZ - MapObject_GetDzFromDir(dir);
        } else if (FollowerMon_IsLedgeTile(fieldSystem, playerX - MapObject_GetDxFromDir(dir), playerZ - MapObject_GetDzFromDir(dir), dir)) {
            startX = playerX - MapObject_GetDxFromDir(dir) * 2;
            startZ = playerZ - MapObject_GetDzFromDir(dir) * 2;
        } else {
            // A jump without a ledge tile (e.g. the Eterna Gym clock hands): seen at its start.
            startX = xPrev;
            startZ = zPrev;
        }

        FollowerMon_SetLedgePending(data, dir, startX, startZ);

        if (xCur == startX && zCur == startZ) {
            return 0;
        }

        // HG: one tile at WALK_SLOW toward the tile the player jumped from.
        int moveDir = GetDirectionBetweenPoints(xCur, zCur, startX, startZ);
        sub_02065668(mapObj, MovementAction_TurnActionTowardsDir(moveDir, MOVEMENT_ACTION_WALK_SLOW_NORTH));
        return 1;
    }

    return FOLLOWER_LEDGE_NOT_HANDLED;
}

// ENTRY P87: TRUE while a follow-the-player object is taking a step, or has one to take (the player moved since the
// object last looked - e.g. the same frame a trainer spots the player). Used by ScrCmd_LockAll/LockLastTalked.
BOOL FollowerMon_IsStepPending(MapObject *mapObj)
{
    if (MapObject_IsMoving(mapObj)) {
        return TRUE;
    }

    if (MapObject_GetMovementType(mapObj) != MOVEMENT_TYPE_FOLLOW_PLAYER) {
        return FALSE;
    }

    UnkStruct_02069CA8 *data = sub_02062A78(mapObj);

    if (data->unk_01 == 0) {
        return FALSE;
    }

    return sub_02069CFC(mapObj, data) == 1;
}

static int FollowerMon_Follow(MapObject *mapObj, UnkStruct_02069CA8 *param1)
{
    int result = FollowerMon_TryLedgeFollow(mapObj, param1);

    if (result != FOLLOWER_LEDGE_NOT_HANDLED) {
        return result;
    }

    return sub_02069D8C(mapObj);
}

void sub_02069DF4(MapObject *mapObj)
{
    UnkStruct_02069F48 *v0 = sub_02062A54(mapObj, (sizeof(UnkStruct_02069F48)));

    sub_02069F48(mapObj, v0);
    sub_02062A0C(mapObj, 0x0);
    sub_02062D10(mapObj);

    v0->unk_01 = 0;
}

void sub_02069E1C(MapObject *mapObj)
{
    UnkStruct_02069F48 *v0 = sub_02062A78(mapObj);

    if (sub_02069F48(mapObj, v0) == 0) {
        return;
    }

    while (Unk_020EF8EC[v0->unk_00](mapObj, v0) == 1) {
        (void)0;
    }
}

void sub_02069E4C(MapObject *mapObj)
{
    return;
}

void sub_02069E50(MapObject *mapObj)
{
    UnkStruct_02069F48 *v0 = sub_02062A78(mapObj);
    v0->unk_01 = 0;
}

static int sub_02069E5C(MapObject *mapObj, UnkStruct_02069F48 *param1)
{
    sub_02062D10(mapObj);
    MapObject_SetEndMovementOff(mapObj);

    if (sub_02069FE8(mapObj, param1) == 1) {
        if (sub_0206A034(mapObj, param1) == 1) {
            sub_02062D04(mapObj);
            param1->unk_00++;
            return 1;
        }
    }

    return 0;
}

static int sub_02069E98(MapObject *mapObj, UnkStruct_02069F48 *param1)
{
    if (sub_020658DC(mapObj) == 0) {
        return 0;
    }

    sub_02062D10(mapObj);
    param1->unk_00 = 0;
    return 0;
}

static int (*const Unk_020EF8EC[])(MapObject *, UnkStruct_02069F48 *) = {
    sub_02069E5C,
    sub_02069E98
};

MapObject *sub_02069EB8(MapObject *mapObj)
{
    int v0 = 0;
    int v1 = MapObject_GetTrainerType(mapObj);
    enum MapHeaderID mapHeaderID = MapObject_GetMapHeaderID(mapObj);
    u32 v3 = MapObject_GetTrainerID(mapObj);
    const MapObjectManager *mapObjMan = MapObject_MapObjectManager(mapObj);
    MapObject *v5;

    switch (v1) {
    case 0x1:
    case 0x2:
    case 0x3:
    case 0x4:
    case 0x5:
    case 0x6:
    case 0x7:
    case 0x8:
        while (MapObjectMan_FindObjectWithStatus(mapObjMan, &v5, &v0, (1 << 0)) == 1) {
            if ((mapObj != v5) && (MapObject_GetMapHeaderID(v5) == mapHeaderID)) {
                if (MapObject_GetTrainerID(v5) == v3) {
                    return v5;
                }
            }
        }
    }

    return NULL;
}

static int sub_02069F48(MapObject *mapObj, UnkStruct_02069F48 *param1)
{
    int v0;
    enum MapHeaderID v1;
    u32 v2;
    MapObject *v3;
    const MapObjectManager *mapObjMan = MapObject_MapObjectManager(mapObj);

    v0 = 0;
    v1 = MapObject_GetMapHeaderID(mapObj);
    v2 = MapObject_GetTrainerID(mapObj);

    while (MapObjectMan_FindObjectWithStatus(mapObjMan, &v3, &v0, (1 << 0)) == 1) {
        if ((mapObj != v3) && (MapObject_GetMapHeaderID(v3) == v1) && (MapObject_GetTrainerID(v3) == v2)) {
            if (param1->unk_01 == 0) {
                sub_02069FC4(mapObj, param1, v3);
            }

            return 1;
        }
    }

    param1->unk_01 = 0;
    return 0;
}

static void sub_02069FC4(MapObject *mapObj, UnkStruct_02069F48 *param1, MapObject *param2)
{
    param1->unk_01 = 1;
    param1->unk_02 = MapObject_GetX(param2);
    param1->unk_04 = MapObject_GetZ(param2);
    param1->unk_06 = 0xff;
    param1->unk_08 = param2;
}

static int sub_02069FE8(MapObject *mapObj, UnkStruct_02069F48 *param1)
{
    MapObject *v0 = param1->unk_08;
    int v1 = MapObject_GetX(mapObj);
    int v2 = MapObject_GetZ(mapObj);
    int v3 = MapObject_GetXPrev(v0);
    int v4 = MapObject_GetZPrev(v0);

    if (((v1 != v3) || (v2 != v4)) && ((MapObject_IsMoving(v0) == 1) || (!MapObject_CheckStatus(v0, (MAP_OBJ_STATUS_11 | MAP_OBJ_STATUS_12 | MAP_OBJ_STATUS_PAUSE_MOVEMENT))))) {
        return 1;
    }

    return 0;
}

static int sub_0206A034(MapObject *mapObj, UnkStruct_02069F48 *param1)
{
    int v0 = MapObject_GetX(mapObj);
    int v1 = MapObject_GetZ(mapObj);
    int v2 = MapObject_GetX(param1->unk_08);
    int v3 = MapObject_GetZ(param1->unk_08);
    int v4 = MapObject_GetXPrev(param1->unk_08);
    int v5 = MapObject_GetZPrev(param1->unk_08);
    int v6;

    if ((v0 == v2) && (v1 == v3)) {
        return 0;
    }

    v6 = GetDirectionBetweenPoints(v0, v1, v4, v5);
    v0 += MapObject_GetDxFromDir(v6);
    v1 += MapObject_GetDzFromDir(v6);

    if ((v0 != v2) || (v1 != v3)) {
        u32 v7 = 0xc;

        v7 = MovementAction_TurnActionTowardsDir(v6, v7);
        sub_02065668(mapObj, v7);
        return 1;
    }

    return 0;
}

static void sub_0206A0BC(MapObject *mapObj, int param1)
{
    UnkStruct_0206A0BC *v0 = sub_02062A54(mapObj, (sizeof(UnkStruct_0206A0BC)));

    v0->unk_01 = param1;

    sub_02062A0C(mapObj, 0x0);
    sub_02062D10(mapObj);
    MapObject_SetStatusFlagOn(mapObj, MAP_OBJ_STATUS_HIDE_SHADOW);

    VecFx32 v1 = { 0, FX32_ONE * -32, 0 };

    MapObject_SetSpriteJumpOffset(mapObj, &v1);
}

void sub_0206A104(MapObject *mapObj)
{
    sub_0206A0BC(mapObj, 0);
}

void sub_0206A110(MapObject *mapObj)
{
    sub_0206A0BC(mapObj, 1);
}

void sub_0206A11C(MapObject *mapObj)
{
    sub_0206A0BC(mapObj, 2);
}

void sub_0206A128(MapObject *mapObj)
{
    sub_0206A0BC(mapObj, 3);
}

void sub_0206A134(MapObject *mapObj)
{
    UnkStruct_0206A0BC *v0 = sub_02062A78(mapObj);

    while (Unk_020EF8DC[v0->unk_00](mapObj, v0) == 1) {
        (void)0;
    }
}

void sub_0206A158(MapObject *mapObj)
{
    OverworldAnimManager *v0 = sub_0206A224(mapObj);

    if (v0 != NULL) {
        FieldEffectManager_FinishAnimManager(v0);
    }
}

void sub_0206A168(MapObject *mapObj)
{
    UnkStruct_0206A0BC *v0 = sub_02062A78(mapObj);

    v0->unk_00 = 0;
    sub_0206A218(mapObj, NULL);

    if (v0->unk_02 == 0) {
        VecFx32 v1 = { 0, (FX32_ONE * -32), 0 };

        MapObject_SetSpriteJumpOffset(mapObj, &v1);
        MapObject_SetStatusFlagOn(mapObj, MAP_OBJ_STATUS_HIDE_SHADOW);
    }
}

static int sub_0206A1AC(MapObject *mapObj, UnkStruct_0206A0BC *param1)
{
    if (param1->unk_02 == 0) {
        OverworldAnimManager *v0 = ov5_021F3D90(mapObj, param1->unk_01);

        sub_0206A218(mapObj, v0);
    }

    sub_02062D10(mapObj);
    MapObject_SetEndMovementOff(mapObj);
    param1->unk_00++;

    return 0;
}

static int sub_0206A1DC(MapObject *mapObj, UnkStruct_0206A0BC *param1)
{
    if (param1->unk_02 == 0) {
        OverworldAnimManager *v0 = sub_0206A224(mapObj);

        if (v0 == NULL) {
            if (sub_02062DFC(mapObj) == 1) {
                v0 = ov5_021F3D90(mapObj, param1->unk_01);
                sub_0206A218(mapObj, v0);
            }
        }

        MapObject_SetStatusFlagOn(mapObj, MAP_OBJ_STATUS_HIDE_SHADOW);
    }

    return 0;
}

static int (*const Unk_020EF8DC[])(MapObject *, UnkStruct_0206A0BC *) = {
    sub_0206A1AC,
    sub_0206A1DC
};

void sub_0206A218(MapObject *mapObj, OverworldAnimManager *param1)
{
    UnkStruct_0206A0BC *v0 = sub_02062A78(mapObj);
    v0->unk_04 = param1;
}

OverworldAnimManager *sub_0206A224(MapObject *mapObj)
{
    UnkStruct_0206A0BC *v0 = sub_02062A78(mapObj);
    return v0->unk_04;
}

void sub_0206A230(MapObject *mapObj)
{
    UnkStruct_0206A0BC *v0 = sub_02062A78(mapObj);
    v0->unk_02 = 1;
}

static int (*const Unk_020EF918[5])(MapObject *, UnkStruct_0206A23C *);

static void sub_0206A23C(MapObject *mapObj, int param1, u32 param2)
{
    UnkStruct_0206A23C *v0 = sub_02062A54(mapObj, (sizeof(UnkStruct_0206A23C)));

    v0->unk_01 = -1;
    v0->unk_02 = param2;

    MapObject_TryFace(mapObj, 0);
}

void sub_0206A25C(MapObject *mapObj)
{
    sub_0206A23C(mapObj, 0, 0);
}

void sub_0206A268(MapObject *mapObj)
{
    sub_0206A23C(mapObj, 1, 0);
}

void sub_0206A274(MapObject *mapObj)
{
    sub_0206A23C(mapObj, 2, 0);
}

void sub_0206A280(MapObject *mapObj)
{
    sub_0206A23C(mapObj, 3, 0);
}

void sub_0206A28C(MapObject *mapObj)
{
    sub_0206A23C(mapObj, 0, 1);
}

void sub_0206A298(MapObject *mapObj)
{
    sub_0206A23C(mapObj, 1, 1);
}

void sub_0206A2A4(MapObject *mapObj)
{
    sub_0206A23C(mapObj, 2, 1);
}

void sub_0206A2B0(MapObject *mapObj)
{
    sub_0206A23C(mapObj, 3, 1);
}

void sub_0206A2BC(MapObject *mapObj)
{
    UnkStruct_0206A23C *v0 = sub_02062A78(mapObj);

    while (Unk_020EF918[v0->unk_00](mapObj, v0) == 1) {
        (void)0;
    }
}

static int sub_0206A2E0(MapObject *mapObj, UnkStruct_0206A23C *param1)
{
    int v0 = MapObject_GetFacingDir(mapObj);

    v0 = MovementAction_TurnActionTowardsDir(v0, MOVEMENT_ACTION_FACE_NORTH);

    sub_02065668(mapObj, v0);
    sub_02062D10(mapObj);
    MapObject_SetEndMovementOff(mapObj);

    param1->unk_00 = 1;
    return 1;
}

static int sub_0206A30C(MapObject *mapObj, UnkStruct_0206A23C *param1)
{
    if (sub_020658DC(mapObj) == 1) {
        param1->unk_00 = 2;
        return 1;
    }

    return 0;
}

static int sub_0206A324(MapObject *mapObj, UnkStruct_0206A23C *param1)
{
    if (param1->unk_01 == -1) {
        FieldSystem *fieldSystem = MapObject_FieldSystem(mapObj);

        param1->unk_01 = PlayerAvatar_GetFacingDir(fieldSystem->playerAvatar);
    }

    sub_02062D10(mapObj);
    MapObject_SetEndMovementOff(mapObj);

    param1->unk_00 = 3;
    return 1;
}

static u32 sub_0206A354(MapObject *mapObj, int param1)
{
    u32 v0 = MapObject_GetTileBehaviorFromDir(mapObj, param1);

    if (TileBehavior_IsVeryTallGrass(v0) == 0) {
        v0 = (1 << 1);
    }

    v0 |= sub_02063EBC(mapObj, param1);
    return v0;
}

static void sub_0206A37C(MapObject *mapObj, int param1, int param2, u32 param3)
{
    u32 v0;

    if (param3 == 0) {
        v0 = sub_02063EBC(mapObj, param1);
    } else {
        v0 = sub_0206A354(mapObj, param1);
    }

    if (v0 != 0) {
        param2 = MovementAction_TurnActionTowardsDir(param1, MOVEMENT_ACTION_FACE_NORTH);
    } else {
        param2 = MovementAction_TurnActionTowardsDir(param1, param2);
        sub_02062D04(mapObj);
    }

    sub_02065668(mapObj, param2);
}

static int sub_0206A3BC(MapObject *mapObj, UnkStruct_0206A23C *param1)
{
    int v0;
    FieldSystem *fieldSystem = MapObject_FieldSystem(mapObj);
    int v2 = PlayerAvatar_GetFacingDir(fieldSystem->playerAvatar);
    u32 v3 = PlayerAvatar_GetMovementActionSpeed(fieldSystem->playerAvatar);

    switch (v3) {
    case PLAYER_ACTION_SPEED_NONE:
    case PLAYER_ACTION_SPEED_NOT_MOVING:
        v0 = MovementAction_TurnActionTowardsDir(v2, MOVEMENT_ACTION_FACE_NORTH);
        sub_02065668(mapObj, v0);
        break;
    case PLAYER_ACTION_SPEED_SLOWER:
        sub_0206A37C(mapObj, v2, MOVEMENT_ACTION_WALK_SLOWER_NORTH, param1->unk_02);
        break;
    case PLAYER_ACTION_SPEED_SLOW:
        sub_0206A37C(mapObj, v2, MOVEMENT_ACTION_WALK_SLOW_NORTH, param1->unk_02);
        break;
    case PLAYER_ACTION_SPEED_NORMAL:
        sub_0206A37C(mapObj, v2, MOVEMENT_ACTION_WALK_NORMAL_NORTH, param1->unk_02);
        break;
    case PLAYER_ACTION_SPEED_FAST:
        sub_0206A37C(mapObj, v2, MOVEMENT_ACTION_WALK_FAST_NORTH, param1->unk_02);
        break;
    case PLAYER_ACTION_SPEED_FASTER:
        sub_0206A37C(mapObj, v2, MOVEMENT_ACTION_WALK_FASTER_NORTH, param1->unk_02);
        break;
    }

    param1->unk_00 = 4;
    return 1;
}

static int sub_0206A454(MapObject *mapObj, UnkStruct_0206A23C *param1)
{
    if (sub_020658DC(mapObj) == 0) {
        return 0;
    }

    sub_02062D10(mapObj);
    MapObject_SetEndMovementOff(mapObj);

    param1->unk_00 = 2;
    return 0;
}

static int (*const Unk_020EF918[5])(MapObject *, UnkStruct_0206A23C *) = {
    sub_0206A2E0,
    sub_0206A30C,
    sub_0206A324,
    sub_0206A3BC,
    sub_0206A454
};

static int (*const Unk_020EF90C[3])(MapObject *, UnkStruct_0206A47C *);

static void sub_0206A47C(MapObject *mapObj, int param1, int param2)
{
    UnkStruct_0206A47C *v0 = sub_02062A54(mapObj, (sizeof(UnkStruct_0206A47C)));

    v0->unk_04 = param1;
    v0->unk_08 = param2;
}

void sub_0206A490(MapObject *mapObj)
{
    sub_0206A47C(mapObj, 0, 0);
}

void sub_0206A49C(MapObject *mapObj)
{
    sub_0206A47C(mapObj, 1, 1);
}

void sub_0206A4A8(MapObject *mapObj)
{
    sub_0206A47C(mapObj, 2, 0);
}

void sub_0206A4B4(MapObject *mapObj)
{
    sub_0206A47C(mapObj, 2, 1);
}

void sub_0206A4C0(MapObject *mapObj)
{
    UnkStruct_0206A47C *v0 = sub_02062A78(mapObj);

    while (Unk_020EF90C[v0->unk_00](mapObj, v0) == 1) {
        (void)0;
    }
}

static int sub_0206A4E4(MapObject *mapObj, UnkStruct_0206A47C *param1)
{
    sub_02062D10(mapObj);
    param1->unk_00++;
    return 1;
}

static int sub_0206A4F8(MapObject *mapObj, UnkStruct_0206A47C *param1)
{
    sub_0206A630(mapObj, param1, 0xc);
    param1->unk_00++;
    return 1;
}

static int sub_0206A50C(MapObject *mapObj, UnkStruct_0206A47C *param1)
{
    if (sub_020658DC(mapObj) == 1) {
        param1->unk_00 = 0;
        return 1;
    }

    return 0;
}

static int (*const Unk_020EF90C[3])(MapObject *, UnkStruct_0206A47C *) = {
    sub_0206A4E4,
    sub_0206A4F8,
    sub_0206A50C
};

static const int Unk_020EF92C[4][2] = {
    { -1, 0x1 },
    { 0x1, -1 },
    { 0x0, 0x0 },
    { 0x0, 0x0 }
};

static const int Unk_020EF94C[4][2] = {
    { 0x0, 0x0 },
    { 0x0, 0x0 },
    { 0x1, -1 },
    { -1, 0x1 }
};

static const int Unk_020EF98C[4][2] = {
    { -1, 0x1 },
    { 0x1, -1 },
    { 0x1, 0x1 },
    { -1, -1 }
};

static const int Unk_020EF9AC[4][2] = {
    { 0x1, 0x1 },
    { -1, -1 },
    { 0x1, -1 },
    { -1, 0x1 }
};

static const int Unk_020EF96C[4][2] = {
    { 0x2, 0x3 },
    { 0x3, 0x2 },
    { 0x1, 0x0 },
    { 0x0, 0x1 }
};

static const int Unk_020EF9CC[4][2] = {
    { 0x3, 0x2 },
    { 0x2, 0x3 },
    { 0x0, 0x1 },
    { 0x1, 0x0 }
};

static const int Unk_020EF8E4[2] = {
    0x1,
    0x0
};

static BOOL sub_0206A524(FieldSystem *fieldSystem, int param1, int param2, int param3, int param4)
{
    BOOL v0;

    param1 += Unk_020EF92C[param3][param4];
    param2 += Unk_020EF94C[param3][param4];

    v0 = TerrainCollisionManager_CheckCollision(fieldSystem, param1, param2);
    return v0;
}

static BOOL sub_0206A54C(FieldSystem *fieldSystem, int param1, int param2, int param3, int param4)
{
    BOOL v0;

    param1 += Unk_020EF98C[param3][param4];
    param2 += Unk_020EF9AC[param3][param4];

    v0 = TerrainCollisionManager_CheckCollision(fieldSystem, param1, param2);
    return v0;
}

static int sub_0206A574(MapObject *mapObj, int param1, int param2)
{
    FieldSystem *fieldSystem = MapObject_FieldSystem(mapObj);
    int v1 = MapObject_GetX(mapObj);
    int v2 = MapObject_GetZ(mapObj);
    BOOL v3 = sub_0206A524(fieldSystem, v1, v2, param1, param2);

    return v3;
}

static int sub_0206A5A4(MapObject *mapObj, int param1, int param2)
{
    FieldSystem *fieldSystem = MapObject_FieldSystem(mapObj);
    int v1 = MapObject_GetX(mapObj);
    int v2 = MapObject_GetZ(mapObj);
    BOOL v3 = sub_0206A54C(fieldSystem, v1, v2, param1, param2);

    return v3;
}

static int sub_0206A5D4(MapObject *mapObj, int param1, int param2)
{
    if (sub_0206A574(mapObj, param1, param2) == 0) {
        if (sub_0206A5A4(mapObj, param1, param2) == 0) {
            return -1;
        }

        param1 = Unk_020EF96C[param1][param2];
    }

    return param1;
}

static u32 sub_0206A60C(MapObject *mapObj, int *param1, int param2)
{
    u32 v0;

    *param1 = sub_0206A5D4(mapObj, *param1, param2);

    if (*param1 != -1) {
        v0 = sub_02063EBC(mapObj, *param1);
        return v0;
    }

    return 0;
}

static int sub_0206A630(MapObject *mapObj, UnkStruct_0206A47C *param1, int param2)
{
    u32 v0;
    int v1 = param1->unk_08;
    int dir = MapObject_GetFacingDir(mapObj);

    v0 = sub_0206A60C(mapObj, &dir, v1);

    if (dir == -1) {
        dir = MapObject_GetFacingDir(mapObj);
        param2 = MovementAction_TurnActionTowardsDir(dir, MOVEMENT_ACTION_WALK_ON_SPOT_SLOW_NORTH);
        sub_02065668(mapObj, param2);
        return 0;
    }

    if (v0 == 0) {
        param2 = MovementAction_TurnActionTowardsDir(dir, param2);
        sub_02062D04(mapObj);
        sub_02065668(mapObj, param2);
        return 1;
    }

    if ((v0 & (1 << 0)) && (param1->unk_04 == 2)) {
        dir = Direction_GetOpposite(MapObject_GetFacingDir(mapObj));
        v1 = Unk_020EF8E4[v1];
        param1->unk_08 = v1;

        v0 = sub_0206A60C(mapObj, &dir, v1);

        if (dir == -1) {
            dir = MapObject_GetFacingDir(mapObj);
            param2 = MovementAction_TurnActionTowardsDir(dir, MOVEMENT_ACTION_WALK_ON_SPOT_SLOW_NORTH);
            sub_02065668(mapObj, param2);
            return 0;
        }

        if (v0 == 0) {
            param2 = MovementAction_TurnActionTowardsDir(dir, param2);
            sub_02062D04(mapObj);
            sub_02065668(mapObj, param2);
            return 1;
        }
    }

    if (v0 & (1 << 1)) {
        dir = Unk_020EF9CC[dir][v1];
        v0 = sub_0206A60C(mapObj, &dir, v1);

        if (dir == -1) {
            dir = MapObject_GetFacingDir(mapObj);
            param2 = MovementAction_TurnActionTowardsDir(dir, MOVEMENT_ACTION_WALK_ON_SPOT_SLOW_NORTH);
            sub_02065668(mapObj, param2);
            return 0;
        }

        if (v0 == 0) {
            param2 = MovementAction_TurnActionTowardsDir(dir, param2);
            sub_02062D04(mapObj);
            sub_02065668(mapObj, param2);
            return 1;
        }
    }

    dir = MapObject_GetFacingDir(mapObj);
    param2 = MovementAction_TurnActionTowardsDir(dir, MOVEMENT_ACTION_WALK_ON_SPOT_SLOW_NORTH);

    sub_02065668(mapObj, param2);
    return 0;
}
