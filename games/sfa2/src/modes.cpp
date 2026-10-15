// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy
//
// Street Fighter Alpha 2: the "Alternative modes" page at the head of the in-game menu (as Vampire Savior has it):
// training (training.cpp), attract mode on a loop, and a viewer for the stages. Each is started by a command and
// ends when the board is reset, when another is started, or (attract mode) with Start.
//
// Attract mode is the game's own loop of notices, introduction, rankings, title and two demonstration fights,
// with no coin in. "The intro only" and "the fights only" leave parts of the loop out by moving the game's own
// step on (kMode) where a wanted part ends.
//
// The stage viewer is a round of a two-player game on the stage chosen, in which nothing is drawn but the stage:
// the players stand a fixed distance apart and are moved together, so that the game's own camera follows them;
// their objects and the panel's are left out of the picture (Video::hideObject), and so is the first scroll
// screen, which holds the bars. The stage's pieces move and its music plays as in a fight. "Infinite" is every
// stage in turn, each scrolled from end to end and then out of the picture as the next comes in: two machines,
// the port's and a spare run beside it every frame, the picture made of the two and the sound mixed.
#include "modes.h"
#include "state.h"

#include <algorithm>
#include <cstring>
#include <iterator>

namespace cps2recomp {

void StartTraining(Machine &machine); // training.cpp

namespace {
    using namespace sfa2;

    constexpr const char *kModes = "Alternative modes", *kAttract = "Attract mode", *kViewer = "Stage viewer";

    enum class Mode : int32_t { None, Attract, Viewer };
    struct State {
        Mode mode = Mode::None;
        bool starting = false;
        int32_t frames = 0;
        int32_t stage = 0;     // the viewer: the stage shown (1-18)
        int32_t camera = 0;    //   where the picture's left edge is wanted, in 256ths of a pixel
        int32_t direction = 1; //   which way it scrolls by itself
        int32_t music = 0;     //   the "Music" choice that is playing
        bool infinite = false;
        int32_t next = 0;
        bool nextReady = false;
        int32_t nextFrames = 0;
        int32_t slide = 0;
    } g;
    std::vector<uint8_t> g_other, g_mine;
    std::vector<int16_t> g_otherSound, g_carry;
    size_t g_soundBefore = 0;
    bool g_fast = false;
    int g_chosenStage = -1;

    constexpr int kStartMost = 9000;

    void Fast(bool on) {
        if (on != g_fast) {
            g_fast = on;
            SetFastForward(on);
        }
    }

    void End() {
        if (g.mode == Mode::None) {
            return;
        }
        g = State();
        g_chosenStage = -1;
        Wanted() = MatchWanted();
        g_other.clear();
        g_otherSound.clear();
        g_carry.clear();
        Fast(false);
    }

    // --- attract mode ---

    // The parts of the game's loop, by kMode (each step is two; the steps between are fades). The loop starts
    // again from 0 after the second fight.
    constexpr uint16_t kNotices = 0x04;     // the notices: the FBI's, QSound's, the maker's
    constexpr uint16_t kIntro = 0x10;       // the introduction
    constexpr uint16_t kScores = 0x14;      // the score ranking
    constexpr uint16_t kFirstFight = 0x16;  // a demonstration fight (from the step that sets it up)
    constexpr uint16_t kTitle = 0x1C;       // the advisory, the maker's notice, the title, the versus ranking
    constexpr uint16_t kSecondFight = 0x2C; // another fight
    constexpr uint16_t kLoopEnd = 0x32;
    constexpr uint32_t kAttractFight = 0xFF0C68; // counts the demonstration fights, two for each: picks who fights
    enum { kShowAll, kShowIntro, kShowFights };

    void StartAttract(Machine &machine) {
        End();
        EndBattle();
        SetFeature("training_mode", false);
        g.mode = Mode::Attract;
        g.starting = true;
        machine.Reset();
    }

    void GoTo(uint16_t part) {
        SetRam16(kMode, part);
        for (uint32_t at = kMode + 2; at < kMode + 0x10; at += 2) {
            SetRam16(at, 0);
        }
    }

