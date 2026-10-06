# ENTRY P63: HeartGold's follower-talk data, copied verbatim from the HG repo by
# tools/follower_talk/install_talk_data.py (see src/follower_mon_talk.c).
prebuilt_files = [
    'talk_cond.narc',
    'talk_action.narc',
    'talk_anim.narc',
    'talk_species.narc',
]

foreach f : prebuilt_files
    nitrofs_files += fs.copyfile(f)
endforeach
