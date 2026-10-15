# Changelog

## 0.1.0-beta.2

The first public release: builds for macOS (Apple silicon and Intel), Linux x86_64 (Steam Deck included)
and Windows, a training mode, the alternative modes, widescreen, custom colours and a colour editor, and since
0.1.0-beta.1:

- In-game menu: Right on a row that leads to a page goes into that page. The launcher's Features
  page has a search box (Ctrl+F or Cmd+F; Esc empties it, a second Esc leaves it; with a gamepad, A
  goes in and B leaves with what was typed kept). (toolkit cps2recomp 0.3.0, recomp-common 0.5.0)
- Board settings (the launcher's and the menu's Board page): the test menu's operator settings (difficulty,
  damage level, timer speed, game speed, max round, event, automatic, continue, demo sound), each as the board
  is set unless chosen; the board's settings memory is not changed. The in-game menu has a Test menu row (the
  board's test switch, as F2; toolkit cps2recomp#22).
- Invisible walls (on the Display page, in the launcher's Features and the in-game menu): fights shown in 16:9 but played exactly as on the board, the players stopped where the board's picture
  ends. Off unless switched on; nothing without widescreen.
- Display options (the toolkit's, cps2recomp#19): Scaling (Sharp, Whole pixels, Smooth), Picture shape (4:3, Square
  pixels, Fill) and Window size (3x to 6x) on the Display page. The defaults are how it was drawn before.
- The in-game menu's help for Alternative modes names every mode.
- README: the title screen, the colour editor's random colours and the model viewer as animations, an endless
  battle, a fight in custom colours, the in-game menu and the launcher's Game page; widescreen and the colour
  editor explained in full.
- Straight to the title (on the Game page): at a start or a reset, everything before the title
  (the board's start, the warning, the notices and the introduction) runs unseen; with a coin in or free
  play, the picture comes back with the title that asks for Start.

## 0.1.0-beta.1 (private)

The first release: builds for macOS (Apple silicon and Intel), Linux x86_64 (Steam Deck included) and
Windows, a training mode, the alternative modes, widescreen, and since the last private build:

- Custom colours and a colour editor (the toolkit's, as Vampire Savior Recomp has them): text files a character
  and a PalMod-changed `sz2u.07` in `palettes/`, and the editor on the Alternative modes page.
- Model viewer: animations loop (they stopped at their last frame), Play all goes through every animation of the
  character, and Stage in front draws what a stage has in front of the fighters in front of the character too.
- Colour editor: the Line row goes to an animation that shows that line, random colours only from the Random row and at a second press (HK
  undoes them), and its file commands are on its menu page.
- The game's program opened by itself starts the launcher.

## 0.1.0-alpha.1 (private)

First build for hand testing: training mode, modes, widescreen, readable rewrites (games/sfa2/PROGRESS.md).
