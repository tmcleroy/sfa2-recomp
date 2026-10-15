// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy
//
// Street Fighter Alpha 2: the players' state machine, written again as C++ (rewrites: lift.h). Each frame of a fight
// the players are stepped (PlayersStep, in an order by chance); a player in a fight (PlayerFighting) goes on by its
// kControl (kPlayerDispatch, 0214C0): in control of itself (PlayerInControl: its timers, a hit's freeze, the
// computer's turn, then the routine for what it is doing, kAction: standing, crouching, walking, jumping, turning,
// attacking, guarding, a special move, stunned), reeling from a hit (PlayerReeling: the hit's landing and the
// reaction it gives, the freeze, pushed back, knocked down and getting up, recovering), or held in a throw. What is
// left to the game's own code: the characters' own moves (their attack and special move routines, by table) and
// the few states no movie reaches (see PROGRESS.md).
#include "sfa2.h"

namespace cps2recomp {

namespace {
    using namespace sfa2;

    // Timers of a player's that count down to 0, a step at a time while it is in control (bytes)
    constexpr uint32_t kTimer28A = 0x28A, kTimer256 = 0x256, kTimer25B = 0x25B, kTimer287 = 0x287;
    // Not zero: the computer is not asked what to do this step (a byte of the player's; and the game's own three,
    // fight.cpp's: the clock ran out, the round is stopped (decided, or held), the round is held)
    constexpr uint32_t kComputerWaits = 0x24E;
    constexpr int32_t kRoundTimeOver = 0x10D, kRoundStopped = 0x10E, kRoundPaused = 0x8A;
    constexpr uint32_t kMoveNumber = 0x133; // byte: which special move the player is in
    constexpr uint8_t kActionThrow = 0x0E;
    constexpr uint8_t kChunLi = 4, kChunLiClassic = 0x12, kSodom = 6, kDhalsim = 0x0F;

    constexpr uint32_t kComputerThinks = 0x0280D4; // the computer chooses this player's input
    constexpr uint32_t kChunLiFrozen = 0x0322D6;   // Chun-Li's own, every step she stands frozen by a hit
    constexpr uint32_t kKeepIn = 0x0236F0;
    constexpr uint32_t kActions = 0x02171E; // the routines by kAction (words, from here)

    void CountDown(uint32_t address) {
        if (Ram8(address)) {
            SetRam8(address, static_cast<uint8_t>(Ram8(address) - 1));
        }
    }

    // Does the computer choose this player's input this step?
    bool ComputerChooses(M68K &c, uint32_t player) {
        if (!Ram8(player + kComputer) || Ram8(player + kComputerWaits)) {
            return false;
        }
        const uint8_t stopped = Ram8(Var(c, kRoundTimeOver)) | Ram8(Var(c, kRoundStopped)) | Ram8(Var(c, kRoundPaused));
        SetLow8(c.d[0], stopped);
        if (stopped || Ram8(player + kAction) == kActionStunned) {
            return false;
        }
        // (nor in Sodom's or Dhalsim's throw while its move number is 0C)
        const bool throwing = Ram16(player + kState) == 0x0200 && Ram8(player + kAction) == kActionThrow;
        const uint8_t who = Ram8(player + kCharacter);
        return !(throwing && (who == kSodom || who == kDhalsim) && Ram8(player + kMoveNumber) == 0x0C);
    }

    // --- standing (kAction kActionStand) ---

    // The game's routines a standing player's step is made of. A test answers with the Z flag clear for yes; what
    // follows a yes is a routine that starts the new action (a jump to it, in the game's code).
    constexpr uint32_t kSetAnimation = 0x023136; // D0: the character's animation of that number, from its start
    constexpr uint32_t kAnimate = 0x0231D4;      // the animation on by a step (two in a Custom Combo)
    constexpr uint32_t kTimeOverPose = 0x023638; // the clock ran out: the winner's or the loser's pose
    constexpr uint32_t kOtherBeaten = 0x023A1A, kWinPose = 0x023A2E;
    constexpr uint32_t kPausedPose = 0x023D6A;   // the round held: a pose by who has more health (answers in D0)
    constexpr uint32_t kMustTurn = 0x0233B2, kTurn = 0x0233C6;          // the other has gone past: turn round
    constexpr uint32_t kPressed = 0x023B6A, kAttack = 0x023B74;         // a button pressed: an attack
    constexpr uint32_t kHoldsDown = 0x0233FA, kCrouch = 0x023402;
    constexpr uint32_t kHoldsUp = 0x0232AA, kJump = 0x0232BC;
    constexpr uint32_t kGuardTest = 0x02343C, kGuardPose = 0x0235F6;     // GuardRange (guard.cpp)
    constexpr uint32_t kHoldsAcross = 0x02322A, kWalk = 0x023242;
    constexpr uint32_t kCrouchRises = 0x02342A; // let go of down: the crouch's rising (kActionCrouch, step 4)
    constexpr uint32_t kTakeObject = 0x01963A;   // a free record for a thing of the game's (A4), Z clear if one
    constexpr uint32_t kStandActions = 0x021742; // the standing routine's steps (words, from here)
    constexpr uint32_t kLow = 0x130;             // byte: set while crouched (standing's step 4)
    constexpr uint32_t kTurnFast = 0x143;        // byte: cleared at every step of standing
    constexpr uint32_t kSodomsThing = 0x35;      // byte: Sodom's: a thing of his to be made as he stands
    constexpr uint8_t kBison = 0x0A;

    bool Asks(M68K &c, uint32_t test) {
        Call(c, test);
        return !c.flagZ;
    }
    bool Starts(M68K &c, uint32_t routine) {
        Call(c, routine);
        return true;
    }

    // What a standing player can go on to, in the order the game asks
    bool StandingChoices(M68K &c, uint32_t player) {
        if (Ram8(Var(c, kRoundTimeOver))) {
            return Starts(c, kTimeOverPose);
        }
        if (Asks(c, kOtherBeaten)) {
            return Starts(c, kWinPose);
        }
        if (Asks(c, kPausedPose) || Ram8(Var(c, kRoundStopped))) {
            return false;
        }
        if (Asks(c, kMustTurn)) {
            return Starts(c, kTurn);
        }
        if (Ram8(player + kComputer)) {
            return false;
        }
        if (Asks(c, kPressed)) {
            return Starts(c, kAttack);
        }
        if (Asks(c, kHoldsDown)) {
            return Starts(c, kCrouch);
        }
        if (Ram8(player + kCustomCombo)) {
            return false;
        }
        if (Asks(c, kHoldsUp)) {
            return Starts(c, kJump);
        }
        if (Asks(c, kGuardTest)) {
            return Starts(c, kGuardPose);
        }
        if (Asks(c, kHoldsAcross)) {
            return Starts(c, kWalk);
        }
        return false;
    }

    // ... and crouched: still holding down, it may jump or guard (or stays crouched); let go, it may walk, or else
    // it rises
    enum class Then { Started, Stays, Rises };
    Then CrouchedChoices(M68K &c, uint32_t player) {
        if (Ram8(Var(c, kRoundTimeOver))) {
            Starts(c, kTimeOverPose);
            return Then::Started;
        }
        if (Asks(c, kOtherBeaten)) {
            Starts(c, kWinPose);
            return Then::Started;
        }
        if (Asks(c, kPausedPose)) {
            return Then::Stays;
        }
        if (Asks(c, kMustTurn)) {
            Starts(c, kTurn);
            return Then::Started;
        }
        if (Ram8(player + kComputer)) {
            return Then::Stays;
        }
        if (Asks(c, kPressed)) {
            Starts(c, kAttack);
            return Then::Started;
        }
        if (Asks(c, kHoldsDown)) {
            if (Asks(c, kHoldsUp)) {
                Starts(c, kJump);
                return Then::Started;
            }
            if (Asks(c, kGuardTest)) {
                Starts(c, kGuardPose);
                return Then::Started;
            }
            return Then::Stays;
        }
        if (!Ram8(player + kCustomCombo) && Asks(c, kHoldsAcross)) {
            Starts(c, kWalk);
            return Then::Started;
        }
        return Then::Rises;
    }

    // 021856. The standing step's end: Sodom's thing made if he has one to make, and the animation on
    void StandingEnd(M68K &c, uint32_t player) {
        if (Ram8(player + kCharacter) == kSodom && Ram8(player + kSodomsThing)) {
            SetRam8(player + kSodomsThing, 0);
            if (Asks(c, kTakeObject)) {
                const uint32_t thing = c.a[4] & 0xFFFFFF;
                SetRam8(thing, static_cast<uint8_t>(Ram8(thing) + 1));
                SetRam16(thing + 0x02, 0x3A05);
                SetRam16(thing + 0x36, static_cast<uint16_t>(c.a[6]));
            }
        }
        Call(c, kAnimate);
    }

    // 021732. A standing player's step (kActionStep): 0 the standing begins (its animation), 2 standing, 4 crouched
    // (the game keeps a crouch here once its going down is over: kLow), 6 (only Sodom's thing, and the animation)
    void Standing(M68K &c) {
        const uint32_t player = c.a[6];
        SetRam8(player + kTurnFast, 0);
        uint8_t step = Ram8(player + kActionStep);
        SetLow8(c.d[0], step);
        SetLow16(c.d[1], Code16(c, kStandActions + step));
        if (step == 0) {
            // 02174A: the animation to begin with: standing (M. Bison's own; in a Custom Combo its own), or the
            // computer's crouched one
            step = 2;
            c.d[0] = 0;
            if (!Ram8(player + kComputer)) {
                if (Ram8(player + kCharacter) == kBison) {
                    c.d[0] = 0x30;
                }
                if (Ram8(player + kCustomCombo)) {
                    SetRam8(player + 0x270, 0);
                    c.d[0] = 2;
                }
            } else if (Ram8(player + kLow)) {
                step = 4;
                c.d[0] = 1;
            }
            SetRam8(player + kActionStep, static_cast<uint8_t>(Ram8(player + kActionStep) + step));
            Call(c, kSetAnimation);
            step = 2; // (and on as step 2 does)
        }
        if (step == 2) {
            // 02177E
            SetRam8(player + kLow, 0);
            if (Ram8(player + kCustomCombo)) {
                Call(c, 0x01FE5E);
                if (Ram8(player + kCustomCombo) == 1) {
                    // (the Custom Combo's last step: back to standing's own animation)
                    SetRam8(player + kThrowing, 0);
                    c.d[0] = 0;
                    Call(c, kSetAnimation);
                }
            }
            if (!StandingChoices(c, player)) {
                Call(c, kAnimate);
            }
            return;
        }
        if (step == 4) {
            // 0217FA
            SetRam8(player + kLow, 1);
            switch (CrouchedChoices(c, player)) {
            case Then::Started: return;
            case Then::Rises: Call(c, kCrouchRises); return;
            case Then::Stays: break;
            }
        }
        StandingEnd(c, player);
    }

    // --- crouching (kAction kActionCrouch) ---

    constexpr uint32_t kCrouchActions = 0x021890; // the crouching routine's steps (words, from here)
    constexpr uint32_t kFrameTime = 0x32;          // word: steps left of the animation frame
    constexpr uint32_t kFrameWord = 0x34;          // word: the frame's second: bit 15, the animation's end (a jump
                                                   // on); the rest picks the crouch's going down and coming up
    constexpr uint32_t kCrouched = 0x023EB0;      // the going down's animation over: crouched (standing's step 4)
    constexpr uint32_t kRisenStanding = 0x023E82; // the rising's animation over: standing (step 0)

    // The animation by the frame's number (+4: coming up; +7: going down again), the frame's time kept
    void CrouchAnimation(M68K &c, uint32_t player, uint16_t plus) {
        SetLow16(c.d[3], Ram16(player + kFrameTime));
        SetLow16(c.d[0], static_cast<uint16_t>((Ram16(player + kFrameWord) & 0x7FFF) + plus));
        Call(c, kSetAnimation);
        SetRam16(player + kFrameTime, static_cast<uint16_t>(c.d[3]));
    }

    // Holding nothing new: turned round if the other has gone past, and the animation on (or, at its end, `next`)
    void CrouchGoesOn(M68K &c, uint32_t player, uint32_t next) {
        if (Asks(c, kMustTurn)) {
            Call(c, kTurn);
        } else if (static_cast<int16_t>(Ram16(player + kFrameWord)) < 0) {
            Call(c, next);
        } else {
            Call(c, kAnimate);
        }
    }

    // What a crouching player goes on to: true if something new began
    bool CrouchChoices(M68K &c) {
        if (Ram8(Var(c, kRoundTimeOver))) {
            return Starts(c, kTimeOverPose);
        }
        if (Asks(c, kOtherBeaten)) {
            return Starts(c, kWinPose);
        }
        return false;
    }
    bool CrouchMoves(M68K &c) {
        if (Asks(c, kPressed)) {
            return Starts(c, kAttack);
        }
        if (Asks(c, kHoldsUp)) {
            return Starts(c, kJump);
        }
        if (Asks(c, kGuardTest)) {
            return Starts(c, kGuardPose);
        }
        return false;
    }

