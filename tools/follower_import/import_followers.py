#!/usr/bin/env python3
"""Import HeartGold follower species (base forms) into the Platinum port. Idempotent: only species not
already imported are added. Run with --dry-run first.

Lessons baked in (Changelog ENTRY P55-P58):
  * everything lands in EXISTING files - the palette tables go into src/follower_mon.c as statics. A separate
    .c file called from follower_mon.c got local linkage from the Metrowerks linker and crashed on first call.
  * every follower row uses BILLBOARD_FRAME_SEQ_HG_TSURE_POKE/sHGTsurePokeBillboardAnims (8-frame textures);
    any other table reads missing texture indices -> NULL deref.
  * "has a native OBJ_EVENT_GFX_<NAME>" (needs _HG suffix + _hg.nsbtx basename to avoid a ninja output
    collision) is a different question from "already has a FOLLOWER_GRAPHICS_TABLE row" (replace vs append).
  * palette row i <-> graphics ID FIRST_ID + i, so rows are appended in the same order as the IDs.
  * "too_tall" species (Changelog ENTRY P61): HG gives them the 64x64 billboard model (mmodel 269, byte-identical
    to Platinum's generic_64x64.nsbmd) instead of the 32x32 one (266) and 16,832-byte textures of 8 frames of 64x64;
    frame sequence / anim table are the same shared ones. Run with --tall to import them. Rows that are forms or
    gender variants of a species (STEELIX_F, GIRATINA_ORIGIN, ARCEUS_<TYPE>) go into FOLLOWER_FORM_TABLE.
"""
import csv
import os
import re
import subprocess
import sys

DRY = "--dry-run" in sys.argv
TALL = "--tall" in sys.argv  # also import the too_tall rows (64x64 model); default run skips them

# paths are derived from this file's location (pokeheartgold-master/pokeplatinum-main/tools/follower_import/), so the tool
# works from any checkout location and under both cygwin/msys python (/c/... paths) and native Windows python (C:/...)
SCRATCH = os.path.dirname(os.path.abspath(__file__))
PLAT = os.path.abspath(os.path.join(SCRATCH, "..", "..")).replace("\\", "/")
HG = os.path.dirname(PLAT)

NITROARC = PLAT + "/build/tools/nitroarc/nitroarc.exe"
NITROBTX = PLAT + "/build/tools/nitrobtx/nitrobtx.exe"
MMODEL_NARC = HG + "/files/data/mmodel/mmodel.narc"
TP_PARAM = HG + "/files/fielddata/tsurepoke/tp_param.csv"
SPECIES_H = PLAT + "/build/generated/species.h"

GFX_TXT = PLAT + "/generated/object_events_gfx.txt"
ORDER = PLAT + "/res/graphics/field_sprites/field_sprites.order"
MESON = PLAT + "/res/graphics/field_sprites/meson.build"
GFX_DATA = PLAT + "/src/overlay005/object_event_gfx_data.c"
FOLLOWER_C = PLAT + "/src/follower_mon.c"
NSBTX_DIR = PLAT + "/res/graphics/field_sprites/pokemon_hg"

EXTRACT_DIR = SCRATCH + "/.cache/mmodel_extract"
PAL_DIR = SCRATCH + "/.cache/palettes"
MMODEL_BASE = 297  # mmodel.narc index = 297 + tp_param row (ENTRY P16)
EXPECT_SIZE = {False: 4544, True: 16832}  # 8 frames x (32x32 | 64x64), 2 palettes; keyed by too_tall
MODEL = {False: "BILLBOARD_MODEL_GENERIC_32x32", True: "BILLBOARD_MODEL_GENERIC_64x64"}
# tp_param rows that are a form / gender variant of a species rather than a species of their own:
#   row name -> (species name, form value as returned by Pokemon_GetValue(MON_DATA_FORM), gender constant, also_base)
# Arceus's form value is its type (Pokemon_GetArceusTypeOf), which is exactly the order of HG's ARCEUS_<TYPE> rows;
# Giratina's is GIRATINA_FORM_ORIGIN = 1 (include/constants/forms.h); STEELIX_F is HG's female Steelix sprite.
# also_base: the row doubles as the plain-species fallback row in FOLLOWER_GRAPHICS_TABLE (Arceus has no plain row).
ANY = "FOLLOWER_GENDER_ANY"
ARCEUS_TYPES = ["NORMAL", "FIGHTING", "FLYING", "POISON", "GROUND", "ROCK", "BUG", "GHOST", "STEEL", "MYSTERY",
                "FIRE", "WATER", "GRASS", "ELECTRIC", "PSYCHIC", "ICE", "DRAGON", "DARK"]
