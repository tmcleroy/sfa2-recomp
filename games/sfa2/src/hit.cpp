// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy
//
// Street Fighter Alpha 2: a hit landing, written again as C++ (a rewrite: lift.h). The hit test (collision.cpp)
// has found the attack at A3, of the object at A6, touching the player at A1. This sets the player reeling, and
// decides whether the hit is guarded: only from a guarding pose (the victim's animation frame says which), and
// then by what the attack's kind lets be guarded that way (a table of the game's: bit 0 standing, bit 1
// crouching). In the air only things thrown are guarded. A low attack that met the legs before the body cannot
// be guarded standing even when its kind could.
#include "sfa2.h"

namespace cps2recomp {

namespace {
    using namespace sfa2;

    // From A5
    constexpr int32_t kHitGuarded = -0x57D7, kHitParts = -0x57D8;
    constexpr int32_t kHeadDistance = -0x5800, kBodyDistance = -0x57FE, kLegsDistance = -0x57FC;
    // In an attack's entry
    constexpr uint32_t kAttackKind = 0x0D, kAttackStrength = 0x08, kAttackNumber = 0x12, kAttackHitStop = 0x11;
    // In a player's record
    constexpr uint32_t kGuardBits = 0x69;   // the attack's: what guards it (bit 0 standing, bit 1 crouching)
    constexpr uint32_t kHitByNumber = 0x25C, kHitByCount = 0x280;

    constexpr uint32_t kGuardTables[2] = {0x011438, 0x011456}; // by kind; the second for attacks from the air

    // 01076E. Guarded
    void Guarded(M68K &c) {
        const uint32_t attacker = c.a[6], victim = c.a[1], attack = c.a[3];
        if (Mem8(c, attack + kAttackKind) == 0x1D) {
            SetRam8(victim + 0x297, 0x30);
        }
        SetRam8(victim + kReaction, 0xFF);
        SetRam8(Var(c, kHitGuarded), static_cast<uint8_t>(Ram8(Var(c, kHitGuarded)) + 1));
        c.d[1] = Mem8(c, attacker + 0x5A);
        c.a[0] = victim + kHitByCount;
        SetRam8(c.a[0] + c.d[1], 0);
    }

    // 0108CC. The reaction is the attack's kind
    void ReactionByKind(M68K &c) {
        const uint32_t victim = c.a[1], attack = c.a[3];
        SetRam8(victim + kReaction, Mem8(c, attack + kAttackKind));
        if (Ram8(victim + kReaction) == 0x1D) {
            SetRam8(victim + kReaction, 0x12);
        }
    }

    // 0108B6. Not guarded from a guarding pose (but a player guarding by itself guards anyway)
    void NotGuarded(M68K &c) {
        const uint32_t victim = c.a[1], attack = c.a[3];
        if (Mem8(c, victim + kAutoGuardOn)) {
            return Guarded(c);
        }
        if (Mem8(c, attack + kAttackKind) == 4) {
            SetRam8(victim + kKnocksDown, 1);
        }
        ReactionByKind(c);
    }

    // 0107B0. Hit: the reaction the victim goes into
    void Hit(M68K &c) {
        const uint32_t attacker = c.a[6], victim = c.a[1], attack = c.a[3];
        if (const uint8_t special = Mem8(c, attacker + kCustomCombo); special && special < 0x0A) {
            SetRam8(victim + kReaction, 7);
            SetRam8(victim + kKnocksDown, 1);
            return;
        }
        const uint8_t kind = Mem8(c, attack + kAttackKind);
        SetLow16(c.d[0], static_cast<uint16_t>(static_cast<int16_t>(static_cast<int8_t>(kind)) * 2));
        if (Ram8(victim + kReaction) == 0x1D) {
            SetRam8(victim + kReaction, 0x12);
        }
        const uint32_t table = Mem8(c, victim + kInAir) ? 0x010834 : 0x0107F0;
        const uint16_t offset = Code16(c, table + (c.d[0] & 0xFFFF));
        SetLow16(c.d[1], offset);
        switch (table + offset) {
        case 0x0108A4: { // the head hit, nearer than the body: a reaction of its own
            const uint16_t head = Ram16(Var(c, kHeadDistance));
            SetLow16(c.d[1], head);
            if (head < Ram16(Var(c, kBodyDistance))) {
                return ReactionByKind(c);
            }
            SetRam8(victim + kReaction, 1);
            return;
        }
        case 0x0108C6:
            SetRam8(victim + kKnocksDown, 1);
            return ReactionByKind(c);
        case 0x010870: { // (hit in the air: knocked down by a strong one)
            SetRam8(victim + kReaction, 3);
            if (kind == 0x10) {
                return;
            }
            if ((Mem8(c, attack + kAttackStrength) & 0x80) || kind == 7 || kind == 4 || kind == 0x1C) {
                SetRam8(victim + kKnocksDown, 1);
            }
            return;
        }
        default: return ReactionByKind(c);
        }
    }

