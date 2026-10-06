#!/usr/bin/env python3
"""Writes docs/follower_talk/HG_FOLLOWER_TALK_REFERENCE.txt: every HeartGold follower-talk message and the exact
conditions under which it occurs, decoded from HG's own data (see hg_talk_data.py for where each byte comes from)."""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from hg_talk_data import HGTalkData, PLAT  # noqa: E402

OUT = os.path.join(PLAT, "docs", "follower_talk", "HG_FOLLOWER_TALK_REFERENCE.txt")

NATURES = ["Hardy", "Lonely", "Brave", "Adamant", "Naughty", "Bold", "Docile", "Relaxed", "Impish", "Lax", "Timid",
           "Hasty", "Serious", "Jolly", "Naive", "Modest", "Mild", "Quiet", "Bashful", "Rash", "Calm", "Gentle",
           "Sassy", "Careful", "Quirky"]
NATURE_CAT = [4, 5, 4, 4, 1, 4, 3, 2, 1, 2, 5, 6, 3, 1, 1, 3, 6, 3, 5, 6, 2, 2, 1, 3, 6]  # ov02_02253AC0
TYPES = {1: "Normal", 2: "Fire", 3: "Water", 4: "Electric", 5: "Grass", 6: "Ice", 7: "Fighting", 8: "Poison",
         9: "Ground", 10: "Flying", 11: "Psychic", 12: "Bug", 13: "Rock", 14: "Ghost", 15: "Dragon", 16: "Dark",
         17: "Steel"}
EMOTES = {1: "heart", 2: "face 1 (happy)", 3: "face 2 (angry)", 4: "face 3 (hurt/sad)", 5: "face 4", 6: "face 5",
          7: "music note", 8: "?", 9: "!", 10: "sweat drop", 11: "troubled", 12: "poison", 13: "... (silence)",
          14: "lightbulb"}
HP = {1: "HP = 100%", 2: "HP 75-99%", 3: "HP 50-74%", 4: "HP 25-49%", 5: "HP < 25%"}
MOOD = {1: "mood = 127 (max)", 2: "mood 100..126", 3: "mood 50..99", 4: "mood 30..49", 5: "mood -29..29",
        6: "mood -49..-30", 7: "mood -126..-50", 8: "mood = -127 (min)", 9: "mood >= 0", 10: "mood <= -1"}
FRIEND = {1: "friendship = 255", 2: "friendship 200..254", 3: "friendship 150..199", 4: "friendship 90..149",
          5: "friendship 60..89", 6: "friendship 30..59", 7: "friendship 1..29", 8: "friendship = 0",
          9: "friendship >= 90", 10: "friendship < 60"}
STATUS = {1: "no status", 2: "burned", 3: "frozen", 4: "paralyzed", 5: "poisoned", 7: "any status (incl. sleep)",
          8: "asleep"}
ITEM = {1: "holding a Poke Ball", 2: "holding medicine", 3: "holding a battle item", 4: "holding a general item",
        5: "holding mail", 6: "holding a berry", 7: "holding a TM/HM", 8: "holding nothing", 9: "holding any item"}
TIME = {1: "morning", 2: "day", 3: "evening", 4: "night", 5: "late night"}
FACING = {1: "follower faces east", 2: "follower faces west", 3: "follower faces north", 4: "follower faces south"}
WEATHER = {1: "weather clear", 2: "weather (never matches)", 3: "raining"}
SPECCAT = {0xFA: "species category <= 19", 0xFB: "species category <= 130 (smaller follower)",
           0xFC: "species category 140..149", 0xFD: "species category >= 160 (bigger follower)",
           0xFE: "species category >= 220"}
TILES = {2: "tall grass", 3: "very tall grass", 16: "river water", 22: "puddle", 23: "shallow water", 32: "ice",
         33: "sand", 48: "tile 48", 49: "tile 49"}


