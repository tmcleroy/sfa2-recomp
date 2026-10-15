# Roadmap

Where the port is against where it is going: a native, readable program with nothing between the
game's rules and the player's machine.

## Where it stands

| Part | Today | |
| --- | --- | --- |
| The game's 68000 program | Translated ahead of time to C++ (about 98,500 instructions, 7,900 routines); code no run has reached yet runs in the port's own interpreter | No emulator. No instruction falls back to the interpreter over the ladder, every alternative mode and long endless battles |
| Readable rewrites | 45 routines written by hand as named C++ (the hit tests, pushing apart, the guard decision, damage, the throw, the round's clock, the camera, reading the controls, and the players' own state machine), each checked call by call | The characters' own moves and the rest of the game are still translated code |
| The Z80 sound driver | Translated too (about 2,000 instructions); the statements are calls of the interpreter's own primitives | |
| Picture | The port's own line renderer for the board's scroll screens and objects, shown through SDL's GPU API | A model of the hardware, not of the game's drawing calls |
| Sound | The port's own model of what the QSound chip's program does | No "Q1" stereo filter; volume and pan curves are approximations |
| Input | Read where the game's own routine reads the controls (a readable rewrite) | Native |
| Settings memory | A model of the board's serial EEPROM chip, kept in a file | A model of the hardware |
| The ROM set's cipher | MAME's implementation (BSD-3-Clause), the only file here from another emulator | To be written again from the published description of the cipher |

## Next

1. **Hand play on every system**: picture, sound, sticks and pads, speed, Steam Deck's Game Mode,
   Windows.
2. **Sound by ear** against a board or a trusted recording; the stereo filter if it can be done
   without the chip's internal ROM.
3. **Training mode**: frame data compared with more of the published tables; an input history that
   reads motions; a buffered reversal.
4. **More of the fight as readable code**: the characters' own moves, things thrown against each
   other, what happens when a hit lands.
5. **Other releases of the game** (other regions and revisions of `sfa2`) and *Street Fighter Zero 2
   Alpha*, a separate game on the same board.
6. **Video options**: scaling, picture shape and window size (the toolkit's, coming), a scanline or
   CRT shader, all off unless chosen.
