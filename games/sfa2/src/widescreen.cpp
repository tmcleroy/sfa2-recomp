// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy
//
// Street Fighter Alpha 2: widescreen, as Vampire Savior has it. A fight is shown 512 columns wide where the board
// shows 384 (16:9 for 4:3, the height and the size of everything as they were): more of the stage on each side.
// The fight is played in that picture too: the players can go to its edges, and the camera stops where its edges
// come to the stage's ends. With the feature off none of that is there and the game is the board's. It is off in
// every preset: it changes how the game plays.
//
// With "Invisible walls" (widescreen_walls) as well, the fight is shown in the wide picture but played as on the
// board: nothing here touches the rules (the margin is 0), so the players are stopped where the board's picture
// ends and the camera goes as far as the board's does. Only the picture is wider, and near a stage's end the view
// is kept inside the stage, so the board's picture is then off the middle of the wide one.
//
// Alpha 2's scroll screens are whole pictures in the board's memory for the camera's whole range (seen: the wide
// picture's sides are the stage's own art on every stage, at every place of the camera between its ends), so the
// picture needs nothing from here but its width and, near a stage's end, the view kept where it was 64 from it.
//
// The rules the wide picture changes are written again here as readable C++ (rewrites: lift.h), each with the
// margin as a line in it: the camera's place across (its ends 64 nearer the middle), and keeping the players
// inside the picture (64 further out on each side).
#include "modes.h"
#include "sfa2.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>

namespace cps2recomp {

bool RingOutLetsOut(uint32_t player); // battle.cpp

namespace {
    using namespace sfa2;

    constexpr int kSide = Video::kMaxSide;
    // The camera's record (a scroll screen's: the stage's own plane), and in it
    constexpr uint32_t kCamera = 0xFF8280;
    constexpr uint32_t kCameraX = 0x20, kCameraShake = 0x50, kCameraBefore = 0x28, kCameraGoing = 0x3B;
    constexpr uint32_t kCameraEndLeft = 0x44, kCameraEndRight = 0x46;
    constexpr uint32_t kCameraY = 0x24, kCameraShakeY = 0x54, kCameraBeforeY = 0x2C, kCameraGoingY = 0x3A;
    constexpr uint32_t kCameraEndTop = 0x40, kCameraEndBottom = 0x42;
    constexpr uint32_t kPlaneShownX = 0x10; // where a scroll screen is shown across (what a player is kept inside)
    constexpr uint32_t kHalfWidth = 0x12A;  // a player's half width, for the picture's edges
    constexpr uint32_t kAtEdge = 0x13C;     // 1 at the picture's left edge, 2 at its right

    bool g_play = false;  // fights are played in the wide picture
    bool g_shown = false; // fights are shown in the wide picture (played in it, or played as on the board)
    int g_side = 0, g_shift = 0, g_cameraBefore = 0;

    int Margin() {
        return g_play ? kSide : 0;
    }
    // The camera's right end a little nearer still where a stage's own plane is painted short of what the wide
    // picture shows there (Dan's: 7 columns short of the camera's end and the 384 from it)
    constexpr int kPaintedShort[kStages] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 8, 0, 0, 0, 0, 0};
    int RightMargin() {
        const int stage = Ram16(kStage) / 2;
        return Margin() + (g_play && stage < kStages ? kPaintedShort[stage] : 0);
    }
    // How far in from the camera's right end the wide view stops (whether or not the camera does)
    int ViewRightMargin() {
        const int stage = Ram16(kStage) / 2;
        return kSide + (stage < kStages ? kPaintedShort[stage] : 0);
    }

    // --- the camera ---