    void AttractFrame() {
        const uint16_t part = Ram16(kMode);
        const int show = FeatureValue("attract_show");
        if (show == kShowAll) {
            Fast(false);
        } else if (show == kShowIntro) {
            if (part < kIntro || part > kIntro + 2) {
                GoTo(kIntro);
            }
            Fast(false);
        } else {
            // (the game counts its fights on, two a fight, in a part after each that is left out here: the count
            // picks who fights next)
            if (part < kFirstFight) {
                GoTo(kFirstFight);
            } else if (part > kFirstFight + 4 && part < kSecondFight) {
                SetRam8(kAttractFight, static_cast<uint8_t>(Ram8(kAttractFight) + 2));
                GoTo(kSecondFight);
            } else if (part >= kLoopEnd) {
                SetRam8(kAttractFight, static_cast<uint8_t>(Ram8(kAttractFight) + 2));
                GoTo(kFirstFight);
            }
            Fast(false);
        }
    }

    // --- the stage viewer ---

    constexpr uint16_t kCameraLeft = 256, kCameraRight = 640;
    constexpr uint16_t kApart = 0xC0;
    constexpr int32_t kStick = 384;
    constexpr int32_t kScroll[] = {0, 64, 128, 256};
    constexpr int32_t kLeftEnd = kCameraLeft * 256, kRightEnd = kCameraRight * 256;
    constexpr int kInfinite = kStages;
    constexpr int32_t kAcross = Video::kWidth * 256;
    constexpr int kOnTheWay = 3;
    constexpr size_t kSoundAhead = 16;

    void Place(int32_t camera) {
        const uint16_t middle = static_cast<uint16_t>(camera / 256 + kApart);
        SetRam16(Player(0) + kX, static_cast<uint16_t>(middle - kApart / 2));
        SetRam16(Player(1) + kX, static_cast<uint16_t>(middle + kApart / 2));
    }

    void StartViewer(Machine &machine) {
        End();
        EndBattle();
        SetFeature("training_mode", false);
        g.mode = Mode::Viewer;
        g.starting = true;
        g_chosenStage = FeatureValue("viewer_stage");
        g.infinite = g_chosenStage == kInfinite;
        g.stage = g.infinite ? 1 : std::clamp(g_chosenStage, 0, kStages - 1) + 1;
        g.next = g.stage % kStages + 1;
        g.camera = g.infinite ? kLeftEnd : (kLeftEnd + kRightEnd) / 2;
        Wanted().stage = g.stage;
        machine.Reset();
    }

    void ViewerBeforeFrame(Machine &machine) {
        g_soundBefore = machine.audio.size();
        Wanted().stage = g.stage;
        if (g.starting) {
            Fast(true);
            if (ToRound(machine, g.frames)) {
                g.starting = false;
                Fast(false);
            } else if (g.frames > kStartMost) {
                End();
            }
            if (g.starting || g.mode != Mode::Viewer) {
                return;
            }
        }
        const uint16_t stick = (machine.players[0] | machine.players[1]) & (kLeft | kRight);
        machine.players[0] = machine.players[1] = 0;
        const int scroll = std::clamp(FeatureValue("viewer_scroll"), 0, static_cast<int>(std::size(kScroll)) - 1);
        const int32_t byStick = stick == kRight ? kStick : stick == kLeft ? -kStick : 0;
        if (g.infinite) {
            int32_t by = scroll == 0 ? byStick : kScroll[scroll];
            if (g.slide > 0 || (g.camera + by >= kRightEnd && by > 0)) {
                by -= kRightEnd - g.camera;
                g.camera = kRightEnd;
                g.slide = g.nextReady ? std::clamp(g.slide + by, 0, kAcross) : 0;
            } else {
                g.camera = std::max(g.camera + by, kLeftEnd);
            }
        } else {
            g.camera += scroll == 0 ? byStick : g.direction * kScroll[scroll];
            if (g.camera >= kRightEnd) {
                g.camera = kRightEnd;
                g.direction = -1;
            } else if (g.camera <= kLeftEnd) {
                g.camera = kLeftEnd;
                g.direction = 1;
            }
        }
        Place(g.camera);
    }

