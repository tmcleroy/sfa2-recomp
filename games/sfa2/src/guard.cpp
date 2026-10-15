// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy
//
// Street Fighter Alpha 2: the test of whether a player can guard, written again as C++ (a rewrite: lift.h). A
// player goes into its guarding pose, and a hit is guarded, only when this says so: something of the other
// side's is near enough (a thing thrown within 80 across; the other player's attack box within 88 of the player,
// or the other player within 160 while attacking with no box out yet) and the player holds away from it.
#include "sfa2.h"

namespace cps2recomp {

namespace {
    using namespace sfa2;

    // (neg.w and the 68000's word arithmetic, flags and all, as the original leaves them)
    uint16_t Neg16(M68K &c, uint16_t value) {
        return static_cast<uint16_t>(c.Sub(0, value, 1, false, false));
    }

    enum class Range { Near, Far };

    // 023510. The other player (or a helper) at A4: near enough to guard?
    Range OtherPlayer(M68K &c) {
        const uint32_t player = c.a[6], other = c.a[4] & 0xFFFFFF;
        // (in the air, only what comes from the air is guarded, or a helper's, or a Custom Combo's)
        if (Mem8(c, player + kInAir) && !Mem8(c, other + kHelperOf) && !Mem8(c, other + kCustomCombo) && !Mem8(c, other + kInAir)) {
            return Range::Far;
        }
        if (Mem8(c, other + kCustomCombo) || Mem8(c, other + kThrowing)) {
            return Range::Near;
        }
        if (!Mem8(c, other + kAttackOut)) {
            return Range::Far;
        }
        // 0235A6. Its attack's boxes, in this frame and those that follow in the move, within 88 of the player
        c.a[0] = Mem32(c, other + kFrame);
        if (static_cast<int8_t>(Mem8(c, c.a[0] + kFrameHarmless)) < 0) {
            return Range::Far;
        }
        for (;;) {
            c.a[1] = Mem32(c, other + kAttackBoxes);
            const uint8_t number = Mem8(c, c.a[0] + kFrameAttack);
            SetLow8(c.d[0], number);
            if (number) {
                c.a[1] += static_cast<uint32_t>(static_cast<int16_t>(static_cast<int8_t>(number)) << 5);
                SetLow16(c.d[0], static_cast<uint16_t>(static_cast<int16_t>(static_cast<int8_t>(number)) << 5));
                uint16_t across = Mem16(c, c.a[1] + kBoxX);
                SetLow16(c.d[0], across);
                if (Mem8(c, other + kFacesRight)) {
                    across = Neg16(c, across);
                }
                across = static_cast<uint16_t>(c.Add(across, Mem16(c, other + kX), 1, false, false));
                across = static_cast<uint16_t>(c.Sub(across, Mem16(c, player + kX), 1, false, false));
                if (across & 0x8000) {
                    across = Neg16(c, across);
                }
                SetLow16(c.d[0], across);
                const uint16_t reach = static_cast<uint16_t>(c.Add(Mem16(c, c.a[1] + kBoxHalfWidth), 0x58, 1, false, false));
                SetLow16(c.d[1], reach);
                c.Sub(across, reach, 1, false, false, false);
                if (across <= reach) {
                    return Range::Near;
                }
            }
            // (the next frame of the move, unless this was its last)
            c.a[0] += kFrameSize;
            if (static_cast<int16_t>(Mem16(c, c.a[0] + 2)) < 0) {
                break;
            }
        }
        // 023542. No box out: the other player itself within 160 across
        uint16_t apart = static_cast<uint16_t>(c.Sub(Mem16(c, other + kX), Mem16(c, player + kX), 1, false, false));
        apart = static_cast<uint16_t>(c.Add(apart, 0xA0, 1, false, false));
        SetLow16(c.d[0], apart);
        c.Sub(apart, 0x140, 1, false, false, false);
        return apart > 0x140 ? Range::Far : Range::Near;
    }

    // A thing thrown at A4 is near: within 80 across of the player (and not one that need not be guarded)
    bool ThrownNear(M68K &c, uint32_t thrown, bool sideTest) {
        const uint32_t player = c.a[6];
        if (Mem8(c, thrown + kActive) != 1 || static_cast<int8_t>(Mem8(c, thrown + kThrownGuardSkip)) < 0) {
            return false;
        }
        if (sideTest) {
            const uint8_t side = Mem8(c, player + kSide);
            SetLow8(c.d[0], side);
            if (side == Mem8(c, thrown + kSide)) {
                return false;
            }
        }
        uint16_t apart = static_cast<uint16_t>(c.Sub(Mem16(c, thrown + kX), Mem16(c, player + kX), 1, false, false));
        apart = static_cast<uint16_t>(c.Add(apart, 0x50, 1, false, false));
        SetLow16(c.d[0], apart);
        c.Sub(apart, 0xA0, 1, false, false, false);
        return apart < 0xA0;
    }