    // 021880. A crouching player's step (kActionStep): 0 the going down begins, 2 going down, 4 the rising begins, 6
    // rising. (Once down, the player is kept as standing's step 4.)
    void Crouching(M68K &c) {
        const uint32_t player = c.a[6];
        SetRam8(player + kTurnFast, 0);
        const uint8_t step = Ram8(player + kActionStep);
        SetLow8(c.d[0], step);
        SetLow16(c.d[1], Code16(c, kCrouchActions + step));
        if (step > 6) {
            return Call(c, kCrouchActions + static_cast<uint32_t>(static_cast<int16_t>(c.d[1] & 0xFFFF)));
        }
        if (step == 0 || step == 4) {
            // 021898, 021914: (the computer's own animation: crouching, or standing up)
            SetRam8(player + kActionStep, static_cast<uint8_t>(step + 2));
            if (Ram8(player + kComputer)) {
                c.d[0] = step == 0 ? 7 : 4;
                Call(c, kSetAnimation);
            }
        }
        if (step <= 2) {
            // 0218A8. Down: goes on down; let go, it rises (from the frame it has reached)
            if (CrouchChoices(c)) {
                return;
            }
            if (!Ram8(player + kComputer)) {
                if (CrouchMoves(c)) {
                    return;
                }
                if (!Asks(c, kHoldsDown)) {
                    if (Asks(c, kHoldsAcross)) {
                        return static_cast<void>(Starts(c, kWalk));
                    }
                    SetRam8(player + kActionStep, static_cast<uint8_t>(Ram8(player + kActionStep) + 2));
                    return CrouchAnimation(c, player, 4);
                }
            }
            return CrouchGoesOn(c, player, kCrouched);
        }
        // 021924. Rising: down again goes back down (step 0)
        if (CrouchChoices(c)) {
            return;
        }
        if (!Ram8(player + kComputer)) {
            if (CrouchMoves(c)) {
                return;
            }
            if (Asks(c, kHoldsDown)) {
                SetRam8(player + kActionStep, 0);
                return CrouchAnimation(c, player, 7);
            }
            if (Asks(c, kHoldsAcross)) {
                return static_cast<void>(Starts(c, kWalk));
            }
        }
        CrouchGoesOn(c, player, kRisenStanding);
    }

    // --- walking (kAction kActionWalk) ---

    constexpr uint32_t kWalkActions = 0x0219A0; // the walking routine's steps (words, from here)
    constexpr uint32_t kWalkingBack = 0x3C;     // byte: 1 walking back, 0 forward (by the stick); also the number of
                                                // the stick's bit (kStick) for the other way
    constexpr uint32_t kWalkSpeed = 0x40;       // long: added to the place across (kX and its fraction) each step
    constexpr uint32_t kWalkSpeeds = 0x09886C;  // by character: two longs, walking forward and back (as if facing left)
    constexpr uint32_t kComputerWalksBack = 0x214; // byte: the way the computer wants to walk
    constexpr uint32_t kHoldsBackOrForward = 0x0233A8;
    constexpr uint32_t kStopWalking = 0x023E70;     // standing again (M. Bison's own standing)
    constexpr uint32_t kWalkOn = 0x023294;          // the other way held: the walk again that way; and a step across

    // 0219AC. The walk's animation and speed, facing the other
    void WalkBegins(M68K &c, uint32_t player) {
        SetRam8(player + kLow, 0);
        SetRam8(player + kFacesRight, Ram8(player + 0x131));
        c.d[0] = Ram8(player + kWalkingBack) ? 3 : 2;
        Call(c, kSetAnimation);
        c.d[1] = 0;
        if (Ram8(player + kWalkingBack)) {
            SetLow8(c.d[1], 4);
        }
        SetLow16(c.d[0], static_cast<uint16_t>((static_cast<int16_t>(static_cast<int8_t>(Ram8(player + kCharacter))) << 3) + (c.d[1] & 0xFFFF)));
        c.a[0] = kWalkSpeeds;
        uint32_t speed = Mem32(c, kWalkSpeeds + static_cast<uint32_t>(static_cast<int16_t>(c.d[0] & 0xFFFF)));
        if (Ram8(player + kFacesRight)) {
            speed = 0u - speed;
        }
        c.d[0] = speed;
        SetRam32(player + kWalkSpeed, speed);
    }

    // 021990. A walking player's step (kActionStep): 0 the walk begins, 2 walking
    void Walking(M68K &c) {
        const uint32_t player = c.a[6];
        SetRam8(player + kTurnFast, 0);
        const uint8_t step = Ram8(player + kActionStep);
        SetLow8(c.d[0], step);
        SetLow16(c.d[1], Code16(c, kWalkActions + step));
        if (step > 2) {
            return Call(c, kWalkActions + static_cast<uint32_t>(static_cast<int16_t>(c.d[1] & 0xFFFF)));
        }
        if (step == 0) {
            SetRam8(player + kActionStep, 2);
            WalkBegins(c, player);
        }
        for (;;) {
            // 0219EA
            if (Ram8(Var(c, kRoundTimeOver))) {
                return Call(c, kTimeOverPose);
            }
            if (Asks(c, kOtherBeaten)) {
                return Call(c, kWinPose);
            }
            if (Asks(c, kPausedPose)) {
                return Call(c, kAnimate);
            }
            if (Ram8(player + kComputer)) {
                // 021A3E. The computer's walk: turned round, or the other way, it begins again
                if (Asks(c, kMustTurn)) {
                    WalkBegins(c, player);
                    continue;
                }
                const uint8_t way = Ram8(player + kComputerWalksBack);
                SetLow8(c.d[0], way);
                if (way != Ram8(player + kWalkingBack)) {
                    SetRam8(player + kWalkingBack, way);
                    WalkBegins(c, player);
                    continue;
                }
                c.d[0] = Ram32(player + kWalkSpeed);
                SetRam32(player + kX, Ram32(player + kX) + c.d[0]);
                return Call(c, kAnimate);
            }
            if (Asks(c, kPressed)) {
                return Call(c, kAttack);
            }
            if (Asks(c, kHoldsUp)) {
                return Call(c, kJump);
            }
            if (Asks(c, kGuardTest)) {
                return Call(c, kGuardPose);
            }
            if (Asks(c, kHoldsDown)) {
                return Call(c, kCrouch);
            }
            if (!Asks(c, kHoldsBackOrForward)) {
                return Call(c, kStopWalking);
            }
            if (Asks(c, kMustTurn)) {
                return Call(c, kTurn);
            }
            Call(c, kWalkOn);
            return Call(c, kAnimate);
        }
    }

    // --- jumping (kAction kActionJump) ---

    constexpr uint32_t kJumpActions = 0x021A70; // the jumping routine's steps (words, from here)
    constexpr uint32_t kFly = 0x02331A;          // a step through the air: across and up by the speeds, the
                                                 // speeds by theirs; C set once below the floor (kFloor)
    constexpr uint32_t kFloor = 0x64;            // word: the height of the floor under the player
    constexpr uint32_t kYFraction = 0x16;        // word: below kY
    constexpr uint32_t kJumpingWay = 0x3C;       // byte (as the jump began): 1 forward, FF back, 0 straight up
    constexpr uint32_t kLands = 0x01472E;        // landing: dust, sound and the like
    constexpr uint32_t kAirAttack = 0x0232F6;    // an attack in the air (the character's table of them)
    constexpr uint32_t kWallJumpTest = 0x023CD4, kWallJump = 0x023D3C; // (Chun-Li, Guy: near a wall, high enough)
    constexpr uint32_t kAirGuard = 0x023D5E;
    constexpr uint32_t kComputerAirAttacks = 0x027E32, kComputerWallJumps = 0x028034, kComputerAirGuards = 0x028086;
    constexpr uint32_t kComputerKeepsGuarding = 0x027E94;
    constexpr uint32_t kBounceSpeeds = 0x09892C; // by character, 64 bytes; at +20 the wall jump's: across, up, and
                                                 // what is added to each every step
    constexpr uint8_t kRolento = 0x0E, kSagat = 0x0B, kShinAkuma = 0x14;
    constexpr uint8_t kJumpAnimations[3] = {0x12, 0x13, 0x14}; // straight up, forward, back
    constexpr uint8_t kLandingAnimation = 0x11;

    // Through the air a step; true on landing (then put on the floor, out of the air, its attack over)
    bool Flies(M68K &c, uint32_t player, bool clearFraction) {
        Call(c, kFly);
        if (!c.flagC) {
            return false;
        }
        SetRam16(player + kY, Ram16(player + kFloor));
        if (clearFraction) {
            SetRam16(player + kYFraction, 0);
        }
        SetRam8(player + kInAir, 0);
        SetRam8(player + kAttackOut, 0);
        Call(c, kLands);
        return true;
    }

    void LandingAnimation(M68K &c, uint32_t player, bool add) {
        SetRam8(player + kActionStep, add ? static_cast<uint8_t>(Ram8(player + kActionStep) + 2) : 4);
        c.d[0] = kLandingAnimation;
        Call(c, kSetAnimation);
    }

    // Landed after an attack, a guard or a move in the air: it may guard at once, else the landing
    void LandsAfter(M68K &c, uint32_t player) {
        if (!Ram8(player + kComputer) && Asks(c, kGuardTest)) {
            return Call(c, kGuardPose);
        }
        LandingAnimation(c, player, false);
    }

