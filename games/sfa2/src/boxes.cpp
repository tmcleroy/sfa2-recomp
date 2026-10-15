// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy
//
// Street Fighter Alpha 2: the boxes the game tests (boxes.h), read out of the records and the ROM's tables the
// way the game's own tests read them (the hit test from 0103AA, the box test 0113AA), and how an attack has to be
// guarded (the hit's rules from 010564).
#include "boxes.h"

#include "sfa2.h"

namespace cps2recomp {

namespace {
    using namespace sfa2;

    bool IsTable(uint32_t address) {
        return address >= 0x1000 && address < 0x400000 && !(address & 1);
    }

    void Place(Machine &machine, uint32_t object, fight::BoxKind kind, uint32_t box, bool thrown, std::vector<fight::StageBox> &boxes) {
        const int16_t x = static_cast<int16_t>(machine.Read16(box + kBoxX)), y = static_cast<int16_t>(machine.Read16(box + kBoxY));
        const int16_t halfWidth = static_cast<int16_t>(machine.Read16(box + kBoxHalfWidth));
        const int16_t halfHeight = static_cast<int16_t>(machine.Read16(box + kBoxHalfHeight));
        if (halfWidth <= 0 || halfHeight <= 0) {
            return;
        }
        const int across = static_cast<int16_t>(Ram16(object + kX)) + (Ram8(object + kFacesRight) ? -x : x);
        const int up = static_cast<int16_t>(Ram16(object + kY)) + y;
        boxes.push_back({kind, across, up, halfWidth, halfHeight, thrown});
    }

    void PlayerBoxes(Machine &machine, uint32_t player, std::vector<fight::StageBox> &boxes) {
        if (!Ram8(player + kActive) || Ram8(player + kState) != 2) {
            return;
        }
        const uint32_t frame = Ram32(player + kFrame);
        if (!IsTable(frame)) {
            return;
        }
        static constexpr struct {
            fight::BoxKind kind;
            uint32_t table, number, size;
        } kParts[] = {{fight::BoxKind::Push, kPushBoxes, kFramePush, 8},
                      {fight::BoxKind::Hurt, kHeadBoxes, kFrameHead, 8},
                      {fight::BoxKind::Hurt, kBodyBoxes, kFrameBody, 8},
                      {fight::BoxKind::Hurt, kLegBoxes, kFrameLegs, 8},
                      {fight::BoxKind::Attack, kAttackBoxes, kFrameAttack, kAttackBoxSize}};
        for (const auto &part : kParts) {
            const uint32_t table = Ram32(player + part.table);
            const int8_t number = static_cast<int8_t>(machine.Read8(frame + part.number));
            if (number > 0 && IsTable(table)) {
                if (part.kind == fight::BoxKind::Attack && (machine.Read8(frame + kFrameHarmless) & 0x80)) {
                    continue;
                }
                Place(machine, player, part.kind, table + static_cast<uint32_t>(number) * part.size, false, boxes);
            }
        }
    }

    // A thing thrown: its tables are a header of word offsets from its kThrownTables (+2 the attacks)
    void ThrownBoxes(Machine &machine, uint32_t thrown, std::vector<fight::StageBox> &boxes) {
        if (Ram8(thrown + kActive) != 1) {
            return;
        }
        const uint32_t frame = Ram32(thrown + kFrame), tables = Ram32(thrown + kThrownTables);
        if (!IsTable(frame) || !IsTable(tables)) {
            return;
        }
        const int8_t number = static_cast<int8_t>(machine.Read8(frame + kFrameAttack));
        if (number > 0) {
            const uint32_t attacks = tables + static_cast<uint32_t>(static_cast<int16_t>(machine.Read16(tables + 2)));
            Place(machine, thrown, fight::BoxKind::Attack, attacks + static_cast<uint32_t>(number) * kAttackBoxSize, true, boxes);
        }
    }

    // How the game lets an attack be guarded, by its kind (its entry's byte +0D): bit 0 standing, bit 1 crouching
    // (the table at 011438; another at 011456 for attacks from the air)
    uint8_t GuardBits(M68K &c, uint32_t attacker, uint8_t kind) {
        const uint32_t table = Mem8(c, attacker + kInAir) ? 0x011456 : 0x011438;
        return kind < 0x1E ? Mem8(c, table + kind) : 3;
    }
} // namespace

std::vector<fight::StageBox> AllBoxes(Machine &machine) {
    std::vector<fight::StageBox> boxes;
    for (const uint32_t record : {Player(0), Player(1), kHelper1, kHelper2}) {
        PlayerBoxes(machine, record, boxes);
    }
    for (int n = 0; n < kThrownRecords; n++) {
        ThrownBoxes(machine, kThrown + static_cast<uint32_t>(n) * kThrownSize, boxes);
    }
    return boxes;
}

// For the training dummy: is what is coming at the player at `player` to be guarded standing (true) or crouching?
// Standing only for what cannot be guarded crouching (overheads, attacks from the air); crouching otherwise (which
// guards the lows too).
bool GuardStanding(M68K &c, uint32_t player) {
    const uint32_t other = 0xFF0000u | Mem16(c, player + kOpponent);
    // The other player's attack: in this frame or the next that has one in the move
    uint32_t frame = Mem32(c, other + kFrame);
    for (int n = 0; n < 64 && frame < 0x400000; n++, frame += kFrameSize) {
        if (const uint8_t number = Mem8(c, frame + kFrameAttack); number && number < 0x80) {
            const uint8_t kind = Mem8(c, Mem32(c, other + kAttackBoxes) + number * kAttackBoxSize + 0x0D);
            const uint8_t bits = GuardBits(c, other, kind);
            return (bits & 1) && !(bits & 2);
        }
        if (static_cast<int16_t>(Mem16(c, frame + kFrameSize + 2)) < 0) {
            break;
        }
    }
    return Ram8(other + kAction) == kActionJump;
}

} // namespace cps2recomp
