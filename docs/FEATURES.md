# Features

Every change to how the game plays, looks or is controlled is a switch, set in the launcher's
Features page or in the in-game menu (Esc, `F1`, a gamepad's left stick click) and kept in
`settings.ini`. With every one off the game runs exactly as the arcade board does. The launcher's
presets: **Pure** (all off), **Enhanced** (the default: free play and custom colours on), **Custom**.

The tables are what `sfa2 --list-features` prints. How features are written is in
[cps2recomp/docs/FEATURES.md](../cps2recomp/docs/FEATURES.md).

## Alternative modes

"Alternative modes", the first row of the in-game menu, has ways of running the game that the
board does not have. Each mode has a page there with a Start row and its choices, and ends when
another is started or with the menu's Reset.

- **Training mode**: straight to the character choice with two players and a fight with no clock
  that nobody loses (the README says more; the Training mode and Dummy features below are its aids).
- **Attract mode**: the game's own show on a loop with no coin in, even with free play on.
- **Stage viewer**: any of the eighteen stages with nobody on it and no panel, scrolled with the stick
  or by itself, with any tune; "Infinite" scrolls through every stage in turn.
- **Model viewer**: one character by itself, on a stage or plain white or black, from a quarter of its
  size to 25 times. It stands as in a fight and player 1's controls move it, or it goes through one of
  its animations over and over (named by the game's three lists and a number: A34; left and right go
  to the one before and after), or through all of them in turn (Play all).
- **Colour editor**: the model viewer's character with a panel worked by player 1's controls: the
  set of colours (P, K, PP, KK, AUTOP, AUTOK), its line (the character's own colours or an effect's;
  choosing one shows an animation that uses it, found by running each animation on a copy of the
  machine), the colour, and its red, green and blue (the board's 16 levels each). LP copies a colour,
  MP pastes it, HP puts back the game's, LK twice on the Random colours row gives the set random
  colours and HK takes them back. Changes are saved to the character's file in the `palettes` folder;
  "Save set" keeps a set in the character's library and "Load" puts one back.
- **Survival**: one computer opponent after another on the health you have left.
- **Endless battle**: two fighters the game plays, with no bars and no clock; touch a player's
  controls to take that fighter over.
- **Ring out**: no health; knock your opponent over the end of the stage and jump down to the next.

## Attract mode

| Feature | Name in `settings.ini` | What it does |
| --- | --- | --- |
| Shows | `attract_show` | What attract mode plays: everything the board does, the introduction over and over, or one demonstration fight after another. Choices: Everything, The intro only, The fights only. |

## Stage viewer

| Feature | Name in `settings.ini` | What it does |
| --- | --- | --- |
| Stage | `viewer_stage` | The stage the viewer shows, or Infinite: every stage in turn, each scrolling into the next with its own tune. Changing it while viewing goes to the new one. Choices: Ryu's (moonlit snow), Ken's (the yacht) ... Infinite (19). |
| Scrolling | `viewer_scroll` | How the viewer moves across the stage: with the stick, or by itself from one end to the other and back (Infinite: always onwards). Choices: With the stick, Slow, Medium, Fast. |
| Music | `viewer_music` | The tune that plays in the viewer: the stage's own, or another's. (Infinite plays each stage's own.) Choices: The stage's own, Ryu's (moonlit snow) ... Versus (21). |

## Survival

| Feature | Name in `settings.ini` | What it does |
| --- | --- | --- |
| Best | `survival_best` | The most opponents beaten in one survival. (Left puts it back.) Choices: 0, 1 ... 99 (100). |

## Ring out

| Feature | Name in `settings.ini` | What it does |
| --- | --- | --- |
| Best | `ringout_best` | The most ring outs in one go. (Left puts it back.) Choices: 0, 1 ... 99 (100). |

## Endless battle

| Feature | Name in `settings.ini` | What it does |
| --- | --- | --- |
| Fighter 1 | `endless_one` | Who is on player 1's side: someone new each time, or always the same. Choices: Random, Ryu ... Gen (19). |
| Fighter 2 | `endless_two` | Who is on player 2's side: someone new each time, or always the same. Choices: Random, Ryu ... Gen (19). |
| Stage | `endless_stage` | Where they fight. Shuffle: a stage that fades into another every so often. Scroll: the stages go by, one after another, and the fighters are carried along. Or one stage. Choices: Shuffle, Scroll ... Gen's (the ruins) (20). |
| Shuffle every | `endless_shuffle` | Shuffle: how long they fight on a stage before it fades into another. Choices: 1 minute, 2 minutes, 3 minutes, 5 minutes, 10 minutes. |
| Scroll speed | `endless_scroll` | Scroll: how fast the stages go by. Choices: Slow, Medium, Fast. |

## Model viewer

| Feature | Name in `settings.ini` | What it does |
| --- | --- | --- |
| Character | `model_character` | Who is shown. Changing it while viewing goes to the new one. Choices: Ryu, Ken ... Evil Ryu (24). |
| Background | `model_background` | What is behind the character: one of the stages, or plain white or black. Changing it while viewing goes to the new one. Choices: Ryu's (moonlit snow), Ken's (the yacht) ... Black (20). |
| Stage in front | `model_foreground` | What the stage has in front of the fighters: in front of the character too, as in a fight, or behind it so that all of it is seen. In front only at the character's own size, and not in the colour editor. Choices: In front, Behind. |
| Size | `model_size` | How large the character is drawn, from a quarter of its own size to 25 times. Choices: 1x, 1.5x ... 0.75x (13). |
| Animation | `model_animation` | What the character does: stands as in a fight, moved with player 1's controls, or goes through one of its animations over and over. They are named by the game's three lists of them and a number (A34); the ones a character has not got are passed over. While one is showing, left and right go to the one before and the one after, and its name is shown. Choices: Standing (you control it), A0 ... C255 (769). |
| Speed | `model_speed` | How fast the viewer runs: slow motion for looking at an animation frame by frame. (The menu's own speed, and the - and = keys, slow it further.) Choices: Full, 1/2, 1/4, 1/8. |
| Repeat after | `model_repeat` | How long an animation's last frame is held before it starts again. Choices: At once, 1 second, 2 seconds, 3 seconds, 4 seconds, 5 seconds, 6 seconds, 7 seconds, 8 seconds, 9 seconds, 10 seconds. |
| Play all | `model_all` | Goes through all of the character's animations, one after another: each is shown once (and its last frame held for "Repeat after") and then the next starts, round from the last to the first. Left and right still step through them. |

## Board

The board's own operator settings, as its test menu has them (Configuration: System and Game). Each
is "As the board is set" unless chosen, and a choice is taken when the game starts or is reset; the
board's settings memory is not changed. The in-game menu's "Test menu" row (or F2) opens the game's
own test menu instead.

| Feature | Name in `settings.ini` | What it does |
| --- | --- | --- |
| Difficulty | `board_difficulty` | How well the computer fights: Easy, Normal, Hard 1 to 5, Expert (a new board: Hard 2). |
| Damage level | `board_damage` | How much damage a hit does: Low, Medium, Normal, High (Medium). |
| Timer speed | `board_timer` | How fast the round's clock runs: Slow, Normal, Fast, Fastest (Slow). |
| Game speed | `board_speed` | Normal, Turbo 1 or 2, or Free select 1 or 2 (the players choose; Turbo 2). |
| Max round | `board_rounds` | Rounds a match lasts at most: 1, 3 or 5 (3). |
| Event | `board_event` | Off, or one match only (and no continue). |
| Automatic | `board_automatic` | Whether players may choose the automatic guard: Off, or Free select. |
| Continue | `board_continue` | Whether a game over can be continued. |
| Demo sound | `board_demo_sound` | Whether the attract mode plays sound. |

## Display

| Feature | Name in `settings.ini` | What it does |
| --- | --- | --- |
| Custom colours | `custom_colours` | Characters in the colours of the files in the "palettes" folder in the game's data folder: an sz2u.07 changed with PalMod, and text files, one a character (Sakura.txt), which go before it. With no files the game's own colours. They are taken when a player is coloured: at the next round. |

## Colour editor

| Feature | Name in `settings.ini` | What it does |
| --- | --- | --- |
| Animation | `colours_animation` | What the character does in the editor: stands in the game's idle, or goes through one of its animations (as in the model viewer). The panel has this row too. Choices: Standing, A0 ... C255 (769). |

## Game

| Feature | Name in `settings.ini` | What it does |
| --- | --- | --- |
| Hidden characters | `hidden_forms` | Choosing Ryu, Akuma, Chun-Li, Dhalsim or Zangief gives their hidden form instead: Evil Ryu, Shin Akuma, the classic Chun-Li, and Dhalsim's and Zangief's other forms. For player 1, player 2 or both. Choices: Off, Player 1, Player 2, Both. |
| Free play | `free_play` | Start works without a coin. |
| Straight to the title | `skip_intro` | When the game starts, and when it is reset, goes straight to the title: the board's memory test, the notices and the introduction are run unseen, in a second or so. |
| Run-ahead | `run_ahead` | Shows the game a frame or two further on than it is, so that what you press is seen sooner than on the board (input lag taken away). The game runs as it does without it; it takes two or three times the processor. Off by itself in the game's own modes beyond fights and training. Choices: Off, 1 frame, 2 frames. |

## Training mode

| Feature | Name in `settings.ini` | What it does |
| --- | --- | --- |
| Start over at | `restart_at` | Where "Start over" puts the players: the middle or a corner, player 1 on the left or (swapped) on the right. Choices: Middle, Left corner, Right corner, Middle, swapped, Left corner, swapped, Right corner, swapped. |
| Stage | `training_stage` | Where a training session's fight is: the game's own choice (the stage of one of the two characters) or a stage named here. Takes effect when the characters are next chosen. Choices: The game's choice, Ryu's (moonlit snow) ... Gen's (the ruins) (19). |
| Combo readout | `damage_readout` | Under the bar of the player hit: what the hit took off; for a combo (hits landing before the player is free again) the hits and the damage; the round's best. |
| Stun readout | `stun_readout` | Under each bar: how much stun the player has taken and how much stuns them (it drains when they are left alone for a while). |
| Custom Combo time | `custom_combo_readout` | While a player's Custom Combo goes on, how much of its time is left, under that player's bar. |
| Input display | `input_display` | What each player presses and for how many frames: the stick as a keypad number from the player's side (6 forward, 4 back, 2 down), then the buttons. |
| Hit boxes | `hit_boxes` | Draws the boxes the game tests: where a player can be hit (blue), attacks (red; orange for things thrown), and the boxes that keep the players apart (green). |
| Throw range | `throw_range` | With hit boxes on: the zone in front of each player that the other's push box edge (the yellow line) has to be in for a throw, as the game tests it (both on the floor and free to act). |
| Frame data | `frame_data` | After a move: the step it can first hit on (startup), how many it can hit for (active) and how many follow before the player can act (recovery). After a hit or a guarded one: who can act first, by how many steps (+: the attacker). |
| Frame data log | `frame_log` | Writes every move's frame data to frame_data.csv, beside the settings file: the character, what was held as it began, startup, active, recovery and the advantage after a hit or a guarded one. |
| Frame meter | `frame_meter` | A row of cells for each player, one for every step of the game: green before a move can hit, red while it can, blue after; yellow reeling from a hit, light blue from a guarded one; grey in the air. |
| Health refills | `health_refill` | A player's health comes back when they have not been hit for a moment. |
| Fixed damage | `fixed_damage` | The damage of a hit has no random part (the average of the game's own in its place), so the same combo always does the same damage. (On the board each hit's damage varies a little.) |

## Dummy

| Feature | Name in `settings.ini` | What it does |
| --- | --- | --- |
| Stands | `dummy_stand` | Player 2 stands still, whatever its controls do. |
| Crouches | `dummy_crouch` | Player 2 crouches. |
| Jumps | `dummy_jump` | Player 2 jumps on the spot, again and again. |
| Record | `dummy_record` | Player 1's controls work player 2, and what they do is recorded in the slot chosen (20 seconds at most). Switch to Replay when done. |
| Replay | `dummy_replay` | Player 2 does what was recorded in the slot chosen, over and over. |
| Replay any slot | `dummy_replay_any` | Player 2 does what was recorded, over and over, each time from a slot picked at random. |
| Recording slot | `dummy_slot` | Which of three recordings Record makes and Replay plays. Choices: 1, 2, 3. |
| Guards | `dummy_guard` | Player 2 guards player 1's attacks, standing or crouching as each needs: always; only for a moment after being hit (to see whether a combo really is one); or one attack in two, at random. Choices: Off, Always, After a hit, At random. |
| While guarding | `dummy_guard_action` | What player 2 does while guarding a hit: an Alpha counter (back, down-back, down with a punch or a kick; it costs a level of the super combo gauge, so pair it with "Full gauge, player 2"). Choices: Nothing, Alpha counter, punch, Alpha counter, kick, At random. |
| Answers with the recording | `dummy_reversal` | Player 2 plays the recording once as soon as it can act after guarding, after being hit (or knocked down, or thrown), or both. Choices: Off, After guarding, After being hit, After either. |
| Answers now and then | `dummy_reversal_random` | With an answer chosen: player 2 answers only one time in two, with any slot that has a recording, so you cannot learn when or how it will. |
| Softens throws | `dummy_tech` | Player 2 does a throw's own command while being thrown (a "tech hit", which takes less damage). Choices: Off, Always, At random. |
| Gets up | `dummy_getup` | How player 2 gets up after being knocked down. Choices: Where it fell, Quick rise, At random. |
| Mashes out of stun | `dummy_stun_mash` | Stunned, the dummy works the stick and the buttons as fast as it can, as a player does to get out sooner. |
| Played by the game | `dummy_computer` | Player 2 is the game's own computer player, as in a one-player game (at the board's difficulty setting). |

## Cheats

| Feature | Name in `settings.ini` | What it does |
| --- | --- | --- |
| No damage | `no_damage` | Player 1's health does not go down. |
| No damage, player 2 | `no_damage_2` | Player 2's health does not go down (the opponent's, in a one-player game). |
| One-hit knockouts | `one_hit` | Player 2 (the opponent, in a one-player game) goes down with the first hit. |
| Full gauge | `full_gauge` | Player 1's super combo gauge is always full. |
| Full gauge, player 2 | `full_gauge_2` | Player 2's super combo gauge is always full. |
| Infinite time | `infinite_time` | The round's clock stands still. |
| No stun | `no_stun` | Player 1 is never stunned (no stun builds up). |
| No stun, player 2 | `no_stun_2` | Player 2 is never stunned (no stun builds up). |

## Always there

Not features, because they change nothing in the game: the in-game menu (with its Test menu row, the
board's test switch), save states (nine slots),
stopping the game and going on a frame at a time, slow motion, a coin button, Reset, and choosing
keys and gamepad buttons in the launcher. The menu's "Start over" row and the rows that start a
mode are commands, not features.
