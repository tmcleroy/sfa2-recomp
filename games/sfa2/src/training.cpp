// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy
//
// Street Fighter Alpha 2: the training mode. A session reached from the menu (the board reset, the coins and
// Start buttons worked unseen, stopping at the character choice), a fight with no clock and no knock-outs,
// starting over from where the round began, and the aids the toolkit's fight module keeps (fight.h): frame data
// and its meter, the input display, the combo readout, the dummy, hit boxes. This file says, from the game's
// memory, what those need to know.
#include "boxes.h"
#include "fight.h"
#include "modes.h"
#include "sfa2.h"
#include "state.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <string>

namespace cps2recomp {

bool AttackInRange(M68K &c, uint32_t player); // guard.cpp
int WideSide();                               // widescreen.cpp
int WideStage();
bool GuardStanding(M68K &c, uint32_t player); // boxes.cpp

namespace {
    using namespace sfa2;

    constexpr int kRefillAfter = 90; // frames without a hit after which health comes back

    fight::Frames g_frames;
    fight::InputHistory g_inputs;
    fight::Combos g_combos;
    fight::Dummy g_dummy;
    bool g_fighting = false;

    // Getting to a training session from switching on: coins, then both Start buttons, until both players are
    // choosing their characters (the game runs unseen meanwhile)
    bool g_boot = false;
    int g_bootFrames = 0;
    constexpr int kBootMost = 4000;
    // The machine as it was when the round's fighting began, for starting over
    std::vector<uint8_t> g_roundStart;
    bool g_roundFought = false;
    std::vector<HudBox> g_nextBoxes; // the hit boxes for the next picture

    // The stage: the players stand from one of these to the other, the picture's left edge goes from one of these
    // to the other; a round starts with the players at kStartX
    constexpr uint16_t kStageLeft = 280, kStageRight = 1000, kCameraLeft = 256, kCameraRight = 640;
    constexpr uint16_t kStartX[2] = {544, 736};
    constexpr uint16_t kCornerApart = 100;
    constexpr uint32_t kCameraCopies[] = {0xFF8026, 0xFF82A0, 0xFF82A8, 0xFF8290};
    constexpr uint32_t kCameraX = 0xFF8290, kCameraY = 0xFF8294;
    constexpr int kPictureFloor = 239; // the picture's line that height 0 is on, the camera at the bottom (a player on the floor is at kGround, 40: its feet on line 199, measured)
    constexpr uint32_t kPlayersIn = 0xFF80A8, kPlayersChosen = 0xFF80AC;

    bool Fighting() {
        if (!RoundFought()) {
            return false;
        }
        for (int n = 0; n < 2; n++) {
            if (static_cast<int16_t>(Ram16(Player(n) + kHealth)) > kHealthFull || Ram8(Player(n) + kState) != 2) {
                return false;
            }
        }
        return true;
    }
    bool DummyOnRight() {
        return Ram16(Player(1) + kX) >= Ram16(Player(0) + kX);
    }

    void Afresh(bool fighting) {
        g_fighting = fighting;
        g_frames.Reset();
        g_inputs.Clear();
        g_combos.Clear();
        g_dummy.Afresh();
    }

    // --- a training session ---

    void BootToChoice(Machine &machine) {
        machine.Reset();
        g_boot = true;
        g_bootFrames = 0;
        SetFastForward(true);
    }

    void Start(Machine &machine) {
        EndModes();
        EndBattle();
        SetFeature("training_mode", true);
        BootToChoice(machine);
    }

    void Boot(Machine &machine) {
        uint16_t &one = machine.players[0], &two = machine.players[1];
        const bool press = g_bootFrames % 16 < 6;
        one = two = 0;
        if (Ram8(kPlayersIn) == 3 || ++g_bootFrames > kBootMost) {
            g_boot = false;
            SetFastForward(false);
        } else if (Ram8(kCredits) < 2 && !FeatureOn("free_play")) {
            one = press ? kCoin : 0;
        } else if (!(Ram8(kPlayersIn) & 1)) {
            one = press ? kStart : 0;
        } else {
            two = press ? kStart : 0;
        }
    }