    // 021A64. A jumping player's step (kActionStep): 0 about to leave the floor, 2 in the air, 4 landed, 6 attacking
    // in the air, 8 guarding in the air, A bouncing off the wall (a wall jump), C a move of its own in the air
    void Jumping(M68K &c) {
        const uint32_t player = c.a[6];
        const uint8_t step = Ram8(player + kActionStep);
        SetLow8(c.d[0], step);
        SetLow16(c.d[1], Code16(c, kJumpActions + step));
        const bool computer = Ram8(player + kComputer) != 0;
        switch (step) {
        case 0: // 021A80. Off the floor once the crouch before the jump is over
            if (static_cast<int16_t>(Ram16(player + kFrameWord)) >= 0) {
                return Call(c, kAnimate);
            }
            SetRam8(player + kActionStep, static_cast<uint8_t>(Ram8(player + kActionStep) + 2));
            SetRam8(player + kLow, 0);
            SetRam8(player + kInAir, 1);
            c.d[0] = Ram8(player + kJumpingWay) == 1 ? kJumpAnimations[1] : Ram8(player + kJumpingWay) == 0xFF ? kJumpAnimations[2] : kJumpAnimations[0];
            return Call(c, kSetAnimation);
        case 2: // 021AB2. In the air: an attack, a wall jump or a guard; on landing at once an attack, a guard, a turn
            if (Flies(c, player, true)) {
                if (!computer) {
                    if (Ram8(player + kCharacter) == kRolento && Asks(c, 0x04431E)) {
                        return Call(c, 0x04433C); // (Rolento's own landing)
                    }
                    if (Asks(c, kPressed)) {
                        return Call(c, kAttack);
                    }
                    if (Asks(c, kGuardTest)) {
                        return Call(c, kGuardPose);
                    }
                    if (Asks(c, kMustTurn)) {
                        return Call(c, kTurn);
                    }
                }
                return LandingAnimation(c, player, true);
            }
            if (computer) {
                if (Asks(c, kComputerAirAttacks)) {
                    return Call(c, kAirAttack);
                }
                if (Asks(c, kComputerWallJumps)) {
                    return Call(c, kWallJump);
                }
                if (Asks(c, kComputerAirGuards)) {
                    return Call(c, kAirGuard);
                }
                return Call(c, kAnimate);
            }
            if (Asks(c, kPressed)) {
                return Call(c, kAirAttack);
            }
            if (Asks(c, kWallJumpTest)) {
                return Call(c, kWallJump);
            }
            if (Asks(c, kGuardTest)) {
                return Call(c, kAirGuard);
            }
            return Call(c, kAnimate);
        case 4: // 021B52. Landed: the landing's animation, then standing (or crouched, holding down)
            if (computer) {
                return Call(c, static_cast<int16_t>(Ram16(player + kFrameWord)) < 0 ? kRisenStanding : kAnimate);
            }
            if (Asks(c, kPressed)) {
                return Call(c, kAttack);
            }
            if (Asks(c, kMustTurn)) {
                return Call(c, kTurn);
            }
            if (Asks(c, kGuardTest)) {
                return Call(c, kGuardPose);
            }
            if (static_cast<int16_t>(Ram16(player + kFrameWord)) >= 0) {
                return Call(c, kAnimate);
            }
            return Call(c, Asks(c, kHoldsDown) ? kCrouch : kRisenStanding);
        case 6: // 021B92. Attacking in the air (in a Custom Combo, once the attack is over, free to attack again)
            if (Flies(c, player, false)) {
                return LandsAfter(c, player);
            }
            if (static_cast<int16_t>(Ram16(player + kFrameWord)) < 0 && Ram8(player + kCustomCombo)) {
                SetRam8(player + kActionStep, 2);
                SetRam8(player + kAttackOut, 0);
            }
            Call(c, kAnimate);
            if (Ram8(player + kCharacter) == kSagat && Ram8(player + kSodomsThing)) {
                SetRam8(player + kSodomsThing, 0);
                return Call(c, 0x027320); // (Sagat's own)
            }
            if (Ram8(player + kCharacter) == kShinAkuma) {
                return Call(c, 0x04FEE8); // (Shin Akuma's own)
            }
            return;
        case 8: // 021C08. Guarding in the air: while still guarding stays so, else back to the jump
            if (Flies(c, player, false)) {
                return LandsAfter(c, player);
            }
            if (computer) {
                if (Asks(c, kComputerAirAttacks)) {
                    return Call(c, kAirAttack);
                }
                if (!Asks(c, kComputerKeepsGuarding)) {
                    SetRam8(player + kActionStep, 2);
                }
                return Call(c, kAnimate);
            }
            if (Asks(c, kPressed)) {
                return Call(c, kAirAttack);
            }
            if (!Asks(c, kGuardTest)) {
                SetRam8(player + kActionStep, 2);
            }
            return Call(c, kAnimate);
        case 0x0A: { // 021C6E. Off the wall, its animation over: the wall jump's speeds, facing the other way
            if (!(Ram8(player + kFrameWord) & 0x80)) {
                return Call(c, kAnimate);
            }
            SetRam8(player + kActionStep, 2);
            c.d[0] = 0;
            SetLow8(c.d[0], Ram8(player + kCharacter));
            SetLow16(c.d[0], static_cast<uint16_t>(static_cast<int16_t>(static_cast<int8_t>(c.d[0])) << 6));
            c.a[0] = kBounceSpeeds + 0x20 + static_cast<uint32_t>(static_cast<int16_t>(c.d[0] & 0xFFFF));
            uint32_t across = Mem32(c, c.a[0]);
            SetRam32(player + 0x44, Mem32(c, c.a[0] + 4));
            uint32_t acrossMore = Mem32(c, c.a[0] + 8);
            SetRam32(player + 0x4C, Mem32(c, c.a[0] + 12));
            c.a[0] += 12;
            SetRam8(player + kFacesRight, Ram8(player + kFacesRight) ^ 1);
            if (Ram8(player + kFacesRight)) {
                across = 0u - across;
                acrossMore = 0u - acrossMore;
            }
            c.d[1] = acrossMore;
            SetRam32(player + kWalkSpeed, across);
            SetRam32(player + 0x48, acrossMore);
            c.d[0] = 0x13;
            return Call(c, kSetAnimation);
        }
        case 0x0C: // 021CB8. A move of its own in the air, till its animation is over
            if (Flies(c, player, false)) {
                return LandsAfter(c, player);
            }
            if (static_cast<int16_t>(Ram16(player + kFrameWord)) < 0) {
                SetRam8(player + kActionStep, 2);
                SetRam8(player + kAttackOut, 0);
            }
            return Call(c, kAnimate);
        default:
            return Call(c, kJumpActions + static_cast<uint32_t>(static_cast<int16_t>(c.d[1] & 0xFFFF)));
        }
    }

    // --- attacking (kAction kActionAttack) ---

    constexpr uint32_t kAttacks = 0x097AEC, kComputerAttacks = 0x097B4C; // by character: its attacking routine
    constexpr uint32_t kCustomComboStep = 0x01FE5E;                       // a Custom Combo's own, every step of it
    constexpr uint32_t kTimer28E = 0x28E, kTimer293 = 0x293, kTimer240 = 0x240;

    // 021E04. An attacking player's step: the character's own routine for its attacks (the computer's own)
    void Attacking(M68K &c) {
        const uint32_t player = c.a[6];
        if (Ram8(player + kCustomCombo)) {
            Call(c, kCustomComboStep);
        }
        CountDown(player + kTimer25B);
        if (!Ram8(player + kComputer)) {
            CountDown(player + kTimer28E);
            CountDown(player + kTimer293);
            CountDown(player + kTimer240);
        }
        SetLow8(c.d[0], Ram8(player + kCharacter));
        SetLow16(c.d[0], static_cast<uint16_t>((c.d[0] & 0xFFFF) << 2));
        const uint32_t table = Ram8(player + kComputer) ? kComputerAttacks : kAttacks;
        c.a[0] = Mem32(c, table + static_cast<uint32_t>(static_cast<int16_t>(c.d[0] & 0xFFFF)));
        Call(c, c.a[0]);
    }

    // --- guarding (kAction kActionGuardPose) ---

    constexpr uint32_t kGuardActions = 0x021E64; // the guarding routine's steps (words, from here)
    constexpr uint32_t kGuardsLow = 0x023414;    // holding down to guard (or, after some moves, held then)
    enum : uint8_t { kGuardHighBegins = 0x15, kGuardHigh, kGuardHighEnds, kGuardLowBegins, kGuardLow, kGuardLowEnds }; // animations

    void FaceOther(uint32_t player) {
        SetRam8(player + kFacesRight, Ram8(player + 0x131));
    }
    void GuardAnimation(M68K &c, uint32_t player, uint8_t step, uint8_t animation) {
        SetRam8(player + kActionStep, step);
        FaceOther(player);
        c.d[0] = animation;
        Call(c, kSetAnimation);
    }
    void GuardStands(M68K &c, uint32_t player) { // 021F8E
        SetRam8(player + kActionStep, 0);
        SetRam8(player + kLow, 0);
        FaceOther(player);
        c.d[0] = kGuardHigh;
        Call(c, kSetAnimation);
    }
    void GuardCrouches(M68K &c, uint32_t player) { // 021ED6
        SetRam8(player + kActionStep, 4);
        SetRam8(player + kLow, 1);
        FaceOther(player);
        c.d[0] = kGuardLow;
        Call(c, kSetAnimation);
    }
    void GuardGoesOn(M68K &c, uint32_t player) {
        FaceOther(player);
        Call(c, kAnimate);
    }
    // The other (A4): crouched, and on the floor (the computer guards low against it)
    bool OtherLow(M68K &c, uint32_t player) {
        c.a[4] = static_cast<uint32_t>(static_cast<int16_t>(Ram16(player + kOpponent)));
        const uint32_t other = c.a[4] & 0xFFFFFF;
        return Ram8(other + kLow) && !Ram8(other + kInAir);
    }

    // 021E54. A guarding player's step (kActionStep): 0 guarding standing, 2 the standing guard let go, 4 guarding
    // crouched, 6 the crouching guard let go, 8 the guard begins (low when the other is low). The player holds the
    // guard while something is near and it holds away; down changes the one guard into the other. The computer
    // guards while it chooses to, high or low as the other is.
    void Guarding(M68K &c) {
        const uint32_t player = c.a[6];
        SetRam8(player + kTurnFast, 0);
        const uint8_t step = Ram8(player + kActionStep);
        SetLow8(c.d[0], step);
        SetLow16(c.d[1], Code16(c, kGuardActions + step));
        const bool computer = Ram8(player + kComputer) != 0;
        if (step == 8) {
            // 021FDA
            FaceOther(player);
            c.d[1] = 0;
            c.d[0] = kGuardHighBegins;
            if (OtherLow(c, player)) {
                SetLow8(c.d[1], 4);
                c.d[0] = kGuardLowBegins;
            }
            SetRam8(player + kActionStep, static_cast<uint8_t>(c.d[1]));
            return Call(c, kSetAnimation);
        }
        if (step > 8) {
            return Call(c, kGuardActions + static_cast<uint32_t>(static_cast<int16_t>(c.d[1] & 0xFFFF)));
        }
        const bool low = step >= 4;
        if (step == 0 || step == 4) {
            if (Ram8(Var(c, kRoundTimeOver))) {
                return Call(c, kTimeOverPose);
            }
            if (Asks(c, kOtherBeaten)) {
                return Call(c, kWinPose);
            }
        }
        if (computer) {
            if (step == 2 || step == 6) {
                // 021F14, 021FC8
                if (static_cast<int16_t>(Ram16(player + kFrameWord)) < 0) {
                    return Call(c, low ? kCrouched : kRisenStanding);
                }
                return GuardGoesOn(c, player);
            }
            // 021EBC, 021F74
            if (!Asks(c, kComputerKeepsGuarding)) {
                return Call(c, low ? kCrouched : kRisenStanding);
            }
            const bool otherLow = OtherLow(c, player);
            if (otherLow == low) {
                return GuardGoesOn(c, player);
            }
            return low ? GuardStands(c, player) : GuardCrouches(c, player);
        }
        if (Asks(c, kPressed)) {
            return Call(c, kAttack);
        }
        if (Asks(c, kHoldsUp)) {
            return Call(c, kJump);
        }
        switch (step) {
        case 0: // 021E84
            if (!Asks(c, kGuardTest)) {
                return GuardAnimation(c, player, static_cast<uint8_t>(Ram8(player + kActionStep) + 2), kGuardHighEnds);
            }
            if (Asks(c, kGuardsLow)) {
                return GuardCrouches(c, player);
            }
            return GuardGoesOn(c, player);
        case 4: // 021F3C
            if (!Asks(c, kGuardTest)) {
                return GuardAnimation(c, player, static_cast<uint8_t>(Ram8(player + kActionStep) + 2), kGuardLowEnds);
            }
            if (!Asks(c, kHoldsDown)) {
                return GuardStands(c, player);
            }
            return GuardGoesOn(c, player);
        default: // 2 (021EF4), 6 (021FA8): guarding again, or walking away, or the animation over
            if (Asks(c, kGuardTest)) {
                return low ? GuardCrouches(c, player) : GuardStands(c, player);
            }
            if (Asks(c, kHoldsAcross)) {
                return Call(c, kWalk);
            }
            if (static_cast<int16_t>(Ram16(player + kFrameWord)) < 0) {
                return Call(c, low ? kCrouched : kRisenStanding);
            }
            return GuardGoesOn(c, player);
        }
    }

    // --- special moves (kAction 0E, 10) ---

    constexpr uint32_t kSpecials = 0x097BAC, kComputerSpecials = 0x097C0C; // by character: its special moves' routine

    // 022000. A player in a special move: the character's own routine for them (the computer's own)
    void SpecialMove(M68K &c) {
        const uint32_t player = c.a[6];
        CountDown(player + kTimer25B);
        SetRam8(player + 0x24C, 0);
        SetLow8(c.d[0], Ram8(player + kCharacter));
        SetLow16(c.d[0], static_cast<uint16_t>((c.d[0] & 0xFFFF) << 2));
        const uint32_t table = Ram8(player + kComputer) ? kComputerSpecials : kSpecials;
        c.a[0] = Mem32(c, table + static_cast<uint32_t>(static_cast<int16_t>(c.d[0] & 0xFFFF)));
        Call(c, c.a[0]);
    }

    // --- stunned (kAction kActionStunned) ---

    constexpr uint32_t kStunActions = 0x022032; // the stunned routine's steps (words, from here)
    constexpr uint32_t kStunLeft = 0x3A;         // word: steps left of the stun
    constexpr uint32_t kMashesStick = 0x022094;  // the stick newly moved (the computer: its own, 0280BA)
    constexpr uint32_t kMashesStickToo = 0x0220A4;
    constexpr uint32_t kMashesButtons = 0x0220B8; // a button newly pressed
    constexpr uint32_t kStunOver = 0x0220D4;

    // One step off the stun; true once it is over
    bool StunCountsDown(uint32_t player) {
        const uint16_t left = static_cast<uint16_t>(Ram16(player + kStunLeft) - 1);
        SetRam16(player + kStunLeft, left);
        return left == 0;
    }