def describe_conditions(e, mapsec_names):
    c = []
    if e["chance"] != 100:
        c.append("%d%% chance" % e["chance"] if e["chance"] else "0% chance (NEVER fires)")
    if e["status"]:
        c.append(STATUS.get(e["status"], "status %d" % e["status"]))
    if e["hp"]:
        c.append(HP.get(e["hp"], "hp %d" % e["hp"]))
    if e["mood"]:
        c.append(MOOD.get(e["mood"], "mood %d" % e["mood"]))
    if e["friend"]:
        c.append(FRIEND.get(e["friend"], "friendship %d" % e["friend"]))
    if e["nature"]:
        c.append("nature in {%s}" % ", ".join(n for n, k in zip(NATURES, NATURE_CAT) if k == e["nature"]))
    if e["gender"]:
        c.append("male" if e["gender"] == 1 else "female or genderless")
    if e["item"]:
        c.append(ITEM.get(e["item"], "item %d" % e["item"]))
    if e["type"]:
        c.append("has type %s" % TYPES.get(e["type"], e["type"]))
    if e["athlon"]:
        c.append("best Pokeathlon stat code %d" % e["athlon"])
    if e["grass"]:
        c.append("follower on wild-encounter ground" if e["grass"] == 1 else "follower NOT on wild-encounter ground")
    if e["speccat"]:
        c.append(SPECCAT.get(e["speccat"], "species category %d" % e["speccat"]))
    if e["leaf"]:
        c.append("Shiny Leaf %s not yet obtained" % "ABCDE"[e["leaf"] - 1])
    if e["weather"]:
        c.append(WEATHER.get(e["weather"], "weather %d" % e["weather"]))
    if e["facing"]:
        c.append(FACING.get(e["facing"], "facing %d" % e["facing"]))
    if e["npcs"]:
        c.append("%s people next to the player" % ("5+" if e["npcs"] == 5 else e["npcs"]))
    if e["time"]:
        c.append(TIME.get(e["time"], "time %d" % e["time"]))
    if e["map"]:
        c.append("on HG map id %d" % (e["map"] - 1))
    if e["metatile"]:
        c.append("follower standing on %s" % TILES.get(e["metatile"], "tile %d" % e["metatile"]))
    if e["level"]:
        c.append("level field %d (NEVER matches in HG - bug)" % e["level"])
    if e["npcsprite"]:
        c.append("npc-sprite field %d (ALWAYS fails in HG - bug)" % e["npcsprite"])
    if e["hidden"]:
        c.append("%s uncollected hidden items on the map" % ("4+" if e["hidden"] == 4 else e["hidden"]))
    if e["flag"]:
        c.append("HG flag %d set" % e["flag"])
    return c or ["always"]


def describe_action(D, aid, depth=0, seen=None):
    seen = set() if seen is None else seen
    pad = "        " + "    " * depth
    out = []
    if aid in seen or not (1 <= aid <= len(D.actions)):
        return [pad + "(action %d, see above)" % aid]
    seen.add(aid)
    a = D.actions[aid - 1]
    for i, s in enumerate(a["steps"]):
        parts = []
        if s["anim"]:
            parts.append("anim %d" % s["anim"])
        if s["snd"] == 2379:
            parts.append("cry")
        elif s["snd"] == 2380:
            parts.append("cry (alternate/sad)")
        elif s["snd"]:
            parts.append("sound %d" % s["snd"])
        if s["emote"]:
            parts.append("emote: %s" % EMOTES.get(s["emote"], s["emote"]))
        if s["wait"]:
            parts.append("wait %d" % s["wait"])
        line = pad + "step %d: %s" % (i + 1, ", ".join(parts) or "-")
        if s["msg"]:
            line += '\n' + pad + '    "%s"' % D.messages.get(s["msg"] - 1, "?").replace("\n", "\\n")
        out.append(line)
    eff = []
    if a["mood"]:
        eff.append("mood %+d" % a["mood"])
    if a["friend"]:
        eff.append("friendship %+d" % a["friend"])
    if a["acc"]:
        eff.append("gives accessory #%d" % (a["acc"] - 1))
    if a["leaf"]:
        eff.append("gives Shiny Leaf %s" % "ABCDE"[a["leaf"] - 1])
    if eff:
        out.append(pad + "effects: " + ", ".join(eff))
    if a["yesno"]:
        out.append(pad + "YES -> action %d:" % a["yes"] if a["yes"] else pad + "YES -> (nothing)")
        if a["yes"]:
            out += describe_action(D, a["yes"], depth + 1, seen)
        out.append(pad + "NO  -> action %d:" % a["no"] if a["no"] else pad + "NO  -> (nothing)")
        if a["no"]:
            out += describe_action(D, a["no"], depth + 1, seen)
    return out


def write_table(D, f, entries, first_slot):
    for i, e in enumerate(entries):
        if not e["action"]:
            continue
        f.write("  slot %2d  IF %s\n" % (first_slot + i, "; ".join(describe_conditions(e, D.mapsec_names))))
        f.write("      THEN action %d\n" % e["action"])
        f.write("\n".join(describe_action(D, e["action"])) + "\n")