    // "Start over": the round as it began, with the players put where the player wants them. The stick held as it
    // is asked for chooses the place (left or right: a corner; down: sides swapped; up: the middle again).
    void StartOver(Machine &machine) {
        if (g_roundStart.empty() || !FeatureOn("training_mode")) {
            return;
        }
        int place = FeatureValue("restart_at"); // 0 middle, 1 left corner, 2 right corner; +3: sides swapped
        const uint16_t stick = machine.players[0] & 0x0F;
        if (stick) {
            int where = place % 3;
            bool swapped = place >= 3;
            if (stick & kUp) {
                where = 0;
                swapped = false;
            }
            if (stick & kLeft) {
                where = 1;
            } else if (stick & kRight) {
                where = 2;
            }
            if (stick & kDown) {
                swapped = !swapped;
            }
            place = where + (swapped ? 3 : 0);
            SetFeatureValue("restart_at", place);
        }
        Archive ar(g_roundStart, false);
        machine.Serialize(ar);
        uint16_t x[2] = {kStartX[0], kStartX[1]}, camera = Ram16(kCameraX);
        if (place % 3 == 1) {
            x[0] = kStageLeft;
            x[1] = kStageLeft + kCornerApart;
            camera = kCameraLeft;
        } else if (place % 3 == 2) {
            x[0] = kStageRight - kCornerApart;
            x[1] = kStageRight;
            camera = kCameraRight;
        }
        if (place >= 3) {
            std::swap(x[0], x[1]);
        }
        for (int n = 0; n < 2; n++) {
            SetRam16(Player(n) + kX, x[n]);
        }
        for (const uint32_t copy : kCameraCopies) {
            SetRam16(copy, camera);
        }
        Afresh(true);
    }

    // 00797C: the game has chosen the stage for a match (D0, from the character of one of the players: kStageOf). In
    // a training session with a stage asked for, that one goes in its place.
    bool ChooseStage(M68K &c) {
        const int stage = Wanted().stage ? Wanted().stage : FeatureOn("training_mode") ? FeatureValue("training_stage") : 0;
        if (stage >= 1 && stage <= kStages) {
            SetLow8(c.d[0], static_cast<uint8_t>(stage - 1));
        }
        return false;
    }

    // The hidden characters: the other forms the board has (kCharacterNames 18-23) of the characters on the screen
    // they are chosen from, as the "Hidden characters" feature gives them for one who chooses the first form. (Gen's
    // other form is left out: it is the style he changes to in a fight.)
    constexpr struct {
        uint8_t first, other;
    } kHiddenForms[] = {{0, 23}, {2, 20}, {4, 18}, {15, 22}, {16, 21}};
    bool HiddenFormFor(int n) {
        const int who = FeatureValue("hidden_forms"); // 1 player 1, 2 player 2, 3 both
        return (who & (1 << n)) != 0;
    }

    // A character with a hidden form, chosen by player n: that form instead, if asked for
    void HiddenForm(int n) {
        if (!HiddenFormFor(n)) {
            return;
        }
        for (const auto &form : kHiddenForms) {
            if (Ram8(Player(n) + kCharacter) == form.first) {
                SetRam8(Player(n) + kCharacter, form.other);
                return;
            }
        }
    }

    // 0628D8: a player has chosen on the character screen (A4 its record), after the game's own codes for the
    // hidden forms (holding Start on Chun-Li, 0628B2): the hidden form goes in here, where the game puts its own,
    // so that it sets everything up for that form (its plate, face and colours: 062902)
    bool CharacterChosen(M68K &c) {
        const uint32_t record = c.a[4] & 0xFFFFFF; // (the address bus's 24 bits: A4 may hold FFFF8400)
        const int n = record == (Player(0) & 0xFFFFFF) ? 0 : record == (Player(1) & 0xFFFFFF) ? 1 : -1;
        if (n >= 0) {
            HiddenForm(n);
        }
        return false;
    }