    // 01B662. The camera's place across: it follows the players' middle when they are far apart, and otherwise
    // moves only when one of them comes near the picture's edge; at most 5 a frame, and not past its ends (in the
    // wide picture, 64 nearer the middle). Then (01B72E) its shake, which way it went, and the strip of the map
    // drawn as it goes.
    void CameraAcross(M68K &c) {
        const uint32_t camera = c.a[6];
        Call(c, 0x01BFE0); // (the players' outer edges: D0 the left, D1 the right)
        const uint16_t left = static_cast<uint16_t>(c.d[0]), right = static_cast<uint16_t>(c.d[1]);
        const uint16_t place = Ram16(camera + kCameraX);
        const int margin = Margin();
        // (D2 holds each difference as the game works it out; the step is at most 5)
        auto step = [&](int16_t by) {
            if (static_cast<uint16_t>(by) > 5) {
                c.d[2] = 5;
            } else {
                SetLow16(c.d[2], static_cast<uint16_t>(by));
            }
            return static_cast<uint16_t>(c.d[2]);
        };
        uint16_t to = place;
        int16_t by = 0;
        int way = 0; // 1 right, -1 left, 0 stays
        if (CameraAt().x >= 0) {
            // (a mode wants the camera at a place of its own: it goes there, as far as its ends let it)
            const int end0 = Ram16(camera + kCameraEndLeft) + margin, end1 = Ram16(camera + kCameraEndRight) - RightMargin();
            to = static_cast<uint16_t>(std::clamp(CameraAt().x, end0, end1));
            SetRam16(camera + kCameraX, to);
            SetLow16(c.d[2], 0);
            goto placed;
        }
        SetLow16(c.d[2], static_cast<uint16_t>(right - left));
        if (static_cast<uint16_t>(right - left) >= 0xC0) {
            // (far apart: their middle in the middle)
            by = static_cast<int16_t>(static_cast<uint16_t>(((right + left) & 0xFFFF) >> 1) - place - 0xC0);
            way = by < 0 ? -1 : 1;
        } else if ((by = static_cast<int16_t>(right - place - 0x120)) >= 0) {
            way = 1;
        } else if ((by = static_cast<int16_t>(left - place - 0x60)) <= 0) {
            way = -1;
        }
        SetLow16(c.d[2], static_cast<uint16_t>(by));
        if (way > 0) {
            const uint16_t moved = static_cast<uint16_t>(place + step(by));
            SetRam16(camera + kCameraX, moved);
            // (the right end: in the wide picture 64 nearer the middle)
            const uint16_t end = static_cast<uint16_t>(Ram16(camera + kCameraEndRight) - RightMargin());
            to = static_cast<int16_t>(end) <= static_cast<int16_t>(moved) ? end : moved;
        } else if (way < 0) {
            const uint16_t moved = static_cast<uint16_t>(place - step(static_cast<int16_t>(-by)));
            SetRam16(camera + kCameraX, moved);
            const uint16_t end = static_cast<uint16_t>(Ram16(camera + kCameraEndLeft) + margin);
            to = static_cast<int16_t>(end) >= static_cast<int16_t>(moved) ? end : moved;
        }
    placed:
        // 01B72E
        const uint16_t shown = static_cast<uint16_t>(to + Ram16(camera + kCameraShake));
        SetLow16(c.d[0], shown);
        SetRam8(camera + kCameraGoing, static_cast<int16_t>(shown) < static_cast<int16_t>(Ram16(camera + kCameraBefore)) ? 0x08 : 0x0C);
        SetRam16(camera + kCameraX, shown);
        if (!Ram8(camera + 0x03)) {
            return;
        }
        // (a scroll screen drawn into the map as it goes: a strip at each 16 pixels crossed)
        SetLow8(c.d[1], Ram8(camera + 0x38));
        SetLow8(c.d[0], (shown & 0x10) ^ Ram8(camera + 0x38));
        if (c.d[0] & 0xFF) {
            return;
        }
        SetRam8(camera + 0x38, Ram8(camera + 0x38) ^ 0x10);
        SetLow8(c.d[0], Ram8(camera + kCameraGoing));
        Call(c, 0x01CA9C);
        Call(c, 0x01C9FE);
        Call(c, 0x01CA34);
        Call(c, 0x01CAB8);
    }

