// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy
//
// Street Fighter Alpha 2: parts of a fight. A frame of the round written again as C++ (a rewrite: lift.h): the
// clock, and what happens once the round is decided. The damage a hit does, looked at as the game takes it off
// (hooks: the damage routine is the game's own code still), for the combo readout and the cheats.
#include "sfa2.h"

#include <algorithm>

namespace cps2recomp {

bool TrainingMode();                                         // training.cpp
bool BattleNoClock();                                        // battle.cpp
void TrainingHit(int player, unsigned damage, bool chip);    //
void TrainingDamageTaken(int player);                        //

namespace {
    using namespace sfa2;

    // From A5 (bytes): the round
    constexpr int32_t kRoundClock = 0x109, kRoundClockPart = 0x10A; // seconds left; frames left of the second
    constexpr int32_t kRoundResult = 0x10C;   // 0 going on; bit 0 player 1 has won, bit 1 player 2 (3: both down)
    constexpr int32_t kRoundTimeOver = 0x10D; // not zero: the clock ran out
    constexpr int32_t kRoundPaused = 0x8A;    // not zero: the round is held (the game's own pause)
    constexpr int32_t kClockHeld = 0x125, kClockHeld2 = 0x124;

    // 005820. A frame of a round being fought: the clock, and once the round is decided (by the hit tests, which
    // set kRoundResult, or by the clock), on to its end.
    void RoundFighting(M68K &c) {
        if (Ram8(Var(c, kRoundPaused))) {
            SetRam8(Var(c, 0x12D), 0);
            SetRam8(Var(c, 0xBD), 0xFF);
            SetRam8(Var(c, 0x10E), 0xFF);
            return;
        }
        if (!Ram8(Var(c, kClockHeld)) && !Ram8(Var(c, kClockHeld2)) && !Ram8(Var(c, kRoundResult))) {
            const uint8_t part = static_cast<uint8_t>(Ram8(Var(c, kRoundClockPart)) - 1);
            SetRam8(Var(c, kRoundClockPart), part);
            if (part == 0) {
                Call(c, 0x0066B0); // (the frames of the next second, by the speed setting)
                // The clock stands still with the cheat, and in a training session
                if (!FeatureOn("infinite_time") && !TrainingMode() && !BattleNoClock()) {
                    const uint8_t seconds = static_cast<uint8_t>(Ram8(Var(c, kRoundClock)) - 1);
                    SetRam8(Var(c, kRoundClock), seconds);
                    if (seconds & 0x80) {
                        // Time is up (who has won is worked out at 0065C4)
                        SetRam8(Var(c, kRoundClock), 0);
                        SetRam8(Var(c, kRoundTimeOver), 0xFF);
                        Call(c, 0x0065C4);
                    }
                }
            }
        }
        const uint8_t result = Ram8(Var(c, kRoundResult));
        SetLow8(c.d[0], result);
        if (!result) {
            return;
        }
        // Decided
        if (result == 3) {
            // (both down at once: a draw)
            SetRam8(Var(c, kClockHeld2), 0);
            SetRam8(Var(c, 0x15A), 1);
        } else {
            Call(c, 0x0058DE); // (the winner's count of rounds won, and the match's)
        }
        if (!Ram8(Var(c, kRoundTimeOver))) {
            SetRam8(Var(c, 0x11C), 0x3C);
            SetRam8(Var(c, 0x11D), 3);
            SetRam8(Var(c, 0x11E), 3);
        }
        SetRam16(Var(c, 0x0C), static_cast<uint16_t>(Ram16(Var(c, 0x0C)) + 2)); // (the round's next step)
        c.d[0] = 0;
        SetRam32(Var(c, 0x10), 0);
        SetRam8(Var(c, 0x8F), 1);
        SetRam8(Var(c, 0x107), 1);
        SetRam8(Var(c, 0x10E), 1);
        for (const int32_t offset : {0x12D, 0x136, 0x53D, 0x93D, 0xD3D, 0x113D}) {
            SetRam8(Var(c, offset), 0);
        }
        Call(c, 0x01F89E);
        Call(c, 0x00735C);
    }

    // From A5 (bytes): the KO's slow motion (RoundFighting sets it when a round is decided by a knock-out): pieces
    // left, frames left of this piece and how many a piece has; a frame of it in which nothing moves
    constexpr int32_t kSlowPieces = 0x11C, kSlowFrames = 0x11D, kSlowEach = 0x11E, kStillFrame = 0x15C;
    constexpr int32_t kNothingMoves = 0x10F; // set by a round's step: none of the fight's parts run this frame
    constexpr int32_t kRoundFrames = 0xB4, kShakeLeft = 0x123;
    constexpr uint32_t kRoundSteps = 0x005594; // the round's steps by kRoundStep (words, from here): 0 begins
                                               // (005640), 2 fought (RoundFighting), 4 decided (0059D0), 6 (005B62)

