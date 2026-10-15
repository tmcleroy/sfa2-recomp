// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy
//
// Street Fighter Alpha 2: the boxes the game tests for hits and for the players pushing each other.
#pragma once

#include "fight.h"
#include "machine.h"

#include <vector>

namespace cps2recomp {

// Of both players, their helpers and everything thrown, as they are now, placed in the stage
std::vector<fight::StageBox> AllBoxes(Machine &machine);

} // namespace cps2recomp
