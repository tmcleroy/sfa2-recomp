// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy
//
// Street Fighter Alpha 2: cheats and free play. Optional features (the launcher's Features page and the
// in-game menu switch them).
#include "sfa2.h"

#include "board.h"

namespace cps2recomp {

bool AttractMode(); // modes.cpp

namespace {
    using namespace sfa2;

    void Frame(Machine &) {
        if (FeatureOn("free_play") && !AttractMode() && Ram8(kCredits) < 2) {
            SetRam8(kCredits, 2); // (two: a second player can always join)
        }
        // (in a fight only: the records hold other things outside one)
        if (Ram16(kMode) != 2 || Ram16(kGameStep) != kGameMatch) {
            return;
        }
        static const char *const kFullGauge[2] = {"full_gauge", "full_gauge_2"};
        for (int n = 0; n < 2; n++) {
            if (FeatureOn(kFullGauge[n]) && Ram8(Player(n) + kState) == 2) {
                SetRam16(Player(n) + kSpecialGauge, kSpecialGaugeFull);
            }
        }
    }
} // namespace

void CheatsFrame(Machine &machine) {
    Frame(machine);
}

void RegisterCheats() {
    // The settings memory, read and written directly (board.h). The two routines are Capcom's for every
    // CPS-2 game; these are where this game has them and their variables.
    SettingsMemory memory;
    memory.readWords = HookAt(0x00B444);
    memory.writeWords = HookAt(0x00B50C);
    memory.buffer = -0x5E2A;
    memory.count = -0x5E24;
    memory.start = -0x5E20;
    memory.busy = 0x87;
    AddSettingsMemory(memory);
    Game &game = TheGame();
    game.features.push_back({"free_play", "Game", "Free play", "Start works without a coin.", kFeatureEnhanced});
    game.features.push_back({"no_damage", "Cheats", "No damage", "Player 1's health does not go down."});
    game.features.push_back({"no_damage_2", "Cheats", "No damage, player 2", "Player 2's health does not go down (the opponent's, in a one-player game)."});
    game.features.push_back({"one_hit", "Cheats", "One-hit knockouts", "Player 2 (the opponent, in a one-player game) goes down with the first hit."});
    game.features.push_back({"full_gauge", "Cheats", "Full gauge", "Player 1's super combo gauge is always full."});
    game.features.push_back({"full_gauge_2", "Cheats", "Full gauge, player 2", "Player 2's super combo gauge is always full."});
    game.features.push_back({"infinite_time", "Cheats", "Infinite time", "The round's clock stands still."});
}

} // namespace cps2recomp
