// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy
//
// Street Fighter Alpha 2: what the modes of the menu's "Alternative modes" page share (modes.cpp: attract mode
// and the stage viewer; battle.cpp: survival). As in Vampire Savior, most of it is for running more than one
// machine: the port's own, which is what is seen, and others kept as save states ("spares"), loaded into the
// port's machine, run and put away again within a frame.
#pragma once

#include "machine.h"
#include "sfa2.h"
#include "spare.h"

#include <cstdint>
#include <functional>
#include <vector>

namespace cps2recomp {

// What the machine that is running is to fight when the game next makes a match (the hooks where the game
// chooses the stage: training.cpp): the stage (1-18; 0: the game's choice) and the two characters (-1: the ones
// chosen at the character choice). Whoever runs a machine sets this first.
struct MatchWanted {
    int stage = 0;
    int characters[2] = {-1, -1};
};
MatchWanted &Wanted();

// Where the machine that is running is to have its camera (the picture's left edge and bottom, in the stage),
// whatever the players do: -1 leaves that to the game (widescreen.cpp's camera routines ask)
struct CameraWanted {
    int x = -1, y = -1;
};
CameraWanted &CameraAt();

// A round of a match is being fought
bool RoundFought();
// The controls of a machine on its way from switching on to a round of a two-player game: coins in, both Starts
// until both players are in, then the characters taken as they come (player 1's left to its own controls when
// `choose` is set). True once the round is on.
bool ToRound(Machine &machine, int32_t &frames, bool choose = false);
// A sound asked for as the game asks (the ring at FFCF60): a tune's number, or kTuneOff before another tune
void PlaySound(uint16_t code);
constexpr uint16_t kTuneOff = 0xFF05;
// Each stage's own tune (found by playing each stage)
inline constexpr uint16_t kStageTunes[sfa2::kStages] = {0x01, 0x02, 0x0B, 0x09, 0x03, 0x06, 0x05, 0x08, 0x07,
                                                  0x0A, 0x0C, 0x04, 0x0D, 0x12, 0x11, 0x0E, 0x0F, 0x10};
constexpr uint16_t kTuneChoose = 0x14, kTuneVersus = 0x15;

// Objects by what they are (for Video::hideObject): colour sets 0-26 are the players', what they throw, sparks and
// some of the panel; 27-31 the stage's own pieces, and the panel's bars, names and gauges (told by their tiles).
bool NotTheStage(uint32_t tile, uint16_t attributes);
bool ThePanel(uint32_t tile, uint16_t attributes);
constexpr unsigned kLayersWithoutPanel = 0xF; // (all the scroll screens: the panel is objects)

// Machines kept as save states and run beside the port's (the toolkit's runtime/spare.h): Spare, RunSpare,
// Promote, Take, Give. What is wanted (above) is kept around a spare's frame, and fresh for it.
void RegisterMachines();

// The wide picture (widescreen.cpp), for the modes that put their picture together from more than one machine:
// how many columns it adds on each side of the board's 384 (0: none), and how far short of each end the camera
// stops in the picture fights are played in (64, or 0)
int WideSide();
int WideMargin();
// A spare's frame is about to run, drawn as wide as the port's picture; and the port's machine is back
void WideForSpare(Machine &machine);
void WideForPort(Machine &machine);

// Modes by the files that have them: whether one is on, and ending it
bool AttractMode(); // modes.cpp
bool StageViewer();
void EndModes();
bool BattleMode(); // battle.cpp
void EndBattle();
bool BattleNoClock();
bool BattleNoKnockOut();

} // namespace cps2recomp