FORM_ROWS = {"STEELIX_F": ("STEELIX", 0, "GENDER_FEMALE", False),
             "GIRATINA_ORIGIN": ("GIRATINA", 1, ANY, False)}
for _i, _t in enumerate(ARCEUS_TYPES):
    FORM_ROWS["ARCEUS_" + _t] = ("ARCEUS", _i, ANY, _t == "NORMAL")
# ENTRY P84: every other HG form / gender row. HG picks the row as <species' first row> + form (or +1 for a female when
# the species has a female row - FollowMon_GetSpriteID), so a row's form value is its offset from the species' first
# row. Platinum's form values (include/constants/forms.h) are the same numbers: Unown A..Z = 0..25 then 26/27 (HG's
# rows 230/231 are labelled QMARK/EXCL but sit at offsets 26/27, the same as UNOWN_FORM_EXC/QUE - picked by offset like
# HG does), Deoxys Normal/Attack/Defense/Speed 0..3, Burmy/Wormadam Plant/Sandy/Trash 0..2, Shellos/Gastrodon West/East
# 0/1, Rotom base/Heat/Wash/Frost/Fan/Mow 0..5, Shaymin Sky 1, HG's Spiky-eared Pichu 1.
# Unown, Shellos and Gastrodon have no plain-species row in HG, so their form-0 row doubles as the fallback (also_base).
for _n in ("VENUSAUR", "PIKACHU", "MEGANIUM", "WOBBUFFET", "HERACROSS", "COMBEE", "GIBLE", "GABITE", "GARCHOMP",
           "HIPPOPOTAS", "HIPPOWDON"):
    FORM_ROWS[_n + "_F"] = (_n, 0, "GENDER_FEMALE", False)
FORM_ROWS["PICHU_SPIKY"] = ("PICHU", 1, ANY, False)
for _i, _l in enumerate("ABCDEFGHIJKLMNOPQRSTUVWXYZ"):
    FORM_ROWS["UNOWN_" + _l] = ("UNOWN", _i, ANY, _l == "A")
FORM_ROWS["UNOWN_QMARK"] = ("UNOWN", 26, ANY, False)
FORM_ROWS["UNOWN_EXCL"] = ("UNOWN", 27, ANY, False)
for _i, _f in enumerate(("ATK", "DEF", "SPD"), 1):
    FORM_ROWS["DEOXYS_" + _f] = ("DEOXYS", _i, ANY, False)
for _n in ("BURMY", "WORMADAM"):
    FORM_ROWS[_n + "_SANDY"] = (_n, 1, ANY, False)
    FORM_ROWS[_n + "_TRASH"] = (_n, 2, ANY, False)
for _n in ("SHELLOS", "GASTRODON"):
    FORM_ROWS[_n + "_WEST"] = (_n, 0, ANY, True)
    FORM_ROWS[_n + "_EAST"] = (_n, 1, ANY, False)
for _i, _f in enumerate(("HEAT", "WASH", "FROST", "FAN", "MOW"), 1):
    FORM_ROWS["ROTOM_" + _f] = ("ROTOM", _i, ANY, False)
FORM_ROWS["SHAYMIN_SKY"] = ("SHAYMIN", 1, ANY, False)
ORIGINAL_9 = {"CHIKORITA", "BAYLEEF", "MEGANIUM", "CYNDAQUIL", "QUILAVA", "TYPHLOSION", "TOTODILE", "CROCONAW", "FERALIGATR"}

M_RENDERERS = "    // HG-FOLLOWER-IMPORT-END (renderers)"
M_TEXTURES = "    // HG-FOLLOWER-IMPORT-END (textures)"
M_ANIMS = "    // HG-FOLLOWER-IMPORT-END (model/anims)"
M_DETAILS = "    // HG-FOLLOWER-IMPORT-END (render details)"
M_TABLE = "    // HG-FOLLOWER-TABLE-END"
M_FORMS = "    // HG-FOLLOWER-FORM-TABLE-END"
M_TALL = "    // HG-FOLLOWER-TALL-TABLE-END"
M_PRIS = "    /* HG-BULK-PRISTINE-END */"
M_SHINY = "    /* HG-BULK-SHINY-END */"

ENV = dict(os.environ)
ENV["PATH"] = ("C:/msys64/mingw64/bin" if sys.platform == "win32" else "/c/msys64/mingw64/bin") + os.pathsep + ENV.get("PATH", "")


CRLF = {}


