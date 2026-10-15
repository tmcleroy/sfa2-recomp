// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy
//
// Street Fighter Alpha 2: a hit's damage, written again as C++ (a rewrite: lift.h). How much a hit takes off is
// worked out from tables in the ROM: a base by the attack's strength and the attacker's character, a random part
// (more of it for an attacker low on health), what the victim's character takes, how the victim was hit (in the
// air, on the ground, by a special move), the board's damage setting; a guarded hit takes a quarter or an eighth
// of it as chip damage, and only specials do; stun builds as the hit lands. The cheats and the training mode's
// readout and no-knock-out are lines here, where the damage is taken.
#include "sfa2.h"

#include <algorithm>

namespace cps2recomp {

bool TrainingMode();                                      // training.cpp
void TrainingHit(int player, unsigned damage, bool chip); //

namespace {
    using namespace sfa2;

    // From A5
    constexpr int32_t kRoundResult = 0x10C, kRoundTimeOver = 0x10D, kHitGuarded = -0x57D7;
    constexpr int32_t kAttackerSide = -0x57DA; // word: the record of the attacker's side's player (FFxxxx)
    constexpr int32_t kKeptA6 = -0x57DE, kKeptA4 = -0x57E2;
    // In an attack's entry (A3)
    constexpr uint32_t kAttackStrength = 0x08;     // bits 0-4 the strength; bit 7 a special move, bit 6 a super
    constexpr uint32_t kAttackHelperStrength = 0x1A; // (the strength, for a helper's attack)
    constexpr uint32_t kAttackStun = 0x0C;
    constexpr uint32_t kAttackRandom = 0x0E, kAttackRandomLow = 0x0F; // which row of random parts (the second when the attacker's health is low)
    constexpr uint32_t kAttackChips = 0x14;        // not zero: takes chip damage when guarded
    // The tables
    constexpr uint32_t kBaseTable = 0x09C044, kRandomTable = 0x09C444, kTakesTable = 0x09C844, kTakesTableAuto = 0x09CB44;
    constexpr uint32_t kLowHealthTable = 0x09CE44, kDamageTable = 0x09D144, kCharacterRows = 0x09E1C4, kCharacterTable = 0x09E1DC;
    constexpr uint16_t kLowHealth = 0x28;

    int PlayerOf(uint32_t record) {
        record &= 0xFFFFFF;
        return record == Player(0) ? 0 : record == Player(1) ? 1 : -1;
    }

    uint32_t SideRecord(M68K &c, uint32_t object) {
        return Var(c, Mem8(c, object + kSide) ? 0x800 : 0x400);
    }

    // 010C1A: a hit on a player in the middle of an attack, or on a player with a weakness, is worth more (by
    // the attacker's +294 or the victim's: twice, a quarter more, a half more)
    void Multiplier(M68K &c) {
        const uint32_t attacker = c.a[6], victim = c.a[1];
        c.d[0] = 0;
        SetLow16(c.d[2], c.d[2] & 0xFF);
        if (Mem8(c, attacker + 0x08)) {
            return;
        }
        uint8_t which = 0;
        if (Mem8(c, victim + kAttackOut) && (which = Mem8(c, attacker + 0x294)) != 0) {
            SetLow8(c.d[0], which);
            c.a[0] = Mem32(c, victim + kFrame);
            if (!(Mem8(c, c.a[0] + kFrameHarmless) & 0x80)) {
                goto apply;
            }
        }
        which = Mem8(c, victim + 0x294);
        SetLow8(c.d[0], which);
        if (!which) {
            return;
        }
    apply:
        // 010C4E: (the jump table's own word is left in D0)
        SetLow16(c.d[0], static_cast<uint16_t>(Code16(c, 0x010C58 + (c.d[0] & 0xFF) * 2u)));
        const uint16_t base = static_cast<uint16_t>(c.d[2]);
        switch (which) {
        case 1: SetLow16(c.d[2], static_cast<uint16_t>(base << 1)); break;
        case 2:
            SetLow16(c.d[0], static_cast<uint16_t>(base >> 2));
            SetLow16(c.d[2], static_cast<uint16_t>(base + (base >> 2)));
            break;
        case 3:
            SetLow16(c.d[0], static_cast<uint16_t>(base >> 1));
            SetLow16(c.d[2], static_cast<uint16_t>(base + (base >> 1)));
            break;
        default: break;
        }
    }