    // 022026. A stunned player's step (kActionStep): 0 stunned, 2 over. The stun counts down a step at a time, and
    // a step more (the animation with it) for each of: the stick moved, then, if it was, the stick again (a second
    // test of its own), and a button pressed. Shin Akuma is never stunned for long.
    void Stunned(M68K &c) {
        const uint32_t player = c.a[6];
        const uint8_t step = Ram8(player + kActionStep);
        SetLow8(c.d[0], step);
        SetLow16(c.d[1], Code16(c, kStunActions + step));
        auto over = [&] { SetRam8(player + kActionStep, static_cast<uint8_t>(Ram8(player + kActionStep) + 2)); };
        if (step == 0) {
            // 022036
            SetRam8(player + kKnocksDown, 0);
            if (Ram8(player + kCharacter) == kShinAkuma) {
                return Call(c, kStunOver);
            }
            if (Ram8(Var(c, kRoundTimeOver)) || Asks(c, kOtherBeaten) || StunCountsDown(player)) {
                return over();
            }
            if (Asks(c, kMashesStick)) {
                Call(c, kAnimate);
                if (StunCountsDown(player)) {
                    return over();
                }
                if (Asks(c, kMashesStickToo)) {
                    Call(c, kAnimate);
                    if (StunCountsDown(player)) {
                        return over();
                    }
                }
            }
            if (Asks(c, kMashesButtons)) {
                Call(c, kAnimate);
                if (StunCountsDown(player)) {
                    return over();
                }
            }
            return Call(c, kAnimate);
        }
        if (step == 2) {
            // 0220D4. Over: standing (or crouched, or guarding, as held)
            Call(c, 0x0272EC);
            Call(c, 0x014874);
            SetRam8(player + 0x5D, 0);
            SetRam8(player + 0x5E, 0);
            SetRam32(player + 0x146, 0);
            FaceOther(player);
            SetRam8(player + kTimer28A, 0x14);
            if (Ram8(player + kComputer)) {
                return Call(c, kRisenStanding);
            }
            if (Asks(c, kGuardTest)) {
                return Call(c, kGuardPose);
            }
            return Call(c, (Ram8(player + kStick) & kStickHeldDown) ? kCrouched : kRisenStanding);
        }
        Call(c, kStunActions + static_cast<uint32_t>(static_cast<int16_t>(c.d[1] & 0xFFFF)));
    }

    // --- turning round (kAction 08) ---

    constexpr uint32_t kTurnActions = 0x021D32; // the turning routine's steps (words, from here)

    // The turn's animation over: standing, or crouched (the computer's: as it was, kLow); otherwise on
    void TurnGoesOn(M68K &c, uint32_t player, uint32_t over) {
        if (static_cast<int16_t>(Ram16(player + kFrameWord)) < 0) {
            return Call(c, over);
        }
        SetRam8(player + kFacesRight, Ram8(player + 0x131));
        Call(c, kAnimate);
    }

    // 021D22. A player turning round (kActionStep): 0 standing, 2 crouched. Holding down or not changes one into
    // the other from the frame reached.
    void Turning(M68K &c) {
        const uint32_t player = c.a[6];
        SetRam8(player + kTurnFast, 0);
        const uint8_t step = Ram8(player + kActionStep);
        SetLow8(c.d[0], step);
        SetLow16(c.d[1], Code16(c, kTurnActions + step));
        if (step > 2) {
            return Call(c, kTurnActions + static_cast<uint32_t>(static_cast<int16_t>(c.d[1] & 0xFFFF)));
        }
        if (Ram8(player + kComputer)) {
            // 021D92
            if (static_cast<int16_t>(Ram16(player + kFrameWord)) >= 0) {
                return Call(c, kAnimate);
            }
            return Call(c, Ram8(player + kLow) ? kCrouched : kRisenStanding);
        }
        if (Asks(c, kPressed)) {
            return Call(c, kAttack);
        }
        if (Asks(c, kHoldsUp)) {
            return Call(c, kJump);
        }
        if (step == 0) {
            // 021D36. Standing
            if (Asks(c, kHoldsAcross)) {
                return Call(c, kWalk);
            }
            if (Asks(c, kHoldsDown)) {
                SetRam8(player + kActionStep, static_cast<uint8_t>(Ram8(player + kActionStep) + 2));
                return CrouchAnimation(c, player, 0x0D);
            }
            if (Asks(c, kGuardTest)) {
                return Call(c, kGuardPose);
            }
            return TurnGoesOn(c, player, kRisenStanding);
        }
        // 021DA8. Crouched
        if (Asks(c, kGuardTest)) {
            return Call(c, kGuardPose);
        }
        if (Asks(c, kHoldsAcross)) {
            return Call(c, kWalk);
        }
        if (!Asks(c, kHoldsDown)) {
            SetRam8(player + kActionStep, 0);
            return CrouchAnimation(c, player, 0x0A);
        }
        TurnGoesOn(c, player, kCrouched);
    }

    // 021620
    void PlayerInControl(M68K &c) {
        const uint32_t player = c.a[6];
        CountDown(player + kThrowProtection);
        // (0216DC: a Custom Combo going on drains the super combo gauge, one a step)
        if (Ram8(player + kCustomCombo)) {
            const int16_t gauge = static_cast<int16_t>(Ram16(player + kSpecialGauge) - 1);
            SetRam16(player + kSpecialGauge, gauge < 0 ? 0 : static_cast<uint16_t>(gauge));
        }
        if (Ram8(player + kHitStop)) {
            SetRam8(player + kHitStop, static_cast<uint8_t>(Ram8(player + kHitStop) - 1));
            const uint8_t who = Ram8(player + kCharacter);
            if (who == kChunLi || who == kChunLiClassic) {
                Call(c, kChunLiFrozen);
            }
            return Call(c, kKeepIn);
        }
        CountDown(player + kTimer28A);
        CountDown(player + kTimer256);
        CountDown(player + kTimer25B);
        CountDown(player + kTimer287);
        if (ComputerChooses(c, player)) {
            Call(c, kComputerThinks);
            c.d[0] = 0;
        }
        const uint8_t action = Ram8(player + kAction);
        SetLow8(c.d[0], action);
        SetLow16(c.d[1], Code16(c, kActions + action));
        Call(c, kActions + static_cast<uint32_t>(static_cast<int16_t>(c.d[1] & 0xFFFF)));
        Call(c, kKeepIn);
    }
} // namespace

namespace {
    // --- reeling (kControl kControlReeling) ---

    constexpr uint32_t kReactions = 0x02212A; // the routines by kAction while reeling (words, from here): 0 on the
                                              // floor, 2 (0225E6), 4 (022D20)

    constexpr uint32_t kAutoGuardHeld = 0x249;

    // 022D20. The reeling is over: the player is in control again, and may at once attack or guard (the computer as
    // it chooses), else stands (or crouches, holding down)
    void Recovered(M68K &c) {
        const uint32_t player = c.a[6];
        Call(c, 0x0272EC);
        SetRam8(player + 0x5D, 0);
        SetRam8(player + 0x5E, 0);
        SetRam32(player + 0x146, 0);
        SetRam8(player + kHitStop, 0);
        SetRam8(player + 0x282, 0);
        SetRam16(player + 0x280, 0);
        // (023A5C)
        if (Ram8(player + kAutoGuardHeld) && Ram8(player + kAutoGuardOn)) {
            SetRam8(player + kAutoGuardHeld, 0);
        }
        SetRam8(player + kNoThrow, 5);
        FaceOther(player);
        if (!Ram8(player + kComputer)) {
            SetRam8(player + kTimer256, 2);
            if (Asks(c, kPressed)) {
                return Call(c, kAttack);
            }
            if (Asks(c, kGuardTest)) {
                return Call(c, kGuardPose);
            }
            return Call(c, (Ram8(player + kStick) & kStickHeldDown) ? kCrouched : kRisenStanding);
        }
        SetRam8(player + 0x201, 0);
        SetRam16(player + 0x202, 0);
        if (Ram16(player + 0x206)) {
            Call(c, 0x0295EC);
        }
        if (Asks(c, kComputerKeepsGuarding)) {
            return Call(c, 0x027FE2); // (the computer's guard)
        }
        Call(c, Ram8(player + kLow) ? kCrouched : kRisenStanding);
    }

    constexpr uint32_t kReelKind = 0x54;    // byte: how the player reels (2, 6, 0A: with the hit's colours)
    constexpr uint32_t kShake = 0x144;      // word: the picture moved across while frozen by a hit
    constexpr uint32_t kShakes = 0x024068;  // words by the steps of freeze left (read as data: the ROM as it is)
    constexpr uint32_t kReelGoesOn = 0x0225E6; // the reeling after the freeze (by kReelKind)

    // 022544. Reeling, frozen while the hit lands (kHitStop): shaken (on the floor), and flashing as the game's own
    // guard does; once the freeze is over the reeling goes on (kAction on by 2)
    void ReelFrozen(M68K &c) {
        const uint32_t player = c.a[6];
        const uint8_t kind = Ram8(player + kReelKind);
        if (kind == 6 || kind == 0x0A || kind == 2) {
            Call(c, 0x09764C); // (the hit's colours)
        }
        const uint8_t left = static_cast<uint8_t>(Ram8(player + kHitStop) - 1);
        SetRam8(player + kHitStop, left);
        if (left & 0x80) {
            SetRam8(player + kAction, static_cast<uint8_t>(Ram8(player + kAction) + 2));
            SetRam8(player + kActionStep, 0);
            if (Ram8(player + 0x246)) {
                SetRam8(player + 0x246, 0);
            }
            SetRam8(player + kHitStop, 0);
            return Call(c, kReelGoesOn);
        }
        c.a[4] = static_cast<uint32_t>(static_cast<int16_t>(Ram16(player + kOpponent)));
        if (Ram8((c.a[4] & 0xFFFFFF) + kCustomCombo)) {
            Call(c, 0x023C4E);
        }
        if (!Ram8(player + kInAir)) {
            c.a[0] = kShakes;
            SetLow8(c.d[0], Ram8(player + kHitStop));
            SetLow16(c.d[0], static_cast<uint16_t>(static_cast<int16_t>(static_cast<int8_t>(c.d[0])) * 2));
            uint16_t shake = Mem16(c, kShakes + static_cast<uint32_t>(static_cast<int16_t>(c.d[0] & 0xFFFF)));
            if (!Ram8(player + 0x66)) {
                shake = static_cast<uint16_t>(-shake);
            }
            SetLow16(c.d[0], shake);
            SetRam16(player + kShake, shake);
        }
        if (Ram8(player + 0x246) && Ram8(player + kAutoGuardOn) && (kind == 6 || kind == 2) && (Ram8(Var(c, 0x80)) & 1)) {
            SetRam8(player + 0x0C, 0xFF);
            SetRam8(player + kColourSet, static_cast<uint8_t>(Ram8(player + kColourSet) + 4));
        }
        Call(c, 0x02720E);
    }

    constexpr uint32_t kReelKinds = 0x0225F2;   // the reeling routines by kReelKind (words, from here)
    constexpr uint32_t kReelStandingSteps = 0x02260A; // kReelKind 0's steps (words, from here)
    constexpr uint32_t kPushBack = 0x023BEC;    // pushed back a step (by the hit's strength +56); Z clear once it is over
    constexpr uint32_t kPushedSteps = 0x3A;     // word: steps pushed back so far
    constexpr uint32_t kReelAnimation = 0x02319C; // D0: the character's reeling animation of that number
    constexpr uint32_t kKnockedOut = 0x0239E6;
    constexpr uint32_t kReelStarts = 0x09808C;  // by character (8 bytes each): its own routine as a reeling begins
    constexpr uint32_t kComputerAlphaCounter = 0x023B2E;

    // Over: back in control (Recovered), or knocked out (022698, 02279E)
    void ReelOver(M68K &c, uint32_t player) {
        if (Ram16(player + kHealth) & 0x8000) {
            return Call(c, kKnockedOut);
        }
        SetRam32(player + kState, 0x02020400); // (reeling: Recovered)
        SetRam8(player + kKnocksDown, 0);
        SetRam8(player + 0x23F, 0);
    }

    // 0225E6. The reeling after the freeze: the routine for its kReelKind
    void ReelGoesOn(M68K &c) {
        const uint8_t kind = Ram8(c.a[6] + kReelKind);
        SetLow8(c.d[0], kind);
        SetLow16(c.d[1], Code16(c, kReelKinds + kind));
        Call(c, kReelKinds + static_cast<uint32_t>(static_cast<int16_t>(c.d[1] & 0xFFFF)));
    }

    // A reaction to an attack of kind 8 (+55): the player may guard the rest of it, holding back (switched to
    // kReelKind 2, standing or crouched as held); the computer, with a level of gauge, may Alpha counter
    void ReelMayGuard(M68K &c, uint32_t player) {
        if (Ram8(player + kComputer)) {
            // 022762
            if (Ram16(player + kSpecialGauge) >= 0x30 && !(Ram16(player + kHealth) & 0x8000) && Ram8(player + 0x21B)) {
                return Call(c, kComputerAlphaCounter);
            }
            SetRam8(player + kLow, 0);
            c.d[0] = 0x0C;
            if (!Ram8(player + 0x297)) {
                c.a[4] = static_cast<uint32_t>(static_cast<int16_t>(Ram16(player + kOpponent)));
                if (OtherLow(c, player)) {
                    SetRam8(player + kLow, static_cast<uint8_t>(Ram8(player + kLow) + 1));
                    c.d[0] = 0x0F;
                }
            }
            return Call(c, kReelAnimation);
        }
        if (!(Ram8(player + kStick) & kStickHeldBack)) {
            return Call(c, kAnimate);
        }
        SetRam8(player + kReelKind, 2);
        SetRam8(player + kLow, 0);
        c.d[0] = 0x0C;
        if (Ram8(player + kStick) & kStickHeldDown) {
            SetRam8(player + kLow, static_cast<uint8_t>(Ram8(player + kLow) + 1));
            c.d[0] = 0x0F;
        }
        Call(c, kReelAnimation);
    }