    // 01B6DA. The camera's place up and down: it follows the higher player when that one is more than 96 above its
    // bottom (or below), at most 1 a frame, and not past its ends; then its shake, which way it went, and the strip of
    // the map drawn as it goes. (A mode can want it at a place of its own.)
    void CameraUp(M68K &c) {
        const uint32_t camera = c.a[6];
        Call(c, 0x01C1D0); // (the players' heights: D1 the higher)
        const uint16_t high = static_cast<uint16_t>(c.d[1]);
        const uint16_t place = Ram16(camera + kCameraY);
        uint16_t to = place;
        if (CameraAt().y >= 0) {
            to = static_cast<uint16_t>(std::clamp<int>(CameraAt().y, Ram16(camera + kCameraEndBottom), Ram16(camera + kCameraEndTop)));
            SetRam16(camera + kCameraY, to);
            SetLow16(c.d[2], 0);
        } else {
            const int16_t by = static_cast<int16_t>(high - place - 0x60);
            SetLow16(c.d[2], static_cast<uint16_t>(by));
            if (by > 0) {
                if (static_cast<uint16_t>(by) > 1) {
                    c.d[2] = 1;
                }
                const uint16_t moved = static_cast<uint16_t>(place + static_cast<uint16_t>(c.d[2]));
                SetRam16(camera + kCameraY, moved);
                const uint16_t end = Ram16(camera + kCameraEndTop);
                to = static_cast<int16_t>(end) <= static_cast<int16_t>(moved) ? end : moved;
            } else if (by < 0) {
                SetLow16(c.d[2], static_cast<uint16_t>(-by));
                if (static_cast<uint16_t>(-by) > 1) {
                    c.d[2] = 1;
                }
                const uint16_t moved = static_cast<uint16_t>(place - static_cast<uint16_t>(c.d[2]));
                SetRam16(camera + kCameraY, moved);
                const uint16_t end = Ram16(camera + kCameraEndBottom);
                to = static_cast<int16_t>(end) >= static_cast<int16_t>(moved) ? end : moved;
            }
        }
        // 01B77E
        const uint16_t shown = static_cast<uint16_t>(to + Ram16(camera + kCameraShakeY));
        SetLow16(c.d[0], shown);
        SetRam8(camera + kCameraGoingY, static_cast<int16_t>(shown) < static_cast<int16_t>(Ram16(camera + kCameraBeforeY)) ? 0x04 : 0x00);
        SetRam16(camera + kCameraY, shown);
        if (!Ram8(camera + 0x03)) {
            return;
        }
        SetLow8(c.d[1], Ram8(camera + 0x39));
        SetLow8(c.d[0], (shown & 0x10) ^ Ram8(camera + 0x39));
        if (c.d[0] & 0xFF) {
            return;
        }
        SetRam8(camera + 0x39, Ram8(camera + 0x39) ^ 0x10);
        SetLow8(c.d[0], Ram8(camera + kCameraGoingY));
        Call(c, 0x01CA9C);
        Call(c, 0x01C9FE);
        Call(c, 0x01CA34);
        Call(c, 0x01CB40);
    }

    // --- the players inside the picture ---

    // 0236F0. The player at A6 kept inside the picture (in the wide picture, its edges); a player pressed against
    // an edge while reeling passes the push back to the other (if they are near enough).
    void KeepIn(M68K &c) {
        const uint32_t player = c.a[6];
        SetRam8(player + kAtEdge, 0);
        // (the ring out: a player knocked about at an end of the stage goes over it)
        if (RingOutLetsOut(player & 0xFFFFFF)) {
            return;
        }
        c.a[4] = static_cast<uint32_t>(static_cast<int16_t>(Mem16(c, player + kOpponent)));
        const uint32_t other = c.a[4];
        c.d[6] = (c.d[6] & 0xFFFF0000) | 0x100;
        if (!(Ram16(player + kHealth) & 0x8000) && !(Ram16(other + kHealth) & 0x8000)) {
            SetLow16(c.d[6], 0x150);
        }
        uint16_t apart = static_cast<uint16_t>(Mem16(c, other + kX) - Mem16(c, player + kX));
        if (apart & 0x8000) {
            apart = static_cast<uint16_t>(0u - apart);
        }
        SetLow16(c.d[1], apart);
        Call(c, 0x019216); // (A3: the scroll screen the player stands in)
        const uint16_t half = Mem16(c, player + kHalfWidth);
        SetLow16(c.d[2], half);
        const uint16_t shown = static_cast<uint16_t>(Mem16(c, c.a[3] + kPlaneShownX) - Margin());
        const uint16_t x = Mem16(c, player + kX);
        int16_t over = static_cast<int16_t>(static_cast<uint16_t>(x - half - shown));
        int edge = 0;
        if (static_cast<uint16_t>(x - half) <= shown) {
            edge = 1; // (left: over is how far past, below nothing)
        } else {
            SetLow16(c.d[2], static_cast<uint16_t>(2 * half));
            over = static_cast<int16_t>(static_cast<uint16_t>(x - half - shown + 2 * half - (0x180 + 2 * Margin())));
            if (over < 0) {
                SetLow16(c.d[0], static_cast<uint16_t>(over));
                return;
            }
            edge = 2;
        }
        SetLow16(c.d[0], static_cast<uint16_t>(over));
        SetRam16(player + kX, static_cast<uint16_t>(x - over));
        SetRam8(player + kAtEdge, static_cast<uint8_t>(edge));
        if (Ram16(player + kState) != 0x0202 || Ram8(player + 0x239)) {
            return;
        }
        if (Ram8(player + kInAir)) {
            if (!Ram8(player + 0x23F)) {
                return;
            }
            over = static_cast<int16_t>(over - Ram16(player + 0x40));
            SetLow16(c.d[0], static_cast<uint16_t>(over));
        }
        if (apart >= static_cast<uint16_t>(c.d[6]) || Ram8(player + 0x13F) || Ram8(other + kInAir)) {
            return;
        }
        SetRam16(other + kX, static_cast<uint16_t>(Ram16(other + kX) - over));
    }