    // 007914: the game is about to choose the stage, from the characters chosen. The characters a mode wants go in
    // their place first (two alike are told apart by the game's own colours); then, if asked for, a character with
    // a hidden form is that form (in the modes too: the endless battle's fighters).
    bool ChooseCharacters(M68K &) {
        for (int n = 0; n < 2; n++) {
            if (Wanted().characters[n] >= 0) {
                SetRam8(Player(n) + kCharacter, static_cast<uint8_t>(Wanted().characters[n]));
            }
            HiddenForm(n); // (the modes' fighters, which are not chosen on the character screen)
        }
        return false;
    }

    bool Choosing() {
        return Ram16(kMode) == 2 && Ram16(kGameStep) == kGameChoosing && Ram8(kPlayersIn) == 3;
    }

    bool g_choosingTwo = false; // player 1's controls are choosing player 2's character

    void BeforeFrame(Machine &machine) {
        if (g_boot) {
            Boot(machine);
            return;
        }
        if (FeatureOn("training_mode") && Choosing() && (Ram8(kPlayersChosen) & 1) && !(Ram8(kPlayersChosen) & 2)) {
            // Player 1 has chosen: its controls choose for player 2. What was held as it chose is not passed on
            // until let go, or the button that chose for player 1 would choose for player 2 at once.
            static uint16_t heldOver = 0;
            const uint16_t held = machine.players[0] & (fight::kSticks | fight::kButtons);
            heldOver = g_choosingTwo ? heldOver & held : held;
            g_choosingTwo = true;
            machine.players[1] |= held & ~heldOver;
            machine.players[0] &= kStart | kCoin;
            return;
        }
        g_choosingTwo = false;
        fight::DummyView view;
        view.fighting = Fighting() && g_fighting;
        view.doing = g_frames.Now(1);
        view.onRight = DummyOnRight();
        if (view.fighting && FeatureValue("dummy_guard")) {
            view.attackInRange = AttackInRange(machine.cpu, Player(1));
            view.guardStanding = view.attackInRange && GuardStanding(machine.cpu, Player(1));
        }
        view.sinceHit = g_combos.Since(1);
        view.knowsDown = true;
        // (the game plays the dummy: its own computer player, as in a one-player game)
        static bool computerWas = false;
        const bool computer = FeatureOn("dummy_computer") && view.fighting;
        if (computer || computerWas) {
            SetRam8(Player(1) + kComputer, computer ? 1 : 0);
        }
        computerWas = computer;
        view.down = Ram8(Player(1) + kControl) == kControlReeling && Ram8(Player(1) + kKnocksDown) != 0;
        g_dummy.BeforeFrame(machine.players[0], machine.players[1], view);
        // Stunned, the dummy mashes: a new direction and a new button every frame (the stun's count goes down a step
        // more for each, player.cpp's Stunned)
        if (FeatureOn("dummy_stun_mash") && view.fighting && !computer && Ram8(Player(1) + kControl) == kControlOwn &&
            Ram8(Player(1) + kAction) == kActionStunned) {
            static bool other = false;
            other = !other;
            machine.players[1] = other ? static_cast<uint16_t>(kLeft | kPunch1) : static_cast<uint16_t>(kRight | kKick1);
        }
    }

    void LogStep(int n, uint32_t player); // below

    // --- a step of the game: what each player is doing (for frame data) ---

    bool PlayerStep(M68K &c) {
        const uint32_t player = c.a[6] & 0xFFFFFF;
        const int n = player == Player(0) ? 0 : player == Player(1) ? 1 : -1;
        if (n < 0 || Ram8(player + kState) != 2 || !RoundFought()) {
            return false;
        }
        fight::StepView view;
        const uint8_t control = Ram8(player + kControl);
        view.control = control == kControlOwn ? fight::StepView::Own : control == kControlReeling ? fight::StepView::Reeling : fight::StepView::Held;
        if (view.control == fight::StepView::Reeling) {
            g_frames.Contact(n, Ram8(player + kGuardedHit) == 0xFF);
        } else if (view.control == fight::StepView::Held) {
            g_frames.Contact(n, false);
        }
        view.canAct = CanAct(n);
        const uint32_t frame = Ram32(player + kFrame);
        view.frozen = Ram8(player + kHitStop) != 0;
        view.attacking = Ram8(player + kAction) == kActionAttack || Ram8(player + kAttackOut) != 0;
        view.jumping = Ram8(player + kAction) == kActionJump;
        view.attackOut = frame < 0x400000 && Mem8(c, frame + kFrameAttack) != 0 && !(Mem8(c, frame + kFrameHarmless) & 0x80);
        g_frames.Step(n, view);
        LogStep(n, player);
        return false;
    }

