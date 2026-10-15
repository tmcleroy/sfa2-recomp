// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy
//
// Street Fighter Alpha 2: the players' hit test, written again as C++ (rewrites: lift.h). The game's own code for
// these routines no longer runs; `--check-rewrites` runs both and compares.
//
// An attack hits when the attacker's attack box (named by the animation frame it is showing) overlaps one of the
// three boxes the victim can be hit in: head, body, legs (named by the victim's frame too). The test notes which
// parts were hit and where, and then the game's code for a hit that landed takes over (010564 on).
#include "sfa2.h"

namespace cps2recomp {

namespace {
    using namespace sfa2;

    // What a hit test leaves for the code that deals with the hit, from A5 (words unless said)
    constexpr int32_t kOverlapAttackX = -0x5808, kOverlapVictimX = -0x5806; // the middles of the two boxes last tested
    constexpr int32_t kOverlapAttackY = -0x5804, kOverlapVictimY = -0x5802;
    constexpr int32_t kHitDistance[3] = {-0x5800, -0x57FE, -0x57FC}; // per part (head, body, legs): how far apart the middles were (FFFF: not hit)
    constexpr int32_t kHitMiddles[3] = {-0x57FA, -0x57F2, -0x57EA};  // per part: four words: attack x, victim x, attack y, victim y
    constexpr int32_t kHitParts = -0x57D8;                           // byte: bit 0 head, 1 body, 2 legs
    // From A5: the round's result (bit 0 player 1 has won, bit 1 player 2), and what stops the hit tests
    constexpr int32_t kRoundResult = 0x10C, kNoHitTests = 0x15C;
    // In a player's record, for the hit test
    constexpr uint32_t kHitByNumber = 0x25C; // bytes, one for each side: the number of the attack that last hit it
    constexpr uint32_t kHitByCount = 0x280;  // bytes, one for each side: how many times that attack has hit it
    constexpr uint32_t kJuggle = 0x5D;       // byte: how many hits it has taken in the air (an attack needs more "juggle" to hit)
    constexpr uint32_t kSideIndex = 0x5A;    // byte: which side, as an index into those
    constexpr uint32_t kIsThrown = 0x08;     // byte: 8 for a thing thrown
    // In an attack's entry, after its box
    constexpr uint32_t kAttackNumber = 0x12, kAttackMostHits = 0x18;
    constexpr uint32_t kFrameJuggle = 0x13;
    constexpr uint32_t kAtEdge = 0x13C;      // 1 at the picture's left edge, 2 at its right (widescreen.cpp, KeepIn)  // in an animation frame: added to the juggle count it can hit through

    uint16_t Neg16(M68K &c, uint16_t value) {
        return static_cast<uint16_t>(c.Sub(0, value, 1, false, false));
    }