    // 0237BE. Both players (one holding the other: a throw) kept inside the picture together
    void KeepBothIn(M68K &c) {
        const uint32_t player = c.a[6];
        c.a[4] = c.a[0] = static_cast<uint32_t>(static_cast<int16_t>(Mem16(c, player + kOpponent)));
        c.a[1] = player;
        const uint32_t other = c.a[4];
        SetRam8(other + kAtEdge, 0);
        SetLow16(c.d[1], Mem16(c, other + kX));
        if (!(Mem16(c, other + kX) < Mem16(c, player + kX))) {
            c.a[0] = player;
            c.a[1] = other;
        }
        Call(c, 0x019216);
        const uint32_t leftOne = c.a[0], rightOne = c.a[1];
        const uint16_t shown = static_cast<uint16_t>(Mem16(c, c.a[3] + kPlaneShownX) - Margin());
        uint16_t half = Mem16(c, leftOne + kHalfWidth);
        SetLow16(c.d[2], half);
        int16_t over = static_cast<int16_t>(static_cast<uint16_t>(Mem16(c, leftOne + kX) - half - shown));
        SetLow16(c.d[0], static_cast<uint16_t>(over));
        if (over < 0) {
            SetRam16(player + kX, static_cast<uint16_t>(Ram16(player + kX) - over));
            SetRam16(other + kX, static_cast<uint16_t>(Ram16(other + kX) - over));
            SetRam8(other + kAtEdge, 1);
            return;
        }
        half = Mem16(c, rightOne + kHalfWidth);
        SetLow16(c.d[2], half);
        over = static_cast<int16_t>(static_cast<uint16_t>(Mem16(c, rightOne + kX) - shown + half - (0x180 + 2 * Margin())));
        SetLow16(c.d[0], static_cast<uint16_t>(over));
        if (over < 0) {
            return;
        }
        SetRam16(player + kX, static_cast<uint16_t>(Ram16(player + kX) - over));
        SetRam16(other + kX, static_cast<uint16_t>(Ram16(other + kX) - over));
        SetRam8(other + kAtEdge, 2);
    }

    // --- the picture ---

    // The camera's ends, as the stage set them (256 and 640 on every stage seen)
    int CameraLeftEnd() {
        return Ram16(kCamera + kCameraEndLeft);
    }
    int CameraRightEnd() {
        return Ram16(kCamera + kCameraEndRight);
    }
    bool InFight() {
        return Ram16(kMode) == 2 && Ram16(kGameStep) == kGameMatch && Ram16(kMatchStep) == kMatchRound;
    }
    bool OnStage(int camera) {
        return CameraRightEnd() > CameraLeftEnd() + 2 * kSide && camera >= CameraLeftEnd() && camera <= CameraRightEnd();
    }