    // --- after a frame ---

    // What the game read off the panel for a player this frame, as Button bits
    uint16_t PanelHeld(int player) {
        const uint16_t word = Ram16(0xFF8000 + static_cast<uint32_t>(kControls[player]));
        uint16_t held = word & 0x0F;
        held |= static_cast<uint16_t>(((word >> 8) & 0x07) << 4);  // punches
        held |= static_cast<uint16_t>(((word >> 12) & 0x07) << 7); // kicks
        return held;
    }

    // --- the frame data log: every move's numbers in frame_data.csv beside settings.ini (fight::MoveLog) ---

    fight::MoveLog g_log;

    fight::MoveLog::Start MoveStart(int n) {
        const uint32_t player = Player(n);
        const uint8_t who = Ram8(player + kCharacter);
        fight::MoveLog::Start start;
        start.character = who < kCharacters ? kCharacterNames[who] : "?";
        start.input = fight::Notation(PanelHeld(n), Ram8(player + kFacesRight) == 0);
        start.air = Ram8(player + kAction) == kActionJump || Ram16(player + kY) != kGround;
        start.id = Ram32(player + kFrame); // (the move's first animation frame)
        return start;
    }

    void LogStep(int n, uint32_t) {
        if (FeatureOn("frame_log")) {
            g_log.Step(n, g_frames, MoveStart);
        }
    }

    // A Custom Combo going on: its time left, as a bar of text under the player's own bar
    std::string CustomComboLine(int n) {
        const int left = Ram8(Player(n) + kCustomCombo);
        if (!left) {
            return {};
        }
        constexpr int kFull = 0xB0, kCells = 12;
        std::string bar = "CUSTOM COMBO ";
        const int cells = std::clamp((left * kCells + kFull - 1) / kFull, 0, kCells);
        bar.append(static_cast<size_t>(cells), '|');
        return bar;
    }

    // The stun gauge as a line under the combo readout: how far to being stunned
    std::string StunLine(int n) {
        const uint8_t stun = Ram8(Player(n) + kStun), most = Ram8(Player(n) + kStunMost);
        if (!stun || !most) {
            return {};
        }
        char text[48];
        snprintf(text, sizeof(text), "stun %u / %u", stun, most);
        return text;
    }

    // The throw's reach, as the game tests it (0260E8): a point 20 in front of the thrower must be within 20 of the
    // near edge of the other player's push box. Drawn as that zone in front of each player on the floor, and the
    // edge it is tested against as a line.
    constexpr int kThrowAhead = 20, kThrowReach = 20, kThrowZoneHeight = 0x48;
    void ThrowRanges(Machine &machine, std::vector<fight::StageBox> &boxes) {
        for (int n = 0; n < 2; n++) {
            const uint32_t player = Player(n), other = Player(1 - n);
            if (Ram16(player + kY) != kGround || !CanAct(n)) {
                continue;
            }
            const bool right = Ram8(player + kFacesRight) != 0;
            const int point = static_cast<int16_t>(Ram16(player + kX)) + (right ? kThrowAhead : -kThrowAhead);
            boxes.push_back({fight::BoxKind::ThrowRange, point, kThrowZoneHeight / 2, kThrowReach, kThrowZoneHeight / 2, false});
            // (the other's push box's edge: its offset less its half width, turned about as the box is)
            const uint32_t frame = Ram32(other + kFrame), table = Ram32(other + kPushBoxes);
            if (frame >= 0x400000 || table >= 0x400000) {
                continue;
            }
            const int8_t number = static_cast<int8_t>(machine.Read8(frame + kFramePush));
            if (number <= 0) {
                continue;
            }
            const uint32_t box = table + static_cast<uint32_t>(number) * 8;
            int edge = static_cast<int16_t>(machine.Read16(box)) - static_cast<int16_t>(machine.Read16(box + kBoxHalfWidth));
            if (Ram8(other + kFacesRight)) {
                edge = -edge;
            }
            edge += static_cast<int16_t>(Ram16(other + kX));
            boxes.push_back({fight::BoxKind::Throw, edge, kThrowZoneHeight / 2, 1, kThrowZoneHeight / 2, false});
        }
    }