    // 0113AA. Do the box at A3 (belonging to the object at A6) and the box at A2 (of the object at A1) overlap? The
    // answer is in the flags, as the game's callers read it: "higher" means they do not. Also left behind: the
    // boxes' middles in the stage.
    void BoxesOverlap(M68K &c) {
        const uint32_t attacker = c.a[6], attackBox = c.a[3], victim = c.a[1], victimBox = c.a[2];
        // Across: a box's offset is the other way about when its object is turned
        uint16_t attackX = Mem16(c, attackBox + kBoxX);
        if (Mem8(c, attacker + kFacesRight)) {
            attackX = Neg16(c, attackX);
        }
        uint16_t victimX = Mem16(c, victimBox + kBoxX);
        if (Mem8(c, victim + kFacesRight)) {
            victimX = Neg16(c, victimX);
        }
        attackX = static_cast<uint16_t>(c.Add(attackX, Mem16(c, attacker + kX), 1, false, false));
        victimX = static_cast<uint16_t>(c.Add(victimX, Mem16(c, victim + kX), 1, false, false));
        SetRam16(Var(c, kOverlapAttackX), attackX);
        SetRam16(Var(c, kOverlapVictimX), victimX);
        uint16_t apart = static_cast<uint16_t>(c.Sub(victimX, attackX, 1, false, false));
        if (apart & 0x8000) {
            apart = Neg16(c, apart);
        }
        uint16_t half = Mem16(c, attackBox + kBoxHalfWidth);
        uint16_t reach = static_cast<uint16_t>(c.Add(Mem16(c, victimBox + kBoxHalfWidth), half, 1, false, false));
        SetLow16(c.d[0], half);
        SetLow16(c.d[1], apart);
        SetLow16(c.d[2], reach);
        c.Sub(apart, reach, 1, false, false, false); // (cmp: the flags the caller reads)
        if (apart > reach) {
            return;
        }
        // Up and down
        const uint16_t attackY = static_cast<uint16_t>(c.Add(Mem16(c, attackBox + kBoxY), Mem16(c, attacker + kY), 1, false, false));
        const uint16_t victimY = static_cast<uint16_t>(c.Add(Mem16(c, victimBox + kBoxY), Mem16(c, victim + kY), 1, false, false));
        SetRam16(Var(c, kOverlapAttackY), attackY);
        SetRam16(Var(c, kOverlapVictimY), victimY);
        apart = static_cast<uint16_t>(c.Sub(victimY, attackY, 1, false, false));
        if (apart & 0x8000) {
            apart = Neg16(c, apart);
        }
        half = Mem16(c, attackBox + kBoxHalfHeight);
        reach = static_cast<uint16_t>(c.Add(Mem16(c, victimBox + kBoxHalfHeight), half, 1, false, false));
        SetLow16(c.d[0], half);
        SetLow16(c.d[1], apart);
        SetLow16(c.d[2], reach);
        c.Sub(apart, reach, 1, false, false, false);
    }

    // 011324. Nothing has been hit yet
    void ClearHit(M68K &c) {
        for (const int32_t distance : kHitDistance) {
            SetRam16(Var(c, distance), 0xFFFF);
        }
        SetRam8(Var(c, kHitParts), 0);
        for (const int32_t middles : kHitMiddles) {
            SetRam32(Var(c, middles), 0);
            SetRam32(Var(c, middles + 4), 0);
        }
        c.d[0] = 0;
        c.Logic(0, 2);
    }

    bool Overlaps(M68K &c) {
        BoxesOverlap(c);
        return !c.Condition(2); // (not "higher")
    }

    // One of the victim's parts: is the box its frame names for it (numbered in the table at `table`) hit? If so,
    // where is noted for that part.
    // (`reg`: the data register the box's number is worked in: D0 for a player's attack, D3 for a thing thrown's)
    void TestPart(M68K &c, uint32_t frameByte, uint32_t table, int part, int reg = 0) {
        const uint32_t victim = c.a[1];
        const uint8_t number = Mem8(c, c.a[0] + frameByte);
        SetLow8(c.d[reg], number);
        if (!number) {
            return;
        }
        const uint16_t offset = static_cast<uint16_t>(static_cast<int16_t>(static_cast<int8_t>(number)) << 3);
        SetLow16(c.d[reg], offset);
        c.a[2] = Mem32(c, victim + table) + static_cast<uint32_t>(static_cast<int16_t>(offset));
        if (!Overlaps(c)) {
            return;
        }
        SetRam8(Var(c, kHitParts), static_cast<uint8_t>(c.Add(Ram8(Var(c, kHitParts)), 1u << part, 0, false, false)));
        // The two middles, and how far apart they are across and up, added
        const uint16_t attackX = Ram16(Var(c, kOverlapAttackX)), victimX = Ram16(Var(c, kOverlapVictimX));
        SetRam16(Var(c, kHitMiddles[part]), attackX);
        SetRam16(Var(c, kHitMiddles[part] + 2), victimX);
        uint16_t apartX = static_cast<uint16_t>(c.Sub(attackX, victimX, 1, false, false));
        if (apartX & 0x8000) {
            apartX = Neg16(c, apartX);
        }
        SetRam16(Var(c, kHitDistance[part]), apartX);
        const uint16_t attackY = Ram16(Var(c, kOverlapAttackY)), victimY = Ram16(Var(c, kOverlapVictimY));
        SetRam16(Var(c, kHitMiddles[part] + 4), attackY);
        SetRam16(Var(c, kHitMiddles[part] + 6), victimY);
        uint16_t apartY = static_cast<uint16_t>(c.Sub(attackY, victimY, 1, false, false));
        if (apartY & 0x8000) {
            apartY = Neg16(c, apartY);
        }
        SetLow16(c.d[0], apartY);
        SetRam16(Var(c, kHitDistance[part]), static_cast<uint16_t>(c.Add(Ram16(Var(c, kHitDistance[part])), apartY, 1, false, false)));
    }

