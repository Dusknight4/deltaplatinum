#!/usr/bin/env python3
"""Pre-build wiring check for HG follower species. Exit 1 on any failure. Run after import_followers.py and
before every build that contains imported species (Changelog ENTRY P58). ENTRY P61: also knows the 64x64 "tall"
species (16,832-byte textures, BILLBOARD_MODEL_GENERIC_64x64, FOLLOWER_TALL_TABLE) and FOLLOWER_FORM_TABLE."""
import os
import re
import struct
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))  # location-independent


def rd(rel):
    return open(os.path.join(ROOT, rel), "rb").read().decode("utf-8", errors="surrogateescape").replace("\r\n", "\n")


gfx = rd("src/overlay005/object_event_gfx_data.c")
meson = rd("res/graphics/field_sprites/meson.build")
fm = rd("src/follower_mon.c")
gfx_txt = [l.strip() for l in rd("generated/object_events_gfx.txt").split("\n") if l.strip()]
order = [l.strip() for l in rd("res/graphics/field_sprites/field_sprites.order").split("\n") if l.strip()]
species_h = rd("build/generated/species.h")

errors = []


def err(m):
    errors.append(m)


hg_files = re.findall(r"'pokemon_hg/([a-z0-9_]+)\.nsbtx'", meson)
if len(hg_files) != len(set(hg_files)):
    err("duplicate files in meson pokemon_hg list")

# 0. no leftover separate bulk translation unit (linker bug, ENTRY P57)
for rel in ("src/follower_mon_hg_bulk.c", "include/follower_mon_hg_bulk.h"):
    if os.path.exists(os.path.join(ROOT, rel)):
        err(f"{rel} exists - bulk palette code must live inside follower_mon.c")
if "follower_mon_hg_bulk" in rd("src/meson.build") or '#include "follower_mon_hg_bulk' in fm:
    err("follower_mon_hg_bulk still referenced by the build or an include")

# 1. file -> graphics ID via the texture table; one row per table per ID
tex = {}
for g, f in re.findall(r"\{ (OBJ_EVENT_GFX_[A-Z0-9_]+), ([a-z0-9_]+)_nsbtx \}", gfx):
    tex.setdefault(f, []).append(g)
hg_gfx = []
model_of = {}   # file -> "32x32" | "64x64"
gfx_model = {}  # graphics ID -> "32x32" | "64x64"
for f in hg_files:
    gs = tex.get(f, [])
    if len(gs) != 1:
        err(f"{f}: {len(gs)} texture-table rows (need exactly 1)")
        continue
    g = gs[0]
    hg_gfx.append(g)
    if f + ".nsbtx" not in order:
        err(f"{f}.nsbtx missing from field_sprites.order")
    if g not in gfx_txt:
        err(f"{g} missing from generated/object_events_gfx.txt")
    # model/anims: GENERIC_32x32 (normal) or GENERIC_64x64 (tall), always the shared 8-frame HG_TSURE_POKE sequence/anims -
    # an 8-frame texture indexed by any other table reads missing frames -> NULL deref
    models = re.findall(rf"\{{ {g}, BILLBOARD_MODEL_GENERIC_(32x32|64x64), BILLBOARD_FRAME_SEQ_HG_TSURE_POKE, sHGTsurePokeBillboardAnims \}}", gfx)
    if len(models) == 1:
        model_of[f] = models[0]
        gfx_model[g] = models[0]
    else:
        err(f"{g}: {len(models)} 'model/anims' rows with GENERIC_32x32/64x64 + HG_TSURE_POKE/sHGTsurePokeBillboardAnims (need exactly 1)")
    rows = {
        "renderers": rf"\{{ {g}, &Unk_ov5_021FAFD8 \}}",
        "render details": rf"\{{ {g}, MODEL_TYPE_BILLBOARD, \.hasShadow = TRUE, TRACK_TYPE_FOOTSTEPS, \.hasReflection = TRUE \}}",
    }
    for tname, pat in rows.items():
        n = len(re.findall(pat, gfx))
        if n != 1:
            err(f"{g}: {n} matching '{tname}' rows (need exactly 1)")
    any_anim = re.findall(rf"\{{ {g}, BILLBOARD_MODEL_[A-Za-z0-9_]+, (BILLBOARD_FRAME_SEQ_[A-Z_]+),", gfx)
    if any_anim != ["BILLBOARD_FRAME_SEQ_HG_TSURE_POKE"]:
        err(f"{g}: frame sequences {any_anim}")