    // 0225FE. Reeling, kReelKind 0 (kActionStep): 0 begins (the character's own routine), 2 pushed back, 4 over (to
    // Recovered, or knocked out), 6 knocked out and pushed on
    void ReelStanding(M68K &c) {
        const uint32_t player = c.a[6];
        uint8_t step = Ram8(player + kActionStep);
        SetLow8(c.d[0], step);
        SetLow16(c.d[1], Code16(c, kReelStandingSteps + step));
        if (step == 0) {
            // 022612, 0226C0
            SetRam8(player + kActionStep, static_cast<uint8_t>(Ram8(player + kActionStep) + 2));
            c.a[1] = kReelStarts;
            SetLow8(c.d[1], Ram8(player + kCharacter));
            SetLow16(c.d[1], static_cast<uint16_t>(static_cast<int16_t>(static_cast<int8_t>(c.d[1])) << 3));
            if (!(Ram16(player + kHealth) & 0x8000)) {
                c.a[1] = Mem32(c, kReelStarts + static_cast<uint32_t>(static_cast<int16_t>(c.d[1] & 0xFFFF)));
                Call(c, c.a[1]);
            }
            step = 2;
        }
        switch (step) {
        case 2: { // 02261A
            Call(c, 0x02720E);
            if (Asks(c, kPushBack)) {
                SetRam8(player + kActionStep, static_cast<uint8_t>(Ram8(player + kActionStep) + 2));
                Call(c, 0x027204);
                SetRam8(player + 0x250, 0);
                SetRam8(player + 0x251, 0);
                SetRam8(player + 0x252, 0);
            }
            const bool out = Ram16(player + kHealth) & 0x8000;
            if (out && Ram16(player + kPushedSteps) >= 8) {
                SetRam8(player + kActionStep, 6);
                c.d[0] = 0x1B;
                return Call(c, kReelAnimation);
            }
            if (out || Ram8(player + kReaction) != 8) {
                return Call(c, kAnimate);
            }
            return ReelMayGuard(c, player);
        }
        case 4: // 022698
            return ReelOver(c, player);
        case 6: // 0226B2
            if (Asks(c, kPushBack)) {
                SetRam8(player + kActionStep, 4);
            }
            return;
        default:
            return Call(c, kReelStandingSteps + static_cast<uint32_t>(static_cast<int16_t>(c.d[1] & 0xFFFF)));
        }
    }

    constexpr uint32_t kReelGuardSteps = 0x0226F2; // kReelKind 2's steps (words, from here)

    // 0226E6. Reeling, kReelKind 2: pushed back guarding (kActionStep): 0 begins, 2 pushed back, guarding high or low
    // as held (a new press of down, after some moves, to guard low), 4 over. Pushed back to the end, it faces the
    // way the hit came from.
    void ReelGuarding(M68K &c) {
        const uint32_t player = c.a[6];
        uint8_t step = Ram8(player + kActionStep);
        SetLow8(c.d[0], step);
        SetLow16(c.d[1], Code16(c, kReelGuardSteps + step));
        if (step == 0) {
            SetRam8(player + kActionStep, static_cast<uint8_t>(Ram8(player + kActionStep) + 2));
            step = 2;
        }
        if (step == 4) {
            return ReelOver(c, player);
        }
        if (step != 2) {
            return Call(c, kReelGuardSteps + static_cast<uint32_t>(static_cast<int16_t>(c.d[1] & 0xFFFF)));
        }
        // 0226FC
        if (Asks(c, kPushBack)) {
            SetRam8(player + kActionStep, static_cast<uint8_t>(Ram8(player + kActionStep) + 2));
            SetLow8(c.d[0], (Ram8(player + 0x66) ^ 1) & 1);
            SetRam8(player + kFacesRight, static_cast<uint8_t>(c.d[0]));
            return ReelOver(c, player);
        }
        bool newDown = Ram16(player + kHealth) & 0x8000; // (knocked out: only a new press of down changes it)
        if (!newDown) {
            if (Ram8(player + kComputer)) {
                return ReelMayGuard(c, player);
            }
            if (!(Ram8(player + kStick) & kStickHeldBack)) {
                return Call(c, kAnimate);
            }
            SetRam8(player + kLow, 0);
            c.d[0] = 0x0C;
            if (Ram8(player + 0x297)) {
                newDown = true;
            } else if (Ram8(player + kStick) & kStickHeldDown) {
                SetRam8(player + kLow, static_cast<uint8_t>(Ram8(player + kLow) + 1));
                c.d[0] = 0x0F;
            }
        }
        if (newDown && (Ram8(player + kStickNow) & kStickHeldDown)) {
            // 02274C
            SetRam8(player + 0x297, 0);
            SetRam8(player + kLow, static_cast<uint8_t>(Ram8(player + kLow) + 1));
            c.d[0] = 0x0F;
        }
        Call(c, kReelAnimation);
    }

    constexpr uint32_t kReelHopSteps = 0x022B5E;
    constexpr uint32_t kAtEdge = 0x13C; // byte: 1 at the picture's left edge, 2 at its right (KeepIn)

    // 022B52. Reeling, kReelKind 6: hopped back through the air by a hit there (kActionStep): 0 the hop's speeds, 2
    // flying (pushed back too, against the picture's edge), 4 flying. Its animation over, the player is in control
    // in the air (a jump's step 2); landing, it may guard at once.
    void ReelHops(M68K &c) {
        const uint32_t player = c.a[6];
        const uint8_t step = Ram8(player + kActionStep);
        SetLow8(c.d[0], step);
        SetLow16(c.d[1], Code16(c, kReelHopSteps + step));
        switch (step) {
        case 0: { // 022B64
            SetRam8(player + kActionStep, static_cast<uint8_t>(Ram8(player + kActionStep) + 2));
            SetRam8(player + kJumpingWay, 0xFF);
            SetRam8(player + kInAir, 0xFF);
            SetRam16(player + kPushedSteps, 0);
            SetRam32(player + 0x44, 0x00010000);     // (up)
            SetRam32(player + 0x4C, 0xFFFF9800);     // (and less each step)
            SetRam32(player + 0x48, 0);
            c.d[0] = 0x00060000;                     // (across, away from the hit)
            if (!Ram8(player + 0x66)) {
                c.d[0] = 0u - c.d[0];
            }
            SetRam32(player + kWalkSpeed, c.d[0]);
            return;
        }
        case 2: // 022BA0
            SetRam8(player + 0x23F, 0);
            if (Ram8(player + kAtEdge) && Asks(c, kPushBack)) {
                SetRam8(player + kActionStep, static_cast<uint8_t>(Ram8(player + kActionStep) + 2));
            }
            [[fallthrough]];
        case 4: // 022BB4
            Call(c, kFly);
            if (c.flagC) {
                // Landed: in control, a jump's landing (and may guard at once)
                SetRam32(player + kState, 0x02000604);
                SetRam16(player + kY, Ram16(player + kFloor));
                SetRam8(player + kInAir, 0);
                SetRam8(player + kAttackOut, 0);
                Call(c, kLands);
                if (Asks(c, kGuardTest)) {
                    return Call(c, kGuardPose);
                }
                c.d[0] = kLandingAnimation;
                return Call(c, kSetAnimation);
            }
            if (static_cast<int16_t>(Ram16(player + kFrameWord)) < 0) {
                SetRam32(player + kState, 0x02000602); // (in control, in the air: Jumping's step 2)
                SetRam8(player + kAttackOut, 0);
            }
            return Call(c, kAnimate);
        default:
            return Call(c, kReelHopSteps + static_cast<uint32_t>(static_cast<int16_t>(c.d[1] & 0xFFFF)));
        }
    }

    // --- knocked down (kReelKind 4) ---

    constexpr uint32_t kReelDownSteps = 0x0227C4; // kReelKind 4's steps (words, from here)
    constexpr uint32_t kDizzyAfter = 0x13B;        // byte: stunned once it is up (the stun reached while down)
    constexpr uint32_t kReelStartsDown = 0x098090; // by character (8 bytes each): its own routine as a knock-down begins

    // 0226C8: the character's own routine as a knock-down begins (not when knocked out)
    void DownStarts(M68K &c, uint32_t player) {
        c.a[1] = kReelStartsDown;
        SetLow8(c.d[1], Ram8(player + kCharacter));
        SetLow16(c.d[1], static_cast<uint16_t>(static_cast<int16_t>(static_cast<int8_t>(c.d[1])) << 3));
        if (!(Ram16(player + kHealth) & 0x8000)) {
            c.a[1] = Mem32(c, kReelStartsDown + static_cast<uint32_t>(static_cast<int16_t>(c.d[1] & 0xFFFF)));
            Call(c, c.a[1]);
        }
    }

    // On the floor: put there, out of the air
    void OnTheFloor(uint32_t player) {
        SetRam8(player + kInAir, 0);
        SetRam16(player + kY, Ram16(player + kFloor));
    }

    // 0228DA. Landed from the fall: the computer's Sodom and Dhalsim may at once begin a move of theirs (one it
    // chooses, 0280BA with 09BFC4); else the landing's animation
    void DownLanded(M68K &c, uint32_t player) {
        const uint8_t who = Ram8(player + kCharacter);
        bool move = Ram8(player + kComputer) && (who == kSodom || who == kDhalsim) && !Ram8(player + kDizzyAfter) &&
                    !Ram8(player + 0x258) && !(Ram8(player + kHealth) & 0x80);
        if (move) {
            const uint8_t stopped = Ram8(Var(c, kRoundTimeOver)) | Ram8(Var(c, kRoundStopped)) | Ram8(Var(c, kRoundPaused));
            SetLow8(c.d[0], stopped);
            move = !stopped && !Asks(c, kOtherBeaten);
        }
        if (move) {
            c.a[1] = 0x09BFC4;
            move = Asks(c, 0x0280BA);
        }
        if (!move) {
            c.d[0] = 0x14;
            return Call(c, kReelAnimation);
        }
        SetRam32(player + kState, 0x02000E00); // (in control: a special move)
        SetRam8(player + 0x102, 2);
        SetRam8(player + 0x250, 0);
        SetRam8(player + 0x251, 0);
        SetRam8(player + 0x252, 0);
        SetRam8(player + kMoveNumber, 0x0C);
        SetRam8(player + kAttackOut, who == kDhalsim ? 0 : 1);
    }

    // 02287A. Falling: the animation goes on (but not while still going up), or, not knocked down (kKnocksDown),
    // the player recovers in the air (step 0C) as it starts to fall
    void DownFalls(M68K &c, uint32_t player) {
        const uint8_t reaction = Ram8(player + kReaction);
        const bool rising = !(Ram16(player + 0x44) & 0x8000);
        auto otherInCustomCombo = [&] {
            c.a[4] = static_cast<uint32_t>(static_cast<int16_t>(Ram16(player + kOpponent)));
            return Ram8((c.a[4] & 0xFFFFFF) + kCustomCombo) != 0;
        };
        if (reaction == 0x13) {
            if (otherInCustomCombo() && !rising) {
                return;
            }
            return Call(c, kAnimate);
        }
        if (reaction == 0x11) {
            return Call(c, kAnimate);
        }
        if (rising || otherInCustomCombo()) {
            return;
        }
        if ((Ram16(player + kHealth) & 0x8000) || Ram8(player + kKnocksDown)) {
            return Call(c, kAnimate);
        }
        SetRam8(player + kActionStep, 0x0C);
        if (Ram8(player + 0x29B)) {
            SetRam8(player + 0x29B, 0);
            Call(c, 0x0157A4);
        }
        c.d[0] = 0x18;
        Call(c, kReelAnimation);
    }