    // 0103AA. Does the attack of the player at A6 hit the player at A1? If it does, the game's code for a hit that
    // landed runs.
    void HitTest(M68K &c) {
        const uint32_t attacker = c.a[6], victim = c.a[1];
        // The victim has to be able to be hit
        if (Mem8(c, victim + 0x25B) || Mem8(c, victim + 0x273) || Mem8(c, victim + 0x13B)) {
            return;
        }
        // A victim in the air that has been hit there already: only an attack that can juggle so many hits
        c.a[0] = Mem32(c, attacker + kFrame);
        c.d[0] = 0;
        bool juggleTest = true;
        if (Mem8(c, attacker + 0x3E) && (Mem8(c, victim + 0x28D) || Mem8(c, victim + 0x287))) {
            juggleTest = false;
        }
        if (juggleTest) {
            const uint8_t juggle = static_cast<uint8_t>(c.Add(0, Mem8(c, c.a[0] + kFrameJuggle), 0, false, false));
            SetLow8(c.d[0], juggle);
            if (juggle < Mem8(c, victim + kJuggle)) {
                c.Sub(juggle, Mem8(c, victim + kJuggle), 0, false, false, false);
                return;
            }
        }
        // The attack the frame names
        const uint8_t number = Mem8(c, c.a[0] + kFrameAttack);
        SetLow8(c.d[0], number);
        if (!number) {
            return;
        }
        const uint16_t offset = static_cast<uint16_t>(static_cast<int16_t>(static_cast<int8_t>(number)) << 5);
        SetLow16(c.d[0], offset);
        c.a[3] = Mem32(c, attacker + kAttackBoxes) + static_cast<uint32_t>(static_cast<int16_t>(offset));
        // Not the attack that last hit the victim from this side, and not once too often
        const uint8_t attackNumber = Mem8(c, c.a[3] + kAttackNumber) & 0x7F;
        SetLow8(c.d[0], attackNumber);
        c.d[1] = 0;
        const uint8_t side = Mem8(c, attacker + kSideIndex);
        c.a[0] = victim + kHitByNumber;
        SetLow8(c.d[1], Mem8(c, c.a[0] + side));
        if (attackNumber == (c.d[1] & 0xFF)) {
            c.Sub(attackNumber, c.d[1] & 0xFF, 0, false, false, false);
            return;
        }
        if (Mem8(c, attacker + kIsThrown) != 8) {
            c.d[1] = 0;
            c.a[0] = victim + kHitByCount;
            const uint8_t times = Mem8(c, c.a[0] + side);
            SetLow8(c.d[1], times);
            c.Sub(times, Mem8(c, c.a[3] + kAttackMostHits), 0, false, false, false);
            if (times > Mem8(c, c.a[3] + kAttackMostHits)) {
                return;
            }
        }
        // The victim's three parts
        c.a[0] = Mem32(c, victim + kFrame);
        TestPart(c, kFrameHead, kHeadBoxes, 0);
        TestPart(c, kFrameBody, kBodyBoxes, 1);
        TestPart(c, kFrameLegs, kLegBoxes, 2);
        if (!Ram8(Var(c, kHitParts))) {
            return;
        }
        // A hit: what follows
        SetRam8(victim + 0x239, 0);
        Call(c, kHitLands);
        if (!Mem8(c, attacker + 0x295)) {
            Call(c, 0x0108E2);
            Call(c, 0x010DF0);
            Call(c, 0x0112CE);
        }
    }