# 1b. every texture must be the exact 8-frame, 2-palette layout the shared frame table (hg_tsure_poke.bin) assumes, at
#     the frame size its billboard model was built for. 32x32 model: 4544 bytes, frame names tsure_poke.1 and
#     .10-.16 (2-9 absent, ENTRY P17). 64x64 model: 16832 bytes, frame names tsure_poke.1-.8 (ENTRY P61). Palettes
#     tsure_poke0/tsure_poke1. Frame dimensions/format are read from the texture dictionary itself, not inferred
#     from the file size.
WANT = {"32x32": (4544, {1, 10, 11, 12, 13, 14, 15, 16}, 32), "64x64": (16832, {1, 2, 3, 4, 5, 6, 7, 8}, 64)}


def frame_infos(blob):
    """[(name, width, height, fmt)] for each texture in an NSBTX, in dictionary (= frame index) order."""
    tex0 = struct.unpack_from("<I", blob, 0x10)[0]
    if blob[tex0:tex0 + 4] != b"TEX0":
        return None
    ofs_dict = struct.unpack_from("<H", blob, tex0 + 0x0E)[0]
    d = tex0 + ofs_dict
    num = blob[d + 1]
    unit = d + 8 + (num + 1) * 4
    unit_size = struct.unpack_from("<H", blob, unit)[0]
    data = unit + 4
    names = data + num * unit_size
    out = []
    for k in range(num):
        name = blob[names + k * 16: names + k * 16 + 16].split(b"\x00")[0].decode()
        tip = struct.unpack_from("<I", blob, data + k * unit_size)[0]
        out.append((name, 8 << ((tip >> 20) & 7), 8 << ((tip >> 23) & 7), (tip >> 26) & 7))
    return out


for f in hg_files:
    blob = open(os.path.join(ROOT, "res/graphics/field_sprites/pokemon_hg", f + ".nsbtx"), "rb").read()
    model = model_of.get(f)
    if model is None:
        continue  # already reported above
    want_size, want_frames, dim = WANT[model]
    frames = {int(x) for x in re.findall(rb"tsure_poke\.(\d+)", blob)}
    if len(blob) != want_size:
        err(f"{f}.nsbtx: size {len(blob)} != {want_size} (model GENERIC_{model})")
    if frames != want_frames:
        err(f"{f}.nsbtx: frame names {sorted(frames)} != {sorted(want_frames)} (model GENERIC_{model})")
    if not (b"tsure_poke0" in blob and b"tsure_poke1" in blob):
        err(f"{f}.nsbtx: missing tsure_poke0/tsure_poke1 palettes")
    infos = frame_infos(blob)
    if infos is None or len(infos) != 8:
        err(f"{f}.nsbtx: texture dictionary has {None if infos is None else len(infos)} frames, need 8")
    else:
        for name, w, h, fmt in infos:
            if (w, h) != (dim, dim) or fmt != 3:
                err(f"{f}.nsbtx: frame {name} is {w}x{h} fmt {fmt}; GENERIC_{model} needs {dim}x{dim} 16-color (fmt 3)")

if len(hg_gfx) != len(set(hg_gfx)):
    err("two files map to the same graphics ID")
if len(gfx_txt) != len(set(gfx_txt)):
    err("duplicate names in generated/object_events_gfx.txt")