    // 0227B8. Reeling, kReelKind 4: knocked down (kActionStep): 0 begins, 2 falling, 4 landed (a moment), 6 bounced
    // up, 8 lying, 0A getting up (stunned, if the stun was reached), 0C recovered in the air (lands on its feet), 0E
    // sliding (from a bounce), 10 lying (from a bounce)
    void ReelDown(M68K &c) {
        const uint32_t player = c.a[6];
        uint8_t step = Ram8(player + kActionStep);
        SetLow8(c.d[0], step);
        SetLow16(c.d[1], Code16(c, kReelDownSteps + step));
        const bool out = Ram16(player + kHealth) & 0x8000;
        switch (step) {
        case 0: // 0227D6
            SetRam8(player + kActionStep, static_cast<uint8_t>(Ram8(player + kActionStep) + 2));
            SetRam8(player + kAttackOut, 0);
            SetRam8(player + kLow, 0);
            Call(c, 0x023930);
            SetRam8(player + kInAir, 0xFF);
            Call(c, 0x02720E);
            if (Ram8(player + 0x23B)) {
                Call(c, 0x022258);
                if (!Ram8(player + kKnocksDown)) {
                    SetRam8(player + 0x57, 6);
                    Call(c, 0x023930);
                } else {
                    c.d[0] = 3;
                    Call(c, 0x026DB0);
                    DownStarts(c, player);
                }
            } else {
                DownStarts(c, player);
            }
            if (Ram8(player + kDizzyAfter)) {
                Call(c, 0x05DA40); // (the stars)
            }
            [[fallthrough]];
        case 2: // 022826
            Call(c, 0x02720E);
            Call(c, kFly);
            if (!c.flagC) {
                return DownFalls(c, player);
            }
            SetRam8(player + kActionStep, static_cast<uint8_t>(Ram8(player + kActionStep) + 2));
            Call(c, out ? 0x014916 : 0x01467A); // (the landing's sound)
            SetRam8(player + kInAir, 0);
            SetRam8(player + kPushedSteps, 4);
            SetRam16(player + kY, Ram16(player + kFloor));
            Call(c, 0x023A9A);
            Call(c, 0x027204);
            SetRam8(player + 0x250, 0);
            SetRam8(player + 0x251, 0);
            SetRam8(player + 0x252, 0);
            return DownLanded(c, player);
        case 4: { // 022966
            const uint8_t left = static_cast<uint8_t>(Ram8(player + kPushedSteps) - 1);
            SetRam8(player + kPushedSteps, left);
            if (left) {
                return DownFalls(c, player);
            }
            SetRam8(player + kActionStep, static_cast<uint8_t>(Ram8(player + kActionStep) + 2));
            SetRam8(player + kInAir, 0xFF);
            Call(c, 0x02399A);
            c.d[0] = 0x15;
            return Call(c, kReelAnimation);
        }
        case 6: // 022982
            Call(c, kFly);
            if (!c.flagC) {
                return Call(c, kAnimate);
            }
            SetRam8(player + kActionStep, static_cast<uint8_t>(Ram8(player + kActionStep) + 2));
            Call(c, 0x023A6E);
            SetRam8(player + kInAir, 0);
            SetRam8(player + kPushedSteps, 8);
            SetRam16(player + kY, Ram16(player + kFloor));
            c.d[0] = 0x16;
            Call(c, kReelAnimation);
            return Call(c, (Ram16(player + kHealth) & 0x8000) ? 0x0148C8 : 0x01462C);
        case 8: { // 0229BC
            const uint8_t left = static_cast<uint8_t>(Ram8(player + kPushedSteps) - 1);
            SetRam8(player + kPushedSteps, left);
            if (left) {
                return Call(c, kAnimate);
            }
            if (out) {
                return Call(c, kKnockedOut);
            }
            SetRam8(player + kActionStep, static_cast<uint8_t>(Ram8(player + kActionStep) + 2));
            c.d[0] = 0x19;
            return Call(c, kReelAnimation);
        }
        case 0x0A: // 0229D4. Getting up; up, stunned (for a time by chance and the character) or in control
            if (static_cast<int16_t>(Ram16(player + kFrameWord)) >= 0) {
                return Call(c, kAnimate);
            }
            if (!Ram8(player + kDizzyAfter)) {
                SetRam32(player + kState, 0x02020400); // (reeling: Recovered)
                SetRam8(player + kKnocksDown, 0);
                SetRam8(player + 0x23F, 0);
                return;
            }
            SetRam32(player + kState, 0x02001200); // (in control: stunned)
            c.d[0] = 0;
            SetRam8(player + kStunTimer, 0);
            SetRam8(player + kStun, 0);
            SetRam8(player + kDizzyAfter, 0);
            SetRam8(player + kStunMost, static_cast<uint8_t>(Ram8(player + kStunMost) + 5));
            Call(c, 0x0272EC);
            SetRam8(player + 0x5D, 0);
            SetRam8(player + 0x5E, 0);
            SetRam32(player + 0x146, 0);
            SetRam8(player + kLow, 0);
            Call(c, 0x01975A); // (a number by chance)
            SetLow16(c.d[0], static_cast<uint16_t>(c.d[0] & 0x1E));
            c.d[1] = 0;
            SetLow8(c.d[1], Ram8(player + kCharacter));
            SetLow16(c.d[1], static_cast<uint16_t>((c.d[1] & 0xFFFF) << 2));
            c.a[0] = Mem32(c, 0x09AE84 + static_cast<uint32_t>(static_cast<int16_t>(c.d[1] & 0xFFFF)));
            Call(c, c.a[0]); // (the character's own: D1 its row)
            SetLow16(c.d[1], static_cast<uint16_t>((c.d[1] & 0xFFFF) << 3));
            SetLow16(c.d[0], static_cast<uint16_t>((c.d[0] & 0xFFFF) + (c.d[1] & 0xFFFF)));
            c.a[0] = 0x09814C;
            SetRam16(player + kStunLeft, Mem16(c, 0x09814C + static_cast<uint32_t>(static_cast<int16_t>(c.d[0] & 0xFFFF))));
            c.d[0] = 0x1A;
            return Call(c, kReelAnimation);
        case 0x0C: // 022A58. Recovered in the air: landing, in control
            Call(c, kFly);
            if (!c.flagC) {
                return Call(c, kAnimate);
            }
            SetRam32(player + kState, 0x02020400);
            SetRam8(player + kKnocksDown, 0);
            SetRam8(player + 0x23F, 0);
            OnTheFloor(player);
            SetRam16(player + kYFraction, 0);
            return Call(c, kLands);
        case 0x0E: // 022A86. Sliding: on across till the animation is over and the time is out
            if (static_cast<int16_t>(Ram16(player + kFrameWord)) < 0) {
                const uint8_t left = static_cast<uint8_t>(Ram8(player + kPushedSteps) - 1);
                SetRam8(player + kPushedSteps, left);
                if (!left) {
                    SetRam32(player + kState, 0x02020400);
                    SetRam8(player + kKnocksDown, 0);
                    SetRam8(player + 0x23F, 0);
                    SetRam8(player + 0x13F, 0);
                    if (out) {
                        return Call(c, kKnockedOut);
                    }
                    FaceOther(player);
                    SetRam8(player + kLow, 1);
                    return;
                }
            }
            c.d[0] = Ram32(player + kWalkSpeed);
            SetRam32(player + kX, Ram32(player + kX) + c.d[0]);
            return Call(c, kAnimate);
        case 0x10: { // 022ACA. Lying (from a bounce), then getting up
            const uint8_t left = static_cast<uint8_t>(Ram8(player + kPushedSteps) - 1);
            SetRam8(player + kPushedSteps, left);
            if (left) {
                return;
            }
            SetRam8(player + kActionStep, 0x0A);
            SetRam8(player + 0x13F, 0);
            if (out) {
                return Call(c, kKnockedOut);
            }
            c.d[0] = 0x19;
            return Call(c, kReelAnimation);
        }
        default:
            return Call(c, kReelDownSteps + static_cast<uint32_t>(static_cast<int16_t>(c.d[1] & 0xFFFF)));
        }
    }

    // --- a reeling begins (kAction 0 of reeling, step 0) ---

    constexpr uint32_t kReelOnFloorSteps = 0x02213C; // (words, from here): 0 begins (022140), 2 frozen (ReelFrozen)
    constexpr uint32_t kReactions2 = 0x02402C;       // by reaction (+55), two bytes: the animation, the kReelKind
    constexpr int32_t kFirstAttack = 0x135;          // from A5: set by the round's first hit ("First attack")
    constexpr uint32_t kReelAnimationNow = 0x5C;     // byte: the reeling's animation (from kReactions2)
    constexpr uint32_t kHitStrength = 0x56;          // byte: the hit's strength (0 light, 1 medium, 2 and on heavy)
    constexpr uint32_t kOnFire = 0x251, kShocked = 0x250, kFrozenStiff = 0x252; // bytes: the hit's effect begun

    // The reeling's animation by the reaction's base and the hit's strength (at most 2 more); then frozen at once
    // if the freeze is already over (ReelFrozen), else next step
    void ReelByStrength(M68K &c, uint32_t player, bool pushBack) {
        SetLow8(c.d[1], Ram8(player + kHitStrength));
        if ((c.d[1] & 0xFF) >= 2) {
            c.d[1] = 2;
        }
        SetLow8(c.d[0], static_cast<uint8_t>((c.d[0] & 0xFF) + (c.d[1] & 0xFF)));
        Call(c, kReelAnimation);
        if (pushBack) {
            Call(c, 0x023AC6);
        }
        c.d[0] = 0;
        if (!Ram8(player + kHitStop)) {
            Call(c, 0x022544);
        }
    }
    void ReelThenFrozen(M68K &c, uint32_t player) {
        Call(c, kReelAnimation);
        c.d[0] = 0;
        if (!Ram8(player + kHitStop)) {
            Call(c, 0x022544);
        }
    }

    // A light reaction, standing or crouched (0222E6; 022330 for kind 1A)
    void ReelLight(M68K &c, uint32_t player, uint8_t reaction, bool kind1A) {
        SetRam8(player + kReelKind, 0);
        SetRam16(player + kPushedSteps, 0);
        FaceOther(player);
        c.d[0] = 7;
        if (!Ram8(player + kLow)) {
            c.d[0] = kind1A ? 4 : (reaction == 8 || reaction == 0) ? 1 : 4;
        }
        ReelByStrength(c, player, true);
    }
    // ... standing only (02234A: kinds 12, 18)
    void ReelStandingOnly(M68K &c, uint32_t player, uint8_t reaction) {
        SetRam8(player + kReelKind, 0);
        SetRam16(player + kPushedSteps, 0);
        FaceOther(player);
        SetRam8(player + kLow, 0);
        c.d[0] = reaction == 0x18 ? 4 : 1;
        ReelByStrength(c, player, false);
    }

    // 022438, 0224A8: a burning or a shocking knock-down (the effect begun once)
    void ReelBurnt(M68K &c, uint32_t player) {
        if (!Ram8(player + kOnFire)) {
            SetRam8(player + kOnFire, static_cast<uint8_t>(Ram8(player + kOnFire) + 1));
            Call(c, 0x05DEBE);
        }
        Call(c, 0x0149F4);
        Call(c, 0x02720E);
        SetRam8(player + kKnocksDown, 1);
        c.d[0] = 0x13;
        Call(c, kReelAnimation);
    }
    void ReelShocked(M68K &c, uint32_t player) {
        if (!Ram8(player + kShocked)) {
            SetRam8(player + kShocked, static_cast<uint8_t>(Ram8(player + kShocked) + 1));
            Call(c, 0x05DF28);
            Call(c, 0x0149F4);
        }
        SetRam8(player + kKnocksDown, 1);
        Call(c, 0x02720E);
        c.d[0] = 0x13;
        Call(c, kReelAnimation);
    }
    // Launched by a burning or shocking hit while in the air: knocked down (kReelKind 4)
    void ReelLaunched(uint32_t player) {
        SetRam8(player + kInAir, 0xFF);
        SetRam8(player + 0x5D, static_cast<uint8_t>(Ram8(player + 0x5D) + 1));
        SetRam8(player + kReelAnimationNow, 0x13);
        SetRam8(player + kReelKind, 4);
    }

    // A guarded hit (022386): pushed back guarding (kReelKind 2), high or low; in the air (6); after some moves (+297)
    // kReelKind 0A
    void ReelGuarded(M68K &c, uint32_t player) {
        if (Ram8(player + 0x297)) {
            // 02240C
            SetRam8(player + kReelKind, 0x0A);
            SetRam16(player + kPushedSteps, 0);
            SetRam8(player + kHitStrength, 0x0A);
            FaceOther(player);
            SetRam8(player + kLow, 0);
            c.d[0] = 0x0C;
            return ReelThenFrozen(c, player);
        }
        FaceOther(player);
        if (Ram8(player + kInAir)) {
            // 0223C2
            SetRam8(player + kReelKind, 6);
            SetRam8(player + kInAir, 0xFF);
            c.d[0] = 0x12;
            return ReelThenFrozen(c, player);
        }
        SetRam8(player + kReelKind, 2);
        SetRam16(player + kPushedSteps, 0);
        if (Ram8(player + kComputer)) {
            // 0223E0
            SetRam8(player + kLow, 0);
            c.d[0] = 0x0C;
            c.a[4] = static_cast<uint32_t>(static_cast<int16_t>(Ram16(player + kOpponent)));
            if (OtherLow(c, player)) {
                SetRam8(player + kLow, static_cast<uint8_t>(Ram8(player + kLow) + 1));
                c.d[0] = 0x0F;
            }
            return ReelThenFrozen(c, player);
        }
        c.d[0] = Ram8(player + kLow) ? 0x0F : 0x0C;
        Call(c, kReelAnimation);
    }

