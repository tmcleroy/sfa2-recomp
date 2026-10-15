# Street Fighter Alpha 2: progress

The USA arcade release of 1996-04-30 (ROM set `sfa2u`, parent `sfa2`), on Capcom's CPS-2 board: the version
played online (Fightcade's "Street Fighter Alpha 2 (960430 USA)"). Started 2026-10-14 as the third game on the
cps2recomp toolkit, after Vampire Savior and another CPS-2 game.

Run-ahead (the toolkit's `run_ahead`, 1 or 2 frames): off by itself in the modes (`noRunAhead` in game.cpp: attract,
stage viewer, battle modes, model viewer); the state hooks skip their loading resets while `RunningAhead()`. Checked:
the machine's state is the same frame for frame with it on and off (ladder, versus, training with every aid and
widescreen, model viewer, endless with set fighters), and the picture shown at frame N is the one the plain game
shows at N+1 (N+2). The endless battle's own picks are seeded from the system each time (two runs differ anyway).

## Where it stands (2026-10-14, morning)

- Boots and plays on the toolkit's own machine: warning, QSound logo, title, attract mode, character choice,
  versus fights, the whole one-player ladder to Akuma, an ending and the credits (headless pictures).
- 68000 program: about 98,500 instructions translated to C++ (2,684 entries from made-up play and the movies,
  about 60,000 instructions from guessed entries). No instruction falls back to the interpreter (checked
  2026-10-14 over the ladder, every alternative mode and twelve 30-minute endless battles, with and without
  widescreen; the interpreter stays only as the fallback for a place no run has reached).
- Checks: `make check` (translated code against the interpreter run the same way, block by block; two runs
  alike) over 8,000 frames of the scripted fight; `make check-rewrites` over the movies (45 rewrites: the fight's
  rules and the players' whole state machine but the characters' own moves; about 2.5 minutes, eight at once);
  `make check-widescreen`.
- Sound: the game has Capcom's sound driver 1.05C (the toolkit's hand-written driver is 2.01b), so the Z80
  program runs translated, as Vampire Savior's does. Not listened to.
- Free play (on in Enhanced), cheats, the in-game menu, save states, speed, the launcher (Game, Features,
  Controllers with remapping) and the app bundle: the toolkit's, as for the other CPS-2 ports.
- Straight to the title (`skip_intro`, Game page, `src/title.cpp`): at a start or a reset, everything before the
  title runs fast-forwarded and unseen. Boot steps (kMode) 0, 2, 4, 6, 8, 0; then the show (2, 4 the notices, ...,
  10 the introduction, whose kGameStep 0E is the title fading in, frame 4008), or with a coin in (free play) 2, 4:
  the title asking for Start (frame 467). The two share their step numbers, so the credits (kCredits) tell them
  apart. Checked: per-frame state hashes the same with it on and off (plain start, free play, training, attract);
  a menu Reset skips again; a game's own step 0 between screens does not (only Machine::frame 1 begins it, the
  fix Vampire Savior's tester found). Not seen in a window.
- Every fighting-game feature of Vampire Savior's (training mode and its dummy, frame data, boxes, the modes,
  widescreen), plus Alpha 2's own: stun and Custom Combo readouts, throw range, Alpha counters, throw softening,
  quick rise and stun mashing for the dummy, and a CSV log of every move's frame data.

## Training mode

"Alternative modes" at the head of the in-game menu has a Training mode page (`src/training.cpp`; the aids
themselves are the toolkit's `fight/` module, shared with any CPS-2 fighting game):

- **Start** resets the board, works the coins and both Start buttons with the game running unseen, and stops at
  the character choice. Once player 1 has chosen, player 1's controls choose for player 2. The fight has no
  clock and nobody is knocked out (both lines in the rewritten rules: RoundFighting, TakeDamage).
- **Stage**: any of the eighteen stages or the game's choice (hook at `00797C`, where the stage number has just
  been worked out from a character).
- **Start over** (Backspace, left shoulder): the machine as it was when the round's fighting began, then the
  players and the camera's copies placed: middle, either corner, sides swapped or not.
