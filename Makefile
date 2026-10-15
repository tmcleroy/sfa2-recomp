# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Tommy McLeroy
#
# Build a game with the cps2recomp toolkit. Pick the game with GAME=<id>
# (a directory under games/ with a game.cfg); see cps2recomp/build.mk for targets.
GAME ?= sfa2
CR ?= cps2recomp
include $(CR)/build.mk

# Nothing from the game, no keys, no personal paths, no unlisted third-party code (CONTRIBUTING.md)
.PHONY: audit
audit:
	@tools/audit.sh

# The build with no translated code (the interpreters; what CI builds) against the translated build
.PHONY: check-release
check-release: port
	@tools/check_release.sh $(FRAMES)

# Widescreen: the wide picture's ends painted on every stage, and no code left to the interpreter with it on
.PHONY: check-widescreen
check-widescreen: port
	@games/$(GAME)/check_widescreen.sh
