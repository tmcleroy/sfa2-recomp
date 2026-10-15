#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Tommy McLeroy
#
# Checks the game's rewritten routines (src/) against the code they replace: every call of each, over the
# game's movies, run both ways and compared (cps2recomp/docs/READABLE.md). Inner rewrites are only called by
# outer ones, so each movie is run again with the outer ones left out. Run from the repository's top.
GAME=sfa2
port=${B:-build/$GAME}/$GAME
movies=games/$GAME/movies
status=0
. cps2recomp/tools/at-once.sh
check() { # movie, frames, features, rewrites to leave out, more of the port's options
    out=$(CPS2RECOMP_SKIP_REWRITES="$4" $port --rom "${ROM:-rom}" --headless --frames "$2" --movie "$movies/$1.txt" --features "$3" $5 --check-rewrites 2>&1)
    failed=$?
    echo "== $1${4:+ (without $4)}${5:+ ($5)}"
    echo "$out" | grep -E "^rewrite|did not" | grep -v " 0 calls"
    echo "$out" | grep -E "differs|calls, [1-9][0-9]* differed" > /dev/null && return 1
    return $failed
}
run() {
    at_once check "$@"
}
for skip in "" $(sh games/$GAME/layers.sh); do
    run versus 3000 free_play "$skip"
    run versus_attacks 7800 free_play "$skip"
    run versus_attacks 7800 free_play,full_gauge,full_gauge_2 "$skip"
    run clash 3600 free_play "$skip"
    run air_guard 12700 free_play "$skip"
    run custom_combo 4200 free_play,full_gauge "$skip"
    run crouch 10800 free_play "$skip"
    run stun 11000 free_play "$skip"
    run throw_tech 5000 free_play "$skip"
    run versus_attacks 4000 free_play "$skip" "--poke FF8482=0A"
    run wall_jump 6000 free_play "$skip" "--poke FF8482=04"
    for who in 09 07 03; do # (Rose, Guy, Charlie: their hits' reactions)
        run versus_attacks 7800 free_play,full_gauge "$skip" "--poke FF8482=$who"
    done
    run ladder 30000 free_play "$skip"
done
# A training session (player 1 sweeps; the dummy's quick rise): the training mode's own lines act inside the round's
# clock and the damage (and what encloses them), so those are left out, and the players' routines are checked
run training_rise 5500 free_play,dummy_getup=1 "RoundFrame,PlayersStep,PlayerFighting,RoundFighting,AllHitTests,PlayersAgainstPlayers,HitTest,TakeDamage" "--keys 3:ftraining_stage=1,5:ctraining_start"
at_once_wait || status=1
[ $status -eq 0 ] && echo "ok: every rewrite does what the game's code does" || echo "FAILED"
exit $status
