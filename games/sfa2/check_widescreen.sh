#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Tommy McLeroy
#
# Checks the widescreen feature (src/widescreen.cpp). On every stage a training session is walked to both of
# the stage's ends with widescreen on, and the wide picture's outer 64 columns are looked at there: a column of a
# plane the game never painted shows as a run of black down it. The columns with 40 or more black pixels running
# down them (above the floor) are counted at each end, and more than kMost of them fails (the fighters standing in
# the corner are too small to make such a run). The ladder
# is played with it on, and no instruction may be left to the interpreter. That the game with the feature off is
# what it was is `make check`'s and the movies' business. (The plan check: see below.)
#
# With invisible walls (widescreen_walls) the game must be the board's: the ladder is played with widescreen and the
# walls on and with neither, and the game's memory, video and object memory and registers are compared every 4,000
# frames (the frame hash would not do: it takes the picture in, which is meant to differ). And the stages are walked
# to their ends with the walls on: there the wide view keeps to the stage, and its 128 columns beyond the board's
# picture are looked at for unpainted columns as above. Run from the repository's top.
GAME=sfa2
port=${B:-build/$GAME}/$GAME
status=0
dir=$(mktemp -d)
for stage in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18; do
    SFA2_HOME=$dir/home_s$stage SFA2_WIDE_CHECK=1 $port --rom "${ROM:-rom}" --headless --frames 3300 --features free_play,widescreen --movie games/$GAME/movies/corners.txt \
        --keys "3:ftraining_stage=$stage,5:ctraining_start" --shot-every 1100:$dir/s${stage}_ 2> $dir/plan$stage.txt > /dev/null &
    SFA2_HOME=$dir/home_w$stage $port --rom "${ROM:-rom}" --headless --frames 3300 --features free_play,widescreen,widescreen_walls \
        --movie games/$GAME/movies/corners.txt --keys "3:ftraining_stage=$stage,5:ctraining_start" --shot-every 1100:$dir/w${stage}_ > /dev/null 2>&1 &
    [ $((stage % 6)) -eq 0 ] && wait
done
wait
# Where the 8-pixel plane is drawn into the map a strip at a time, the wide picture's sides come from the stage's
# plan: it has to give what the game itself drew (the run counts the tiles compared and those that differ)
compared=$(cat $dir/plan*.txt | grep "^widescreen:" | awk '{n += $4} END {print n + 0}')
differ=$(cat $dir/plan*.txt | grep "^widescreen:" | awk '{n += $(NF - 1)} END {print n + 0}')
echo "the stage's plan against the map: $compared tiles, $differ differ"
[ "$compared" -gt 0 ] && [ "$differ" -eq 0 ] || status=1
python3 - "$dir" <<'PY' || status=1
import struct, sys, zlib
kMost = 20  # columns (Ryu's stage's sky, painted 480 wide, leaves some)
kMostWalls = 30  # (the stage's own art at Guy's left end: a black girder and its shadow, 22 columns)
def png(path):
    d = open(path, 'rb').read(); i = 8; idat = b''
    while i < len(d):
        n = struct.unpack('>I', d[i:i + 4])[0]; t = d[i + 4:i + 8]; c = d[i + 8:i + 8 + n]
        if t == b'IHDR': w, h, _, kind = struct.unpack('>IIBB', c[:10])
        if t == b'IDAT': idat += c
        i += 12 + n
    raw = zlib.decompress(idat); bpp = 3 if kind == 2 else 4; stride = w * bpp; rows = []; prev = bytearray(stride)
    for y in range(h):
        f = raw[y * (stride + 1)]; line = bytearray(raw[y * (stride + 1) + 1:(y + 1) * (stride + 1)])
        for x in range(stride):
            a = line[x - bpp] if x >= bpp else 0; b = prev[x]; c = prev[x - bpp] if x >= bpp else 0
            if f == 1: line[x] = (line[x] + a) & 255
            elif f == 2: line[x] = (line[x] + b) & 255
            elif f == 3: line[x] = (line[x] + (a + b) // 2) & 255
            elif f == 4:
                p = a + b - c; pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                line[x] = (line[x] + (a if pa <= pb and pa <= pc else b if pb <= pc else c)) & 255
        rows.append(line); prev = line
    return w, bpp, rows
bad = 0
for stage in range(1, 19):
    shares = []
    for frame, side in ((2200, 0), (3300, 1)):
        w, bpp, rows = png('%s/s%d_%06d.png' % (sys.argv[1], stage, frame))
        x0 = 0 if side == 0 else w - 64
        columns = 0
        for x in range(x0, x0 + 64):
            run = best = 0
            for y in range(24, 180):
                run = run + 1 if sum(rows[y][x * bpp:x * bpp + 3]) < 12 else 0
                best = max(best, run)
            columns += best >= 40
        shares.append(columns)
    worst = max(shares)
    print('stage %2d: unpainted columns at the left end %2d, at the right end %2d%s' % (stage, shares[0], shares[1], '  TOO MANY' if worst > kMost else ''))
    bad += worst > kMost
    # (invisible walls: at the left end the wide picture's 128 columns beyond the board's are on the right, and the
    # other way round)
    shares = []
    for frame, x0 in ((2200, 384), (3300, 0)):
        w, bpp, rows = png('%s/w%d_%06d.png' % (sys.argv[1], stage, frame))
        columns = 0
        for x in range(x0, x0 + 128):
            run = best = 0
            for y in range(24, 180):
                run = run + 1 if sum(rows[y][x * bpp:x * bpp + 3]) < 12 else 0
                best = max(best, run)
            columns += best >= 40
        shares.append(columns)
    worst = max(shares)
    print('stage %2d, invisible walls: unpainted columns at the left end %2d, at the right end %2d%s' % (stage, shares[0], shares[1], '  TOO MANY' if worst > kMostWalls else ''))
    bad += worst > kMostWalls
sys.exit(1 if bad else 0)
PY
# Invisible walls: the game is the board's
differ=0
for frame in 4000 8000 12000 16000 20000 24000 28000; do
    for run in off walls; do
        features=free_play,no_damage,one_hit
        [ $run = walls ] && features=$features,widescreen,widescreen_walls
        SFA2_HOME=$dir/home_$run$frame $port --rom "${ROM:-rom}" --headless --frames $((frame + 2)) --movie games/$GAME/movies/ladder.txt \
            --features $features --dump-ram $frame:$dir/$run$frame.bin > /dev/null 2>&1 &
    done
    wait
    for part in "" .video .objects .regs; do
        cmp -s $dir/off$frame.bin$part $dir/walls$frame.bin$part || { echo "invisible walls: frame $frame$part differs from the board's"; differ=1; }
    done
done
[ $differ -eq 0 ] && echo "invisible walls: the game's memory and registers are the board's at 7 places of the ladder" || status=1
rm -rf "$dir"
out=$($port --rom "${ROM:-rom}" --headless --frames 30000 --movie games/$GAME/movies/ladder.txt --features free_play,no_damage,one_hit,widescreen --stats 2>&1) || status=1
echo "$out" | grep "ran in the interpreter"
[ $status -eq 0 ] && echo "ok: widescreen" || echo "FAILED"
exit $status