# 2. FOLLOWER_GRAPHICS_TABLE: each plain-species HG graphics ID's species points at it, no duplicate species rows.
#    Form/gender rows (FOLLOWER_FORM_TABLE) are checked in 2b instead.
tbl = re.search(r"FOLLOWER_GRAPHICS_TABLE\[\] = \{(.*?)\n\};", fm, re.S).group(1)
rows = re.findall(r"\{ SPECIES_([A-Z0-9_]+), (OBJ_EVENT_GFX_[A-Z0-9_]+) \}", tbl)
species_rows = [s for s, _ in rows]
if len(species_rows) != len(set(species_rows)):
    err("duplicate species rows in FOLLOWER_GRAPHICS_TABLE")
tmap = dict(rows)

form_tbl = re.search(r"FOLLOWER_FORM_TABLE\[\] = \{(.*?)\n\};", fm, re.S)
form_rows = re.findall(r"\{ SPECIES_([A-Z0-9_]+), (\d+), ([A-Z_]+), (OBJ_EVENT_GFX_[A-Z0-9_]+) \}", form_tbl.group(1)) if form_tbl else None
if form_rows is None:
    err("FOLLOWER_FORM_TABLE not found")
    form_rows = []
form_gfx = {g for _, _, _, g in form_rows}

for g in hg_gfx:
    if g in form_gfx:
        continue
    sp = g[len("OBJ_EVENT_GFX_"):]
    sp = sp[:-3] if sp.endswith("_HG") else sp
    if tmap.get(sp) != g:
        err(f"FOLLOWER_GRAPHICS_TABLE: SPECIES_{sp} -> {tmap.get(sp)}, expected {g}")

# 2b. FOLLOWER_FORM_TABLE: real species, unique (species, form, gender) keys, every row points at an imported graphics ID,
#     the Arceus block is complete and in type order, and a plain-species fallback row exists for every form species
species_set = set(re.findall(r"^\s*SPECIES_([A-Z0-9_]+)\s*=", species_h, re.M))
seen = set()
for sp, form, gender, g in form_rows:
    if sp not in species_set:
        err(f"FOLLOWER_FORM_TABLE: SPECIES_{sp} does not exist")
    if (sp, form, gender) in seen:
        err(f"FOLLOWER_FORM_TABLE: duplicate key SPECIES_{sp} form {form} gender {gender}")
    seen.add((sp, form, gender))
    if g not in hg_gfx:
        err(f"FOLLOWER_FORM_TABLE: {g} is not an imported HG follower graphics ID")
    if sp not in tmap:
        err(f"FOLLOWER_FORM_TABLE: SPECIES_{sp} has no plain-species fallback row in FOLLOWER_GRAPHICS_TABLE")
ARCEUS_TYPES = ["NORMAL", "FIGHTING", "FLYING", "POISON", "GROUND", "ROCK", "BUG", "GHOST", "STEEL", "MYSTERY",
                "FIRE", "WATER", "GRASS", "ELECTRIC", "PSYCHIC", "ICE", "DRAGON", "DARK"]
arc = {int(form): g for sp, form, _, g in form_rows if sp == "ARCEUS"}
if arc:
    if sorted(arc) != list(range(18)):
        err(f"FOLLOWER_FORM_TABLE: Arceus forms {sorted(arc)} != 0..17")
    for k, t in enumerate(ARCEUS_TYPES):
        if k in arc and arc[k] not in (f"OBJ_EVENT_GFX_ARCEUS_{t}", f"OBJ_EVENT_GFX_ARCEUS_{t}_HG"):
            err(f"FOLLOWER_FORM_TABLE: Arceus form {k} should be {t}, is {arc[k]}")
    if tmap.get("ARCEUS") != "OBJ_EVENT_GFX_ARCEUS_NORMAL":
        err(f"FOLLOWER_GRAPHICS_TABLE: SPECIES_ARCEUS -> {tmap.get('ARCEUS')}, expected OBJ_EVENT_GFX_ARCEUS_NORMAL")

# 2c. FOLLOWER_TALL_TABLE == exactly the graphics IDs on the 64x64 model
tall_tbl = re.search(r"FOLLOWER_TALL_TABLE\[\] = \{(.*?)\n\};", fm, re.S)
if tall_tbl is None:
    err("FOLLOWER_TALL_TABLE not found")