    // The two other scroll screens' places across (their records' kCameraX) with the camera at each of its ends, by
    // stage (measured: a training session walked to both ends of each stage). A plane is painted for what the board
    // shows of it at those places and the 384 columns from them; in the wide picture, near a stage's end, more of it
    // would show than that. So each is drawn moved by as much as its wide view would reach past its painting (a
    // picture-only change: nothing the game works out is touched). Planes that go the other way, or round (their
    // picture repeats), are left as they are.
    struct PlaneEnds {
        int16_t back[2], far[2]; // the 8-pixel and the 32-pixel scroll screens
    };
    constexpr PlaneEnds kPlaneEnds[kStages] = {
        {{304, 592}, {272, 368}}, {{0, 0}, {280, 616}},   {{27, 357}, {65, 574}},   {{212, 592}, {352, 544}}, {{280, 616}, {0, 0}},
        {{0, 0}, {304, 592}},     {{273, 622}, {352, 544}}, {{8, 888}, {336, 496}},   {{308, 587}, {308, 587}}, {{339, 695}, {362, 533}},
        {{16, 112}, {352, 544}},  {{337, 558}, {305, 334}}, {{313, 583}, {332, 564}}, {{358, 538}, {403, 493}}, {{30, 97}, {381, 514}},
        {{373, 522}, {282, 357}}, {{268, 628}, {280, 616}}, {{256, 640}, {256, 640}}};
    constexpr uint32_t kPlanes[2] = {0xFF8200, 0xFF8300};
    int g_planeBefore[2] = {0, 0};

    // How far to draw a plane moved so that its wide view stays inside its painting
    int PlaneShift(const int16_t ends[2], int place) {
        const int lo = ends[0], hi = ends[1];
        if (hi <= lo) {
            return 0; // (going the other way, or round: as it is)
        }
        const int from = place - kSide, to = place + Video::kWidth + kSide; // (the wide view of it)
        const int painted = hi - lo + Video::kWidth, wide = Video::kWidth + 2 * kSide;
        if (painted < wide) {
            // (painted narrower than the wide picture: the shortfall split between its two sides, wherever the camera
            // is; such a plane is far off and moves little)
            return from - (lo - (wide - painted) / 2);
        }
        if (from < lo) {
            return from - lo;
        }
        if (to > hi + Video::kWidth) {
            return to - (hi + Video::kWidth);
        }
        return 0;
    }

    // The 8-pixel scroll screen (record FF8200) is drawn into the map a 32-pixel strip at a time as its plane moves,
    // on most stages (`+03` not zero: then how its plan is laid out), from the stage's plan: a byte for each
    // 256-pixel block (at -54BC from A5), a block being 8 x 8 words (down then across), each the number of a
    // 32-pixel cell in a table the record names (+30), a cell 4 x 4 tiles (down then across) of two words each, in
    // another (+34). The game keeps up only what its own picture shows, so the wide picture's sides are read from
    // the plan here, as the game's own strip routines (01C574, 01C5AA, 01C668) find them.
    constexpr uint32_t kBackPlane = 0xFF8200, kBackPlan = 0xFF2B44;
    Machine *g_machine = nullptr;
    int g_backShown = 0, g_backShift = 0; // (the plane's place for the picture being made, and how far it is drawn moved)
    bool StagePlanTile(uint32_t x, uint32_t y, uint16_t &code, uint16_t &attributes) {
        unsigned block = 0;
        switch (Ram8(kBackPlane + 0x03)) {
        case 2: block = ((x & 0x300) >> 8) + ((y & 0x100) >> 6); break;
        case 4: block = ((x & 0x700) >> 8) + ((y & 0x100) >> 5); break;
        case 6: block = ((x & 0x100) >> 8) + ((y & 0x300) >> 7); break;
        default: return false;
        }
        const uint8_t which = Ram8(kBackPlan + block);
        const uint32_t blocks = Ram32(kBackPlane + 0x30), cells = Ram32(kBackPlane + 0x34);
        const uint16_t cell = g_machine->Read16(blocks + which * 128u + ((x & 0xE0) >> 1) + ((y & 0xE0) >> 4));
        const uint32_t tile = cells + cell * 64u + (((x >> 3) & 3) * 4 + ((y >> 3) & 3)) * 4;
        code = g_machine->Read16(tile);
        attributes = g_machine->Read16(tile + 2);
        return true;
    }
    // What the picture asks (Video::wide.tile) for a tile outside the board's own columns. The map's column says
    // where in the stage only within 512 pixels: the one meant is the one nearest to where that column of the
    // picture looks.
    bool SideTile(int which, int column, int row, int at, uint16_t &code, uint16_t &attributes) {
        if (which != 0 || !g_machine) {
            return false;
        }
        const int looksAt = g_backShown - g_side - g_backShift + at;
        const int x = column * 8 + ((looksAt - column * 8 + 256) & ~511);
        return StagePlanTile(static_cast<uint32_t>(x), static_cast<uint32_t>(row) * 8, code, attributes);
    }

