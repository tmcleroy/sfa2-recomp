// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy
//
// Street Fighter Alpha 2: the players' own colours for the characters and the colour editor, which are the
// toolkit's (fight/colours.h). These are the game's facts for them: where its characters' colours are (sfa2.h),
// the ROM file PalMod changes, and how the editor shows a set on player 1 in the model viewer (model.cpp).
#include "colours.h"
#include "machine.h"
#include "sfa2.h"

#include <iterator>
#include <vector>

namespace cps2recomp {

void StartModelViewer(Machine &machine); // model.cpp
bool ModelViewer();
int ModelCharacter();
int ModelCharacterNumber(int choice);
int ModelAnimationStep(Machine &machine, int choice, int by);
const std::vector<const char *> &ModelAnimationNames();
int ModelAnimationForLine(Machine &machine, int line);

namespace {
    using namespace sfa2;

    uint32_t SetAddress(int number, int set) {
        return kColourSets + static_cast<uint32_t>(number * kColourSetsEach + set) * kColourSetSize;
    }
    std::vector<colours::Range> Ranges() {
        return {{kColourSets, kColourSets + kCharacters * kColourSetsEach * kColourSetSize},
                {kEffectColours, kEffectColours + kCharacters * kEffectPalettes * kColoursInPalette * 2}};
    }

    // Player 1 in the set: its choice and the set it keeps, as the game would have them; its palettes are where
    // the game put them for the round
    uint32_t ShowSet(Machine &, int number, int set, int &lines) {
        SetRam8(Player(0) + kColourChoice, static_cast<uint8_t>(set));
        SetRam32(Player(0) + kColours, SetAddress(number, set));
        lines = kPalettesInSet;
        const uint32_t to = Ram32(Player(0) + kColoursTo) & 0xFFFFFF;
        return to >= 0x900000 && to < 0x930000 ? to : 0;
    }

    // The stages as the panel names them (the menu's names are longer): the character's whose stage it is
    std::vector<const char *> Backgrounds() {
        static const char *const names[kStages] = {"Ryu's", "Ken's", "Akuma's", "Charlie's", "Chun-Li's", "Adon's",
                                                    "Sodom's", "Guy's", "Birdie's", "Rose's", "M. Bison's", "Sagat's",
                                                    "Dan's", "Sakura's", "Rolento's", "Dhalsim's", "Zangief's", "Gen's"};
        std::vector<const char *> list(std::begin(names), std::end(names));
        list.push_back("White");
        list.push_back("Black");
        return list;
    }
} // namespace

void RegisterColours() {
    colours::Description d;
    for (int n = 0; n < kCharacters; n++) {
        // (the names the game has in Japan, for files called by them)
        const char *abroad = n == 3 ? "Nash" : n == 10 ? "Vega" : nullptr;
        d.characters.push_back({n, kCharacterNames[n], abroad});
    }
    d.setNames = {"P", "K", "PP", "KK", "AUTOP", "AUTOK"};
    d.palettesInSet = kPalettesInSet;
    d.setAddress = SetAddress;
    d.ranges = Ranges;
    // The last but one of the set's program files has every character's colours (and other palettes, stages,
    // faces); PalMod never changes its first part, which is the game's data
    d.romFile = "sz2u.07";
    d.romFileAt = 0x200000;
    d.romFileSize = 0x80000;
    d.same = {0x200000, 0x220000};
    d.romFileWhose = "the USA set's, 960430";
    d.featureDescription = "Characters in the colours of the files in the \"palettes\" folder in the game's data folder: "
                           "an sz2u.07 changed with PalMod, and text files, one a character (Sakura.txt), which go before "
                           "it. With no files the game's own colours. They are taken when a player is coloured: at the "
                           "next round.";
    d.originalHelp = "# To change them, copy this file to the palettes folder and change the colours there. Each set\n"
                     "# is chosen by the buttons in its name ([P]: a punch, [K]: a kick, [PP]: two punches at once,\n"
                     "# [KK]: two kicks; [AUTOP] and [AUTOK]: a punch or a kick with auto guard) and has five lines of\n"
                     "# 16 colours; the character is drawn in the first line's, the others are its effects. The 16th\n"
                     "# colour of a line is never seen. A colour is #RRGGBB or #RGB, rounded to the board's 16 levels\n"
                     "# of each; - keeps the game's. Sets and lines that are left out keep the game's colours.\n";
    d.showSet = ShowSet;
    d.startViewer = StartModelViewer;
    d.viewerOn = ModelViewer;
    d.viewerCharacter = ModelCharacter;
    d.characterNumber = ModelCharacterNumber;
    d.animationStep = ModelAnimationStep;
    d.animationNames = ModelAnimationNames;
    d.animationForLine = ModelAnimationForLine;
    d.backgrounds = Backgrounds();
    colours::Register(d);
}

} // namespace cps2recomp
