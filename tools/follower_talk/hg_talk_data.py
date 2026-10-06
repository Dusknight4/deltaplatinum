#!/usr/bin/env python3
"""Decoder for HeartGold's follower-Pokemon "talk" data (read-only access to the sibling HeartGold repo).

HeartGold implements talking to the follower (press A while facing it) as a DATA-DRIVEN engine in overlay 2
(asm/overlay_02_02248728.s: Task_FollowMonInteract and the ov02_0224EF94..ov02_02250738 helpers, undecompiled).
This module decodes the four data files it uses, with the exact field layouts read from that asm:

  files/a/2/2/0  (NARC 222)  condition tables. Member 0 = general table (70 entries = table slots 30..99);
                             member mapsec+1 = that map section's own table (30 entries = slots 0..29).
                             Evaluation order: slots 30-41, then 0-29 (map table), then 42-99. First entry whose
                             conditions all pass wins; its action id (1..1023) is run.
  files/a/2/2/1  (NARC 223)  1023 actions, 52 bytes: 5 steps of {u16 anim, u16 msg, u16 sound, u8 emote, u8 wait}
                             (anim 0xFFFF = end), then u32 yesno, u16 yesAction, u16 noAction, s8 friendshipDelta,
                             s8 moodDelta, u8 accessory, u8 shinyLeaf.
  files/a/2/2/2  (NARC 224)  108 animations, 10 frames x 8 bytes {u8 facing(0=keep,1..4=N,S,W,E; 0xFF=end),
                             u8 duration, s8 dx, s8 dHeight, s8 dz, u8 playSound, pad, pad}.
  files/a/2/3/1  (NARC 233)  member 0: one byte per species (index species-1), the follower "category" byte
                             tested by the condition field 'speccat'.
Messages: files/msgdata/msg/msg_0265.gmm (759 strings, message id N = index N-1).
"""
import html
import os
import re
import struct

HERE = os.path.dirname(os.path.abspath(__file__))
PLAT = os.path.abspath(os.path.join(HERE, "..", ".."))
HG = os.path.dirname(PLAT)


def narc_members(path):
    """Minimal NARC reader: returns the list of member byte strings."""
    d = open(path, "rb").read()
    assert d[:4] == b"NARC", path
    off = struct.unpack_from("<H", d, 0x0C)[0]
    assert d[off:off + 4] == b"BTAF"
    btaf_size = struct.unpack_from("<I", d, off + 4)[0]
    count = struct.unpack_from("<H", d, off + 8)[0]
    fat = [struct.unpack_from("<II", d, off + 12 + 8 * i) for i in range(count)]
    off += btaf_size
    assert d[off:off + 4] == b"BTNF"
    off += struct.unpack_from("<I", d, off + 4)[0]
    assert d[off:off + 4] == b"GMIF"
    img = off + 8
    return [d[img + s: img + e] for s, e in fat]


def bits(v, lo, n):
    return (v >> lo) & ((1 << n) - 1)


def decode_entry(b):
    h8 = struct.unpack_from("<H", b, 8)[0]
    ha = struct.unpack_from("<H", b, 0xA)[0]
    return dict(
        hp=b[0], mood=bits(b[1], 0, 4), friend=bits(b[1], 4, 4),
        nature=bits(b[2], 0, 3), gender=bits(b[2], 3, 2), status=bits(b[2], 5, 3),
        item=bits(b[3], 0, 5), type=bits(b[4], 0, 5), athlon=bits(b[4], 5, 3),
        grass=b[5], speccat=b[6], leaf=b[7],
        weather=bits(h8, 0, 3), facing=bits(h8, 13, 3),
        npcs=bits(ha, 0, 3), time=bits(ha, 3, 3), action=bits(ha, 6, 10),
        map=struct.unpack_from("<H", b, 0xC)[0], metatile=struct.unpack_from("<H", b, 0xE)[0],
        level=bits(b[0x10], 1, 2), npcsprite=bits(b[0x10], 3, 2), hidden=bits(b[0x10], 5, 3),
        chance=b[0x11], flag=struct.unpack_from("<H", b, 0x12)[0],
    )


def decode_table(member):
    return [decode_entry(member[i:i + 20]) for i in range(0, len(member), 20)]


def decode_action(b):
    steps = []
    for i in range(5):
        anim, msg, snd, emote, wait = struct.unpack_from("<HHHBB", b, i * 8)
        if anim == 0xFFFF:
            break
        steps.append(dict(anim=anim, msg=msg, snd=snd, emote=emote, wait=wait))
    yesno, yes, no, friend, mood, acc, leaf = struct.unpack_from("<IHHbbBB", b, 0x28)
    return dict(steps=steps, yesno=yesno, yes=yes, no=no, friend=friend, mood=mood, acc=acc, leaf=leaf)


def decode_anim(b):
    frames = []
    for i in range(10):
        f = struct.unpack_from("<BBbbbB", b, i * 8)
        if f[0] == 0xFF:
            break
        frames.append(dict(facing=f[0], dur=f[1], dx=f[2], dh=f[3], dz=f[4], sound=f[5]))
    return frames


def load_messages():
    t = open(os.path.join(HG, "files/msgdata/msg/msg_0265.gmm"), encoding="utf-8").read()
    out = {}
    for m in re.finditer(r'<row id="[^"]+" index="(\d+)">.*?<language name="English">(.*?)</language>', t, re.S):
        out[int(m.group(1))] = html.unescape(m.group(2))
    return out


def load_mapsec_names():
    t = open(os.path.join(HG, "include/constants/map_sections.h")).read()
    return {int(v, 0): n for n, v in re.findall(r"#define (MAPSEC_\w+)\s+(\S+)", t)}


class HGTalkData:
    def __init__(self):
        a = os.path.join(HG, "files/a")
        self.cond_members = narc_members(os.path.join(a, "2/2/0"))
        self.action_members = narc_members(os.path.join(a, "2/2/1"))
        self.anim_members = narc_members(os.path.join(a, "2/2/2"))
        self.species_members = narc_members(os.path.join(a, "2/3/1"))
        self.general = decode_table(self.cond_members[0])  # slots 30..99
        self.actions = [decode_action(m) for m in self.action_members]  # action id N = index N-1
        self.anims = [decode_anim(m) for m in self.anim_members]  # anim id N = index N-1
        self.species_cat = self.species_members[0]  # index species-1
        self.messages = load_messages()  # message id N = index N-1
        self.mapsec_names = load_mapsec_names()

    def map_table(self, mapsec):
        m = self.cond_members[mapsec + 1] if mapsec + 1 < len(self.cond_members) else b""
        return decode_table(m) if m else []