    // 00559C. A frame of a round: its step, then, unless the step says not, every part of the fight in its order
    void RoundFrame(M68K &c) {
        const uint16_t step = Ram16(Var(c, 0x0C));
        SetLow16(c.d[0], step);
        SetLow16(c.d[1], Code16(c, kRoundSteps + step));
        Call(c, kRoundSteps + static_cast<uint32_t>(static_cast<int16_t>(c.d[1] & 0xFFFF)));
        c.d[0] = 0;
        if (Ram8(Var(c, kNothingMoves))) {
            c.Logic(Ram8(Var(c, kNothingMoves)), 0); // (tst.b)
            return;
        }
        // The knock-out's slow motion: of each piece's frames, only the last moves
        if (Ram8(Var(c, kSlowPieces))) {
            c.d[0] = 1;
            const uint8_t frames = static_cast<uint8_t>(c.Sub(Ram8(Var(c, kSlowFrames)), 1, 0, false, false)); // (subq.b)
            SetRam8(Var(c, kSlowFrames), frames);
            if (!frames) {
                SetRam8(Var(c, kSlowPieces), static_cast<uint8_t>(c.Sub(Ram8(Var(c, kSlowPieces)), 1, 0, false, false)));
                SetRam8(Var(c, kSlowFrames), Ram8(Var(c, kSlowEach)));
                c.d[0] = 0;
            }
        }
        SetRam8(Var(c, kStillFrame), static_cast<uint8_t>(c.d[0]));
        SetRam8(Var(c, kRoundFrames), static_cast<uint8_t>(c.Add(Ram8(Var(c, kRoundFrames)), 1, 0, false, false))); // (addq.b)
        Call(c, 0x01F738); // (the computer players' own upkeep)
        Call(c, 0x01FF0A); // the players (in an order by chance, each frame; not in a still frame)
        Call(c, 0x027482); // (which player is drawn in front)
        Call(c, 0x007504); // the players' recent frames kept (the shadows of a Custom Combo), by the play mode
        Call(c, 0x007684); // the clock held while a super's flash is on
        Call(c, 0x01DFCC); // the things thrown
        Call(c, 0x00ECCE); // PushApart (collision.cpp)
        Call(c, 0x00FDA0); // (the objects listed from A5+6202)
        Call(c, 0x01E308); // (the objects from A5+2100, from A5+3700, from A5+3F00)
        Call(c, 0x01E582);
        Call(c, 0x01E640);
        Call(c, 0x01A7F6); // the picture's shake
        Call(c, 0x01E6AE); // (the objects from A5+3500)
        if (Ram8(Var(c, kShakeLeft))) {
            SetRam8(Var(c, kShakeLeft), static_cast<uint8_t>(c.Sub(Ram8(Var(c, kShakeLeft)), 1, 0, false, false))); // (subq.b: flags too)
        }
        Call(c, 0x010352); // AllHitTests (collision.cpp)
        Call(c, 0x0197CC); // (nothing: an empty routine)
        Call(c, 0x0127A4); // (the score, and messages such as "First attack")
    }

    int PlayerOf(uint32_t record) {
        record &= 0xFFFFFF;
        return record == Player(0) ? 0 : record == Player(1) ? 1 : -1;
    }

    // D3 is the damage to be taken off the health of the player `victim` (the hit's damage is damage.cpp's). The
    // readout counts it; the cheats change it; in a training session nobody is knocked out.
    bool DamageTo(M68K &c, uint32_t victim, bool chip) {
        victim &= 0xFFFFFF;
        const int player = PlayerOf(victim);
        if (player < 0) {
            return false;
        }
        uint16_t damage = static_cast<uint16_t>(c.d[3]);
        TrainingHit(player, damage, chip);
        if (FeatureOn(player == 0 ? "no_damage" : "no_damage_2")) {
            damage = 0;
        } else if (player == 1 && FeatureOn("one_hit")) {
            damage = kHealthFull + 1;
        }
        if (TrainingMode()) {
            damage = static_cast<uint16_t>(std::min<int>(damage, std::max<int>(0, static_cast<int16_t>(Ram16(victim + kHealth)))));
        }
        SetLow16(c.d[3], damage);
        TrainingDamageTaken(player);
        return false;
    }

    // 026518, in a throw: D3 is the throw's damage to the player at A4 (unless the round is over)
    bool ThrowDamage(M68K &c) {
        if (Ram8(Var(c, 0x10E)) || Ram8(Var(c, kRoundTimeOver))) {
            return false;
        }
        return DamageTo(c, c.a[4], false);
    }

} // namespace

void RegisterFight() {
    AddRewrite(0x005820, "RoundFighting", RoundFighting, kFreeFlags, 129);
    AddRewrite(0x00559C, "RoundFrame", RoundFrame, kFreeFlags, 600);
    AddHook(0x026518, "ThrowDamage", ThrowDamage);
}

} // namespace cps2recomp