    void RunNext(Machine &machine) {
        const int width = machine.PictureWidth();
        const size_t pixels = static_cast<size_t>(width) * Video::kHeight;
        static std::vector<uint32_t> shown;
        shown.assign(machine.Picture(), machine.Picture() + pixels);
        g_soundBefore = std::min(g_soundBefore, machine.audio.size());
        std::vector<int16_t> sound(machine.audio.begin() + static_cast<std::ptrdiff_t>(g_soundBefore), machine.audio.end());
        machine.audio.resize(g_soundBefore);
        const uint16_t held[2] = {machine.players[0], machine.players[1]};

        g_mine.clear();
        {
            Archive ar(g_mine, true);
            machine.Serialize(ar);
        }
        if (g_other.empty()) {
            machine.Reset();
            g.nextReady = false;
            g.nextFrames = 0;
            g_otherSound.assign(kSoundAhead * 2, 0);
        } else {
            Archive ar(g_other, false);
            machine.Serialize(ar);
        }
        Wanted().stage = g.next;
        for (int n = 0; n < kOnTheWay; n++) {
            machine.audio.resize(g_soundBefore);
            g.nextReady = g.nextReady || ToRound(machine, g.nextFrames);
            if (g.nextReady) {
                machine.players[0] = machine.players[1] = 0;
                Place(kLeftEnd);
            }
            machine.RunFrame();
            if (g.nextReady || g.nextFrames > kStartMost) {
                break;
            }
        }
        if (g.nextReady) {
            g_otherSound.insert(g_otherSound.end(), machine.audio.begin() + static_cast<std::ptrdiff_t>(g_soundBefore), machine.audio.end());
        }
        machine.audio.resize(g_soundBefore);

        const int slide = std::clamp(g.slide / 256, 0, width);
        if (slide > 0 && machine.PictureWidth() == width) {
            const uint32_t *next = machine.Picture();
            for (int y = 0; y < Video::kHeight; y++) {
                uint32_t *row = &shown[static_cast<size_t>(y) * width];
                memmove(row, row + slide, static_cast<size_t>(width - slide) * sizeof(uint32_t));
                memcpy(row + width - slide, next + static_cast<size_t>(y) * width, static_cast<size_t>(slide) * sizeof(uint32_t));
            }
        }
        const size_t count = sound.size();
        if (g_otherSound.size() < count) {
            g_otherSound.resize(count, 0);
        }
        const int32_t nextGain = slide * 256 / width, shownGain = 256 - nextGain;
        for (size_t i = 0; i < count; i++) {
            const int32_t mixed = (sound[i] * shownGain + (g.nextReady ? g_otherSound[i] : 0) * nextGain) / 256;
            sound[i] = static_cast<int16_t>(std::clamp(mixed, -32768, 32767));
        }
        g_otherSound.erase(g_otherSound.begin(), g_otherSound.begin() + static_cast<std::ptrdiff_t>(count));

        if (g.slide >= kAcross) {
            g.stage = g.next;
            g.next = g.stage % kStages + 1;
            g.camera = kLeftEnd;
            g.slide = 0;
            g.nextReady = false;
            g.nextFrames = 0;
            g_other.clear();
            g_carry = g_otherSound;
            g_otherSound.clear();
        } else {
            g_other.clear();
            {
                Archive ar(g_other, true);
                machine.Serialize(ar);
            }
            Archive ar(g_mine, false);
            machine.Serialize(ar);
        }
        Wanted().stage = g.stage;
        machine.players[0] = held[0];
        machine.players[1] = held[1];
        memcpy(machine.Picture(), shown.data(), pixels * sizeof(uint32_t));
        machine.audio.insert(machine.audio.end(), sound.begin(), sound.end());
    }

    void ViewerFrame(Machine &machine) {
        if (g.starting) {
            return;
        }
        const int chosen = FeatureValue("viewer_stage");
        if (g_chosenStage >= 0 && chosen != g_chosenStage) {
            StartViewer(machine);
            return;
        }
        g_chosenStage = chosen;
        if (g.infinite) {
            if (!g_carry.empty()) {
                machine.audio.insert(machine.audio.begin() + static_cast<std::ptrdiff_t>(std::min(g_soundBefore, machine.audio.size())),
                                     g_carry.begin(), g_carry.end());
                g_carry.clear();
            }
            RunNext(machine);
        } else {
            const int music = FeatureValue("viewer_music"); // 0: the stage's own; then the stages'; then the others
            if (music != g.music) {
                g.music = music;
                const int stage = music == 0 ? g.stage : music;
                PlaySound(kTuneOff);
                PlaySound(stage <= kStages ? kStageTunes[stage - 1] : stage == kStages + 1 ? kTuneChoose : kTuneVersus);
            }
        }
        SetHud({});
        SetBoxes({});
    }