    // 010564. The hit's landing: the victim reeling, and guarded or not
    void HitLands(M68K &c) {
        const uint32_t attacker = c.a[6], victim = c.a[1], attack = c.a[3];
        c.d[0] = 0;
        SetRam8(attacker + 0x295, 0);
        const uint8_t kind = Mem8(c, attack + kAttackKind);
        SetLow8(c.d[0], kind);
        // What guards this attack, kept in the attacker's record
        c.a[0] = kGuardTables[Mem8(c, attacker + kInAir) ? 1 : 0];
        SetRam8(attacker + kGuardBits, Mem8(c, c.a[0] + kind));
        if (!Mem8(c, attacker + 0x08)) {
            c.d[0] = Mem8(c, attacker + kCharacter);
            SetRam8(attacker + 0x28E, Code8(c, 0x01054C + c.d[0]));
        }
        SetRam8(attacker + 0x5B, 1);
        SetRam8(Var(c, kHitGuarded), 0);
        SetRam32(victim + kState, 0x02020000); // (reeling)
        SetRam8(victim + 0x56, Mem8(c, attack + 0x09) & 0x0F);
        if (!Mem8(c, victim + 0x28D)) {
            SetRam8(victim + 0x28D, Mem8(c, attacker + 0x3E));
        }
        if (kind != 0x1A && !(Mem8(c, attack + kAttackStrength) & 0x80) && Mem8(c, attacker + kInAir)) {
            SetRam8(victim + 0x56, 0);
        }
        SetRam8(victim + 0x57, Mem8(c, attack + (Mem8(c, attacker + 0x3E) ? 0x19 : 0x0A)));
        // Which way the victim is knocked
        SetRam8(victim + 0x66, Mem8(c, attacker + kFacesRight));
        const uint8_t away = Mem8(c, attack + 0x0B);
        SetLow8(c.d[0], away);
        if (!(away & 0x80)) {
            SetRam8(victim + 0x66, static_cast<uint8_t>(Ram8(victim + 0x66) ^ away));
        } else {
            c.d[0] = 0;
            const uint16_t from = Mem16(c, attacker + kX);
            SetLow16(c.d[1], from);
            if (from < Mem16(c, victim + kX)) {
                SetLow8(c.d[0], 1);
            }
            SetRam8(victim + 0x66, static_cast<uint8_t>(c.d[0]));
        }
        // The attack's number noted, so that it does not hit again; and how many times it has hit
        c.d[0] = 0;
        c.a[0] = victim + kHitByNumber;
        SetLow8(c.d[0], Mem8(c, attacker + 0x5A));
        SetRam8(c.a[0] + c.d[0], Mem8(c, attack + kAttackNumber) & 0x7F);
        if (Mem8(c, attacker + 0x08) != 8) {
            c.a[0] = victim + kHitByCount;
            SetRam8(c.a[0] + c.d[0], static_cast<uint8_t>(Ram8(c.a[0] + c.d[0]) + 1));
        }
        // Both frozen for the hit (the attacker for a quarter of it when its attack goes through anything)
        if (!Mem8(c, attacker + kThrownGuardSkip)) {
            uint8_t stop = Mem8(c, attack + kAttackHitStop) & 0x1F;
            SetLow8(c.d[0], stop);
            SetRam8(victim + kHitStop, stop);
            if (Mem8(c, attacker + kCustomCombo)) {
                stop = static_cast<uint8_t>(static_cast<int8_t>(stop) >> 2);
                SetLow8(c.d[0], stop);
            }
            SetRam8(attacker + kHitStop, stop);
        }
        // Guarded? Not with no health left; only from a guarding pose
        if (static_cast<int16_t>(Ram16(victim + kHealth)) < 0) {
            return Hit(c);
        }
        c.a[0] = Mem32(c, victim + kFrame);
        const uint8_t pose = Mem8(c, c.a[0] + kFramePose);
        SetLow8(c.d[0], pose);
        if (!pose) {
            return Hit(c);
        }
        SetRam8(victim + 0x297, 0);
        if (Ram8(Var(c, 0x85)) && (Ram8(Var(c, 0x5F65)) & 0x20)) {
            return Guarded(c);
        }
        SetLow16(c.d[0], static_cast<uint16_t>(pose * 2));
        SetLow16(c.d[1], Code16(c, 0x010682 + (c.d[0] & 0xFFFF)));
        const uint8_t bits = Ram8(attacker + kGuardBits);
        switch (pose) {
        case 1: // standing
            if (kind == 2) {
                const uint16_t legs = Ram16(Var(c, kLegsDistance));
                SetLow16(c.d[0], legs);
                if (legs < Ram16(Var(c, kHeadDistance)) && legs < Ram16(Var(c, kBodyDistance))) {
                    return NotGuarded(c);
                }
            }
            return (bits & 1) ? Guarded(c) : NotGuarded(c);
        case 2: // crouching
            return (bits & 2) ? Guarded(c) : NotGuarded(c);
        case 3: // in the air: what a helper throws, a thing thrown, an attack from the air (not those of bit 7)
            if (Mem8(c, attacker + kHelperOf)) {
                return Guarded(c);
            }
            if (Mem8(c, attacker + 0x08) == 8 || Mem8(c, attacker + kInAir)) {
                if (!(Mem8(c, attack + kAttackNumber) & 0x80)) {
                    return Guarded(c);
                }
            }
            return Hit(c);
        default: { // (4: a pose of its own, not seen in the movies)
            const uint8_t side = Mem8(c, attacker + kFacesRight);
            SetLow8(c.d[0], side);
            if (side == Mem8(c, victim + kFacesRight)) {
                return Hit(c);
            }
            Call(c, 0x0106F6);
            if (c.d[0]) {
                return Call(c, 0x0342E8);
            }
            Call(c, 0x010708);
            if (c.d[0]) {
                return Call(c, 0x0342E8);
            }
            return Hit(c);
        }
        }
    }
} // namespace

void RegisterHit() {
    AddRewrite(kHitLands, "HitLands", HitLands, kFreeFlags, 880);
}

} // namespace cps2recomp