    // 010FD8. The things thrown against each other: two of different sides whose boxes for this (the frames' +08,
    // in each one's first table) meet both go (the game's 00F89A says how), and the first looks no further
    void ThrownAgainstThrown(M68K &c) {
        const uint8_t count = Ram8(Var(c, 0x611A));
        SetLow8(c.d[7], count);
        if (!count || count == 1) {
            if (count) {
                SetLow8(c.d[7], 0);
            }
            return;
        }
        SetLow16(c.d[7], static_cast<uint16_t>(count - 1));
        c.a[4] = Var(c, 0x6202);
        c.d[4] = c.a[4];
        for (;;) {
            c.a[4] = c.d[4] - 2;
            c.a[6] = static_cast<uint32_t>(static_cast<int16_t>(Mem16(c, c.a[4])));
            c.d[4] = c.a[4];
            const uint8_t inner = Ram8(Var(c, 0x611A));
            SetLow8(c.d[6], inner);
            if (!inner) {
                return;
            }
            SetLow16(c.d[6], static_cast<uint16_t>(static_cast<int16_t>(static_cast<int8_t>(inner - 1))));
            c.a[4] = Var(c, 0x6202);
            c.d[5] = c.a[4];
            const uint32_t one = c.a[6];
            for (;;) {
                c.a[4] = c.d[5] - 2;
                c.a[1] = static_cast<uint32_t>(static_cast<int16_t>(Mem16(c, c.a[4])));
                c.d[5] = c.a[4];
                const uint32_t other = c.a[1];
                // (0: the next of the others; 1: done with this one)
                const int next = [&]() -> int {
                    if (other == static_cast<uint32_t>(static_cast<int16_t>(one))) {
                        return 0;
                    }
                    if (Mem8(c, one + 0x02) == 0x17) {
                        return 1;
                    }
                    if (Mem8(c, other + 0x02) == 0x17) {
                        return 0;
                    }
                    if (Mem8(c, one + kActive) != 1) {
                        return 1;
                    }
                    if (Mem8(c, other + kActive) != 1) {
                        return 0;
                    }
                    SetLow8(c.d[0], Mem8(c, one + kSide));
                    if (Mem8(c, one + kSide) == Mem8(c, other + kSide)) {
                        return 0;
                    }
                    c.a[0] = Mem32(c, one + kFrame);
                    const uint8_t mine = Mem8(c, c.a[0] + 0x08);
                    SetLow8(c.d[2], mine);
                    if (!mine) {
                        return 1;
                    }
                    c.a[0] = Mem32(c, other + kFrame);
                    const uint8_t theirs = Mem8(c, c.a[0] + 0x08);
                    SetLow8(c.d[3], theirs);
                    if (!theirs) {
                        return 0;
                    }
                    c.a[3] = Mem32(c, one + kThrownTables);
                    SetLow16(c.d[0], Mem16(c, c.a[3]));
                    c.a[3] += static_cast<uint32_t>(static_cast<int16_t>(c.d[0]));
                    SetLow16(c.d[2], static_cast<uint16_t>(static_cast<int16_t>(static_cast<int8_t>(mine)) << 3));
                    c.a[3] += static_cast<uint32_t>(static_cast<int16_t>(c.d[2]));
                    c.a[2] = Mem32(c, other + kThrownTables);
                    SetLow16(c.d[0], Mem16(c, c.a[2]));
                    c.a[2] += static_cast<uint32_t>(static_cast<int16_t>(c.d[0]));
                    SetLow16(c.d[3], static_cast<uint16_t>(static_cast<int16_t>(static_cast<int8_t>(theirs)) << 3));
                    c.a[2] += static_cast<uint32_t>(static_cast<int16_t>(c.d[3]));
                    if (!Overlaps(c)) {
                        return 0;
                    }
                    Call(c, 0x00F89A);
                    return 1;
                }();
                if (next == 1) {
                    break;
                }
                const uint16_t left = static_cast<uint16_t>(c.d[6] - 1);
                SetLow16(c.d[6], left);
                if (left == 0xFFFF) {
                    break;
                }
            }
            const uint16_t left = static_cast<uint16_t>(c.d[7] - 1);
            SetLow16(c.d[7], left);
            if (left == 0xFFFF) {
                return;
            }
        }
    }