- **Readouts**: combo (hits, damage, best), stun (taken / the amount that stuns), input display (numpad
  notation from the player's side, frames held), frame data (startup, active, recovery, advantage on hit and
  on guard, counted in game steps at the player dispatch `0214C0`), the frame meter, hit boxes (drawn a frame
  late, as the picture is), throw range (the zone the throw test `0260E8` reaches and the push-box edge it is
  tested against), health refills, fixed damage (the damage formula's random parts replaced by each row's average,
  the game's random numbers still drawn: the same combo always does the same damage; a line in TakeDamage).
- Cheats added for training: no stun for either player (a line in TakeDamage).
- Custom Combo time: while a player's Custom Combo goes on (`+142`, its time left, from about B0 down), a bar of
  its time under that player's bar.
- Dummy "played by the game": player 2 is the game's own computer player (`+A5`), at the board's difficulty.
- **Dummy** (page under Training mode): stand, crouch, jump; record and replay in three slots, replay a random
  slot; answer with the recording after guarding / being hit; guard always, after a hit, at random, standing
  only for what cannot be guarded crouching (the game's own table of attack kinds, below); while guarding,
  an Alpha counter with a punch or a kick (back, down-back, down with the button: checked, the dummy goes into
  its counter and it hits); softens throws (holding away with two punches tapped: a throw that took 19 took 9,
  as the wiki says a teched throw does); quick rise after a knock-down (quarter circle back with a punch, found
  by trying: a sweep's knock-down took 74 frames, 43 with it; knock-downs told by the game's `+134`); mashes out of
  stun (a new direction and button every frame while kAction is `12`: stunned frames in a scripted session 202 without,
  105 with).

**Frame data checked against SuperCombo's Alpha 2 Ryu page (2026-10-14)**: startup and active of all nine
standing and crouching normals match (st.LP 4/4, st.MP 4/3+6, st.HP 8/6, st.LK 7/6, st.MK 6/16, st.HK 10/6,
cr.LP 4/4, cr.MK 6/6, cr.HK 7/6), and st.MP's -4 on block. Health 144, stun 40 and the teched throw's 9 damage
match too. Hit advantage was not compared (the page gives block advantage only).

Checked with scripted fights only (headless pictures, traces, `SFA2_FRAMES=1` printing every move's numbers).
The `frame_log` feature appends each move to `frame_data.csv` beside settings.ini (character, input held as it
began, in the air, first animation frame, startup/active/recovery, advantage when one follows within 60 steps).
The input column is what was held on the move's first step, so a special shows only its last direction and button.
**Not played by hand.**

## Alternative modes

As Vampire Savior has them (`src/modes.cpp`, `src/battle.cpp`, `src/model.cpp`, `src/machines.cpp`). Checked with
no window only (pictures, traces); nobody has used them in a window or listened.

- **Attract mode**: everything, the intro only, or one demonstration fight after another (the game's step moved
  on past the parts left out, and its fight counter `FF0C68` stepped as it would have).
- **Stage viewer**: any stage with nobody on it and no panel (objects of colour sets 0-26, and the panel's tiles
  in any set, left out), scrolled with the stick or by itself, any stage's tune (or the select and versus tunes);
  "Infinite": every stage in turn, each scrolling into the next on a second machine.
- **Model viewer**: any of the 24 characters alone, on a stage, white or black, from a quarter to 25 times its
  size, standing (moved with player 1's controls) or going through any animation of the game's three tables
  (`09862C`, `09868C`, `09874C`), at full speed or slowed, over and over or all of them in turn (Play all).
  An animation ends on a frame that goes on to itself or one shown for good (time `0FFF`): both are its end, and
  it starts again (a for-good picture shown half a second first). "Stage in front": at 1x on a stage, the stage's
  own picture with every object but player 2's is drawn where the character's pixels are, so what is in front of
  the fighters stays in front (of the 18 stages with the character in the middle, only Guy's pipe). The hidden
  player 2 casts no shadow here (Vampire Savior's needed moving off).
- **Survival, endless battle, ring out** (`src/battle.cpp`: Vampire Savior's, ported onto this game's names):
  survival is one computer opponent after another on the health left (a sixth back for each beaten); the endless
  battle is two fighters the game plays with no bars and no clock, the one knocked down replaced, a fighter taken
  over by touching its controls and given back after ten idle seconds, on one stage, on stages that fade into
  one another (shuffle), or on a floor that scrolls through them (the fighters drawn alone, marked, over a stage
  another machine shows with its camera put where the fighters' is: `CameraAt`, lines in the camera rewrites);
  the ring out has no health, a stage's ends open (a line in KeepIn) and the winner jumps down to the next stage.
  Checked with no window: survival to three wins, shuffle and scroll stages, ring outs both ways (the computer
  knocked over twice, then the player). Nobody has played them.

## Custom colours and the colour editor (2026-10-14)

The toolkit's (`cps2recomp/fight/colours.h`, from Vampire Savior's; the same files, folders, feature and command
names); `src/colours.cpp` gives this game's facts and `src/model.cpp` the viewer's hooks. Facts (`sfa2.h`, "the
characters' colours"): 24 characters × 6 sets × 5 palettes from `26D0E0` (`0122E2`: set = table + (character × 6 +
`+A8`) × `A0`), the choice byte `+A8` (0 punch, 1 kick, 2 two punches, 3 two kicks: `0628D8`; 4/5 with auto guard:
`062C1C`), the set kept at `+CC` and where its five palettes go at `+D0` (the buffer `90C000`: player 1 `90C200`,
player 2 `90C160`), copied by `01240C`; effects' palettes `272AE0` (16 a character, `02725C`; the set put back from
`+CC` by `027204`). Found from a RAM and video RAM dump in a fight matched against the ROM, then the code that
names the table.

- Sets in files: `[P] [K] [PP] [KK] [AUTOP] [AUTOK]`; lines: body and four effects (PalMod calls them: a special's
  colour, an extra, the super trail light and dark). Japan names are read too: `Nash.txt` for Charlie, `Vega.txt`
  for M. Bison.
- PalMod file: `sz2u.07` (`200000`-`27FFFF`, words byte-swapped), refused unless its first `20000` bytes are the
  game's. **PalMod's own SFA2 tables give other places** (Ryu's punch set at `6E2BE` of the file; here it is
  `6D0E0`): either PalMod knows this file by another revision's tables or it handles it some other way. Not tried
  with PalMod itself; a hand-changed copy was taken (Ken's colours) and its outside words reported and left.
- Checked with no window (pictures, files on disk): the export, a text file in a fight (Ryu, Charlie by `Nash.txt`),
  the PalMod-style file, and the editor driven by a movie (levels held and stepped, copy/paste, game's colour,
  random sets, the animation row, Save set, Load, delete with MK twice, the autosaved file). Not used in a window
  by a person.
- The Line row goes to an animation (`AnimationForLine` in `src/model.cpp`, Vampire Savior's way): while the
  editor is on, every frame of every animation is shown on a spare copy of the viewer's machine (8 a frame, at
  most 48 frames an animation) and the objects in each of player 1's five palettes are counted (the first palette
  from `+D0`). Done about 1000 frames after the editor starts. Found: Ryu's line 2 in B42 (his fireball); lines 4
  and 5 nowhere for the characters tried (their effects come only in a fight).

## Widescreen (2026-10-14)

`src/widescreen.cpp`, a switch on the launcher's first page under "Full screen" (`game.cfg`'s `display`) and on
the menu's Display page; off unless switched on, in every preset (it changes how the game plays). A fight is 512
columns wide where the board shows 384, and is played in that picture, as Vampire Savior's is.

- Alpha 2's scroll screens hold the stage's whole picture for the camera's range (the wide sides were the stage's
  own art on all eighteen stages with the camera between its ends), so nothing is drawn by the port; the picture
  only needs its width, the panel kept in the middle, and the camera stopped 64 short of its ends.
- In the rules, as lines in rewritten routines: the camera's ends (`01B662`, CameraAcross) and the players kept
  inside the picture (`0236F0` KeepIn, `0237BE` KeepBothIn: the picture's edges 64 further out). With the feature
  off the three are the game's own code (0 differences over the movies).
- The planes behind move more slowly than the camera, so near a stage's end the wide view of them reaches past
  what the board ever shows of them: each is drawn moved (a picture-only change) by what its view would reach past
  its painting, from its places at the camera's two ends (measured on every stage, `kPlaneEnds`); a plane painted
  narrower than the wide picture (Ryu's sky, Sagat's) has the shortfall split between its sides and so barely
  scrolls in the wide picture.
- `make check-widescreen` walks a training session to both ends of every stage with widescreen on and counts
  the columns at the wide picture's edges with a long run of black down them (an unpainted plane): 0 on all
  eighteen. (Dan's stage's own plane is painted 7 columns short of the camera's right end and the 384 from it:
  there the wide camera stops 8 sooner, `kPaintedShort`.) It also plays the ladder with
  widescreen on: no instruction left to the interpreter, nor in the endless battle (twelve 30-minute runs).
- The 8-pixel plane (record `FF8200`, scroll screen 1) is drawn into the map a 32-pixel strip at a time as it moves
  on most stages (`+03` not zero, which also says how its plan is laid out), and the game keeps up only what the
  board shows: the wide picture's sides are read from the stage's plan (`FF2B44`: a byte a 256-pixel block; the
  record's `+30` blocks of 8 x 8 cell numbers; `+34` cells of 4 x 4 tiles), as the game's strip routines (`01C574`,
  `01C5AA`, `01C668`) find them. Checked by `SFA2_WIDE_CHECK=1` in `make check-widescreen`: the plan against what
  the game drew into the map where the board shows it, 136,989 tiles on the eighteen stages, none differ.
- Survival, the endless battle, the stage and model viewers keep the board's 4:3. Not played by hand. A fireball
  goes on to the wide picture's edge (traced: drawn out to its last column, then gone). Pieces of the stages
  (objects) at the wide picture's edges were not looked at closely.

## Invisible walls (2026-10-15)

`widescreen_walls`, on the Display page (the launcher's Features and the in-game menu, beside
Scaling and Picture shape) (a request): the 16:9 picture with the game played as on the board. The margin
in the rules (`Margin()`, `WideMargin()`) is 0, so CameraAcross, KeepIn, KeepBothIn and the modes are the board's;
only the picture is wide (`g_shown`), and near a stage's end the view is kept on the stage (`ViewRightMargin`, Dan's
painted-short end included), the board's picture then to one side.
- The planes behind are moved from where the view of them already is (`PlaneShift(..., place - g_shift)`): without
  that, Guy's far plane showed a black box at both ends. In the wide play g_shift is 0 in a fight, so nothing changes
  there. (`kPlaneEnds` was measured at the board's camera ends, 256 and 640: checked again.)
- `make check-widescreen` now also walks every stage to both ends with the walls on (the 128 columns beyond the
  board's picture: 0 unpainted but Guy's left end, 22, his black girder), and compares work RAM, video and object
  memory and registers along the ladder with widescreen and the walls on and with neither, every 4,000 frames: all
  the same (the wide play differs at 42 of 56 such places, so the comparison does see a change).
- Not hand-played; not looked at in a window.

## Board settings (2026-10-15)

`board.cpp`, as Vampire Savior's: the test menu's operator settings (Configuration: System and Game) as features
on the Board page, each "As the board is set" unless chosen. The board's start reads the settings memory
(00ACF0, called at 001090) and copies the record (FF20D6) into variables; the hook at 001096 puts the choices
in them; the settings memory is not changed. The test switch puts the board's own values back for the test menu
(which shows, changes and saves the variables), and the choices go back in when a game next begins (004B6C,
before it copies difficulty to FF80BE, max round to FF8108 and continue to FF811F).

The record and the variables (from A5), with the values found by stepping each row of the test menu's pages:
+01 continue 98, +03 demo sound 99; +10 difficulty A0 (0 easy, 1 normal, 2-6 hard 1-5, 7 expert; a new board
3), +11 damage A1 (low, medium, normal, high; 1), +12 timer speed A2 (slow ... fastest; 0), +13 game speed A3
(0 normal, 1-2 turbo 1-2, 3-4 free select 1-2; 2), +14 max round A4 (1, 2, 3 for one, three, five; 2), +15
event A5 (0 off, 1 one match, which clears continue: 00ADF6), +16 automatic BB (0 off, 1 free select), +17
D2 (not on the pages). Checked headless: the variables with and without choices, the test menu showing the
board's, and the choices back in a game after it.

## Readable code

Rewrites (`cps2recomp/docs/READABLE.md`), each run beside the game's own code on every call by `make
check-rewrites` (0 differences over the movies, each layer on its own):

| Routine | Address | File |
| --- | --- | --- |
| FrameRegisters: the frame interrupt's video registers and the control panel | 0013CE | `input.cpp` |
| RoundFrame: a frame of a round: its step, the knock-out's slow motion, then every part of the fight in order (the computer's upkeep, the players (in an order by chance each frame), their recent frames kept for a Custom
| PlayersStep: both players' step: the order by chance each frame (only the one whose super flashes; one in a throw first; one beginning a throw goes again, as the game has it), each one's timers, controls by its facing (newly pressed, let go) and the routine for its state | 01FF0A | `player.cpp` |
| PlayerFighting: a player in a fight (kState 2): the computer's own or a person's special moves' inputs and quick rise, a hold throw's damage, then its kControl's routine (through the game's own dispatch, where the frame data's hook is) | 02149A | `player.cpp` |
| QuickRecovery: falling from a knock-down, a quarter circle back with a punch (Sodom: another motion too) and the player recovers (kControl 10) | 022AEA | `player.cpp` |
Combo's shadows, the clock held in a super's flash, things thrown, PushApart, the objects, the shake, AllHitTests, the score) | 00559C | `fight.cpp` |
| RoundFighting: the round's fought step: the clock (infinite time and training inside), the round decided | 005820 | `fight.cpp` |
| AllHitTests, PlayersAgainstPlayers: a frame's hit tests; a player with no health left has lost | 010352, 010370 | `collision.cpp` |
| HitTest: does this attack hit that player, and where (head, body, legs), juggle and repeat limits | 0103AA | `collision.cpp` |
| ThrownAgainstPlayers: each thing thrown against the other side's player | 011092 | `collision.cpp` |
| ThrownAgainstThrown: things thrown by the two sides meeting (fireballs cancelling) | 010FD8 | `collision.cpp` |
| BoxesOverlap, ClearHit | 0113AA, 011324 | `collision.cpp` |
| PushApart: the players cannot stand in each other (half each; all onto one at the picture's edge) | 00ECCE | `collision.cpp` |
| PlayerInControl: a step of a player in control of itself (timers, hit stop, the computer's turn, the routine for its kAction, KeepIn) | 021620 | `player.cpp` |
| Standing: what a standing player goes on to, in the game's order (time over, win, turn, attack, crouch, jump, guard, walk) | 021732 | `player.cpp` |
| Crouching: going down and rising (held down, the game keeps the player as standing's step 4, `+130` set) | 021880 | `player.cpp` |
| Walking: the walk's speed by character, and what a walking player goes on to (the computer's own walk) | 021990 | `player.cpp` |
| Turning: turning round, standing or crouched (holding down or not changes one into the other) | 021D22 | `player.cpp` |
| Jumping: leaving the floor, flying (attacks, wall jump, air guard), landing; the wall jump's bounce | 021A64 | `player.cpp` |
| Attacking: the character's own attack routine (by table, the computer's own) | 021E04 | `player.cpp` |
| Guarding: the guard held high or low, let go, and the computer's guard (low as the other is low) | 021E54 | `player.cpp` |
| SpecialMove: the character's own special moves' routine (by table) | 022000 | `player.cpp` |
| Stunned: the stun's count down, a step more for each stick move or button (mashing out) | 022026 | `player.cpp` |
| PlayerReeling: a step of a player reeling (the routine for its kAction, KeepIn) | 022116 | `player.cpp` |
| Recovered: reeling over, in control again (5 steps that it cannot be thrown; at once an attack or a guard) | 022D20 | `player.cpp` |
| ReelFrozen: the victim's side of a hit's freeze (its count down, the shake, the auto guard's flash) | 022544 | `player.cpp` |
| ReelGoesOn, ReelStanding, ReelGuarding, ReelHops: the reeling after the freeze by `+54`; kind 6: hopped back through the air, then in control (a jump); kind 2: pushed back guarding (high or low as held); kind 0: pushed back, knocked out, or (an attack of kind 8) guarding the rest holding back, the computer's Alpha counter | 0225E6, 0225FE, 0226E6, 022B52 | `player.cpp` |
| ReelDown: knocked down: the fall, the landing, the bounce, lying, getting up (dizzy if the stun was reached while down, for a time by chance and character), recovering in the air | 0227B8 | `player.cpp` |
| ReelOnFloor, ReelBegins: a hit lands: the reeling it gives by its reaction (light, standing only, knock-down or launch by the table at `02402C`, burning, shocking, frozen stiff, guarded high/low/in the air), the round's first hit | 022130, 022140 | `player.cpp` |
| HeldInThrow: held in a throw (kControl 6): placed and shown by the thrower's frame; the window (`+3A`) in which back or forward with a medium or heavy button softens the throw (`+29B`) | 022DD4 | `player.cpp` |
| ControlFour, ControlEight: kControl 4 and 8, the character's own routines (by table) | 022DAC, 022F1E | `player.cpp` |
| HitLands: the victim reeling, knock-back, hitstop, and the guard decision | 010564 | `hit.cpp` |
| TakeDamage: the damage formula (tables, random parts, chip, stun, the knock-out); the cheats and training inside | 0108E2 | `damage.cpp` |
| GuardRange: can the player guard now (something near enough, holding away) | 02343C | `guard.cpp` |
| ThrowTest: can this player throw the other now (its reach against the other's push box), and the throw's start | 0260E8 | `throws.cpp` |
| CameraAcross, CameraUp: the camera's place across and up, its ends (the wide picture's inside), a mode's own place | 01B662, 01B6DA | `widescreen.cpp` |
| KeepIn, KeepBothIn: the players inside the picture, the corner's push back | 0236F0, 0237BE | `widescreen.cpp` |

Coverage gaps: the guard decision's pose 4 (never seen) and the throw test's path for helpers were not reached by the movies. The air pose (3) is, by `air_guard.txt`: fireballs and jump-ins guarded and not in the air.
Reeling kinds 8 (`022BFA`: flung through the air, landing into kind 4's bounce) and 0A (`022C5C`: as
kind 2, a new press of down putting it to kind 0) were written once but never reached by any movie, so they were
not kept. Knocked down (kind 4): every step but 0E and 10 (entered from kind 8) and the computer's Sodom and
Dhalsim beginning a move as they land. A hit's reaction: 00-04, 06, 07, 09, 0C, 0D, 12, 14, 16, 1A and guarded
(with player 1 poked to Rose, Guy and Charlie for some); not 08, 0B, 0E, 11, 13, 15, 17-19.
Standing: every path but Sodom's thing made as he stands (`+35`, set by a move of his not found); M. Bison's own
standing animation by a poke of player 1's character (`FF8482=0A`) in the check. Crouching: all but walking
away while still going down. Turning: all but pressing down partway through a standing turn and the computer's
turn (the computer was never seen to turn this way). Jumping: all but step `0C` (a move of its own in the air, its
setter not found) and the computer guarding in the air; Rolento's, Sagat's and Shin Akuma's own branches not reached.
A hook (not a rewrite) takes throws' damage at `026518` for the readout and the cheats. The ladder is checked with free play only: with the cheats on
the rules differ from the game's by design.

## Facts about the game

Found by RAM dumps between known situations (`tools/ramfind.py`), write watches (`--watch`), traces of scripted
movies (`--trace`), and reading the listing. Names in `src/sfa2.h`.

- Built like Vampire Savior: variables from A5 = `FF8000`; players' records `FF8400` and `FF8800` (0x400), with
  the same four steps at their head (`+04` 2 in a fight; `+05` who is in charge: 0 the player, 2 reeling, 4/6
  held; `+06` the action: 0 stand, 2 crouch, 4 walk, 6 jump, `0A` attack, `0C` the guarding pose, `0E` the Alpha
  counter); `+0B` facing right; `+0D` its first colour set; `+10`/`+14` place (floor `0x28`); `+1C` the animation
  frame, `+32` its time left; `+67` bit 7: the camera does not follow it; `+142` a Custom Combo's time left; `+A5` the game plays it; `+134` the hit
  knocks it down.
- Health `+50` (taken off at once; below zero knocked out) and `+52` (what the bar shows, a point a frame after
  it): the other way about from Vampire Savior. Full is `0x90`. The super combo gauge `+9E` (word, `0x30` a
  level, three levels: `0x90`; an Alpha counter costs a level). Stun: `+137` taken, `+13A` the amount that stuns,
  `+136` a timer before it drains. `+55` FF when the last hit was guarded; `+5F` the hitstop (both players).
  `+82` the character; `+127` bit 0 holding away; `+38` the opponent's record (word).
- Characters (`+82`): 0 Ryu, 1 Ken, 2 Akuma, 3 Charlie, 4 Chun-Li, 5 Adon, 6 Sodom, 7 Guy, 8 Birdie, 9 Rose,
  10 M. Bison, 11 Sagat, 12 Dan, 13 Sakura, 14 Rolento, 15 Dhalsim, 16 Zangief, 17 Gen; 18-23 other forms
  (classic Chun-Li, Gen, Shin Akuma, Zangief, Dhalsim, Evil Ryu). Seen on the name plates with each put in.
- Stages: word `FF8100`, the stage's number doubled; eighteen, one a character (table `007A7A`; the other forms
  share their first form's). Chosen at `007914` from one of the players' characters.
- Where the game is: `FF8000` 2 in a game; `FF8004` 2 choosing, 4 the match; `FF8008` 4 a round; `FF800C` 2 the
  round being fought. `FF80A8` the players in (a bit each), `FF80AC` who has chosen a character. Credits
  `FF8076` (the board's settings start at two coins a credit: `FF80C3` counts coins). The clock `FF8109`
  (seconds), `FF810A` (frames of the second). From A5: `10C` the round's result, `10D` time over, `8A` paused,
  `138` the game's mode of play (0 the usual).
- The control panel: words at `58(A5)` and `5C(A5)` (buttons above: punches bits 0-2, kicks 4-6; stick below),
  the frame before at `5A`/`5E`.
- Boxes: the animation frame names them: `+08` head, `+09` body, `+0A` legs, `+0B` the attack, `+0C` the push box;
  the record has the tables (`+110` head, `+114` body, `+118` legs, `+11C` attacks of 32 bytes, `+120` push), 8
  bytes a box (x, y, half width, half height; x turned about with the player). Frame `+17` bit 7: nothing in it
  can hit; `+13`: juggle. Things thrown: sixteen records of 0x80 from `FF9400`; their tables from a header at
  `+60` (word offsets: `+2` the attacks).
- Guarding: decided at `010564` on. The victim's frame `+0D` is its guarding pose (1 standing, 2 crouching,
  3 in the air: things thrown and attacks from the air, as Alpha 2's air guard has it); the attack's entry `+0D` is its kind, and the table at `011438` (another
  at `011456` for attacks from the air) gives what guards it: bit 0 standing, bit 1 crouching (kind 4 low;
  6 and `1C` overheads; 5, `0A`, `0F` neither). Chip damage goes through the same damage routine.
- A player's `+9A` is the panel's word for it this frame; `+108` the same as worked on (`+10A` the frame before):
  the buttons (`+108`) and the stick (`+109`), its left and right swapped when facing right. The players are
  stepped in an order chosen by chance each frame (`01FF0A`).
- A player's `+109` is the stick by its facing (bit 0 back, 1 forward, 2 down, 3 up); `+10C` buttons newly pressed
  (bits 0-2 punches, 4-6 kicks); `+10D` the stick newly moved. Walking's `+3C`: 1 back, 0 forward; a jump's:
  1 forward, FF back, 0 straight up.
- CPS2: a table named by its distance from the instruction (`move.w $x(pc,d0.w)`) is read deciphered (Code16);
  one reached through an address register (`lea $x(pc),a0` then `move.w (a0,d0.w)`) is read as data, the ROM as it
  is (Mem16). The rewrite check catches a mix-up at once.
- Damage: `0108E2` on; the amount (a table lookup with a random part, `1975A`) is capped at `0x7F` and taken
  off at `010B26`. Throws take theirs at `026524`. Stun builds in the same routine.
- Camera: `FF8290` the picture's left edge (copies `FF8026`, `FF82A0`, `FF82A8`); `FF8294` its bottom. Players
  stand from 280 to 1000, the camera goes from 256 to 640, a round starts at 544 and 736.
- Settings memory routines `00B444` (read) and `00B50C` (write), variables `-5E2A`, `-5E24`, `-5E20` from A5.
- The frame interrupt reads the panel inside `0013CE`, which also sets the frame's video registers.

## Movies

`games/sfa2/movies`: `versus.txt` (Ryu against Ken, a few hits), `versus_attacks.txt` (a scripted fight of
normals, specials, throws, fireballs, guarding, jumping in, supers and an Alpha counter, both ways round; made by a
script, not kept: lines are `frame P1 P2`), `ladder.txt` (one player up the whole ladder with
`free_play,no_damage,one_hit`), `clash.txt` (both players throwing fireballs at once), `crouch.txt` (crouching, letting go, going back down while
rising, walking out of a crouch, jump-overs with down pressed or let go at times through the turn), `training_rise.txt` (a training session: player 1 sweeps, the dummy's quick rise on), `throw_tech.txt` (each throws the other ten times; the other holds back and presses
a button at times after: medium and heavy ones soften the throw inside the window), `stun.txt` (player 1 lands series of hard hits until player 2 is
stunned; player 2 taps down all along and mashes buttons after some series), `wall_jump.txt` (player 1, poked to be Chun-Li, jumps back into
the left edge and presses right at times through the jump), `custom_combo.txt` (player 1 starts Custom Combos
and presses buttons through them; with `full_gauge`), `air_guard.txt` (each in turn jumps holding away from jump-ins,
fireballs and anti-airs; the players swap sides partway, so the later part is less scripted than it looks).

- Attract loop (`FF8000` with no coin): `04` notices, `10` introduction, `14` score ranking, `16`-`1A` a
  demonstration fight, `1C`-`2A` advisory, title and versus ranking, `2C`-`30` another fight, then `00` again.
  `FF0C68` counts the fights, two a fight (fight k is characters 2k against 2k+1).
- Sounds: a ring of sixteen-byte entries at `FFCF60`, written at the word `FFDF60`, sent from `FFDF62`; a tune's
  number in the first long (`FF05` before a new tune). Each stage's tune is in `modes.h`; `14` the character
  choice, `15` the versus screen.
- The panel is objects (sets 0-6 and 27-31) with tiles `1A400`-`1AAFF` (bars, names, clock, faces), `26Axx`
  (gauges), `0A4xx` outside the players' sets (Charlie's own pictures use `0A400`-`0A6FF` in his sets); the stage's
  own pieces are sets 27-31.
- Animations: three tables by character at `09862C`, `09868C`, `09874C` (others for the computer and in the air at
  `0986EC`, `09880C`): longs to lists of word offsets; frames of 0x18 bytes: `+0` time (`0FFF` for good), `+2`
  bit 15 a jump (to the long in the next slot), `+4` the picture, `+8`-`+0C` boxes, `+0D` guard pose, `+17` bit 7
  harmless.
- The throw test `0260E8` (D0 -20, D1 20 for a normal throw): a point D0 in front of the thrower within D1 of the
  near edge of the other's push box; the other on the floor and free.
- The camera's record `FF8280` (`+20` where it wants to be); `01BFE0` takes the players' extent, leaving out a
  player with bit 7 of `+67`.

## Not done / next

1. Hand play: picture, sound, controls, every training feature and mode in a window. A first pass:
   - Launcher (`build/sfa2/Street Fighter Alpha 2.app`): the ROM set found, Controllers page remapping (keyboard
     and a pad), Play.
   - A one-player game and a versus game (free play: Start alone); the sound (never listened to); the menu (Esc,
     F1, the pad's stick click or Start+Select); save and load a state; widescreen on the Game page.
   - Training mode (menu, Alternative modes): Start, a stage, both characters; hit boxes, frame data and meter,
     input display, combo/stun/Custom Combo readouts; the dummy's guard, Alpha counter, throw softening, quick
     rise, stun mashing, record and replay; Start over; the frame data log's CSV beside settings.ini.
   - The modes: attract, stage viewer (and Infinite), survival, endless battle, ring out, model viewer.
2. Widescreen: the planes' painting at a few stages' ends (Ryu's sky), the stage pieces at the wide edges, hand
   play.
3. The players' own state machines: the step of a player in control (`021620`), standing (`021732`), crouching
   (`021880`), walking (`021990`), turning (`021D22`), jumping (`021A64`), attacking (`021E04`), guarding
   (`021E54`), special moves (`022000`) and the stun (`022026`) are rewritten, and reeling: its dispatch
   (`022116`), a hit's landing (`022140`), the freeze (`022544`), the reeling kinds 0, 2, 4 and 6, and its end
   (`022D20`). Not: reeling kinds 8 and 0A (`0225F2`; no movie reaches them), kControl 0A and 0C-10 (table `0214CE`) and the characters' own attack and special move routines.
4. The sound driver (1.05C) as ordinary code, as the toolkit has 2.01b's.
5. Linux, Steam Deck, Windows builds (the toolkit's release scripts, as for Vampire Savior).