    // A row of the random table at A0: one of its 32 bytes. ("Fixed damage", a training aid: the row's average in
    // its place, so that the same combo always does the same; the game's random numbers are drawn all the same, so
    // that nothing else it does with them changes.)
    uint8_t RandomPart(M68K &c) {
        Call(c, 0x01975A);
        SetLow16(c.d[0], c.d[0] & 0x1F);
        if (FeatureOn("fixed_damage")) {
            int sum = 0;
            for (uint32_t n = 0; n < 32; n++) {
                sum += static_cast<int8_t>(Mem8(c, c.a[0] + n));
            }
            return static_cast<uint8_t>((sum + (sum >= 0 ? 16 : -16)) / 32);
        }
        return Mem8(c, c.a[0] + (c.d[0] & 0xFFFF));
    }

    // The knock-out, and what follows: who did it, and the finishing move's flash for a special or a super
    void KnockedOut(M68K &c) {
        const uint32_t attacker = c.a[6], victim = c.a[1], attack = c.a[3];
        (void)victim;
        if (Ram8(Var(c, kHitGuarded))) {
            Call(c, 0x0107B0);
        } else {
            Call(c, 0x010CB6);
        }
        c.a[0] = SideRecord(c, attacker);
        if (Mem8(c, c.a[0] + 0x140)) {
            return;
        }
        SetLow8(c.d[0], static_cast<uint8_t>(Mem8(c, c.a[0] + kCharacter) + 0x10));
        if (!Mem8(c, attacker + 0x3E)) {
            SetLow8(c.d[0], Mem8(c, attack + 0x15));
            if (Ram8(Var(c, kHitGuarded))) {
                c.d[0] = (Mem8(c, attack + kAttackStrength) & 0x40) ? 0x0B : 0x07;
            }
        }
        SetRam8(c.a[0] + 0x140, static_cast<uint8_t>(c.d[0]));
        SetRam8(Var(c, 0x14B), Mem8(c, c.a[0] + kCharacter));
        if (Mem8(c, attacker + 0x3E)) {
            SetRam16(Var(c, kKeptA4), static_cast<uint16_t>(c.a[4]));
            SetRam8(c.a[0] + 0x24D, 6);
            SetRam16(c.a[0] + kSpecialGauge, 0);
            c.d[0] = 1;
        } else {
            if (!(Mem8(c, attack + kAttackStrength) & 0x40) || Ram8(Var(c, 0x10E))) {
                return;
            }
            const uint8_t flash = Mem8(c, attack + 0x16);
            SetLow8(c.d[0], flash);
            if (!flash) {
                return;
            }
            SetRam16(Var(c, kKeptA4), static_cast<uint16_t>(c.a[4]));
            if (!Mem8(c, attacker + kFacesRight)) {
                if (flash == 2) {
                    SetLow8(c.d[0], 3);
                } else if (flash == 3) {
                    SetLow8(c.d[0], 2);
                }
            }
        }
        SetLow8(c.d[0], static_cast<uint8_t>((c.d[0] & 0xFF) - 1));
        SetRam8(Var(c, 0x12B), static_cast<uint8_t>(c.d[0]));
        Call(c, 0x05CD62);
        c.a[4] = static_cast<uint32_t>(static_cast<int16_t>(Ram16(Var(c, kKeptA4))));
    }