else:
    tall_ids = re.findall(r"OBJ_EVENT_GFX_[A-Z0-9_]+", tall_tbl.group(1))
    if len(tall_ids) != len(set(tall_ids)):
        err("duplicate IDs in FOLLOWER_TALL_TABLE")
    want_tall = {g for g, m in gfx_model.items() if m == "64x64"}
    if set(tall_ids) != want_tall:
        err(f"FOLLOWER_TALL_TABLE != the 64x64-model IDs: missing {sorted(want_tall - set(tall_ids))}, extra {sorted(set(tall_ids) - want_tall)}")

# 3. native rows (Amity Square / other users) must still exist untouched for every _HG-suffixed species
for g in hg_gfx:
    if g.endswith("_HG"):
        native = g[:-3]
        if native not in gfx_txt:
            err(f"native {native} removed from object_events_gfx.txt")
        if not re.search(rf"\{{ {native}, [a-z0-9_]+_nsbtx \}}", gfx):
            err(f"native {native}: texture-table row gone")
        if not re.search(rf"\{{ {native}, BILLBOARD_MODEL_", gfx):
            err(f"native {native}: model/anim row gone")

# 4. palette tables: same species, same order, count define matches, order == graphics ID order
def table_rows(name):
    m = re.search(r"static const u16 " + name + r"\[FOLLOWER_HG_BULK_COUNT\]\[16\] = \{\n(.*?)\n\};", fm, re.S)
    return re.findall(r"/\* (OBJ_EVENT_GFX_[A-Z0-9_]+) \*/ \{ ((?:0x[0-9A-F]{4}(?:, )?){16}) \}", m.group(1)) if m else None


pris = table_rows("sFollowerHGBulkPristinePalettes")
shiny = table_rows("sFollowerHGBulkShinyPalettes")
if pris is None or shiny is None:
    err("bulk palette tables not found")
else:
    pn, sn = [g for g, _ in pris], [g for g, _ in shiny]
    if pn != sn:
        err("pristine and shiny tables differ in species/order")
    count = int(re.search(r"#define FOLLOWER_HG_BULK_COUNT (\d+)", fm).group(1))
    if count != len(pn):
        err(f"FOLLOWER_HG_BULK_COUNT={count} but {len(pn)} rows")
    first = re.search(r"#define FOLLOWER_HG_BULK_FIRST_ID (OBJ_EVENT_GFX_[A-Z0-9_]+)", fm).group(1)
    if first not in gfx_txt:
        err("FOLLOWER_HG_BULK_FIRST_ID not in gfx list")
    else:
        i = gfx_txt.index(first)
        block = gfx_txt[i:i + len(pn)]
        if block != pn:
            diff = next((k for k, (a, b) in enumerate(zip(block, pn)) if a != b), None)
            err(f"graphics ID order != palette row order (first mismatch at row {diff}: id list {block[diff] if diff is not None else '?'} vs palette {pn[diff] if diff is not None else '?'}) - the O(1) lookup would return another species' colors")
    bulk_gfx = set(pn)
    orig9 = {"CHIKORITA", "BAYLEEF", "MEGANIUM", "CYNDAQUIL", "QUILAVA", "TYPHLOSION", "TOTODILE", "CROCONAW", "FERALIGATR"}
    for g in hg_gfx:
        name = g[len("OBJ_EVENT_GFX_"):]
        if name in orig9:
            if f"[FOLLOWER_HG_{name}]" not in fm:
                err(f"{g}: original-9 palette rows missing")
        elif g not in bulk_gfx:
            err(f"{g}: no bulk palette row")
    for g, vals in pris + shiny:
        if len(vals.split(", ")) != 16:
            err(f"{g}: palette not 16 entries")

n64 = sum(1 for m in gfx_model.values() if m == "64x64")
print(f"checked {len(hg_gfx)} HG follower species ({n64} on the 64x64 model) | table rows {len(rows)} | form rows {len(form_rows)} | bulk palette rows {len(pris) if pris else 0}")
if errors:
    print(f"FAILED ({len(errors)}):")
    for e in errors[:40]:
        print("  " + e)
    sys.exit(1)
print("OK")