    // 022140. A hit has landed (or been guarded): the reeling it gives, by the reaction (+55: the attack's kind, FF
    // guarded): light ones standing or crouched, knock-downs and launches (kReelKind 4, by the table at 02402C), burning
    // and shocking ones, guarded ones; the round's first hit announced
    void ReelBegins(M68K &c) {
        const uint32_t player = c.a[6];
        if (Ram8(player + kCharacter) == kBison && Ram8(player + 0x1F7)) {
            Call(c, 0x03CB92);
        }
        SetRam8(player + kActionStep, static_cast<uint8_t>(Ram8(player + kActionStep) + 2));
        c.d[0] = 0;
        for (const uint32_t field : {kAttackOut, 0x23Bu, 0x23Fu, kThrowing}) {
            SetRam8(player + field, 0);
        }
        for (const int32_t offset : {0x65E, 0xA5E, 0xE5E, 0x125E}) {
            SetRam8(Var(c, offset), 0);
        }
        for (const uint32_t field : {kCustomCombo, kTimer25B, kThrowProtection, 0x294u, 0x29Au, 0x299u}) {
            SetRam8(player + field, 0);
        }
        SetRam16(player + 0x15A, 0);
        SetRam16(player + 0x158, 0);
        Call(c, 0x09764C); // (the hit's colours)
        const uint8_t reaction = Ram8(player + kReaction);
        if (reaction == 0xFF) {
            return ReelGuarded(c, player);
        }
        SetRam8(player + 0x5E, static_cast<uint8_t>(Ram8(player + 0x5E) + 1));
        Call(c, 0x01F9A6);
        c.a[4] = static_cast<uint32_t>(static_cast<int16_t>(Ram16(player + kOpponent)));
        const uint32_t other = c.a[4] & 0xFFFFFF;
        SetRam8(other + 0x284, 0);
        SetRam8(other + 0x285, 0);
        SetRam8(other + 0x28F, 0);
        if (!Ram8(Var(c, kFirstAttack))) {
            SetRam8(Var(c, kFirstAttack), 0xFF);
            SetRam8(other + 0x95, 8);
            if (Ram16(Var(c, kPlayMode)) && Ram8(player + 0x08) == 4) {
                SetRam8(Var(c, 0x495), 8);
            }
            c.d[0] = 0x28;
            SetLow8(c.d[1], Ram8(other + 0x81));
            Call(c, 0x005A52); // ("First attack")
            Call(c, 0x01906A);
        }
        // 0221FE
        if (reaction < 3 || reaction == 6 || reaction == 8 || reaction == 9 || reaction == 0x0E) {
            return ReelLight(c, player, reaction, false);
        }
        if (reaction == 0x1A) {
            return ReelLight(c, player, reaction, true);
        }
        if (reaction == 0x12 || reaction == 0x18) {
            return ReelStandingOnly(c, player, reaction);
        }
        if (reaction == 0x19) {
            // 02251E: frozen stiff (in the air: knocked down)
            if (!Ram8(player + kInAir)) {
                return ReelStandingOnly(c, player, reaction);
            }
            SetRam8(player + kReelAnimationNow, 0x21);
            SetRam8(player + kReelKind, 4);
            Call(c, 0x05DF92);
            SetRam8(player + kFrozenStiff, 1);
            c.d[0] = 0x21;
            return Call(c, kReelAnimation);
        }
        // 022258: the reaction's animation and kReelKind from the table
        c.d[0] = 0;
        SetLow8(c.d[1], reaction);
        SetLow16(c.d[1], static_cast<uint16_t>(static_cast<int16_t>(static_cast<int8_t>(reaction)) * 2));
        c.a[0] = kReactions2;
        const uint32_t entry = kReactions2 + static_cast<uint32_t>(static_cast<int16_t>(c.d[1] & 0xFFFF));
        SetLow8(c.d[0], Mem8(c, entry));
        SetRam8(player + kReelAnimationNow, static_cast<uint8_t>(c.d[0]));
        SetRam8(player + kReelKind, Mem8(c, entry + 1));
        if (reaction == 0x14 || reaction == 0x16 || reaction == 0x15 || reaction == 0x17) {
            // 022460, 0224D0: burning, shocking
            const bool fire = reaction == 0x14 || reaction == 0x16;
            if (Ram8(player + kInAir)) {
                ReelLaunched(player);
                c.d[0] = 0x13;
                return fire ? ReelBurnt(c, player) : ReelShocked(c, player);
            }
            if (fire) {
                if (!Ram8(player + kOnFire)) {
                    SetRam8(player + kOnFire, static_cast<uint8_t>(Ram8(player + kOnFire) + 1));
                    Call(c, 0x05DEBE);
                }
                Call(c, 0x0149F4);
                Call(c, 0x02720E);
            } else {
                if (!Ram8(player + kShocked)) {
                    SetRam8(player + kShocked, static_cast<uint8_t>(Ram8(player + kShocked) + 1));
                    Call(c, 0x05DF28);
                    Call(c, 0x0149F4);
                }
                SetRam8(player + kKnocksDown, 1);
                Call(c, 0x02720E);
            }
            return (reaction == 0x16 || reaction == 0x17) ? ReelStandingOnly(c, player, reaction) : ReelLight(c, player, reaction, false);
        }
        SetRam8(player + kInAir, 0xFF);
        SetRam8(player + 0x5D, static_cast<uint8_t>(Ram8(player + 0x5D) + 1));
        if (reaction != 0x0B && Ram8(player + kReelKind) == 4) {
            if (reaction == 0x0C) {
                return ReelBurnt(c, player);
            }
            if (reaction == 0x0D) {
                return ReelShocked(c, player);
            }
            if (reaction == 0x11) {
                // 022532
                Call(c, 0x05DF92);
                SetRam8(player + kFrozenStiff, 1);
                c.d[0] = 0x21;
                return Call(c, kReelAnimation);
            }
        }
        Call(c, kReelAnimation); // (D0: the table's animation)
    }

    // 022130. Reeling on the floor (kActionStep): 0 a hit lands (ReelBegins), 2 frozen (ReelFrozen)
    void ReelOnFloor(M68K &c) {
        const uint8_t step = Ram8(c.a[6] + kActionStep);
        SetLow8(c.d[0], step);
        SetLow16(c.d[1], Code16(c, kReelOnFloorSteps + step));
        Call(c, kReelOnFloorSteps + static_cast<uint32_t>(static_cast<int16_t>(c.d[1] & 0xFFFF)));
    }

    // 022116. A step of a player reeling from a hit (or a guarded one): the routine for its kAction, and kept inside
    // the picture
    void PlayerReeling(M68K &c) {
        const uint32_t player = c.a[6];
        SetRam8(player + kTurnFast, 0);
        const uint8_t action = Ram8(player + kAction);
        SetLow8(c.d[0], action);
        SetLow16(c.d[1], Code16(c, kReactions + action));
        Call(c, kReactions + static_cast<uint32_t>(static_cast<int16_t>(c.d[1] & 0xFFFF)));
        Call(c, kKeepIn);
    }
} // namespace

namespace {
    // --- the other kControl states ---

    constexpr uint32_t kThrowPlaces = 0x0984AC; // by thrower, then by the one thrown: by the thrower's frame (+10 of
                                                // it), 8 bytes: across, up, facing (bit 15: +152 set), the frame to show
    constexpr uint32_t kHeldFrames = 0x0987AC;  // by character: the frames shown while held
    constexpr uint32_t kTechWindow = 0x3A;      // byte: steps left in which a throw can be softened
    constexpr uint32_t kSoftened = 0x29B;       // byte: set when the throw was softened (TakeDamage takes less)
    constexpr uint32_t kKeepBothIn = 0x0237BE;

    // By the character's own table (the computer's own, if it has one): kControl 4 (then KeepIn unless +67), 8
    void ByCharacter(M68K &c, uint32_t table, uint32_t computerTable) {
        const uint32_t player = c.a[6];
        SetLow8(c.d[0], Ram8(player + kCharacter));
        SetLow16(c.d[0], static_cast<uint16_t>((c.d[0] & 0xFFFF) << 2));
        c.a[0] = computerTable && Ram8(player + kComputer) ? computerTable : table;
        c.a[0] = Mem32(c, c.a[0] + static_cast<uint32_t>(static_cast<int16_t>(c.d[0] & 0xFFFF)));
        Call(c, c.a[0]);
    }
    void ControlFour(M68K &c) { // 022DAC
        ByCharacter(c, 0x097C6C, 0x097CCC);
        if (!Ram8(c.a[6] + 0x67)) {
            Call(c, kKeepIn);
        }
    }
    void ControlEight(M68K &c) { // 022F1E
        ByCharacter(c, 0x097D2C, 0);
    }

    // 022DD4. Held in a throw (kControl 6): its effects off; the round's first hit announced; while the window is
    // open, holding back or forward with a medium or heavy button softens the throw (the computer: as it chooses, as
    // the window closes); placed and shown by the thrower's frame, and both kept inside the picture
    void HeldInThrow(M68K &c) {
        const uint32_t player = c.a[6];
        Call(c, 0x027204);
        c.d[0] = 0;
        for (const uint32_t field : {kCustomCombo, kTimer25B, kThrowProtection, kShocked, kOnFire, kFrozenStiff}) {
            SetRam8(player + field, 0);
        }
        if (!Ram8(Var(c, kFirstAttack))) {
            SetRam8(Var(c, kFirstAttack), 0xFF);
            c.a[4] = static_cast<uint32_t>(static_cast<int16_t>(Ram16(player + kOpponent)));
            SetRam8((c.a[4] & 0xFFFFFF) + 0x95, 8);
            if (Ram16(Var(c, kPlayMode))) {
                SetRam8(Var(c, Ram8(player + 0x08) == 4 ? 0x495 : 0xC95), 8);
            }
            c.d[0] = 0x28;
            SetLow8(c.d[1], Ram8((c.a[4] & 0xFFFFFF) + 0x81));
            Call(c, 0x005A52); // ("First attack")
            Call(c, 0x01906A);
        }
        // 022E36: the window
        if (Ram8(player + kTechWindow)) {
            const uint8_t left = static_cast<uint8_t>(Ram8(player + kTechWindow) - 1);
            SetRam8(player + kTechWindow, left);
            bool softened = false;
            if (!left && Ram8(player + kComputer)) {
                c.a[1] = 0x09BDC4;
                softened = Asks(c, 0x0280BA);
            } else {
                SetLow8(c.d[0], Ram8(player + kStick));
                SetLow16(c.d[0], static_cast<uint16_t>(c.d[0] & (kStickHeldBack | kStickHeldForward)));
                if (c.d[0] & 0xFFFF) {
                    SetLow8(c.d[0], Ram8(player + kPressedNow) & 0x66);
                    softened = (c.d[0] & 0xFF) != 0;
                }
            }
            if (softened) {
                SetRam8(player + kKnocksDown, 0);
                SetRam8(player + kSoftened, 1);
            }
        }
        // 022E76: where the thrower's frame puts the one held, and what it shows
        c.d[3] = 0;
        c.a[4] = static_cast<uint32_t>(static_cast<int16_t>(Ram16(player + kOpponent)));
        const uint32_t thrower = c.a[4] & 0xFFFFFF;
        SetLow8(c.d[1], Ram8(thrower + kCharacter));
        SetLow8(c.d[3], Ram8(player + kCharacter));
        SetLow16(c.d[1], static_cast<uint16_t>(static_cast<int16_t>(static_cast<int8_t>(c.d[1])) * 4));
        SetLow16(c.d[3], static_cast<uint16_t>((c.d[3] & 0xFFFF) * 2));
        c.a[1] = Mem32(c, kThrowPlaces + static_cast<uint32_t>(static_cast<int16_t>(c.d[1] & 0xFFFF)));
        SetLow16(c.d[2], Mem16(c, c.a[1] + static_cast<uint32_t>(static_cast<int16_t>(c.d[3] & 0xFFFF))));
        c.a[1] += static_cast<uint32_t>(static_cast<int16_t>(c.d[2] & 0xFFFF));
        c.a[2] = Ram32(thrower + kFrame);
        c.d[0] = 0;
        SetLow8(c.d[0], Mem8(c, c.a[2] + 0x10));
        SetLow16(c.d[0], static_cast<uint16_t>((c.d[0] & 0xFFFF) << 3));
        c.a[1] += static_cast<uint32_t>(static_cast<int16_t>(c.d[0] & 0xFFFF));
        if (!Ram8(player + 0x259)) {
            uint16_t across = Mem16(c, c.a[1]);
            if (Ram8(thrower + kFacesRight)) {
                across = static_cast<uint16_t>(-across);
            }
            SetLow16(c.d[2], across);
            SetRam16(player + kX, static_cast<uint16_t>(Ram16(thrower + kX) + across));
            SetLow16(c.d[2], Mem16(c, c.a[1] + 2));
            SetRam16(player + kY, static_cast<uint16_t>(Ram16(thrower + kY) + (c.d[2] & 0xFFFF)));
        }
        SetRam8(player + 0x152, 0);
        SetLow16(c.d[2], Mem16(c, c.a[1] + 4));
        if (c.d[2] & 0x8000) {
            SetRam8(player + 0x152, 0xFF);
        }
        SetLow16(c.d[2], static_cast<uint16_t>(c.d[2] & 3));
        SetLow8(c.d[0], Ram8(thrower + kFacesRight));
        SetLow8(c.d[2], static_cast<uint8_t>((c.d[2] ^ c.d[0]) & 0xFF));
        SetRam8(player + kFacesRight, static_cast<uint8_t>(c.d[2]));
        SetLow8(c.d[1], Ram8(player + kCharacter));
        SetLow16(c.d[1], static_cast<uint16_t>(static_cast<int16_t>(static_cast<int8_t>(c.d[1])) << 2));
        c.a[0] = Mem32(c, kHeldFrames + static_cast<uint32_t>(static_cast<int16_t>(c.d[1] & 0xFFFF)));
        SetLow16(c.d[2], Mem16(c, c.a[0]));
        c.a[0] += static_cast<uint32_t>(static_cast<int16_t>(c.d[2] & 0xFFFF));
        SetLow16(c.d[0], Mem16(c, c.a[1] + 6));
        c.a[0] += static_cast<uint32_t>(static_cast<int16_t>(c.d[0] & 0xFFFF));
        SetRam32(player + kFrame, c.a[0]);
        SetRam16(player + kFrameWord, 0);
        Call(c, kKeepBothIn);
    }
} // namespace

namespace {
    // --- the players' step (both, each frame of a fight) ---