    // The damage D3 comes off the victim's health (unless the round is decided). The cheats and training are here.
    void TakeOff(M68K &c, uint16_t damage, bool chip) {
        const uint32_t victim = c.a[1] & 0xFFFFFF, attack = c.a[3];
        const int player = PlayerOf(victim);
        if (player >= 0) {
            TrainingHit(player, damage, chip);
            if (FeatureOn(player == 0 ? "no_damage" : "no_damage_2")) {
                damage = 0;
            } else if (player == 1 && FeatureOn("one_hit")) {
                damage = kHealthFull + 1;
            }
            if (TrainingMode()) {
                // (nobody is knocked out: the health stops at nothing left)
                damage = static_cast<uint16_t>(std::min<int>(damage, std::max<int>(0, static_cast<int16_t>(Ram16(victim + kHealth)))));
            }
            SetLow16(c.d[3], damage);
        }
        const int16_t left = static_cast<int16_t>(Ram16(victim + kHealth) - damage);
        SetRam16(victim + kHealth, static_cast<uint16_t>(left));
        if (left >= 0) {
            return;
        }
        // Down. (A guard that holds by itself keeps a guarded special from knocking out.)
        if (Mem8(c, victim + kAutoGuardOn) && Mem8(c, victim + kGuardedHit) == 0xFF && !Mem8(c, attack + kAttackChips)) {
            SetRam16(victim + kHealth, 0);
            return;
        }
        SetRam16(victim + kHealth, 0xFFFF);
        SetRam8(victim + 0x13B, 0);
        SetRam8(victim + kStunTimer, 0);
        SetRam8(victim + kStun, 0);
    }

    // A hit worth nothing takes one point all the same
    void OnePoint(M68K &c) {
        const uint32_t victim = c.a[1] & 0xFFFFFF;
        if (Ram8(Var(c, kRoundResult))) {
            return;
        }
        const int player = PlayerOf(victim);
        if (player >= 0) {
            TrainingHit(player, 1, Mem8(c, victim + kGuardedHit) == 0xFF);
            if (FeatureOn(player == 0 ? "no_damage" : "no_damage_2") || (TrainingMode() && Ram16(victim + kHealth) == 0)) {
                return;
            }
        }
        SetRam16(victim + kHealth, static_cast<uint16_t>(Ram16(victim + kHealth) - 1));
    }