    // 011092. Each thing thrown (the list the game keeps of those in use) against the player of the other side
    void ThrownAgainstPlayers(M68K &c) {
        const uint8_t count = Ram8(Var(c, 0x611A));
        SetLow8(c.d[7], count);
        if (!count) {
            return;
        }
        SetLow16(c.d[7], static_cast<uint16_t>(static_cast<int16_t>(static_cast<int8_t>(count - 1))));
        c.a[4] = Var(c, 0x6202);
        for (;;) {
            c.a[4] -= 2;
            c.a[6] = static_cast<uint32_t>(static_cast<int16_t>(Mem16(c, c.a[4])));
            const uint32_t thrown = c.a[6];
            [&] {
                if (Mem8(c, thrown + kActive) != 1) {
                    return;
                }
                const uint32_t victim = c.a[1] = Var(c, Mem8(c, thrown + kSide) ? 0x400 : 0x800);
                if (Mem8(c, victim + 0x25B) || Mem8(c, victim + 0x273) || Mem8(c, victim + 0x13B)) {
                    return;
                }
                c.a[0] = Mem32(c, thrown + kFrame);
                c.d[0] = 0;
                if (!(Mem8(c, thrown + 0x3E) && Mem8(c, victim + 0x28D))) {
                    const uint8_t juggle = static_cast<uint8_t>(c.Add(0, Mem8(c, c.a[0] + 0x0F), 0, false, false));
                    SetLow8(c.d[0], juggle);
                    if (juggle < Mem8(c, victim + kJuggle)) {
                        return;
                    }
                }
                const uint8_t number = Mem8(c, c.a[0] + kFrameAttack);
                SetLow8(c.d[2], number);
                if (!number) {
                    return;
                }
                // (its attacks are in a table at an offset from its tables' header)
                c.a[3] = Mem32(c, thrown + kThrownTables);
                SetLow16(c.d[0], Mem16(c, c.a[3] + 2));
                c.a[3] += static_cast<uint32_t>(static_cast<int16_t>(c.d[0]));
                SetLow16(c.d[2], static_cast<uint16_t>(static_cast<int16_t>(static_cast<int8_t>(number)) << 5));
                c.a[3] += static_cast<uint32_t>(static_cast<int16_t>(c.d[2]));
                ClearHit(c);
                for (int part = 0; part < 3; part++) {
                    c.a[0] = Mem32(c, victim + kFrame);
                    static constexpr uint32_t kFrameBytes[3] = {kFrameHead, kFrameBody, kFrameLegs}, kTables[3] = {kHeadBoxes, kBodyBoxes, kLegBoxes};
                    TestPart(c, kFrameBytes[part], kTables[part], part, 3);
                }
                if (!Ram8(Var(c, kHitParts))) {
                    return;
                }
                // A hit
                SetRam8(victim + 0x239, 1);
                Call(c, kHitLands);
                const uint8_t kind = Mem8(c, thrown + 0x02);
                if (kind == 0x17) {
                    SetRam8(victim + 0x239, 0);
                    if (Mem8(c, victim + kGuardedHit) != 0xFF && !Mem8(c, victim + kInAir)) {
                        SetRam8(victim + kGuardedHit, 0);
                    }
                }
                Call(c, 0x0108E2);
                if (Mem8(c, victim + kGuardedHit) != 0xFF && !(Ram16(victim + kHealth) & 0x8000)) {
                    c.d[1] = 0x800000;
                    SetLow8(c.d[0], Mem8(c, thrown + 0x02));
                    if (!((c.d[1] >> (c.d[0] & 31)) & 1)) {
                        const bool helper = Mem8(c, thrown + 0x3E);
                        SetRam8(Var(c, 0x11C), helper ? 7 : 0x0F);
                        SetRam8(Var(c, 0x11D), helper ? 1 : 2);
                        SetRam8(Var(c, 0x11E), helper ? 1 : 2);
                    }
                }
                Call(c, 0x010DF0);
                Call(c, 0x0112CE);
                // The thing thrown has one hit fewer left in it (but the kind that goes on through)
                c.d[1] = 0x800000;
                SetLow8(c.d[0], Mem8(c, thrown + 0x02));
                if (!((c.d[1] >> (c.d[0] & 31)) & 1)) {
                    SetRam8(thrown + kActive, static_cast<uint8_t>(Ram8(thrown + kActive) + 1));
                    SetRam32(thrown + kState, 0x02020000);
                    const uint16_t left = static_cast<uint16_t>(Ram16(thrown + kHealth) - 1);
                    SetRam16(thrown + kHealth, left);
                    if (left & 0x8000) {
                        SetRam32(thrown + kState, 0x04000000);
                    }
                }
            }();
            const uint16_t left = static_cast<uint16_t>(c.d[7] - 1);
            SetLow16(c.d[7], left);
            if (left == 0xFFFF) {
                return;
            }
        }
    }