def rd(p):
    # every file this tool edits is CRLF (checked: line count == CRLF count); normalize to \n so anchors match,
    # remember the style, write it back unchanged in wr()
    raw = open(p, "rb").read().decode("utf-8", errors="surrogateescape")
    CRLF[p] = "\r\n" in raw
    return raw.replace("\r\n", "\n")


def wr(p, s):
    if DRY:
        return
    if CRLF.get(p):
        s = s.replace("\n", "\r\n")
    open(p, "wb").write(s.encode("utf-8", errors="surrogateescape"))


def fail(msg):
    print("ERROR: " + msg)
    sys.exit(1)


# ---------------------------------------------------------------- inputs
def read_species():
    return set(re.findall(r"^\s*(SPECIES_[A-Z0-9_]+)\s*=", rd(SPECIES_H), re.M))


def read_tp_rows():
    rows = []
    with open(TP_PARAM, "r") as f:
        for i, r in enumerate(csv.DictReader(f)):
            rows.append((i, r["index"][len("TSURE_POKE_"):], r["too_tall"].strip().lower() == "true"))
    return rows


def extract_narc():
    if os.path.exists(f"{EXTRACT_DIR}/{MMODEL_BASE + 565:05d}.BTX0"):
        return
    os.makedirs(EXTRACT_DIR, exist_ok=True)
    subprocess.run([NITROARC, "-xf", MMODEL_NARC, EXTRACT_DIR], check=True, env=ENV)


def read_pal(path):
    raw = open(path, "rb").read().decode("ascii").replace("\r\n", "\n").replace("\r", "\n")
    lines = [l.strip() for l in raw.split("\n") if l.strip()]
    assert lines[0] == "JASC-PAL"
    n = int(lines[2])
    return [tuple(map(int, lines[3 + i].split())) for i in range(n)]


def bgr555(r, g, b):
    return (round(b * 31 / 255) << 10) | (round(g * 31 / 255) << 5) | round(r * 31 / 255)


def palettes_for(file_base, nsbtx_path):
    out = f"{PAL_DIR}/{file_base}"
    os.makedirs(out, exist_ok=True)
    res = subprocess.run([NITROBTX, "dump", "--spritesheet", "--palettes-dir", out, nsbtx_path, f"{PAL_DIR}/{file_base}_sheet.png"],
                         env=ENV, capture_output=True, text=True)
    if res.returncode != 0:
        fail(f"nitrobtx failed on {file_base}: {res.stderr}")
    pal = []
    for idx in (0, 1):  # 0 = normal, 1 = shiny (ENTRY P23 / P52)
        colors = read_pal(f"{out}/tsure_poke{idx}.pal")
        if len(colors) != 16:
            fail(f"{file_base}: palette {idx} has {len(colors)} colors")
        pal.append([bgr555(*c) for c in colors])
    return pal


# ---------------------------------------------------------------- current state
def current_state():
    meson = rd(MESON)
    files = re.findall(r"'pokemon_hg/([a-z0-9_]+)\.nsbtx'", meson)
    follower = rd(FOLLOWER_C)
    tbl = re.search(r"FOLLOWER_GRAPHICS_TABLE\[\] = \{(.*?)\n\};", follower, re.S)
    in_table = dict(re.findall(r"\{ SPECIES_([A-Z0-9_]+), (OBJ_EVENT_GFX_[A-Z0-9_]+) \}", tbl.group(1)))
    gfx_names = [l.strip() for l in rd(GFX_TXT).split("\n") if l.strip() and "=" not in l]
    imported = set()
    for f in files:
        imported.add((f[:-3] if f.endswith("_hg") else f).upper())
    return files, in_table, set(gfx_names), imported


def build_plan(species_set, rows, gfx_names, imported):
    plan, skipped = [], {"too_tall": 0, "form_or_unmatched": 0, "already": 0, "bad_size": []}
    for row_idx, name, too_tall in rows:
        if name in ORIGINAL_9 or name in imported:
            skipped["already"] += 1
            continue
        if too_tall and not TALL:
            skipped["too_tall"] += 1
            continue
        form = FORM_ROWS.get(name)
        if form is None and "SPECIES_" + name not in species_set:
            skipped["form_or_unmatched"] += 1
            continue
        if form is not None and "SPECIES_" + form[0] not in species_set:
            skipped["form_or_unmatched"] += 1
            continue
        # (every HG form/gender row is in FORM_ROWS since ENTRY P84; an unknown variant row is still skipped above)
        src =f"{EXTRACT_DIR}/{MMODEL_BASE + row_idx:05d}.BTX0"
        if os.path.getsize(src) != EXPECT_SIZE[too_tall]:
            skipped["bad_size"].append(name)
            continue
        collides = "OBJ_EVENT_GFX_" + name in gfx_names
        plan.append({
            "row": row_idx, "name": name, "src": src, "tall": too_tall, "form": form,
            "gfx": "OBJ_EVENT_GFX_" + name + ("_HG" if collides else ""),
            "file": name.lower() + ("_hg" if collides else ""),
            "collides": collides,
        })
    return plan, skipped