    // 0108E2. The attack at A3, of the object at A6, has hit the player at A1: the super combo gauge, stun, and the
    // damage taken off its health.
    void TakeDamage(M68K &c) {
        const uint32_t attacker = c.a[6], victim = c.a[1], attack = c.a[3];
        Call(c, 0x010D26); // (the super combo gauge both sides gain)
        // The attacker's side's player
        c.a[0] = SideRecord(c, attacker);
        SetRam16(Var(c, kAttackerSide), static_cast<uint16_t>(c.a[0]));
        // The base: by the attack's strength and the attacker's own row
        SetLow8(c.d[2], Mem8(c, attack + (Mem8(c, attacker + 0x3E) ? kAttackHelperStrength : kAttackStrength)));
        SetLow16(c.d[2], static_cast<uint16_t>((c.d[2] & 0x1F) << 5));
        SetLow8(c.d[2], static_cast<uint8_t>((c.d[2] & 0xFF) + Mem8(c, c.a[0] + 0xA7)));
        c.a[0] = kBaseTable;
        SetLow8(c.d[2], Mem8(c, c.a[0] + (c.d[2] & 0xFFFF)));
        // A random part, from a row the attack names (another when the attacker is low on health)
        c.d[0] = Mem8(c, attack + kAttackRandom);
        if (Ram16(attacker + kHealth) < kLowHealth) {
            SetLow8(c.d[0], Mem8(c, attack + kAttackRandomLow));
        }
        c.a[0] = kRandomTable;
        SetRam32(Var(c, kKeptA6), attacker);
        SetLow16(c.d[0], static_cast<uint16_t>(c.d[0] << 5));
        c.a[0] += static_cast<uint32_t>(static_cast<int16_t>(c.d[0]));
        const uint8_t base = static_cast<uint8_t>(static_cast<uint8_t>(c.d[2]) + RandomPart(c));
        SetLow8(c.d[0], c.d[0] & 0xFF);
        SetLow8(c.d[2], base);
        if (base & 0x80) {
            return OnePoint(c);
        }
        Multiplier(c);
        if ((c.d[2] & 0xFF) >= 0x80) {
            c.d[2] = 0x7F;
        }

        if (Mem8(c, victim + kGuardedHit) == 0xFF) {
            // Guarded: only what chips takes anything, a quarter of it (an eighth from a player guarding by itself)
            bool quarter = false;
            if (Mem8(c, attacker + 0x08) != 8 && Mem8(c, attacker + 0x282)) {
                quarter = true;
            } else if (Mem8(c, victim + kAutoGuardOn)) {
                quarter = true;
            } else if (!Mem8(c, attack + kAttackChips)) {
                return;
            }
            if (quarter && !Mem8(c, attack + kAttackChips)) {
                if (Ram16(victim + kHealth) == 0) {
                    return;
                }
                quarter = false; // (an eighth)
            }
            const uint8_t chip = static_cast<uint8_t>(static_cast<uint8_t>((c.d[2] & 0xFF) + 7) >> (quarter ? 2 : 3));
            SetLow8(c.d[2], chip);
            if (!chip) {
                return OnePoint(c);
            }
        } else {
            // Landed: stun builds (a special's knock-down marks the victim)
            if (Mem8(c, attack + 0x15) == 5 || (Mem8(c, attack + kAttackStrength) & 0x40)) {
                SetRam8(victim + 0x258, 0xFF);
            }
            uint8_t stun = Mem8(c, attack + kAttackStun);
            SetLow8(c.d[0], stun);
            if (Mem8(c, attacker + 0x3E)) {
                stun = 0;
                SetLow8(c.d[0], 0);
                SetRam8(victim + kStun, 0);
            }
            // (the cheat: no stun builds on that player)
            const int player = PlayerOf(victim);
            if (player >= 0 && FeatureOn(player == 0 ? "no_stun" : "no_stun_2")) {
                stun = 0;
            }
            SetRam8(victim + kStun, static_cast<uint8_t>(Mem8(c, victim + kStun) + stun));
            SetRam8(victim + kStunTimer, 0xD2);
        }
        // 0109F0
        SetLow16(c.d[2], c.d[2] & 0xFF);
        SetLow8(c.d[0], Mem8(c, victim + kStun));
        if ((c.d[0] & 0xFF) >= Mem8(c, victim + kStunMost)) {
            Call(c, 0x010C76); // (stunned)
        }
        // What the victim's character takes: a row by its character, a column by its own setting
        c.a[0] = Mem8(c, victim + 0xA5) ? kTakesTableAuto : kTakesTable;
        SetLow8(c.d[0], Mem8(c, victim + kCharacter));
        SetLow16(c.d[0], static_cast<uint16_t>((c.d[0] & 0x1F) << 5));
        SetLow16(c.d[4], c.d[0]);
        SetLow8(c.d[0], static_cast<uint8_t>((c.d[0] & 0xFF) + Mem8(c, victim + 0xA7)));
        SetLow8(c.d[3], Mem8(c, c.a[0] + (c.d[0] & 0xFFFF)));
        // How the victim was hit
        c.d[0] = 0x1F;
        if (Mem8(c, victim + 0x5E) < 0x1F) {
            c.d[0] = 0;
            bool done = false;
            if (Mem8(c, victim + 0x28D)) {
                SetLow16(c.d[0], (c.d[0] & 0xFFFF) + 0x80);
                if (Mem8(c, attack + 0x1C)) {
                    SetLow16(c.d[0], (c.d[0] & 0xFFFF) + 0x60);
                    c.d[3] = 0;
                    done = true;
                }
            }
            if (!done && (Mem8(c, attack + kAttackStrength) & 0x80)) {
                SetLow16(c.d[0], (c.d[0] & 0xFFFF) + 0x20);
                if (Mem8(c, attack + kAttackStrength) & 0x40) {
                    SetLow16(c.d[0], (c.d[0] & 0xFFFF) + 0x20);
                }
            }
            if ((c.d[0] & 0xFFFF) == 0x80) {
                c.d[0] = 0;
                if (const uint8_t air = Mem8(c, attack + 0x17)) {
                    c.d[0] = air == 1 ? 0x20 : 0x40;
                }
                c.a[0] = 0x011594;
            } else {
                c.a[0] = 0x011494;
            }
            c.a[0] += static_cast<uint32_t>(static_cast<int16_t>(c.d[0]));
            c.d[0] = 0;
            SetLow8(c.d[0], Mem8(c, victim + 0x5E));
            SetLow8(c.d[0], Mem8(c, c.a[0] + (c.d[0] & 0xFFFF)));
        }
        SetLow8(c.d[3], static_cast<uint8_t>((c.d[3] & 0xFF) + (c.d[0] & 0xFF)));
        if (const uint8_t mark = Mem8(c, attack + 0x1B)) {
            SetRam8(victim + 0x287, mark);
        }
        // A victim low on health: a random part more
        if (Ram16(victim + kHealth) < kLowHealth) {
            c.a[0] = kLowHealthTable + static_cast<uint32_t>(static_cast<int16_t>(c.d[4]));
            SetLow8(c.d[4], RandomPart(c));
            SetLow8(c.d[3], static_cast<uint8_t>((c.d[3] & 0xFF) + (c.d[4] & 0xFF)));
        }
        if ((c.d[3] & 0xFF) >= 0x21) {
            c.d[3] = 0x20;
        }
        // The damage: a row by all that, a column by the base
        SetLow16(c.d[3], static_cast<uint16_t>((c.d[3] & 0xFF) << 7));
        c.a[0] = kDamageTable + static_cast<uint32_t>(static_cast<int16_t>(c.d[3]));
        SetLow8(c.d[3], Mem8(c, c.a[0] + (c.d[2] & 0xFFFF)));
        SetLow16(c.d[3], c.d[3] & 0xFF);
        // ... and a part more by the victim's character
        c.d[4] = 0;
        SetLow8(c.d[4], Mem8(c, victim + kCharacter));
        c.a[0] = kCharacterRows;
        SetLow8(c.d[4], Mem8(c, c.a[0] + (c.d[4] & 0xFFFF)));
        SetLow16(c.d[4], static_cast<uint16_t>((c.d[4] & 0xFFFF) << 7));
        c.a[0] = kCharacterTable + static_cast<uint32_t>(static_cast<int16_t>(c.d[4]));
        SetLow8(c.d[4], Mem8(c, c.a[0] + (c.d[3] & 0xFFFF)));
        SetLow16(c.d[4], c.d[4] & 0xFF);
        SetLow16(c.d[3], (c.d[3] + c.d[4]) & 0xFFFF);
        Call(c, 0x010D94); // (the board's damage setting)
        if ((c.d[3] & 0xFFFF) >= 0x80) {
            c.d[3] = 0x7F;
        }
        if (!Ram8(Var(c, kRoundResult)) && !Ram8(Var(c, kRoundTimeOver))) {
            TakeOff(c, static_cast<uint16_t>(c.d[3]), Mem8(c, victim + kGuardedHit) == 0xFF);
        }
        if (static_cast<int16_t>(Ram16(victim + kHealth)) < 0) {
            KnockedOut(c);
        }
    }
} // namespace

void RegisterDamage() {
    Game &game = TheGame();
    game.features.push_back({"fixed_damage", "Training mode", "Fixed damage",
                             "The damage of a hit has no random part (the average of the game's own in its place), so the same "
                             "combo always does the same damage. (On the board each hit's damage varies a little.)"});
    game.features.push_back({"no_stun", "Cheats", "No stun", "Player 1 is never stunned (no stun builds up)."});
    game.features.push_back({"no_stun_2", "Cheats", "No stun, player 2", "Player 2 is never stunned (no stun builds up)."});
    AddRewrite(0x0108E2, "TakeDamage", TakeDamage, kFreeFlags, 1320);
}

} // namespace cps2recomp
