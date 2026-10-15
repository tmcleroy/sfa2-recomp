// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy
//
// Street Fighter Alpha 2: the frame interrupt's work with the board's registers, written again as C++ (a rewrite:
// lift.h): the video registers set for the frame, then the control panel read. Everything the game knows of the
// sticks and buttons comes from here, once a frame.
#include "sfa2.h"

namespace cps2recomp {

namespace {
    using namespace sfa2;

    // The board's input ports (bytes; a line reads 0 while pressed)
    constexpr uint32_t kPortPlayer2 = 0x804000, kPortPlayer1 = 0x804001; // stick in bits 0-3, punches in 4-6
    constexpr uint32_t kPortKicks = 0x804011;   // player 1's kicks in bits 0-2, player 2's light and medium in 4-5
    constexpr uint32_t kPortStarts = 0x804020;  // starts and coins; player 2's hard kick in bit 6
    constexpr uint32_t kPortService = 0x804021; // the service and test switches
    constexpr uint32_t kPortVolume = 0x804030;  // (the volume buttons' word)
    constexpr uint32_t kPortOutputs = 0x804040; // coin counters, the settings memory's lines, the sound's switch
    // From A5: a long each of the starts and coins and of the switches, a byte a frame (the newest first); the
    // control words of the frame before (kControls are this frame's)
    constexpr int32_t kStartsAndCoins = 0x60, kService = 0x64, kVolume = 0x68;
    constexpr int32_t kControlsBefore[2] = {0x5A, 0x5E};

    uint8_t Port(M68K &c, uint32_t address) {
        return static_cast<uint8_t>(~Mem8(c, address));
    }

    // 0013CE. The frame's registers: the objects' bank and the layers' order, then the panel.
    void FrameRegisters(M68K &c) {
        // The video: the layer control word (the high bit of which picks the object registers' value)
        uint16_t layers = static_cast<uint16_t>((Ram8(Var(c, 0x96)) << 8) | Ram16(Var(c, 0x30)));
        SetLow16(c.d[0], layers);
        c.Write(0x800122, 1, layers);
        const uint16_t objects = (layers & 0x8000) ? 0x808E : 0x807D;
        SetLow16(c.d[1], objects);
        c.Write(0x400002, 1, objects);
        c.Write(0x800166, 1, Ram16(Var(c, 0x32)));
        if (!Ram8(Var(c, 0x87))) {
            // (the outputs, unless the settings memory is being worked: then its own routines own the port)
            uint16_t outputs = static_cast<uint16_t>(Ram16(Var(c, 0x6A)) & 0x7F33);
            outputs = static_cast<uint16_t>(outputs | Ram8(Var(c, 0x88)) | Ram8(Var(c, 0xD1)));
            SetLow16(c.d[0], outputs);
            c.Write(kPortOutputs, 1, outputs);
        }

        // The panel. The starts and coins, and the switches: a byte a frame, the last four frames in a long
        SetRam16(Var(c, kVolume), Mem16(c, kPortVolume));
        SetRam32(Var(c, kStartsAndCoins), Ram32(Var(c, kStartsAndCoins)) >> 8);
        c.d[0] = Ram32(Var(c, kService)) >> 8;
        SetRam32(Var(c, kService), c.d[0]);
        const uint8_t starts = Port(c, kPortStarts);
        SetLow8(c.d[1], starts);
        SetRam8(Var(c, kStartsAndCoins), starts);
        const uint8_t service = Port(c, kPortService);
        SetLow8(c.d[0], service);
        SetRam8(Var(c, kService), service);
        for (int player = 0; player < 2; player++) {
            SetRam16(Var(c, kControlsBefore[player]), Ram16(Var(c, kControls[player])));
        }
        // A player's word: the buttons above (punches in bits 0-2, kicks in 4-6), the stick below. Player 2's
        // hard kick is on the starts' port.
        const uint8_t kickPort = Port(c, kPortKicks);
        const uint8_t kicks1 = static_cast<uint8_t>((kickPort << 4) & 0x70);
        const uint8_t kicks2 = static_cast<uint8_t>((kickPort & 0x30) | (starts & 0x40));
        SetLow8(c.d[3], kicks1);
        SetLow8(c.d[4], kicks2);
        SetLow8(c.d[1], starts & 0x40);
        const uint32_t ports[2] = {kPortPlayer1, kPortPlayer2};
        const uint8_t kicks[2] = {kicks1, kicks2};
        for (int player = 0; player < 2; player++) {
            const uint8_t port = Port(c, ports[player]);
            const uint8_t stick = port & 0x0F, buttons = static_cast<uint8_t>((port >> 4) | kicks[player]);
            SetLow8(c.d[0], stick);
            SetLow8(c.d[1], buttons);
            SetRam8(Var(c, kControls[player]) + 1, stick);
            SetRam8(Var(c, kControls[player]), buttons);
        }
        // (the kicks' port as it was read, this frame and the last)
        SetRam8(Var(c, 0x5F69), Ram8(Var(c, 0x5F68)));
        SetRam8(Var(c, 0x5F68), Mem8(c, 0x804010));
        // The scroll registers for the frame (0014CC), and something cleared
        Call(c, 0x0014CC);
        c.d[0] = 0;
        SetRam32(Var(c, 0x5F64), 0);
    }
} // namespace

void RegisterInput() {
    AddRewrite(0x0013CE, "FrameRegisters", FrameRegisters, kFreeFlags, 800);
}

} // namespace cps2recomp