    // For checking StagePlanTile (SFA2_WIDE_CHECK=1): the map as the game has drawn it, where the board shows it,
    // against the plan; every 200 frames of a fight, the tiles compared and how many differ, to stderr
    void CheckPlan(Machine &machine) {
        static const bool check = getenv("SFA2_WIDE_CHECK") != nullptr;
        if (!check || !InFight() || !Ram8(kBackPlane + 0x03) || machine.frame % 200 != 0) {
            return;
        }
        // (the board's columns but for a strip's width at each side, which the game may be about to draw)
        const int left = static_cast<int16_t>(Ram16(kBackPlane + kCameraX)) - 64 + 32;
        const int top = (machine.cpsA[7] + Video::kFirstLine) & 0x1FF;
        int compared = 0, differ = 0;
        for (int x = left & ~7; x < left + Video::kWidth - 64; x += 8) {
            for (int y = top & ~7; y < top + Video::kHeight; y += 8) {
                const int column = (x >> 3) & 63, row = (y >> 3) & 63;
                const uint8_t *map = &machine.videoRam[(static_cast<uint32_t>(machine.cpsA[1]) * 256 & 0x3C000) +
                                                       static_cast<uint32_t>((row & 0x1F) + (column << 5) + ((row & 0x20) << 6)) * 4];
                uint16_t code = 0, attributes = 0;
                StagePlanTile(static_cast<uint32_t>(x), static_cast<uint32_t>(y), code, attributes);
                compared++;
                differ += code != ((map[0] << 8) | map[1]) || attributes != ((map[2] << 8) | map[3]);
            }
        }
        fprintf(stderr, "widescreen: stage %d, %d tiles of the plan against the map, %d differ\n", Ram16(kStage) / 2, compared, differ);
    }

    // An object's place: the panel's stay in the middle, the stage's and the fighters' go with the view
    int MoveObject(uint32_t tile, uint16_t attributes, int) {
        return ThePanel(tile, attributes) ? 0 : g_shift;
    }
} // namespace

bool WidePlay() {
    return g_play;
}
int WideMargin() {
    return Margin();
}

namespace {
    // The wide picture's switches for the frame about to run (the port's, or a spare's): the camera's place now and
    // where it was for the picture being made (a frame behind), and the other planes' places for it
    void SetWide(Machine &machine, int camera, int shown, const int planesShown[2]) {
        g_side = g_shift = 0;
        if (g_shown && OnStage(camera) && OnStage(shown)) {
            g_side = kSide;
            // (the view's left edge is 64 left of the camera, and no further left than the camera's own end: nearer
            // the end than that, the stage is drawn to the side by what is missing. Played in the wide picture the
            // camera never goes there; with invisible walls it does, and the board's picture is then to one side)
            g_shift = shown - std::clamp<int>(shown, CameraLeftEnd() + kSide, CameraRightEnd() - ViewRightMargin());
        }
        Video::Wide &wide = machine.video.wide;
        wide.side = g_side;
        wide.scroll[0] = wide.scroll[1] = wide.scroll[2] = g_shift;
        // (the planes behind, kept inside their painting; each a frame behind, as the picture is)
        const int stage = Ram16(kStage) / 2;
        g_backShown = planesShown[0];
        for (int n = 0; n < 2; n++) {
            if (g_side && stage < kStages) {
                const PlaneEnds &ends = kPlaneEnds[stage];
                // (from where the view of it already is: near a stage's end, with invisible walls, the whole
                // view is drawn moved by g_shift)
                wide.scroll[n == 0 ? 0 : 2] += PlaneShift(n == 0 ? ends.back : ends.far, planesShown[n] - g_shift);
            }
        }
        wide.object = g_side ? MoveObject : nullptr;
        g_machine = &machine;
        g_backShift = wide.scroll[0];
        wide.tile = g_side && Ram8(kBackPlane + 0x03) ? SideTile : nullptr;
    // (a few stages are painted narrower than the wide picture in places: Ryu's lake below the mountains)
    wide.fillHoles = g_side != 0;
    }