# ---------------------------------------------------------------- edits
def insert_before(text, anchor, new):
    if text.count(anchor) != 1:
        fail(f"anchor not unique/found ({text.count(anchor)}): {anchor!r}")
    return text.replace(anchor, new + anchor)


def main():
    species_set = read_species()
    rows = read_tp_rows()
    extract_narc()
    files, in_table, gfx_names, imported = current_state()
    plan, skipped = build_plan(species_set, rows, gfx_names, imported)

    print(f"already imported: {len(imported)} | to import: {len(plan)} | skipped too_tall={skipped['too_tall']} "
          f"form/unmatched={skipped['form_or_unmatched']} bad_size={skipped['bad_size']}")
    replace = [p for p in plan if p["name"] in in_table]
    print(f"  of those, replacing an existing FOLLOWER_GRAPHICS_TABLE row: {len(replace)}")
    print(f"  of those, needing the _HG name (native OBJ_EVENT_GFX_<NAME> exists): {sum(p['collides'] for p in plan)}")
    print(f"  replace-but-no-collision: {[p['name'] for p in replace if not p['collides']]}")
    print(f"  collision-but-not-in-table: {sum(1 for p in plan if p['collides'] and p['name'] not in in_table)}")
    print(f"  tall (64x64 model): {sum(p['tall'] for p in plan)} | form/gender rows (FOLLOWER_FORM_TABLE): {sum(p['form'] is not None for p in plan)}")
    if skipped["bad_size"]:
        fail("species with unexpected texture size - not the 8-frame format, refusing to import")
    if not plan:
        print("nothing to do")
        return

    # nsbtx files + palettes
    for p in plan:
        data = open(p["src"], "rb").read()
        dst = f"{NSBTX_DIR}/{p['file']}.nsbtx"
        if not DRY:
            open(dst, "wb").write(data)
        p["pal"] = palettes_for(p["file"], p["src"])
    print(f"extracted {len(plan)} textures + palettes")

    # --- generated/object_events_gfx.txt
    t = rd(GFX_TXT)
    t = insert_before(t, "OBJ_EVENT_GFX_BERRY_SPROUT = 4096", "".join(p["gfx"] + "\n" for p in plan))
    wr(GFX_TXT, t)

    # --- field_sprites.order
    t = rd(ORDER)
    if not t.endswith("\n"):
        t += "\n"
    t += "".join(p["file"] + ".nsbtx\n" for p in plan)
    wr(ORDER, t)

    # --- field_sprites meson.build
    t = rd(MESON)
    m = re.search(r"(field_sprites_pokemon_hg_import = copy_gen\.process\(files\(\n.*?\n)(\)\))", t, re.S)
    if not m:
        fail("meson pokemon_hg block not found")
    t = t[:m.end(1)] + "".join(f"    'pokemon_hg/{p['file']}.nsbtx',\n" for p in plan) + t[m.end(1):]
    wr(MESON, t)

    # --- object_event_gfx_data.c : four tables, markers after the last imported row
    t = rd(GFX_DATA)
    last_file = files[-1]
    last_gfx = re.search(r"\{ (OBJ_EVENT_GFX_[A-Z0-9_]+), " + re.escape(last_file) + r"_nsbtx \}", t).group(1)
    row_fmt = [
        (M_RENDERERS, f"    {{ {last_gfx}, &Unk_ov5_021FAFD8 }},\n", lambda p: f"    {{ {p['gfx']}, &Unk_ov5_021FAFD8 }},\n"),
        (M_TEXTURES, f"    {{ {last_gfx}, {last_file}_nsbtx }},\n", lambda p: f"    {{ {p['gfx']}, {p['file']}_nsbtx }},\n"),
        (M_ANIMS, f"    {{ {last_gfx}, BILLBOARD_MODEL_GENERIC_32x32, BILLBOARD_FRAME_SEQ_HG_TSURE_POKE, sHGTsurePokeBillboardAnims }},\n",
         lambda p: f"    {{ {p['gfx']}, {MODEL[p['tall']]}, BILLBOARD_FRAME_SEQ_HG_TSURE_POKE, sHGTsurePokeBillboardAnims }},\n"),
        (M_DETAILS, f"    {{ {last_gfx}, MODEL_TYPE_BILLBOARD, .hasShadow = TRUE, TRACK_TYPE_FOOTSTEPS, .hasReflection = TRUE }},\n",
         lambda p: f"    {{ {p['gfx']}, MODEL_TYPE_BILLBOARD, .hasShadow = TRUE, TRACK_TYPE_FOOTSTEPS, .hasReflection = TRUE }},\n"),
    ]
    for marker, anchor_row, mk in row_fmt:
        if marker not in t:
            if t.count(anchor_row) != 1:
                fail(f"first-run anchor for {marker} not unique/found: {anchor_row!r}")
            t = t.replace(anchor_row, anchor_row + marker + "\n")
        t = insert_before(t, marker + "\n", "".join(mk(p) for p in plan))
    wr(GFX_DATA, t)

    # --- follower_mon.c
    t = rd(FOLLOWER_C)
    # FOLLOWER_GRAPHICS_TABLE
    if M_TABLE not in t:
        m = re.search(r"(FOLLOWER_GRAPHICS_TABLE\[\] = \{.*?\n)(\};)", t, re.S)
        t = t[:m.end(1)] + M_TABLE + "\n" + t[m.end(1):]
    for p in replace:
        pat = re.compile(r"\{ SPECIES_" + re.escape(p["name"]) + r", OBJ_EVENT_GFX_[A-Z0-9_]+ \}")
        if len(pat.findall(t)) != 1:
            fail(f"expected exactly one table row for {p['name']}")
        t = pat.sub(f"{{ SPECIES_{p['name']}, {p['gfx']} }}", t, count=1)
    # plain-species rows: species with their own tp_param row, plus the form row that doubles as the species fallback
    # (ARCEUS_NORMAL -> SPECIES_ARCEUS); other form rows only go into FOLLOWER_FORM_TABLE
    new_rows = []
    for p in plan:
        if p["form"] is None:
            if p["name"] not in in_table:
                new_rows.append(("SPECIES_" + p["name"], p["gfx"]))
        elif p["form"][3] and p["form"][0] not in in_table:
            new_rows.append(("SPECIES_" + p["form"][0], p["gfx"]))
    t = insert_before(t, M_TABLE + "\n", "".join(f"    {{ {sp}, {g} }},\n" for sp, g in new_rows))
    # FOLLOWER_FORM_TABLE (species + form + gender -> graphics, consulted before the plain-species table) and
    # FOLLOWER_TALL_TABLE (graphics IDs that use the 64x64 model; see ENTRY P61)
    form_rows = [p for p in plan if p["form"] is not None]
    t = insert_before(t, M_FORMS + "\n", "".join(
        f"    {{ SPECIES_{p['form'][0]}, {p['form'][1]}, {p['form'][2]}, {p['gfx']} }},\n" for p in form_rows))
    t = insert_before(t, M_TALL + "\n", "".join(f"    {p['gfx']},\n" for p in plan if p["tall"]))
    # palette tables
    for marker, idx in ((M_PRIS, 0), (M_SHINY, 1)):
        if marker not in t:
            head = "sFollowerHGBulkPristinePalettes" if idx == 0 else "sFollowerHGBulkShinyPalettes"
            m = re.search(r"static const u16 " + head + r"\[FOLLOWER_HG_BULK_COUNT\]\[16\] = \{\n.*?\n(\};)", t, re.S)
            if not m:
                fail(f"{head} table not found")
            t = t[:m.start(1)] + marker + "\n" + t[m.start(1):]
        rows_txt = "".join("    /* %s */ { %s },\n" % (p["gfx"], ", ".join(f"0x{v:04X}" for v in p["pal"][idx])) for p in plan)
        t = insert_before(t, marker + "\n", rows_txt)
    old_count = int(re.search(r"#define FOLLOWER_HG_BULK_COUNT (\d+)", t).group(1))
    t = t.replace(f"#define FOLLOWER_HG_BULK_COUNT {old_count}", f"#define FOLLOWER_HG_BULK_COUNT {old_count + len(plan)}")
    wr(FOLLOWER_C, t)
    print(f"{'(dry run) ' if DRY else ''}done: bulk palette rows {old_count} -> {old_count + len(plan)}, "
          f"table rows replaced {len(replace)}, appended {len(new_rows)}")
    if not DRY:
        # never ship an import that hasn't passed the wiring check (frame sequences, palettes, ID/row order, natives intact)
        res = subprocess.run([sys.executable, SCRATCH + "/verify_followers.py"], capture_output=True, text=True)
        print(res.stdout.strip())
        if res.returncode != 0:
            fail("verify_followers.py failed - do not build this tree")


if __name__ == "__main__":
    main()