    void Frame(Machine &machine) {
        if (StageViewer() || BattleMode()) {
            g_fighting = false;
            g_roundFought = false;
            return;
        }
        const bool fought = RoundFought();
        if (fought && !g_roundFought && FeatureOn("training_mode")) {
            g_roundStart.clear();
            Archive ar(g_roundStart, true);
            machine.Serialize(ar);
        }
        g_roundFought = fought;
        const bool fighting = Fighting();
        if (!fighting || !g_fighting) {
            Afresh(fighting);
            SetHud({});
            SetBoxes({});
            g_nextBoxes.clear();
            return;
        }
        g_combos.Frame();
        std::vector<HudLine> lines;
        const bool readout = FeatureOn("damage_readout"), refill = FeatureOn("health_refill"), inputs = FeatureOn("input_display");
        const bool stun = FeatureOn("stun_readout");
        for (int n = 0; n < 2; n++) {
            if (refill && g_combos.Since(n) > kRefillAfter && Ram16(Player(n) + kHealth) < kHealthFull && CanAct(n)) {
                SetRam16(Player(n) + kHealth, kHealthFull);
                SetRam16(Player(n) + kHealthShown, kHealthFull);
            }
            if (FeatureOn("custom_combo_readout")) {
                const std::string cc = CustomComboLine(n);
                if (!cc.empty()) {
                    lines.push_back({n == 0 ? 0.03f : 0.97f, 0.11f, n == 0 ? -1 : 1, cc});
                }
            }
            if (readout || stun) {
                const std::string extra = stun ? StunLine(n) : std::string();
                if (readout) {
                    g_combos.Hud(n, lines, 0.17f, true, extra);
                } else if (!extra.empty()) {
                    lines.push_back({n == 0 ? 0.03f : 0.97f, 0.17f, n == 0 ? -1 : 1, extra});
                }
            }
            g_inputs.Push(n, PanelHeld(n), Ram8(Player(n) + kFacesRight) == 0);
            if (inputs) {
                g_inputs.Hud(n, lines, 0.36f, 0.052f);
            }
        }
        std::vector<HudBox> boxes;
        g_frames.Hud(FeatureOn("frame_data"), FeatureOn("frame_meter"), lines, boxes, 0.735f, 190, Video::kWidth, WideSide());
        if (FeatureOn("hit_boxes")) {
            // What the game's memory says now is what the next picture shows: the boxes are drawn a frame late (in a
            // wide picture, as far right as the stage is drawn: widescreen.cpp)
            for (HudBox &box : g_nextBoxes) {
                box.x += WideStage();
            }
            boxes.insert(boxes.begin(), g_nextBoxes.begin(), g_nextBoxes.end());
            g_nextBoxes.clear();
            const int cameraX = static_cast<int16_t>(Ram16(kCameraX)), cameraY = static_cast<int16_t>(Ram16(kCameraY));
            std::vector<fight::StageBox> stage = AllBoxes(machine);
            if (FeatureOn("throw_range")) {
                ThrowRanges(machine, stage);
            }
            for (const fight::StageBox &box : stage) {
                g_nextBoxes.push_back(fight::ToPicture(box, cameraX, cameraY, kPictureFloor));
            }
        } else {
            g_nextBoxes.clear();
        }
        SetBoxes(std::move(boxes));
        g_dummy.Hud(lines, 0.17f);
        SetHud(std::move(lines));
    }
} // namespace

// A hit's damage is being taken (fight.cpp)
void TrainingHit(int player, unsigned damage, bool chip) {
    const fight::Doing doing = g_frames.Now(player);
    g_combos.Hit(player, damage, 0, !chip && (doing == fight::Doing::HitStun || doing == fight::Doing::Held));
}
void TrainingDamageTaken(int) {}

// A training session is on, or the stage viewer: no clock and no knock-outs (fight.cpp, damage.cpp)
bool TrainingMode() {
    return FeatureOn("training_mode") || StageViewer() || BattleNoKnockOut();
}

// "Training mode" on the menu's Alternative modes page (modes.cpp)
void StartTraining(Machine &machine) {
    Start(machine);
}

void TrainingBeforeFrame(Machine &machine) {
    BeforeFrame(machine);
}

void TrainingFrame(Machine &machine) {
    Frame(machine);
}

void TrainingState(Archive &ar) {
    ar.Mark(0x5452414E);
    g_frames.Serialize(ar);
    g_dummy.Serialize(ar);
    ar.Vec(g_roundStart);
    ar.Pod(g_roundFought);
    ar.Pod(g_fighting);
    ar.Pod(g_combos);
    ar.Pod(g_inputs);
}

void RegisterTraining() {
    Game &game = TheGame();
    g_frames.print = getenv("SFA2_FRAMES") != nullptr;
    AddHook(kPlayerDispatch, "PlayerStep", PlayerStep);
    AddHook(0x00797C, "ChooseStage", ChooseStage);
    AddHook(0x007914, "ChooseCharacters", ChooseCharacters);
    AddHook(0x0628D8, "CharacterChosen", CharacterChosen);
    game.commands.push_back({"training_restart", "Training mode", "Start over",
                             "In a training session: the round as it began, with the players where \"Start over at\" says. Backspace "
                             "or a gamepad's left shoulder does this while playing; the stick held as it is pressed chooses the place "
                             "(left, right: a corner; down: sides swapped; up: the middle).",
                             StartOver, kKeyRestart});
    game.features.push_back({"hidden_forms", "Game", "Hidden characters",
                             "Choosing Ryu, Akuma, Chun-Li, Dhalsim or Zangief gives their hidden form instead: Evil Ryu, Shin "
                             "Akuma, the classic Chun-Li, and Dhalsim's and Zangief's other forms. For player 1, player 2 or both.",
                             0, {"Off", "Player 1", "Player 2", "Both"}});
    game.features.push_back({"restart_at", "Training mode", "Start over at",
                             "Where \"Start over\" puts the players: the middle or a corner, player 1 on the left or (swapped) on the right.",
                             0, {"Middle", "Left corner", "Right corner", "Middle, swapped", "Left corner, swapped", "Right corner, swapped"}});
    std::vector<const char *> stages = {"The game's choice"};
    for (const char *name : kStageNames) {
        stages.push_back(name);
    }
    game.features.push_back({"training_stage", "Training mode", "Stage",
                             "Where a training session's fight is: the game's own choice (the stage of one of the two characters) "
                             "or a stage named here. Takes effect when the characters are next chosen.",
                             0, stages});
    game.features.push_back({"training_mode", "Training mode", "Training session",
                             "The fight has no clock and nobody is knocked out. (\"Start\" above switches this on and takes the game to "
                             "the character choice; Reset ends it.)",
                             kFeatureSession});
    game.features.push_back({"damage_readout", "Training mode", "Combo readout",
                             "Under the bar of the player hit: what the hit took off; for a combo (hits landing before the player is "
                             "free again) the hits and the damage; the round's best."});
    game.features.push_back({"stun_readout", "Training mode", "Stun readout",
                             "Under each bar: how much stun the player has taken and how much stuns them (it drains when they are "
                             "left alone for a while)."});
    game.features.push_back({"custom_combo_readout", "Training mode", "Custom Combo time",
                             "While a player's Custom Combo goes on, how much of its time is left, under that player's bar."});
    game.features.push_back({"input_display", "Training mode", "Input display",
                             "What each player presses and for how many frames: the stick as a keypad number from the player's side "
                             "(6 forward, 4 back, 2 down), then the buttons."});
    game.features.push_back({"hit_boxes", "Training mode", "Hit boxes",
                             "Draws the boxes the game tests: where a player can be hit (blue), attacks (red; orange for things "
                             "thrown), and the boxes that keep the players apart (green)."});
    game.features.push_back({"throw_range", "Training mode", "Throw range",
                             "With hit boxes on: the zone in front of each player that the other's push box edge (the yellow line) "
                             "has to be in for a throw, as the game tests it (both on the floor and free to act)."});
    game.features.push_back({"frame_data", "Training mode", "Frame data",
                             "After a move: the step it can first hit on (startup), how many it can hit for (active) and how many "
                             "follow before the player can act (recovery). After a hit or a guarded one: who can act first, by how "
                             "many steps (+: the attacker)."});
    game.features.push_back({"frame_log", "Training mode", "Frame data log",
                             "Writes every move's frame data to frame_data.csv, beside the settings file: the character, what was "
                             "held as it began, startup, active, recovery and the advantage after a hit or a guarded one."});
    game.features.push_back({"frame_meter", "Training mode", "Frame meter",
                             "A row of cells for each player, one for every step of the game: green before a move can hit, red while "
                             "it can, blue after; yellow reeling from a hit, light blue from a guarded one; grey in the air."});
    game.features.push_back({"health_refill", "Training mode", "Health refills",
                             "A player's health comes back when they have not been hit for a moment."});

    fight::DummyOptions dummy;
    // The Alpha counter: while guarding, back, down-back, down with a punch (or a kick)
    dummy.guardActions.push_back({"Alpha counter, punch", [](int frame, uint16_t, uint16_t away) -> uint16_t {
                                      static constexpr int kEach = 2;
                                      const int at = (frame / kEach) % 4;
                                      return at == 0 ? away : at == 1 ? static_cast<uint16_t>(away | kDown) : at == 2 ? static_cast<uint16_t>(kDown | kPunch2) : 0;
                                  }});
    dummy.guardActions.push_back({"Alpha counter, kick", [](int frame, uint16_t, uint16_t away) -> uint16_t {
                                      static constexpr int kEach = 2;
                                      const int at = (frame / kEach) % 4;
                                      return at == 0 ? away : at == 1 ? static_cast<uint16_t>(away | kDown) : at == 2 ? static_cast<uint16_t>(kDown | kKick2) : 0;
                                  }});
    dummy.guardActionsWords = "What player 2 does while guarding a hit: an Alpha counter (back, down-back, down with a punch or a kick; "
                              "it costs a level of the super combo gauge, so pair it with \"Full gauge, player 2\").";
    dummy.heldAction = [](int frame, uint16_t, uint16_t away) -> uint16_t {
        return static_cast<uint16_t>(away | ((frame & 1) ? (kPunch2 | kPunch3) : 0));
    };
    dummy.heldActionTitle = "Softens throws";
    dummy.heldActionWords = "Player 2 does a throw's own command while being thrown (a \"tech hit\", which takes less damage).";
    // Getting up quickly from a knock-down ("quick rise": down, down-back, back with a punch, as it lands; found by
    // trying: a sweep's knock-down took 74 frames, 43 with this)
    dummy.getUp.push_back({"Quick rise", [](int frame, uint16_t, uint16_t away) -> uint16_t {
                               const int at = (frame / 2) % 4;
                               return at == 0 ? kDown : at == 1 ? static_cast<uint16_t>(kDown | away) : at == 2 ? away : static_cast<uint16_t>(away | kPunch1);
                           }});
    fight::RegisterDummy(dummy);
    game.features.push_back({"dummy_stun_mash", "Dummy", "Mashes out of stun",
                             "Stunned, the dummy works the stick and the buttons as fast as it can, as a player does to get out sooner."});
    game.features.push_back({"dummy_computer", "Dummy", "Played by the game",
                             "Player 2 is the game's own computer player, as in a one-player game (at the board's difficulty "
                             "setting).",
                             kFeatureExclusive});
}

} // namespace cps2recomp