def main():
    D = HGTalkData()
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        f.write(HEADER)
        f.write("\n=== GENERAL TABLE (applies everywhere; slots 30-41 are checked FIRST, slots 42-99 LAST) ===\n")
        write_table(D, f, D.general, 30)
        n = 0
        for ms in range(len(D.cond_members) - 1):
            t = D.map_table(ms)
            if not any(e["action"] for e in t):
                continue
            n += 1
            f.write("\n=== LOCATION TABLE: %s (HG map section %d; checked between slot 41 and slot 42) ===\n"
                    % (D.mapsec_names.get(ms, "?"), ms))
            write_table(D, f, t, 0)
        f.write("\n(%d HG map sections have a location table.)\n" % n)
    print("wrote", OUT)


HEADER = """HEARTGOLD FOLLOWER POKEMON: EVERY "TALK" MESSAGE AND THE CONDITIONS FOR IT
==========================================================================
Generated by pokeplatinum-main/tools/follower_talk/gen_reference.py from HeartGold's own data files
(files/a/2/2/0, 2/2/1, 2/2/2, 2/3/1, files/msgdata/msg/msg_0265.gmm) - nothing here is paraphrased or guessed.
The engine that reads them is HeartGold's overlay 2 (asm/overlay_02_02248728.s, Task_FollowMonInteract), started
by the follower's talk script (files/fielddata/script/scr_seq/scr_seq_0163.s: PlaySE SEQ_SE_DP_SELECT, LockAll,
FollowMonFacePlayer, FollowMonInteract, ReleaseAll).

HOW HG PICKS WHAT HAPPENS (every time you press A on the follower)
  1. The follower turns to face you.
  2. HG builds a snapshot of the lead Pokemon and the surroundings: held item, HP %, status, level, types,
     friendship, nature, gender, species category, Shiny Leaves, people next to you, uncollected hidden items on
     the map, weather, the tile under the follower and whether it is wild-encounter ground, the map, time of day,
     the follower's hidden MOOD value, best Pokeathlon stat, and which way the follower faces.
  3. It walks the table slots in this order: general 30-41, the current location's table (slots 0-29), general
     42-99. For each slot it rolls the slot's chance (rand % 100 < chance) and checks every condition; the FIRST
     slot that passes is used. General slot 99 always passes, so something always happens.
  4. The chosen ACTION runs up to 5 steps; each step can play an animation (hops, shakes, turning, walking up to
     you), the Pokemon's cry, show an emote bubble above it, and print a message (A/B to continue).
     Some actions end with a Yes/No question that branches to another action, give a Fashion Case accessory, or
     put a Shiny Leaf on the Pokemon.
  5. The action's mood and friendship changes are applied (mood is clamped to -127..127, friendship to 0..255).

MOOD (hidden per-follower value, stored in the Pokemon's data, -127..127)
  - Reset to 0 whenever the follower becomes a different Pokemon (species or personality changes).
  - Moves 1 step toward 0 on every step you walk while the follower is visible.
  - Field moves (HG script 146 and the field-move tasks; "+1" only when the LEAD has the listed type):
      Cut +20 (+1 Grass), Rock Smash +20 (+1 Fighting), Headbutt +20 (+1 Normal), Strength +1 Normal,
      Flash +1 Psychic, Surf +1 Water, Rock Climb +1 Normal, Sweet Scent +1 Grass, Dig +1 Ground,
      Teleport +1 Psychic, Fly +1 Flying.
  - Talking applies the chosen action's mood delta (most happy actions +10..+60, angry/sad ones -10..-40).

CONDITION VALUES
  speccat 'species category' is a per-species byte from HG's files/a/2/3/1 (e.g. 20/21/22 = Bulbasaur/
  Charmander/Squirtle lines, 75/76/77 = Chikorita/Cyndaquil/Totodile lines).
  Facing is the follower's direction AFTER it turned to face you (so it tells where you stand).
  "people next to the player" counts other map objects within one tile of you.
  Two fields never pass in HG (a level field compared against values it can never take, and an NPC-sprite field
  that always returns false); entries using them are listed but can never happen.

EMOTE BUBBLES (HG fldeff textures 2..15): 1 heart, 2-6 faces, 7 music note, 8 ?, 9 !, 10 sweat, 11 troubled,
  12 poison, 13 "...", 14 lightbulb.
"""

if __name__ == "__main__":
    main()
