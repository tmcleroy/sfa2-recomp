// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy
//
// Street Fighter Alpha 2: what the modes share (modes.h): getting a machine to a round, sounds asked for, telling
// objects apart, and what is wanted of the machine that is running (the machines kept as save states and run
// beside the port's are the toolkit's: runtime/spare.h).
#include "modes.h"

#include "state.h"

#include <algorithm>
#include <cstring>

namespace cps2recomp {

namespace {
    using namespace sfa2;

    constexpr uint32_t kPlayersIn = 0xFF80A8;
    constexpr uint32_t kSoundQueue = 0xFFCF60, kSoundQueueEnd = 0xFFDF60; // (sixteen bytes an entry, 256 of them)

    MatchWanted g_wanted;
} // namespace

MatchWanted &Wanted() {
    return g_wanted;
}
CameraWanted &CameraAt() {
    static CameraWanted camera;
    return camera;
}

bool RoundFought() {
    return Ram16(kMode) == 2 && Ram16(kGameStep) == kGameMatch && Ram16(kMatchStep) == kMatchRound && Ram16(kRoundStep) == kRoundFought;
}

bool ToRound(Machine &machine, int32_t &frames, bool choose) {
    if (RoundFought()) {
        return true;
    }
    if (Ram8(kCredits) < 2) {
        SetRam8(kCredits, 2);
    }
    const bool press = frames++ % 16 < 6;
    const uint8_t in = Ram8(kPlayersIn);
    const uint16_t one = machine.players[0];
    machine.players[0] = machine.players[1] = 0;
    if (!press) {
        // (nothing)
    } else if (!(in & 1)) {
        machine.players[0] = kStart;
    } else if (!(in & 2)) {
        machine.players[1] = kStart;
    } else {
        machine.players[0] = machine.players[1] = kPunch1;
    }
    if (choose && in == 3) {
        // (player 1's own controls, but for Start and the coin, which would do other things here)
        machine.players[0] = one & static_cast<uint16_t>(~(kStart | kCoin));
    }
    return false;
}

void PlaySound(uint16_t code) {
    const uint16_t at = Ram16(kSoundQueueEnd) & 0xFF0;
    SetRam32(kSoundQueue + at, code);
    SetRam32(kSoundQueue + at + 4, 0);
    SetRam32(kSoundQueue + at + 8, 0);
    SetRam16(kSoundQueueEnd, static_cast<uint16_t>((at + 0x10) & 0xFF0));
}

bool ThePanel(uint32_t tile, uint16_t attributes) {
    // (the bars, the names, the clock and the faces, and the gauges below: by their tiles, in whatever colours)
    // (0A400-0A6FF only outside the players' colour sets, 11-25: Charlie's own pictures are there too, and were
    // taken for the panel, so that the modes that lay the fighters over another stage lost him in some attacks)
    const unsigned set = attributes & 0x1F;
    return (tile >= 0x1A400 && tile < 0x1AB00) || (tile >= 0x26A00 && tile < 0x26B00) ||
           (tile >= 0x0A400 && tile < 0x0A700 && (set < 11 || set > 25));
}
bool NotTheStage(uint32_t tile, uint16_t attributes) {
    return (attributes & 0x1F) < 27 || ThePanel(tile, attributes);
}

void RegisterMachines() {
    // (what is wanted of the machine that is running is kept around a spare's frame, and fresh for it)
    static MatchWanted kept;
    static CameraWanted keptCamera;
    SetSpareKeeping({[](Machine &) { kept = g_wanted; keptCamera = CameraAt(); },
                     [](Machine &m) { g_wanted = MatchWanted(); CameraAt() = CameraWanted(); WideForSpare(m); },
                     [](Machine &m) { g_wanted = kept; CameraAt() = keptCamera; WideForPort(m); }});
}

} // namespace cps2recomp