    // The port's own, kept while a spare's frame runs (WideForSpare, WideForPort)
    struct Kept {
        int side, shift, backShown, backShift;
    } g_kept;
} // namespace

void WidescreenBeforeFrame(Machine &machine) {
    const int camera = static_cast<int16_t>(Ram16(kCamera + kCameraX)), shown = g_cameraBefore;
    g_cameraBefore = camera;
    // (fights only; the stage viewer keeps the board's picture. With invisible walls the rules stay the board's)
    g_shown = FeatureOn("widescreen") && InFight() && !StageViewer();
    g_play = g_shown && !FeatureOn("widescreen_walls");
    int planes[2];
    for (int n = 0; n < 2; n++) {
        planes[n] = g_planeBefore[n];
        g_planeBefore[n] = static_cast<int16_t>(Ram16(kPlanes[n] + kCameraX));
    }
    SetWide(machine, camera, shown, planes);
    CheckPlan(machine);
}

// A spare machine's frame (the modes' other machines: battle.cpp) is drawn as wide as the port's picture is, so
// that the two can be put together: from the spare's own camera (where it is now, for want of where it was)
void WideForSpare(Machine &machine) {
    g_kept = {g_side, g_shift, g_backShown, g_backShift};
    const int camera = static_cast<int16_t>(Ram16(kCamera + kCameraX));
    const int planes[2] = {static_cast<int16_t>(Ram16(kPlanes[0] + kCameraX)), static_cast<int16_t>(Ram16(kPlanes[1] + kCameraX))};
    SetWide(machine, camera, camera, planes);
    // (exactly as wide as the port's picture, even where the spare is not on its stage yet)
    if (g_side != g_kept.side) {
        Video::Wide &wide = machine.video.wide;
        g_side = wide.side = g_kept.side;
        g_shift = 0;
        wide.scroll[0] = wide.scroll[1] = wide.scroll[2] = 0;
        wide.object = g_side ? MoveObject : nullptr;
        wide.tile = nullptr;
    }
}
void WideForPort(Machine &machine) {
    g_side = g_kept.side;
    g_shift = g_kept.shift;
    g_backShown = g_kept.backShown;
    g_backShift = g_kept.backShift;
    g_machine = &machine;
}

int WideSide() {
    return g_side;
}
int WideStage() {
    return g_side + g_shift;
}

void RegisterWidescreen() {
    TheGame().widestPicture = [] { return Video::kWidth + (FeatureOn("widescreen") ? 2 * kSide : 0); };
    AddRewrite(0x01B662, "CameraAcross", CameraAcross, kFreeFlags, 253);
    AddRewrite(0x01B6DA, "CameraUp", CameraUp, kFreeFlags, 218);
    AddRewrite(0x0236F0, "KeepIn", KeepIn, kFreeFlags, 230);
    AddRewrite(0x0237BE, "KeepBothIn", KeepBothIn, kFreeFlags, 210);
    // (not in the Enhanced preset: it shows more of the stage than the board does, and a player who wants the game
    // as it is played in competition must not get that without asking)
    TheGame().features.push_back({"widescreen", "Display", "Widescreen",
                                  "Fights are shown and played in 16:9, with more of the stage on each side: the players can go "
                                  "to the wide picture's edges, so they can stand further apart than on the board. Off unless "
                                  "switched on here, in every preset: it changes how the game plays.",
                                  kFeatureFirstPage});
    TheGame().features.push_back({"widescreen_walls", "Display", "Invisible walls",
                                  "With widescreen: fights are still shown in 16:9, but played exactly as on the board. The "
                                  "players are stopped where the board's picture ends, by walls that are not seen, and the "
                                  "camera goes as far as the board's; near a stage's end the view stays on the stage, so the "
                                  "play area is then to one side of the picture. Nothing without widescreen.",
                                  0}); // (on the Display page, beside the picture's other options)
}

} // namespace cps2recomp