    constexpr uint32_t kPanel = 0x9A;            // word: the panel as read for this player this frame (buttons, stick)
    constexpr uint32_t kControlsNow = 0x108, kControlsBefore = 0x10A, kControlsNew = 0x10C, kControlsLetGo = 0x10E;
    constexpr uint32_t kControlsFacing = 0x126, kControlsFacingBefore = 0x128; // the same by the facing it should have
    constexpr uint32_t kSuperFlash = 0x13D;      // byte: the player's super combo's flash is on (the others stand still)
    constexpr uint32_t kHitByNumber = 0x25C;     // byte: the number of the attack that last hit (not again till it ends)
    constexpr uint32_t kStates = 0x021300;       // a player's routines by kState (words, from here)
    constexpr int32_t kStillFrame = 0x15C;       // from A5: a still frame of the knock-out's slow motion (fight.cpp)

    // Left and right swapped (as the stick's bits 0 and 1) when facing right: the stick by the player's facing
    uint16_t ByFacing(M68K &c, uint16_t word) {
        SetLow16(c.d[0], static_cast<uint16_t>(word & 3));
        SetLow8(c.d[0], Code8(c, 0x0212A8 + (c.d[0] & 0xFFFF)));
        return static_cast<uint16_t>((word & 0xFFFC) | (c.d[0] & 0xFF));
    }

    // 02126E. A player's controls this step: by its facing; then what is newly pressed, and let go
    void Controls(M68K &c, uint32_t player) {
        if (Ram8(player + kFacesRight)) {
            SetRam16(player + kControlsNow, ByFacing(c, Ram16(player + kControlsNow)));
        }
        SetLow16(c.d[0], static_cast<uint16_t>(~Ram16(player + kControlsBefore) & Ram16(player + kControlsNow)));
        SetRam16(player + kControlsNew, static_cast<uint16_t>(c.d[0]));
        SetLow16(c.d[0], static_cast<uint16_t>(~Ram16(player + kControlsNow) & Ram16(player + kControlsBefore)));
        SetRam16(player + kControlsLetGo, static_cast<uint16_t>(c.d[0]));
        if (Ram8(player + 0x131)) {
            SetRam16(player + kControlsFacing, ByFacing(c, Ram16(player + kControlsFacing)));
        }
    }

    // 0212CA. One player's step: its timers (not to be thrown; the stun's points kept a while, then gone), its
    // controls (a person's), and the routine for its kState
    void PlayerStepOne(M68K &c, uint32_t player) {
        Spend(c, 500); // (its time: this runs inline, not as a rewrite of its own)
        c.a[6] = player;
        if (Ram8(player + kNoThrow)) {
            SetRam8(player + kNoThrow, static_cast<uint8_t>(Ram8(player + kNoThrow) - 1));
        }
        if (!Ram8(player + kComputer)) {
            Controls(c, player);
        }
        if (Ram8(player + kStunTimer)) {
            const uint8_t left = static_cast<uint8_t>(Ram8(player + kStunTimer) - 1);
            SetRam8(player + kStunTimer, left);
            if (!left) {
                SetRam8(player + kStun, 0);
            }
        }
        c.d[0] = 0;
        SetLow8(c.d[0], Ram8(player + kState));
        SetLow16(c.d[1], Code16(c, kStates + (c.d[0] & 0xFFFF)));
        Call(c, kStates + static_cast<uint32_t>(static_cast<int16_t>(c.d[1] & 0xFFFF)));
        Call(c, 0x024004); // (the character's own after its step)
    }

    // 01FF0A. The players' step in a fight (not in a still frame of the knock-out's slow motion). In the usual
    // order, chosen by chance each frame, one and then the other; while one's super combo flashes, only that one;
    // one in a throw (+67 bit 7) and the other go first. (One that has just begun a throw, +67, goes again, and then
    // the other: as the game has it.) Then, an attack over, the other may be hit by it again.
    void PlayersStep(M68K &c) {
        if (Ram8(Var(c, kStillFrame))) {
            return;
        }
        const uint32_t one = Var(c, 0x400), two = Var(c, 0x800);
        for (const uint32_t player : {one, two}) {
            SetRam16(player + kControlsBefore, Ram16(player + kControlsNow));
            SetRam16(player + kControlsFacingBefore, Ram16(player + kControlsFacing));
        }
        for (const uint32_t player : {one, two}) {
            SetRam16(player + kControlsNow, Ram16(player + kPanel));
        }
        for (const uint32_t player : {one, two}) {
            SetRam16(player + kControlsFacing, Ram16(player + kPanel));
        }
        Call(c, 0x0211C8); // (the helpers' timers, by the play mode)
        Call(c, 0x021112); // (by the play mode)
        if (Ram16(Var(c, kPlayMode))) {
            return Call(c, 0x01FFF2); // (the modes with helpers: their own order)
        }
        auto both = [&](uint32_t first, uint32_t second) {
            PlayerStepOne(c, first);
            PlayerStepOne(c, second);
        };
        if (Ram8(one + 0x67) & 0x80) {
            return both(two, one);
        }
        if (Ram8(two + 0x67) & 0x80) {
            return both(one, two);
        }
        SetLow8(c.d[0], Ram8(one + kSuperFlash) & Ram8(two + kSuperFlash));
        if (!(c.d[0] & 0xFF)) {
            if (Ram8(one + kSuperFlash)) {
                return PlayerStepOne(c, one);
            }
            if (Ram8(two + kSuperFlash)) {
                return PlayerStepOne(c, two);
            }
        }
        Call(c, 0x01975A); // (a number by chance)
        const bool oneFirst = !(c.d[0] & 1);
        const uint32_t first = oneFirst ? one : two, second = oneFirst ? two : one;
        PlayerStepOne(c, first);
        if (Ram8(first + 0x67)) {
            return both(first, second);
        }
        PlayerStepOne(c, second);
        if (Ram8(second + 0x67)) {
            return both(second, first);
        }
        Call(c, 0x02109A); // (each player against the other's push box)
        // 01FFD4. An attack over (its frame has no attack box): the other may be hit by it again
        c.a[0] = Ram32(one + kFrame);
        if (!Mem8(c, c.a[0] + kFrameAttack)) {
            SetRam8(two + kHitByNumber, 0);
        }
        c.a[0] = Ram32(two + kFrame);
        if (!Mem8(c, c.a[0] + kFrameAttack)) {
            SetRam8(one + kHitByNumber + 1, 0);
        }
    }
} // namespace

namespace {
    // 02149A. A player in a fight (kState 2): the computer's own, or a person's special moves' inputs read and its
    // quick rise; a hold throw's damage over time; then the routine for its kControl (kPlayerDispatch, through the
    // game's own dispatch so that what hooks it, the frame data, sees every step)
    void PlayerFighting(M68K &c) {
        const uint32_t player = c.a[6];
        if (Ram8(player + kComputer)) {
            if (Asks(c, 0x029CEA)) {
                Call(c, 0x029E38);
            }
        } else {
            Call(c, 0x023B00); // (the character's special moves' inputs)
            Call(c, 0x022AEA); // QuickRecovery
        }
        Call(c, 0x0214E0); // (held by a hold throw: its damage over time)
        Call(c, 0x021614); // (+297 down)
        Call(c, kPlayerDispatch);
    }
} // namespace

namespace {
    // 022AEA. The quick recovery: reeling in the air (and free to), a quarter circle back with a punch (the motion
    // at 025EA0; Sodom has another as well, 025E98) and the player recovers (kControl 10). Not for Zangief's and
    // Dhalsim's other forms.
    void QuickRecovery(M68K &c) {
        const uint32_t player = c.a[6];
        const uint8_t who = Ram8(player + kCharacter);
        if (Ram8(player + 0x258) || who == 0x15 || who == 0x16) {
            return;
        }
        c.a[4] = player + 0x260;
        if (Asks(c, 0x025162)) {
            if (!Asks(c, 0x026C96)) {
                return;
            }
            SetRam32(player + kState, 0x02100000);
            SetRam8(player + 0x253, 0);
            return Call(c, 0x0277CA);
        }
        if (who != kSodom) {
            return;
        }
        c.a[4] = player + 0x268;
        if (Asks(c, 0x02515A) && Asks(c, 0x026C96)) {
            SetRam32(player + kState, 0x02100000);
            SetRam8(player + 0x253, 1);
        }
    }
} // namespace

void RegisterPlayer() {
    AddRewrite(0x021620, "PlayerInControl", PlayerInControl, kFreeFlags, 400);
    AddRewrite(0x022116, "PlayerReeling", PlayerReeling, kFreeFlags, 100);
    AddRewrite(0x022D20, "Recovered", Recovered, kFreeFlags, 300);
    AddRewrite(0x022544, "ReelFrozen", ReelFrozen, kFreeFlags, 220);
    AddRewrite(0x0225E6, "ReelGoesOn", ReelGoesOn, kFreeFlags, 40);
    AddRewrite(0x0225FE, "ReelStanding", ReelStanding, kFreeFlags, 150);
    AddRewrite(0x0226E6, "ReelGuarding", ReelGuarding, kFreeFlags, 120);
    AddRewrite(0x022B52, "ReelHops", ReelHops, kFreeFlags, 120);
    AddRewrite(0x0227B8, "ReelDown", ReelDown, kFreeFlags, 150);
    AddRewrite(0x022130, "ReelOnFloor", ReelOnFloor, kFreeFlags, 40);
    AddRewrite(0x022140, "ReelBegins", ReelBegins, kFreeFlags, 700);
    AddRewrite(0x022DAC, "ControlFour", ControlFour, kFreeFlags, 60);
    AddRewrite(0x01FF0A, "PlayersStep", PlayersStep, kFreeFlags, 300);
    AddRewrite(0x02149A, "PlayerFighting", PlayerFighting, kFreeFlags, 80);
    AddRewrite(0x022AEA, "QuickRecovery", QuickRecovery, kFreeFlags, 100);
    AddRewrite(0x022DD4, "HeldInThrow", HeldInThrow, kFreeFlags, 400);
    AddRewrite(0x022F1E, "ControlEight", ControlEight, kFreeFlags, 40);
    AddRewrite(0x021732, "Standing", Standing, kFreeFlags, 200);
    AddRewrite(0x021880, "Crouching", Crouching, kFreeFlags, 200);
    AddRewrite(0x021990, "Walking", Walking, kFreeFlags, 200);
    AddRewrite(0x021A64, "Jumping", Jumping, kFreeFlags, 200);
    AddRewrite(0x021D22, "Turning", Turning, kFreeFlags, 150);
    AddRewrite(0x021E04, "Attacking", Attacking, kFreeFlags, 220);
    AddRewrite(0x021E54, "Guarding", Guarding, kFreeFlags, 150);
    AddRewrite(0x022000, "SpecialMove", SpecialMove, kFreeFlags, 120);
    AddRewrite(0x022026, "Stunned", Stunned, kFreeFlags, 120);
}

} // namespace cps2recomp
