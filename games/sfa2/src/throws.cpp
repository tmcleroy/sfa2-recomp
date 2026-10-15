// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy
//
// Street Fighter Alpha 2: the throw test, written again as C++ (a rewrite: lift.h). A throw's move asks it with
// D0 (how far in front of the thrower its reach is measured from: -20 for the usual throws) and D1 (how far that
// reach goes: 20); the training mode's throw range (training.cpp) draws the same.
#include "sfa2.h"

namespace cps2recomp {

namespace {
    using namespace sfa2;

    constexpr uint32_t kThrowReachMore = 0x256; // byte: not zero: 4 more of reach (but in the move that follows a throw)
    constexpr uint32_t kHeldBy = 0x38;          // the thrown player's: who holds it (as kOpponent)

    void Answer(M68K &c, bool yes) {
        c.d[0] = yes ? 1 : 0;
        c.Logic(c.d[0], 2); // (moveq)
    }

    uint16_t Neg16(M68K &c, uint16_t value) {
        return static_cast<uint16_t>(c.Sub(0, value, 1, false, false));
    }

    // Is the near edge of the other's push box within D1 of the point D0 in front of the thrower? (`turnedBy`: the
    // byte of the other's record that says its box is turned about.) D0 is left the distance.
    bool WithinReach(M68K &c, uint32_t thrower, uint32_t other, uint32_t turnedBy, bool more) {
        if (Mem8(c, thrower + kFacesRight)) {
            SetLow16(c.d[0], Neg16(c, static_cast<uint16_t>(c.d[0])));
        }
        SetLow16(c.d[0], static_cast<uint16_t>(c.Add(c.d[0], Mem16(c, thrower + kX), 1, false, false)));
        c.a[3] = Mem32(c, other + kPushBoxes);
        SetLow8(c.d[2], Mem8(c, c.a[1] + kFramePush));
        SetLow16(c.d[2], static_cast<uint16_t>(static_cast<int16_t>(static_cast<int8_t>(c.d[2])) << 3));
        c.a[3] += static_cast<uint32_t>(static_cast<int16_t>(c.d[2]));
        uint16_t edge = static_cast<uint16_t>(c.Sub(Mem16(c, c.a[3] + kBoxX), Mem16(c, c.a[3] + kBoxHalfWidth), 1, false, false));
        SetLow16(c.d[2], edge);
        if (more) {
            const bool inThrow = Mem16(c, thrower + kState) == 0x0200 && (Mem8(c, thrower + kAction) == 0x0E || Mem8(c, thrower + kAction) == 0x10);
            if (!inThrow && Mem8(c, thrower + kThrowReachMore)) {
                SetLow16(c.d[1], static_cast<uint16_t>(c.Add(c.d[1], 4, 1, false, false)));
            }
        }
        if (Mem8(c, other + turnedBy)) {
            edge = Neg16(c, edge);
        }
        edge = static_cast<uint16_t>(c.Add(edge, Mem16(c, other + kX), 1, false, false));
        SetLow16(c.d[2], edge);
        uint16_t apart = static_cast<uint16_t>(c.Sub(c.d[0], edge, 1, false, false));
        if (apart & 0x8000) {
            apart = Neg16(c, apart);
        }
        SetLow16(c.d[0], apart);
        c.Sub(apart, c.d[1], 1, false, false, false);
        return apart <= static_cast<uint16_t>(c.d[1]);
    }

    // The other can be thrown: it has boxes to be hit in, in this frame
    bool HasBoxes(M68K &c, uint32_t other) {
        c.a[1] = Mem32(c, other + kFrame);
        c.d[2] = Mem32(c, c.a[1] + 0x08) & 0xFFFFFF00;
        return c.d[2] != 0;
    }

    // 0261AA. Thrown: the other is held (a Custom Combo it was in costs it a level of gauge), and the throw begins
    void Thrown(M68K &c) {
        const uint32_t thrower = c.a[6], other = c.a[4];
        if (Mem8(c, other + kCustomCombo)) {
            const uint16_t gauge = static_cast<uint16_t>(c.Sub(Mem16(c, other + kSpecialGauge), 0x30, 1, false, false));
            SetRam16(other + kSpecialGauge, (gauge & 0x8000) ? 0 : gauge);
        }
        SetRam32(other + kState, 0x02060000);
        SetRam8(other + 0x67, 0xFF);
        SetRam8(thrower + 0x67, 1);
        SetRam8(other + kKnocksDown, 1);
        SetRam8(other + 0x3A, 4);
        SetRam8(thrower + 0x284, 0);
        SetRam8(thrower + 0x285, 0);
        SetRam8(thrower + 0x28F, 0);
        SetRam16(other + kHeldBy, static_cast<uint16_t>(thrower));
        Call(c, 0x01F93C);
        c.d[0] = 0;
        for (const uint32_t record : {thrower, other}) {
            SetRam8(record + kAttackOut, 0);
        }
        SetRam8(thrower + kThrowing, 0);
        SetRam8(other + kThrowing, 0);
        SetRam16(other + 0x158, 0);
        SetRam16(other + 0x15A, 0);
        SetRam8(other + 0x299, 0);
        SetRam16(other + 0x29A, 0);
        if (Mem8(c, other + kCharacter) == 0x15 || Mem8(c, other + kCharacter) == 0x16) {
            SetRam8(other + 0x3A, 0);
        }
        Answer(c, true);
    }

    // 0260E8. Can the player at A6 throw the other now? D0 1 (and the other is held) or 0.
    void ThrowTest(M68K &c) {
        const uint32_t thrower = c.a[6];
        if (Mem8(c, thrower + 0x3E)) {
            // A helper's throw (02604C): only from its own move
            if (Mem16(c, thrower + kState) != 0x0200 || Mem8(c, thrower + kAction) != 0x0E) {
                return Answer(c, false);
            }
            if (!Mem8(c, thrower + kCustomCombo) && Ram8(Var(c, 0x10E))) {
                return Answer(c, false);
            }
            c.a[4] = static_cast<uint32_t>(static_cast<int16_t>(Mem16(c, thrower + kOpponent)));
            const uint32_t other = c.a[4];
            if (Mem8(c, other + kThrowProtection) || !HasBoxes(c, other) || Mem8(c, c.a[1] + kFramePose)) {
                return Answer(c, false);
            }
            if (!WithinReach(c, thrower, other, 0x131, false) || Mem8(c, other + kInAir)) {
                return Answer(c, false);
            }
            return Thrown(c);
        }
        c.a[4] = static_cast<uint32_t>(static_cast<int16_t>(Mem16(c, thrower + kOpponent)));
        const uint32_t other = c.a[4];
        // The other: on its feet, free, not in the air
        if (Mem8(c, other + kNoThrow) || Mem8(c, other + kThrowProtection) || Mem16(c, other + kState) != 0x0200) {
            return Answer(c, false);
        }
        if (Ram8(Var(c, 0x10E)) || Mem8(c, other + kInAir) || !HasBoxes(c, other)) {
            return Answer(c, false);
        }
        if (!WithinReach(c, thrower, other, kFacesRight, true)) {
            return Answer(c, false);
        }
        Thrown(c);
    }
} // namespace

void RegisterThrows() {
    AddRewrite(0x0260E8, "ThrowTest", ThrowTest, 0, 428);
}

} // namespace cps2recomp
