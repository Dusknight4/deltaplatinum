#!/usr/bin/env python3
"""Installs HeartGold's follower-talk data into the Platinum project (idempotent; reads the HG repo, never writes it).

  1. res/prebuilt/data/follower_talk/talk_cond.narc     <- HG files/a/2/2/0 (condition tables)      verbatim copy
     res/prebuilt/data/follower_talk/talk_action.narc   <- HG files/a/2/2/1 (1023 actions)          verbatim copy
     res/prebuilt/data/follower_talk/talk_anim.narc     <- HG files/a/2/2/2 (108 animations)        verbatim copy
     res/prebuilt/data/follower_talk/talk_species.narc  <- HG files/a/2/3/1 (species category byte) verbatim copy
  2. res/text/follower_talk.json  <- HG files/msgdata/msg/msg_0265.gmm (759 messages, same order, so HG message id N
     is entry N-1) + 2 accessory messages from HG msg_0040 (#32 and #95) appended as entries 759 and 760.
  3. res/prebuilt/data/mmodel/fldeff.narc: appends HG's emote bubble resources as members 201..216:
       201 = HG fldeff 130 (p_ballon_org, the 32x32 billboard quad)
       202 = HG fldeff 150 (frame sequence; all 14 HG emote sequences are byte-identical)
       203..216 = HG fldeff 2..15 (one 2-frame 32x32 texture per emote type 0..13)
     The 201 original members are untouched. A pristine copy is kept as backups/fldeff_vanilla_201_members.narc.
Run with --check to only verify an existing install.
"""
import json
import os
import re
import html
import shutil
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from hg_talk_data import HG, PLAT, narc_members  # noqa: E402

CHECK = "--check" in sys.argv
DATA_DIR = os.path.join(PLAT, "res/prebuilt/data/follower_talk")
COPIES = [("files/a/2/2/0", "talk_cond.narc"), ("files/a/2/2/1", "talk_action.narc"),
          ("files/a/2/2/2", "talk_anim.narc"), ("files/a/2/3/1", "talk_species.narc")]
TEXT_JSON = os.path.join(PLAT, "res/text/follower_talk.json")
FLDEFF = os.path.join(PLAT, "res/prebuilt/data/mmodel/fldeff.narc")
FLDEFF_ORIG_COUNT = 201
HG_EMOTE_MEMBERS = [130, 150] + list(range(2, 16))  # model, sequence, 14 textures


def fail(msg):
    print("ERROR: " + msg)
    sys.exit(1)


def write_narc(path, members):
    """Writes a NARC with no file names, exactly like Platinum's prebuilt ones (4-byte aligned members)."""
    fat = b""
    img = b""
    for m in members:
        start = len(img)
        img += m
        fat += struct.pack("<II", start, start + len(m))
        img += b"\xff" * ((-len(img)) % 4)
    btaf = b"BTAF" + struct.pack("<IHH", 12 + len(fat), len(members), 0) + fat
    btnf = b"BTNF" + struct.pack("<I", 16) + struct.pack("<IHH", 4, 0, 1)
    gmif = b"GMIF" + struct.pack("<I", 8 + len(img)) + img
    body = btaf + btnf + gmif
    hdr = b"NARC" + struct.pack("<HHIHH", 0xFFFE, 0x0100, 16 + len(body), 16, 3)
    open(path, "wb").write(hdr + body)


def gmm_messages(path):
    t = open(path, encoding="utf-8").read()
    out = {}
    for m in re.finditer(r'<row id="[^"]+" index="(\d+)">.*?<language name="English">(.*?)</language>', t, re.S):
        out[int(m.group(1))] = html.unescape(m.group(2))
    return out


def to_lines(text):
    # HG's gmm stores line controls as the two-character escapes \n \r \f; Platinum's JSON uses real control
    # characters, one array element per line (msgenc concatenates them).
    text = text.replace("\\n", "\n").replace("\\r", "\r").replace("\\f", "\f")
    lines, cur = [], ""
    for ch in text:
        cur += ch
        if ch in "\n\r\f":
            lines.append(cur)
            cur = ""
    if cur or not lines:
        lines.append(cur)
    return lines if len(lines) > 1 else lines[0]  # Platinum convention: plain string for single-line messages


def main():
    ok = True
    # 1. verbatim NARC copies
    os.makedirs(DATA_DIR, exist_ok=True)
    for src, dst in COPIES:
        s = open(os.path.join(HG, src), "rb").read()
        p = os.path.join(DATA_DIR, dst)
        if os.path.exists(p) and open(p, "rb").read() == s:
            print("ok      %s (%d bytes)" % (dst, len(s)))
            continue
        if CHECK:
            print("MISSING %s" % dst)
            ok = False
            continue
        open(p, "wb").write(s)
        print("wrote   %s (%d bytes)" % (dst, len(s)))

    # 2. text bank
    msgs = gmm_messages(os.path.join(HG, "files/msgdata/msg/msg_0265.gmm"))
    extra = gmm_messages(os.path.join(HG, "files/msgdata/msg/msg_0040.gmm"))
    texts = [msgs[i] for i in range(len(msgs))] + [extra[32], extra[95]]
    doc = {"key": 0x6A3B, "messages": [{"id": "FollowerTalk_Text_%04d" % i, "en_US": to_lines(t)}
                                       for i, t in enumerate(texts)]}
    new = json.dumps(doc, ensure_ascii=False, indent=2) + "\n"
    old = open(TEXT_JSON, encoding="utf-8").read() if os.path.exists(TEXT_JSON) else None
    if old == new:
        print("ok      follower_talk.json (%d messages)" % len(texts))
    elif CHECK:
        print("STALE   follower_talk.json")
        ok = False
    else:
        open(TEXT_JSON, "w", encoding="utf-8", newline="\n").write(new)
        print("wrote   follower_talk.json (%d messages)" % len(texts))

    # 3. emote resources appended to fldeff.narc
    hg_fldeff = narc_members(os.path.join(HG, "files/a/1/0/3"))
    want = [hg_fldeff[i] for i in HG_EMOTE_MEMBERS]
    cur = narc_members(FLDEFF)
    if len(cur) == FLDEFF_ORIG_COUNT + len(want) and cur[FLDEFF_ORIG_COUNT:] == want:
        print("ok      fldeff.narc (%d members, emotes at %d..%d)" % (len(cur), FLDEFF_ORIG_COUNT, len(cur) - 1))
    elif len(cur) == FLDEFF_ORIG_COUNT:
        if CHECK:
            print("MISSING emote members in fldeff.narc")
            ok = False
        else:
            orig = os.path.join(PLAT, "backups", "fldeff_vanilla_201_members.narc")
            if not os.path.exists(orig):
                os.makedirs(os.path.dirname(orig), exist_ok=True)
                shutil.copyfile(FLDEFF, orig)
            write_narc(FLDEFF, cur + want)
            again = narc_members(FLDEFF)
            if again[:FLDEFF_ORIG_COUNT] != cur or again[FLDEFF_ORIG_COUNT:] != want:
                fail("fldeff.narc round-trip check failed")
            print("wrote   fldeff.narc (%d -> %d members)" % (len(cur), len(again)))
    else:
        fail("fldeff.narc has %d members; expected %d (vanilla) or %d (installed)"
             % (len(cur), FLDEFF_ORIG_COUNT, FLDEFF_ORIG_COUNT + len(want)))
    if not ok:
        sys.exit(1)


if __name__ == "__main__":
    main()