    // 010370. The two players' attacks against each other; then a player with no health left has lost the round
    void PlayersAgainstPlayers(M68K &c) {
        ClearHit(c);
        c.a[6] = Var(c, 0x400);
        c.a[1] = Var(c, 0x800);
        HitTest(c);
        ClearHit(c);
        c.a[6] = Var(c, 0x800);
        c.a[1] = Var(c, 0x400);
        HitTest(c);
        if (Ram16(Var(c, 0x450)) & 0x8000) {
            SetRam8(Var(c, kRoundResult), Ram8(Var(c, kRoundResult)) | 2);
        }
        if (Ram16(Var(c, 0x850)) & 0x8000) {
            SetRam8(Var(c, kRoundResult), Ram8(Var(c, kRoundResult)) | 1);
        }
    }

    // 00ECCE. The two players cannot stand in each other: when their pushing boxes overlap (up and down, and across
    // by more than nothing) each is moved half of it apart; but one pressed against the picture's edge, or one on
    // the floor against one in the air, stays and the other moves all of it. `+298` notes that they touched.
    void PushApart(M68K &c) {
        const uint32_t one = c.a[6] = Var(c, 0x400), two = c.a[4] = Var(c, 0x800);
        SetRam8(one + 0x298, 0);
        SetRam8(two + 0x298, 0);
        c.a[0] = Mem32(c, one + kFrame);
        c.a[1] = Mem32(c, two + kFrame);
        c.d[0] = Mem8(c, c.a[0] + kFramePush);
        if (!c.d[0]) {
            return;
        }
        c.d[1] = Mem8(c, c.a[1] + kFramePush);
        if (!c.d[1]) {
            return;
        }
        c.a[0] = Mem32(c, one + kPushBoxes) + (c.d[0] << 3);
        c.a[1] = Mem32(c, two + kPushBoxes) + (c.d[1] << 3);
        SetLow16(c.d[0], c.d[0] << 3);
        SetLow16(c.d[1], c.d[1] << 3);
        // Up and down: the boxes' middles nearer than their half heights added
        const uint16_t y1 = static_cast<uint16_t>(Mem16(c, c.a[0] + kBoxY) + Mem16(c, one + kY));
        const uint16_t y2 = static_cast<uint16_t>(Mem16(c, c.a[1] + kBoxY) + Mem16(c, two + kY));
        SetLow16(c.d[0], y1);
        c.d[2] = 0;
        uint16_t apart = static_cast<uint16_t>(y2 - y1);
        if (apart & 0x8000) {
            apart = static_cast<uint16_t>(0u - apart);
            c.d[2] = 1;
        }
        const uint16_t tall = static_cast<uint16_t>(Mem16(c, c.a[0] + kBoxHalfHeight) + Mem16(c, c.a[1] + kBoxHalfHeight));
        SetLow16(c.d[0], tall);
        SetLow16(c.d[1], static_cast<uint16_t>(apart - tall));
        if (!(static_cast<uint16_t>(apart - tall) & 0x8000)) {
            return;
        }
        // Across: by how much they overlap (D2 1 when the second is to the left of the first)
        uint16_t x1 = Mem16(c, c.a[0] + kBoxX);
        if (Mem8(c, one + kFacesRight)) {
            x1 = static_cast<uint16_t>(0u - x1);
        }
        x1 = static_cast<uint16_t>(x1 + Mem16(c, one + kX));
        uint16_t x2 = Mem16(c, c.a[1] + kBoxX);
        if (Mem8(c, two + kFacesRight)) {
            x2 = static_cast<uint16_t>(0u - x2);
        }
        x2 = static_cast<uint16_t>(x2 + Mem16(c, two + kX));
        SetLow16(c.d[0], x1);
        c.d[2] = 0;
        apart = static_cast<uint16_t>(x2 - x1);
        if (apart & 0x8000) {
            apart = static_cast<uint16_t>(0u - apart);
            c.d[2] = 1;
        }
        const uint16_t wide = static_cast<uint16_t>(Mem16(c, c.a[0] + kBoxHalfWidth) + Mem16(c, c.a[1] + kBoxHalfWidth));
        SetLow16(c.d[0], wide);
        const int16_t over = static_cast<int16_t>(apart - wide);
        SetLow16(c.d[1], static_cast<uint16_t>(over));
        if (over > 0) {
            return;
        }
        SetRam8(one + 0x298, static_cast<uint8_t>(Ram8(one + 0x298) + 1));
        SetRam8(two + 0x298, static_cast<uint8_t>(Ram8(two + 0x298) + 1));
        // (the overlap as a 16.16 distance)
        c.d[1] = static_cast<uint32_t>(static_cast<uint16_t>(-over)) << 16 | (c.d[1] >> 16);
        auto move = [&](uint32_t player, uint32_t by) { SetRam32(player + kX, Ram32(player + kX) + by); };
        auto negative = [](uint32_t value) { return 0u - value; };
        const bool secondLeft = (c.d[2] & 0xFFFF) != 0;
        const uint8_t edge1 = Mem8(c, one + kAtEdge), edge2 = Mem8(c, two + kAtEdge);
        const bool air1 = Mem8(c, one + kInAir), air2 = Mem8(c, two + kInAir);
        // One against the picture's edge: the other moves it all, away from that edge
        auto firstStays = [&] { move(two, Mem8(c, one + kAtEdge) == 1 ? c.d[1] : (c.d[1] = negative(c.d[1]))); };
        auto secondStays = [&] { move(one, Mem8(c, two + kAtEdge) == 1 ? c.d[1] : (c.d[1] = negative(c.d[1]))); };
        if (edge1) {
            if (!edge2 || (!air1 && air2)) {
                return firstStays();
            }
            return secondStays();
        }
        if (edge2) {
            return secondStays();
        }
        auto halves = [&] {
            c.d[1] >>= 1;
            c.d[0] = negative(c.d[1]);
            if (secondLeft) {
                const uint16_t first = Mem16(c, one + kX);
                SetLow16(c.d[2], first);
                if (!(first < Mem16(c, two + kX))) {
                    c.d[0] = negative(c.d[0]);
                    c.d[1] = negative(c.d[1]);
                }
            }
            move(one, c.d[0]);
            move(two, c.d[1]);
        };
        if (air1) {
            if (air2) {
                return halves();
            }
            if (!secondLeft) {
                c.d[1] = negative(c.d[1]);
            }
            return move(one, c.d[1]);
        }
        if (air2) {
            if (secondLeft) {
                c.d[1] = negative(c.d[1]);
            }
            return move(two, c.d[1]);
        }
        halves();
    }

