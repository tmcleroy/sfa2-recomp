// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy
//
// Street Fighter Alpha 2: the board's settings (the operator's, in its test menu: Configuration, System and Game)
// as features on the launcher's Board page, as in Vampire Savior. Each is "As the board is set" unless chosen,
// and a choice is what the test menu would have set: the game reads its settings from the settings memory once,
// as the board starts (00ACF0, called at 001090), into the variables below; the hook at 001096, where that call
// returns, puts the choices in those variables. The board's settings memory itself is not changed, nor what the
// test menu shows and saves.
//
// The settings record (FF20D6, as the settings memory holds it) and the variable each goes to (from A5):
//   +01 continue      98  0 off, 1 on        +10 difficulty  A0  0 easy, 1 normal, 2-6 hard 1-5, 7 expert
//   +03 demo sound    99  0 off, 1 on                             (a new board: hard 2)
//                                            +11 damage      A1  0 low, 1 medium, 2 normal, 3 high (medium)
//                                            +12 timer speed A2  0 slow, 1 normal, 2 fast, 3 fastest (slow)
//                                            +13 game speed  A3  0 normal, 1-2 turbo 1-2, 3-4 free select 1-2
//                                                                 (turbo 2)
//                                            +14 max round   A4  1, 2, 3: one, three, five rounds (three)
//                                            +15 event       A5  0 off, 1 one match (no continue)
//                                            +16 automatic   BB  0 off, 1 free select
// (the values and their names from the test menu's pages, stepped through; 00ADD2 on copies them)
#include "sfa2.h"

#include "machine.h"

#include <iterator>

namespace cps2recomp {

namespace {
    constexpr uint32_t kVars = 0xFF8000; // (A5)
    constexpr const char *kPage = "Board";

    struct Setting {
        const char *name;
        uint32_t variable;
        int first; // the value of the first choice after "As the board is set"
    };
    constexpr Setting kSettings[] = {
        {"board_difficulty", kVars + 0xA0, 0}, {"board_damage", kVars + 0xA1, 0}, {"board_timer", kVars + 0xA2, 0},
        {"board_speed", kVars + 0xA3, 0},      {"board_rounds", kVars + 0xA4, 1}, {"board_event", kVars + 0xA5, 0},
        {"board_automatic", kVars + 0xBB, 0},  {"board_continue", kVars + 0x98, 0}, {"board_demo_sound", kVars + 0x99, 0},
    };
    constexpr uint32_t kContinue = kVars + 0x98, kEvent = kVars + 0xA5;

    // The board's own values of what was chosen, to put back for its test menu
    uint8_t g_board[std::size(kSettings)];
    uint8_t g_continue = 0;
    bool g_chosen = false; // the choices are in the variables
    bool g_again = false;  // they were taken out for the test menu, and go back in when a game begins

    // The choices put in the variables (the board's own kept first)
    void PutChoices() {
        g_chosen = false;
        for (size_t n = 0; n < std::size(kSettings); n++) {
            g_board[n] = Ram8(kSettings[n].variable);
        }
        g_continue = Ram8(kContinue); // (a one-match event clears it below)
        for (const Setting &setting : kSettings) {
            const int choice = FeatureValue(setting.name);
            if (choice > 0) {
                SetRam8(setting.variable, static_cast<uint8_t>(setting.first + choice - 1));
                g_chosen = true;
            }
        }
        if (g_chosen && Ram8(kEvent)) {
            SetRam8(kContinue, 0); // (no continue in the one-match event, as the game has it)
        }
    }

    // 001096: the board's settings have been read
    bool SettingsRead(M68K &) {
        g_again = false;
        PutChoices();
        return false; // (and on with the game's own code)
    }
    // 004B6C: a game begins (it copies difficulty to FF80BE, max round and continue, 004BD6 on)
    bool GameBegins(M68K &) {
        if (g_again) {
            g_again = false;
            PutChoices();
        }
        return false;
    }
} // namespace

// Before every frame. The board's test switch pressed (its test menu shows, changes and saves the variables)
// puts the board's own values back, so that what the test menu saves is the board's; leaving the test menu does
// not read the settings again, so the choices go back in when a game next begins (demo sound, which the show
// uses, at the next reset)
void BoardBeforeFrame(Machine &machine) {
    if (g_chosen && machine.testSwitch) {
        for (size_t n = 0; n < std::size(kSettings); n++) {
            SetRam8(kSettings[n].variable, g_board[n]);
        }
        SetRam8(kContinue, g_continue);
        g_chosen = false;
        g_again = true;
    }
}

void RegisterBoard() {
    Game &game = TheGame();
    game.pages.push_back({kPage, "", "The board's own settings, as its test menu has them (Configuration): each as the board is set "
                                     "unless chosen here. Taken when the game starts or is reset."});
    constexpr const char *kAsSet = "As the board is set";
    game.features.push_back({"board_difficulty", kPage, "Difficulty", "How well the computer fights.", 0,
                             {kAsSet, "Easy", "Normal", "Hard 1", "Hard 2", "Hard 3", "Hard 4", "Hard 5", "Expert"}});
    game.features.push_back({"board_damage", kPage, "Damage level", "How much damage a hit does.", 0,
                             {kAsSet, "Low", "Medium", "Normal", "High"}});
    game.features.push_back({"board_timer", kPage, "Timer speed", "How fast the round's clock runs.", 0,
                             {kAsSet, "Slow", "Normal", "Fast", "Fastest"}});
    game.features.push_back({"board_speed", kPage, "Game speed",
                             "The game's speed: normal, one of two turbo speeds, or a free choice of speed for the players.", 0,
                             {kAsSet, "Normal", "Turbo 1", "Turbo 2", "Free select 1", "Free select 2"}});
    game.features.push_back({"board_rounds", kPage, "Max round", "Rounds a match lasts at most.", 0,
                             {kAsSet, "1 round", "3 rounds", "5 rounds"}});
    game.features.push_back({"board_event", kPage, "Event", "Off, or one match only (and no continue).", 0, {kAsSet, "Off", "One match"}});
    game.features.push_back({"board_automatic", kPage, "Automatic", "Whether players may choose the automatic guard.", 0,
                             {kAsSet, "Off", "Free select"}});
    game.features.push_back({"board_continue", kPage, "Continue", "Whether a game over can be continued.", 0, {kAsSet, "Off", "On"}});
    game.features.push_back({"board_demo_sound", kPage, "Demo sound", "Whether the attract mode plays sound.", 0, {kAsSet, "Off", "On"}});
    AddHook(HookAt(0x001096), "SettingsRead", SettingsRead);
    AddHook(HookAt(0x004B6C), "GameBegins", GameBegins);
}

} // namespace cps2recomp
