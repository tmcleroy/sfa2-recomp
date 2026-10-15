// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy
//
// Street Fighter Alpha 2: names for what is known of the game's memory and routines (found by experiment:
// PROGRESS.md says how). The game is built like Vampire Savior, a year younger (its variables reached from
// A5 = FF8000, the players' records 0x400 apart from FF8400, the same four steps at the head of a record), so
// many names are the same as there; each one here was checked in this game, and where a field is somewhere
// else or means something else, it says so.
#pragma once

#include "game.h"
#include "lift.h"
#include "machine.h"

#include <cstdint>

namespace sfa2 {

using cps2recomp::M68K;

// --- where the game is ---

// Words from A5, each a step (in twos) of the one before. The first is 2 once a game is being played; the next
// 2 while the players choose their characters and 4 during the match; then 4 while a round is on, and under that
// 2 while it is being fought (0 its introduction)
constexpr uint32_t kMode = 0xFF8000, kGameStep = 0xFF8004, kMatchStep = 0xFF8008, kRoundStep = 0xFF800C;
enum : uint16_t { kGameChoosing = 2, kGameMatch = 4, kMatchRound = 4, kRoundFought = 2 };
// The game's own mode of play, a word from A5 (0 the usual one; others for its hidden modes: the guard-range
// test, 02343C, looks at different lists of things thrown in them)
constexpr int32_t kPlayMode = 0x138;

// Credits paid for and not yet used (found by RAM dumps around coins put in; as in Vampire Savior). The
// board's settings memory starts at two coins a credit: kCoins counts the coins (kHalfCredit the odd one).
constexpr uint32_t kCredits = 0xFF8076, kHalfCredit = 0xFF8071, kCoins = 0xFF80C3;
// The round's clock: seconds left (99 at most)
constexpr uint32_t kClock = 0xFF8109;
constexpr uint8_t kClockFull = 99;

// The stage a match is fought on: a word, the stage's number doubled. Each character has a stage (the table at
// 007A7A; the hidden characters share theirs); the game takes one of the two players' at 007914.
constexpr uint32_t kStage = 0xFF8100;
constexpr int kStages = 18;
// Who a player is (a player's byte kCharacter): seen on the name plates with each number put in. 18 to 23 are other
// forms of six of them (the classic Chun-Li, Shin Akuma and Evil Ryu among them), each with its first form's stage.
constexpr int kCharacters = 24;
inline constexpr const char *kCharacterNames[kCharacters] = {
    "Ryu", "Ken", "Akuma", "Charlie", "Chun-Li", "Adon", "Sodom", "Guy", "Birdie", "Rose", "M. Bison", "Sagat", "Dan", "Sakura",
    "Rolento", "Dhalsim", "Zangief", "Gen", "Chun-Li (classic)", "Gen (another form)", "Shin Akuma", "Zangief (another form)",
    "Dhalsim (another form)", "Evil Ryu"};
// The stages are the first eighteen characters', in the same order
inline constexpr const char *kStageNames[kStages] = {
    "Ryu's (moonlit snow)", "Ken's (the yacht)", "Akuma's (the cave)", "Charlie's (the hangar)", "Chun-Li's (the street market)",
    "Adon's (the temple)", "Sodom's (the truck)", "Guy's (the city street)", "Birdie's (the back alley)", "Rose's (the harbour)",
    "M. Bison's (the airfield)", "Sagat's (the reclining Buddha)", "Dan's (the cafe)", "Sakura's (the shopping street)",
    "Rolento's (the mural)", "Dhalsim's (the elephants)", "Zangief's (the lanterns)", "Gen's (the ruins)"};

// --- the players ---

// A player's record; the second player's follows the first's
constexpr uint32_t kPlayer1 = 0xFF8400, kPlayerSize = 0x400;
inline uint32_t Player(int n) {
    return kPlayer1 + static_cast<uint32_t>(n) * kPlayerSize;
}
// Other records of the same layout, for the things the players throw and the like. The guard-range test
// (02343C) goes through these lists: kThrown, sixteen of 0x80 in the usual mode (a byte kActive of 1 for one in
// use); kThrownOther, ten of 0x80, in the others; and two whole records after the players' (helpers).
constexpr uint32_t kThrown = 0xFF9400, kThrownOther = 0xFF9C00, kThrownSize = 0x80;
constexpr int kThrownRecords = 16, kThrownOtherRecords = 10;
constexpr uint32_t kHelper1 = 0xFF8C00, kHelper2 = 0xFF9000;

constexpr uint32_t kActive = 0x00;     // byte: 1 while the record is in use
// Four bytes of steps, each a step of the one before. In a fight the first is 2; the second says who is in
// charge of the player: 0 the player (kAction then says what it is doing), 2 a hit (being hit, or guarding
// one: "reeling"), more for being thrown and other things done to it
constexpr uint32_t kState = 0x04, kControl = 0x05, kAction = 0x06, kActionStep = 0x07;
enum : uint8_t { kControlOwn = 0x00, kControlReeling = 0x02 };
enum : uint8_t { kActionStand = 0x00, kActionCrouch = 0x02, kActionWalk = 0x04, kActionJump = 0x06, kActionAttack = 0x0A, kActionGuardPose = 0x0C,
               kActionStunned = 0x12 };
constexpr uint32_t kColourSet = 0x0D;  // byte: the first of the player's colour sets (player 1 16, player 2 11)
constexpr uint32_t kFacesRight = 0x0B; // byte: not zero when facing right (box offsets, kept as if facing left, are turned about)
constexpr uint32_t kX = 0x10;          // word: where it stands across the stage
constexpr uint32_t kY = 0x14;          // word: height; kGround on the floor
constexpr uint16_t kGround = 0x28;
constexpr uint32_t kFrame = 0x1C;      // long: the animation frame being shown (in the ROM)
constexpr uint32_t kOpponent = 0x38;   // word: the other player's record (FFxxxx)
// Health: what the player has (a hit takes it off here at once; below zero: knocked out) and what the bar shows
// (it goes down a point a frame after it: 082CFA). Vampire Savior has these the other way about.
constexpr uint32_t kHealth = 0x50, kHealthShown = 0x52; // words
constexpr uint16_t kHealthFull = 0x90;
constexpr uint32_t kGuardedHit = 0x55; // byte: FF when the last hit was guarded
constexpr uint32_t kReaction = 0x55;   // ... the same byte: the last hit's reaction (the attack's kind, FF guarded)
constexpr uint32_t kSide = 0x59;       // byte: which side it is on (a player's own things share it)
constexpr uint32_t kHitStop = 0x5F;    // byte: frames the player stands frozen for while a hit lands (both players)
// bytes: the stick as the player holds it, by its facing (bit 0 back, 1 forward, 2 down, 3 up); the buttons newly
// pressed (bits 0-2 punches, 4-6 kicks); the stick's newly moved
constexpr uint32_t kStick = 0x109, kPressedNow = 0x10C, kStickNow = 0x10D;
constexpr uint8_t kStickHeldBack = 1, kStickHeldForward = 2, kStickHeldDown = 4, kStickHeldUp = 8;
constexpr uint32_t kHeld = 0x127;      // byte: bit 0 set while the player holds away from the other (and can guard)
constexpr uint32_t kComputer = 0xA5;   // byte: 1 while the game plays this player (changing it in a fight hands it over)
constexpr uint32_t kCharacter = 0x82;  // byte: who the player is (kCharacterNames)
constexpr uint32_t kSpecialGauge = 0x9E; // word: the super combo gauge
constexpr uint16_t kSpecialGaugeFull = 0x90;
// The stun gauge (from the damage routine, 0109BC on): what has built up, the frames until it starts to go down
// again, and how much it takes to be stunned
constexpr uint32_t kStun = 0x137, kStunTimer = 0x136, kStunMost = 0x13A; // bytes
// Boxes. The animation frame names the boxes in use: three it can be hit in (head, body, legs) and its
// attack; the record has the tables they are numbers into (eight bytes a box; thirty-two an attack, the box
// first and then what the attack does) and a fifth, its pushing box
constexpr uint32_t kHeadBoxes = 0x110, kBodyBoxes = 0x114, kLegBoxes = 0x118, kAttackBoxes = 0x11C, kPushBoxes = 0x120; // longs
constexpr uint32_t kFrameHead = 0x08, kFrameBody = 0x09, kFrameLegs = 0x0A, kFrameAttack = 0x0B, kFramePush = 0x0C; // bytes
constexpr uint32_t kFrameSize = 0x18;         // the frames of a move follow one another
constexpr uint32_t kFrameHarmless = 0x17;     // bit 7: nothing in this frame can hit (or be guarded)
constexpr uint32_t kFramePose = 0x0D;         // its guarding pose (0 none, 1 standing, 2 crouching, 3 in the air)
constexpr uint32_t kAttackBoxSize = 0x20;
// A box: its middle from where the object stands (across: towards the way it faces), half its width and height
constexpr uint32_t kBoxX = 0, kBoxY = 2, kBoxHalfWidth = 4, kBoxHalfHeight = 6;
// A thing thrown has its tables elsewhere: a long at kThrownTables, a header of word offsets from it
constexpr uint32_t kThrownTables = 0x60;
// More bytes the guard-range test reads (meanings guessed from where they are set)
constexpr uint32_t kCustomCombo = 0x142;   // a Custom Combo's time left (from about B0 down, a little more than one a frame); meanwhile the
                                            // player cannot guard and the other always can
constexpr uint32_t kAttackOut = 0x132;      // not zero while an attack of the player's is out
constexpr uint32_t kThrowing = 0x21F;
constexpr uint32_t kNoThrow = 0x241;         // steps in which the player cannot be thrown: 5 after reeling (022D20)
constexpr uint32_t kThrowProtection = 0x273; // the same, counted down while in control (021620; who sets it not found)
constexpr uint32_t kAutoGuardOn = 0xB2;
constexpr uint32_t kThrownGuardSkip = 0x68; // bit 7: a thing thrown that need not be guarded
constexpr uint32_t kInAir = 0x31;           // 1 while in the air (a jump: set leaving the floor 021A8E, cleared landing 021AC2)
constexpr uint32_t kHelperOf = 0x3F;
constexpr uint32_t kKnocksDown = 0x134;    // set by a hit that knocks the player down (sweeps, specials, ...; HitLands)

// The control panel as the game keeps it, from A5: a word a player, buttons above (punches in bits 0-2, kicks
// in 4-6) and the stick below (bit 0 right, 1 left, 2 down, 3 up)
constexpr int32_t kControls[2] = {0x58, 0x5C};

// --- the characters' colours ---

// A character's colours, in the ROM: from kColourSets, kColourSetsEach sets a character (by its number, all 24),
// each kPalettesInSet palettes of 16 colours (words, 4 bits each of red, green and blue; the game keeps the
// brightness the palette buffer had). The player's byte kColourChoice picks the set: 0 a punch chose the
// character, 1 a kick, 2 two punches at once, 3 two kicks (0628D8); 4 and 5 the same as 0 and 1 with auto guard
// (062C1C). When a round starts the game works out the set (0122E2: kColourSets + (character * 6 + choice) * 0xA0),
// keeps it at the player's long kColours and where its palettes go at kColoursTo (the buffer the board's palette
// is copied from: 90C200 for player 1, 90C160 for player 2, the slots the player's byte kColourSet names), and
// copies all five there (01240C). Effects that colour a player (02725C) take palettes of kEffectColours (16 a
// character, 0x200 bytes) and put the set back from kColours when they end (027204).
constexpr uint32_t kColourSets = 0x26D0E0, kEffectColours = 0x272AE0;
constexpr int kColourSetsEach = 6, kPalettesInSet = 5, kColoursInPalette = 16, kEffectPalettes = 16;
constexpr uint32_t kColourSetSize = kPalettesInSet * kColoursInPalette * 2;
constexpr uint32_t kColourChoice = 0xA8;      // byte
constexpr uint32_t kColours = 0xCC, kColoursTo = 0xD0; // longs

// --- routines (addresses in the program) ---

constexpr uint32_t kPlayerDispatch = 0x0214C0; // a player's routine goes on by kControl (once a step for each)
constexpr uint32_t kGuardRange = 0x02343C;     // is an attack near enough for the player A6 to guard, and does it hold away?
constexpr uint32_t kBoxesOverlap = 0x0113AA;
constexpr uint32_t kHitLands = 0x010564;       // an attack has hit (A6 the attacker, A1 the victim, A3 the attack)

// --- for rewrites: memory and registers by name ---

inline uint8_t Mem8(M68K &c, uint32_t address) {
    return static_cast<uint8_t>(c.Read(address, 0));
}
inline uint16_t Mem16(M68K &c, uint32_t address) {
    return static_cast<uint16_t>(c.Read(address, 1));
}
inline uint32_t Mem32(M68K &c, uint32_t address) {
    return c.Read(address, 2);
}
// The ROM as the program reads it where it names a table by its distance from the instruction (deciphered: not
// what Mem8 and Mem16 see at the same address)
inline uint8_t Code8(M68K &c, uint32_t address) {
    return static_cast<uint8_t>(c.ReadProgram(address, 0));
}
inline uint16_t Code16(M68K &c, uint32_t address) {
    return static_cast<uint16_t>(c.ReadProgram(address, 1));
}
inline uint32_t Var(M68K &c, int32_t offset) {
    return (c.a[5] + static_cast<uint32_t>(offset)) & 0xFFFFFF;
}
inline void SetLow16(uint32_t &reg, uint32_t value) {
    reg = (reg & 0xFFFF0000) | (value & 0xFFFF);
}
inline void SetLow8(uint32_t &reg, uint32_t value) {
    reg = (reg & 0xFFFFFF00) | (value & 0xFF);
}

// The player is free to do anything: in charge of itself, and standing, crouching, walking or in its guarding pose
inline bool CanAct(int n) {
    using namespace cps2recomp;
    const uint8_t action = Ram8(Player(n) + kAction);
    return Ram8(Player(n) + kControl) == kControlOwn &&
           (action == kActionStand || action == kActionCrouch || action == kActionWalk || action == kActionGuardPose);
}

} // namespace sfa2