    // 010352. A frame's hit tests: the players' attacks against each other, the things thrown against each other,
    // the things thrown against the players
    void AllHitTests(M68K &c) {
        if (Ram8(Var(c, kNoHitTests))) {
            return;
        }
        const uint8_t thrown = Ram8(Var(c, 0x53D)) | Ram8(Var(c, 0x93D));
        SetLow8(c.d[0], thrown);
        if (thrown) {
            return;
        }
        PlayersAgainstPlayers(c);
        ThrownAgainstThrown(c);
        ThrownAgainstPlayers(c);
    }
} // namespace

void RegisterCollision() {
    // (each charged the original's average time, the routines it calls as plain C++ included)
    AddRewrite(0x010352, "AllHitTests", AllHitTests, kFreeFlags, 1050);
    AddRewrite(0x010370, "PlayersAgainstPlayers", PlayersAgainstPlayers, kFreeFlags, 928);
    AddRewrite(0x011092, "ThrownAgainstPlayers", ThrownAgainstPlayers, kFreeFlags);
    AddRewrite(0x010FD8, "ThrownAgainstThrown", ThrownAgainstThrown, kFreeFlags);
    AddRewrite(0x00ECCE, "PushApart", PushApart, kFreeFlags, 450);
    AddRewrite(0x0103AA, "HitTest", HitTest, kFreeFlags, 205);
    AddRewrite(0x0113AA, "BoxesOverlap", BoxesOverlap, 0, 182);
    AddRewrite(0x011324, "ClearHit", ClearHit, 0, 152);
}

} // namespace cps2recomp
