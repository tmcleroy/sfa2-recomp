#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Tommy McLeroy
#
# The rewrites to leave out, layer by layer, so that the inner ones are checked on their own calls (one list a
# line, comma-separated; check_rewrites.sh runs every movie once with nothing left out and once for each line)
# (a round's frame encloses nearly all the fight: every line leaves it out)
echo "RoundFrame"
echo "RoundFrame,PlayersStep,AllHitTests"
echo "RoundFrame,PlayersStep,AllHitTests,PlayersAgainstPlayers"
echo "RoundFrame,PlayersStep,AllHitTests,PlayersAgainstPlayers,HitTest"
echo "RoundFrame,PlayersStep,PlayerFighting"
echo "RoundFrame,PlayersStep,PlayerFighting,PlayerReeling,PlayerInControl"
echo "RoundFrame,PlayersStep,PlayerFighting,PlayerReeling,PlayerInControl,ReelOnFloor,ReelGoesOn"
# (the players' action routines are each other's siblings: with all of them left out, what they call (GuardRange,
# KeepIn) is checked on its own calls)
echo "RoundFrame,PlayersStep,PlayerFighting,PlayerReeling,PlayerInControl,Standing,Crouching,Walking,Turning,Jumping,Attacking,Guarding,SpecialMove,Stunned,Recovered,ReelFrozen,ReelGoesOn,ReelStanding,ReelGuarding,ReelHops,ReelDown,ReelOnFloor,ReelBegins"