    void TrainingCommand(Machine &machine) {
        StartTraining(machine);
    }
} // namespace

bool AttractMode() {
    return g.mode == Mode::Attract;
}
bool StageViewer() {
    return g.mode == Mode::Viewer;
}
void EndModes() {
    End();
}

void ModesBeforeFrame(Machine &machine) {
    if (g.mode == Mode::Attract) {
        if ((machine.players[0] | machine.players[1]) & (kStart | kCoin)) {
            End();
        } else {
            machine.players[0] = machine.players[1] = 0;
        }
    } else if (g.mode == Mode::Viewer) {
        ViewerBeforeFrame(machine);
    }
}

void ModesFrame(Machine &machine) {
    if (g.mode == Mode::Attract) {
        // (the board starts up through steps 2 to 8 and then begins its loop at 0; a game begun is at 2 for good)
        if (g.starting) {
            g.frames++;
            g.starting = g.frames < 470;
            Fast(g.starting && FeatureValue("attract_show") != kShowAll);
        } else if (Ram8(0xFF80A8) || Ram8(kCredits)) {
            End(); // (a coin in, or a game begun)
        }
    } else if (g.mode == Mode::Viewer && !g.starting && Ram16(kMode) == 0) {
        End();
    }
    if (g.mode == Mode::Attract && !g.starting) {
        AttractFrame();
    } else if (g.mode == Mode::Viewer) {
        ViewerFrame(machine);
    }
    // (only what this file set is put back)
    static bool hid = false;
    const bool viewing = g.mode == Mode::Viewer && !g.starting;
    if (viewing || hid) {
        machine.video.hideObject = viewing ? NotTheStage : nullptr;
        machine.video.layerMask = viewing ? kLayersWithoutPanel : 0xF;
        hid = viewing;
    }
}

void ModesState(Archive &ar) {
    ar.Mark(0x4D4F4445);
    ar.Pod(g);
    ar.Vec(g_other);
    if (!ar.Saving() && !RunningAhead()) { // (not when run-ahead puts the frame back: runtime/game.h)
        Fast(false);
        g_otherSound.assign(kSoundAhead * 2, 0);
        g_carry.clear();
        g_chosenStage = -1;
        Wanted().stage = g.stage;
    }
}

void RegisterModes() {
    Game &game = TheGame();
    game.pages.push_back({kModes, "", "Other ways to run the game: training, attract mode, viewers for the stages and the characters, the colour editor, survival, endless battle, ring out.", true});
    game.pages.push_back({"Training mode", kModes, "A fight with no clock that nobody loses, and the aids for practising in it."});
    game.pages.push_back({"Dummy", "Training mode", "What player 2 does in a training session."});
    game.pages.push_back({kAttract, kModes, "The game's own show on a loop, with no coin in: the introduction, the rankings, the title, fights."});
    game.pages.push_back({kViewer, kModes, "A stage with nobody on it, to look at and listen to."});
    game.commands.push_back({"training_start", "Training mode", "Start",
                             "Straight to the character choice with two players (player 1's controls choose both), then a fight "
                             "with no clock that nobody loses.",
                             TrainingCommand});
    game.commands.push_back({"attract_start", kAttract, "Start",
                             "Starts attract mode: the game's own show, over and over. Start (or a coin) ends it.", StartAttract});
    game.features.push_back({"attract_show", kAttract, "Shows",
                             "What attract mode plays: everything the board does, the introduction over and over, or one "
                             "demonstration fight after another.",
                             0, {"Everything", "The intro only", "The fights only"}});
    game.commands.push_back({"viewer_start", kViewer, "Start",
                             "Shows the stage chosen below with nobody on it. Left and right scroll it, or it scrolls by itself. "
                             "Reset ends it.",
                             StartViewer});
    std::vector<const char *> stages(std::begin(kStageNames), std::end(kStageNames));
    stages.push_back("Infinite");
    game.features.push_back({"viewer_stage", kViewer, "Stage",
                             "The stage the viewer shows, or Infinite: every stage in turn, each scrolling into the next with its "
                             "own tune. Changing it while viewing goes to the new one.",
                             0, stages});
    game.features.push_back({"viewer_scroll", kViewer, "Scrolling",
                             "How the viewer moves across the stage: with the stick, or by itself from one end to the other and back "
                             "(Infinite: always onwards).",
                             0, {"With the stick", "Slow", "Medium", "Fast"}});
    std::vector<const char *> music = {"The stage's own"};
    music.insert(music.end(), std::begin(kStageNames), std::end(kStageNames));
    music.push_back("Character select");
    music.push_back("Versus");
    game.features.push_back({"viewer_music", kViewer, "Music",
                             "The tune that plays in the viewer: the stage's own, or another's. (Infinite plays each stage's own.)", 0,
                             music});
}

} // namespace cps2recomp