    // A list of things thrown, from `first`: near? (D6 counts down as the game's dbra does; A4 is left on the
    // one found, or past the last)
    bool ThrownListNear(M68K &c, uint32_t first, int count, bool sideTest) {
        c.d[6] = static_cast<uint32_t>(count - 1);
        c.a[4] = first;
        for (;;) {
            if (ThrownNear(c, c.a[4], sideTest)) {
                return true;
            }
            c.a[4] += kThrownSize;
            const uint16_t left = static_cast<uint16_t>(c.d[6] - 1);
            SetLow16(c.d[6], left);
            if (left == 0xFFFF) {
                return false;
            }
        }
    }

    void Answer(M68K &c, bool yes) {
        c.d[0] = yes ? 1 : 0;
        c.Logic(c.d[0], 2); // (moveq)
    }

    // 023554. Something is near: the answer is whether the player holds away from it (or guards by itself)
    bool Decide(M68K &c) {
        const uint32_t player = c.a[6];
        // (Zangief's and Dhalsim's other forms never guard in the air)
        if (Mem8(c, player + kInAir) && (Mem8(c, player + kCharacter) == 0x15 || Mem8(c, player + kCharacter) == 0x16)) {
            return false;
        }
        if ((Mem8(c, player + kHeld) & 1) || Mem8(c, player + kAutoGuardOn)) {
            if (Mem8(c, player + kAutoGuardOn)) {
                SetRam8(player + 0x246, 1);
                SetRam8(player + 0x249, 1);
            }
            return true;
        }
        // (a setting of the game's own that has everyone guard)
        return Ram8(Var(c, 0x85)) && (Ram8(Var(c, 0x5F65)) & 0x20);
    }
    // 023510 as a routine of its own: the record at A4 near, and then the answer
    bool OtherAnswer(M68K &c) {
        const bool yes = OtherPlayer(c) == Range::Near && Decide(c);
        Answer(c, yes);
        return yes;
    }

    // 02343C. Can the player at A6 guard now? D0 1 (and the flags not zero) if something of the other side's is
    // near enough and the player holds away from it (or guards by itself); D0 0 if not.
    void GuardRange(M68K &c) {
        const uint32_t player = c.a[6];
        if (Mem8(c, player + kCustomCombo)) {
            return Answer(c, false);
        }
        const uint16_t mode = Ram16(Var(c, kPlayMode));
        SetLow16(c.d[0], mode);
        static constexpr uint16_t kJumps[] = {0x000A, 0x004A, 0x004A, 0x008C, 0x004A}; // (the table at 023452)
        SetLow16(c.d[1], mode / 2 < 5 ? kJumps[mode / 2] : 0);
        if (mode == 0) {
            if (ThrownListNear(c, Var(c, 0x1400), kThrownRecords, true)) {
                return Answer(c, Decide(c));
            }
            c.a[4] = static_cast<uint32_t>(static_cast<int16_t>(Mem16(c, player + kOpponent)));
            OtherAnswer(c);
            return;
        }
        if (ThrownListNear(c, Var(c, 0x1C00), kThrownOtherRecords, false)) {
            return Answer(c, Decide(c));
        }
        c.a[4] = Var(c, 0xC00);
        if (OtherAnswer(c) || mode == 6) {
            return;
        }
        c.a[4] = Var(c, 0x1000);
        OtherAnswer(c);
    }

    // Anything of the other side's near enough to guard, whatever the player holds
    bool AnythingNear(M68K &c) {
        const uint32_t player = c.a[6];
        const uint16_t mode = Ram16(Var(c, kPlayMode));
        if (ThrownListNear(c, Var(c, mode == 0 ? 0x1400 : 0x1C00), mode == 0 ? kThrownRecords : kThrownOtherRecords, mode == 0)) {
            return true;
        }
        if (mode == 0) {
            c.a[4] = static_cast<uint32_t>(static_cast<int16_t>(Mem16(c, player + kOpponent)));
            return OtherPlayer(c) == Range::Near;
        }
        c.a[4] = Var(c, 0xC00);
        if (OtherPlayer(c) == Range::Near) {
            return true;
        }
        c.a[4] = Var(c, 0x1000);
        return mode != 6 && OtherPlayer(c) == Range::Near;
    }
} // namespace

// For the training dummy: is something of player 1's near enough for the player at `player` to guard, whatever
// it holds? (The game's own test, with the registers it changes kept and put back.)
bool AttackInRange(M68K &c, uint32_t player) {
    M68K::Kept kept = c.Keep();
    c.a[5] = 0xFF8000;
    c.a[6] = player;
    bool yes = !Mem8(c, player + kCustomCombo) && AnythingNear(c);
    if (yes && Mem8(c, player + kInAir) && (Mem8(c, player + kCharacter) == 0x15 || Mem8(c, player + kCharacter) == 0x16)) {
        yes = false;
    }
    c.PutBack(kept);
    return yes;
}

void RegisterGuard() {
    AddRewrite(kGuardRange, "GuardRange", GuardRange, 0, 897);
}

} // namespace cps2recomp
