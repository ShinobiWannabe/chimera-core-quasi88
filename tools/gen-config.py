#!/usr/bin/env python3
"""gen-config.py - writes the package's declarations from one table.

The controls, settings and firmware of the core are listed once, here, and
this script writes everything that has to agree about them:

  waterbox/q88-input.h          the buttons in wire order, for the driver
  waterbox/waterbox.config      what Chimera is told about the machine
  waterbox/default_keybinds.json
  waterbox/file_slots.json
  waterbox/languages/en.json    every text a person reads

Run it after changing anything below; build-package.sh runs it too.
"""
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
WB = os.path.join(ROOT, "waterbox")

CONTROLLER = "PC-88 Controller"

# ---------------------------------------------------------------- the keys
#
# One entry a key of the PC-8801's keyboard: its name (the control's
# identity: a movie's column, a binding, a script), its TAStudio header, the
# one character it writes in a movie, the default binding, and the bits of
# the keyboard matrix it pulls low while held, as (I/O port, bit) pairs.
# Ports 00h-0Eh are the machine's keyboard ports (QUASI88 pc88/keyboard.c,
# keyport and keyport_ext). A key of the later keyboards (FR/MR and on) that
# older software reads as another key pulls that key's bit too, and the ones
# that read as SHIFT plus a key pull SHIFT (port 08h bit 6), as the real
# keyboard does. KANA and CAPS are locking keys: see LOCKS.
K = []


def key(name, header, mnem, bind, *bits):
    K.append({"name": name, "header": header, "mnem": mnem, "bind": bind, "bits": list(bits)})


SHIFT = (0x08, 6)
# the function row
key("Key STOP", "STOP", "o", "Shift+Pause", (0x09, 0))
key("Key COPY", "COPY", "p", "PrintScreen", (0x0A, 4))
for i in range(1, 6):
    key("Key f.%d" % i, "f%d" % i, "!\"#$%"[i - 1], "F%d" % i, (0x09, i))
for i in range(6, 11):
    key("Key f.%d" % i, "f%d" % i, "&'()~"[i - 6], "F%d" % i, (0x0C, i - 6), (0x09, i - 5), SHIFT)
key("Key ROLL UP", "RLUP", "u", "PageUp", (0x0B, 0))
key("Key ROLL DOWN", "RLDN", "y", "PageDown", (0x0B, 1))
# the number row
key("Key ESC", "ESC", "e", "Escape", (0x09, 7))
for i, d in enumerate("1234567"):
    key("Key " + d, None, d, "Number" + d, (0x06, i + 1))
key("Key 8", None, "8", "Number8", (0x07, 0))
key("Key 9", None, "9", "Number9", (0x07, 1))
key("Key 0", None, "0", "Number0", (0x06, 0))
key("Key Minus", None, "-", "Minus", (0x05, 7))
key("Key Caret", None, "^", "Equals", (0x05, 6))
key("Key Yen", "Yen", "\\", "Backtick", (0x05, 4))
key("Key BS", "BS", "b", "Backspace", (0x0C, 5), (0x08, 3))
key("Key INS DEL", "I/D", "D", "", (0x08, 3))
key("Key INS", "INS", "i", "Insert", (0x0C, 6), (0x08, 3), SHIFT)
key("Key DEL", "DEL", "d", "Delete", (0x0C, 7), (0x08, 3))
key("Key HOME CLR", "HOME", "h", "Home", (0x08, 0))
key("Key HELP", "HELP", "?", "End", (0x0A, 3))
# the letter rows
key("Key TAB", "TAB", "t", "Tab", (0x0A, 0))
for c in "QWERTYUIOP":
    port, bit = divmod(ord(c) - ord("@"), 8)
    key("Key " + c, None, c, c, (0x02 + port, bit))
key("Key At", None, "@", "LeftBracket", (0x02, 0))
key("Key Left Bracket", None, "[", "RightBracket", (0x05, 3))
key("Key RETURN", "RET", "r", "Enter, KeypadEnter", (0x0E, 0), (0x01, 7))
key("Key CTRL", "CTRL", "c", "Ctrl, LeftCtrl", (0x08, 7))
key("Key CAPS", "CAPS", "a", "CapsLock")
for c in "ASDFGHJKL":
    port, bit = divmod(ord(c) - ord("@"), 8)
    key("Key " + c, None, c, c, (0x02 + port, bit))
key("Key Semicolon", None, ";", "Semicolon", (0x07, 3))
key("Key Colon", None, ":", "Apostrophe", (0x07, 2))
key("Key Right Bracket", None, "]", "Backslash", (0x05, 5))
key("Key SHIFT", "SHIFT", "s", "Shift, LeftShift", SHIFT)
for c in "ZXCVBNM":
    port, bit = divmod(ord(c) - ord("@"), 8)
    key("Key " + c, None, c, c, (0x02 + port, bit))
key("Key Comma", None, ",", "Comma", (0x07, 4))
key("Key Period", None, "`", "Period", (0x07, 5))
key("Key Slash", None, "/", "Slash", (0x07, 6))
key("Key Underscore", None, "_", "", (0x07, 7))
key("Key Right SHIFT", "RSHIFT", "s", "RightShift", (0x0E, 3), SHIFT)
# the bottom row
key("Key KANA", "KANA", "k", "Ctrl+ScrollLock")
key("Key GRPH", "GRPH", "g", "Alt, LeftAlt", (0x08, 4))
key("Key KETTEI", "KETTEI", "n", "Alt+Comma", (0x0D, 1), (0x09, 6))
key("Key SPACE", "SPACE", "w", "Space", (0x09, 6))
key("Key HENKAN", "HENKAN", "x", "Alt+Period", (0x0D, 0), (0x09, 6))
key("Key PC", "PC", "P", "", (0x0D, 2))
key("Key ZENKAKU", "ZEN", "z", "", (0x0D, 3))
# the cursor keys
key("Key Up", "Up", "{", "Up", (0x08, 1))
key("Key Down", "Down", "}", "Down", (0x0A, 1))
key("Key Left", "Left", "<", "Left", (0x0A, 2))
key("Key Right", "Right", ">", "Right", (0x08, 2))
# the number pad
for i in range(8):
    key("Key Pad %d" % i, "P%d" % i, str(i), "Keypad%d" % i, (0x00, i))
key("Key Pad 8", "P8", "8", "Keypad8", (0x01, 0))
key("Key Pad 9", "P9", "9", "Keypad9", (0x01, 1))
key("Key Pad Asterisk", "P*", "*", "KeypadMultiply", (0x01, 2))
key("Key Pad Plus", "P+", "+", "KeypadAdd", (0x01, 3))
key("Key Pad Equals", "P=", "=", "", (0x01, 4))
key("Key Pad Comma", "P,", ",", "", (0x01, 5))
key("Key Pad Period", "P.", "`", "KeypadDecimal", (0x01, 6))
key("Key Pad Minus", "P-", "-", "KeypadSubtract", (0x0A, 5))
key("Key Pad Slash", "P/", "/", "KeypadDivide", (0x0A, 6))
key("Key Pad RETURN", "PRET", "r", "", (0x0E, 1), (0x01, 7))

# The locking keys: a press turns the lock on or off, and the machine reads
# the lock (QUASI88 does the same, keyboard.c quasi88_tgl_key_kana).
LOCKS = {"Key KANA": (0x08, 5), "Key CAPS": (0x0A, 7)}

# ------------------------------------------------- the rest of the controls
#
# The joystick of the sound board's port (QUASI88's -joystick), and the
# mouse on the same port (-mouse): which one is plugged in is the Port
# setting. Both read the port's bits, so the mouse buttons are the stick's
# buttons. kind: what the driver does with it (q88-driver.c).
OTHER = [
    ("Mouse Left Button", "MLeft", "l", "WMouse L", "JOY", 4),
    ("Mouse Right Button", "MRight", "m", "WMouse R", "JOY", 5),
    ("P1 Joystick Up", "J1Up", None, "J1 POV0U, X1 DpadUp, X1 LStickUp", "JOY", 0),
    ("P1 Joystick Down", "J1Down", None, "J1 POV0D, X1 DpadDown, X1 LStickDown", "JOY", 1),
    ("P1 Joystick Left", "J1Left", None, "J1 POV0L, X1 DpadLeft, X1 LStickLeft", "JOY", 2),
    ("P1 Joystick Right", "J1Right", None, "J1 POV0R, X1 DpadRight, X1 LStickRight", "JOY", 3),
    ("P1 Joystick Button 1", "J1A", None, "J1 B1, X1 A", "JOY", 4),
    ("P1 Joystick Button 2", "J1B", None, "J1 B2, X1 B", "JOY", 5),
    ("Drive 1 Previous Floppy Disk", "D1Prev", "q", "", "FDD1_PREV", 0),
    ("Drive 1 Next Floppy Disk", "D1Next", "j", "", "FDD1_NEXT", 0),
    ("Drive 1 Swap Floppy Disk", "D1Swap", "v", "", "FDD1_SWAP", 0),
    ("Drive 2 Previous Floppy Disk", "D2Prev", "q", "", "FDD2_PREV", 0),
    ("Drive 2 Next Floppy Disk", "D2Next", "j", "", "FDD2_NEXT", 0),
    ("Drive 2 Swap Floppy Disk", "D2Swap", "v", "", "FDD2_SWAP", 0),
    ("Reset", "Reset", "f", "", "RESET", 0),
    ("Drive 1 Eject Floppy Disk", "D1Eject", "z", "", "FDD1_EJECT", 0),
    ("Drive 2 Eject Floppy Disk", "D2Eject", "z", "", "FDD2_EJECT", 0),
    # the data recorder, named and lettered as the NP2kai core's CD drive
    ("Tape Previous", "TPrev", "q", "", "TAPE_PREV", 0),
    ("Tape Next", "TNext", "j", "", "TAPE_NEXT", 0),
    ("Tape Swap", "TSwap", "v", "", "TAPE_SWAP", 0),
    ("Tape Eject", "TEject", "z", "", "TAPE_EJECT", 0),
    ("Tape Rewind", "TRewind", "b", "", "TAPE_REWIND", 0),
]

AXES = [
    {"name": "Mouse X", "min": -127, "max": 127, "neutral": 0, "header": "MX"},
    {"name": "Mouse Y", "min": -127, "max": 127, "neutral": 0, "header": "MY"},
]

TASTUDIO = [
    "Key ESC", "Key TAB", "Key SHIFT", "Key Z", "Key X", "Key C", "Key V",
    "Key Up", "Key Down", "Key Left", "Key Right",
    "Key Pad 1", "Key Pad 2", "Key Pad 3", "Key Pad 4", "Key Pad 5", "Key Pad 6",
    "Key Pad 7", "Key Pad 8", "Key Pad 9",
    "Key SPACE", "Key RETURN", "Key HELP", "Key STOP",
    "Drive 1 Next Floppy Disk", "Drive 1 Swap Floppy Disk", "Drive 2 Next Floppy Disk", "Drive 2 Swap Floppy Disk",
    "Reset",
    "P1 Joystick Up", "P1 Joystick Down", "P1 Joystick Left", "P1 Joystick Right",
    "P1 Joystick Button 1", "P1 Joystick Button 2",
]

# ---------------------------------------------------------------- settings
#
# Each becomes a QUASI88 command-line option (q88-driver.c builds the
# command line from the settings file and hands it to upstream's own
# option parser). "sync": the setting is part of the machine.
SAME = " It is part of the machine, so a movie needs the same value."
SOUND_ONLY = " Only the sound changes, never what the machine does, so a movie plays back the same with any value."

SEPARATE = {"setting": "romFiles", "is": "Separate files"}
SB2 = {"setting": "soundBoard", "is": "Sound Board II"}
RHYTHM = {"setting": "rhythmSamples", "is": True}

SETTINGS = [
    {"name": "romFiles", "type": "enum", "default": "PC88.ROM", "options": ["PC88.ROM", "Separate files"], "sync": True,
     "display": "ROM Files",
     "about": "Where the BASIC and disk unit ROMs come from. PC88.ROM, the default, is the one file that M88 and QUASI88 read, holding all of them from one machine. Separate files are QUASI88's N88.ROM, N88_0.ROM to N88_3.ROM, N88N.ROM and DISK.ROM. The files of one set must all come from the same model: a mixed set starts, but games then show garbage or do not start. The kanji ROMs are separate files either way." + SAME},
    {"name": "basicMode", "type": "enum", "default": "V2", "options": ["V2", "V1H", "V1S", "N"], "sync": True,
     "display": "BASIC Mode",
     "about": "The mode the machine starts in, as the mode switch on the front of a PC-8801 sets it. "
              "V2 is N88-BASIC V2, the mode almost every game is written for and the default. V1H and V1S "
              "are the older N88-BASIC V1 in its fast and standard speeds, and N is N-BASIC, the PC-8001 "
              "compatible mode a few early games need." + SAME},
    {"name": "cpuClock", "type": "enum", "default": "4MHz", "options": ["4MHz", "8MHz"], "sync": True,
     "display": "CPU Clock",
     "about": "The speed of the main CPU. 4MHz is the PC-8801mkIISR's, and the default; 8MHz is the "
              "fast mode of the FR, MR and later models. Most games are written for 4 MHz, and some run "
              "too fast at 8 MHz." + SAME},
    {"name": "soundBoard", "type": "enum", "default": "Sound Board I", "options": ["Sound Board I", "Sound Board II"],
     "sync": True, "display": "Sound Board",
     "about": "Which sound chip the machine has. Sound Board I is the YM2203 (OPN) of the PC-8801mkIISR, "
              "and the default. Sound Board II is the YM2608 (OPNA) of the later models and of the sound "
              "board II, with stereo, rhythm and ADPCM sound; games that support it say so." + SAME},
    {"name": "soundGenerator", "type": "enum", "default": "MAME", "options": ["MAME", "fmgen"], "sync": True,
     "display": "Sound Generator",
     "about": "Which code makes the sound of the FM chip. MAME is the YM2203 and YM2608 emulation QUASI88 "
              "takes from MAME, and QUASI88's default; fmgen is cisc's FM sound generator, which many "
              "prefer for the YM2608. The chip's timers are QUASI88's own either way." + SAME},
    {"name": "cpuTiming", "type": "enum", "default": "0", "options": ["0", "1", "2"], "sync": True,
     "display": "CPU Timing",
     "about": "How QUASI88 runs the main CPU and the disk unit's CPU together (its -cpu option). 0, the "
              "default, runs the disk unit's CPU only while the main CPU waits for it, the fastest. 1 runs "
              "them side by side while they talk to each other, and 2 runs them in turns of 5 microseconds "
              "all the time, the slowest and the closest to a real machine. A few games need 1 or 2 to "
              "load." + SAME},
    {"name": "fdcWait", "type": "bool", "default": True, "sync": True, "display": "Disk Access Time",
     "about": "Makes the floppy disk controller take as long as a real one to move the head and wait for the "
              "sector to come round (QUASI88's -fdc_wait). On by default, so games load in about the time "
              "they take on a real PC-8801. Off, the controller answers almost at once and games load "
              "sooner (up to about 2 seconds in the games tried), but that is not as accurate to real "
              "hardware." + SAME},
    {"name": "memoryWait", "type": "bool", "default": False, "sync": True, "display": "Memory Wait",
     "about": "Slows the CPU down by the wait states a real PC-8801's memory adds (QUASI88's -mem_wait). "
              "Off by default, as in QUASI88." + SAME},
    {"name": "monitor", "type": "enum", "default": "24kHz", "options": ["24kHz", "15kHz"], "sync": True,
     "display": "Monitor",
     "about": "Which monitor the machine is told is connected. 24kHz, the default, is the high resolution "
              "monitor that shows the 640x400 modes; 15kHz is a TV-like monitor, on which the machine "
              "shows 200 lines only. Some games check it." + SAME},
    {"name": "port", "type": "enum", "default": "Joystick", "options": ["Joystick", "Mouse", "None"], "sync": True,
     "display": "Joystick Port",
     "about": "What is plugged into the sound board's controller port: a joystick (the default), a mouse, "
              "or nothing." + SAME},
    {"name": "dipSwitches", "type": "string", "default": "391A", "sync": True, "display": "DIP Switches",
     "about": "The machine's DIP switches, as four hexadecimal digits (QUASI88's -dipsw). The default, "
              "391A, is QUASI88's. They set the terminal mode, the number of text lines and columns and "
              "the serial line. A value that is not hexadecimal is refused, and the default is used." + SAME},
    {"name": "bootFrom", "type": "enum", "default": "Disk", "options": ["Disk", "ROM"], "sync": True,
     "display": "Boot From",
     "about": "Whether the machine starts from the floppy disk in drive 1 (the default) or straight into "
              "BASIC in ROM." + SAME},
    {"name": "extraRam", "type": "int", "default": 0, "min": 0, "max": 16, "sync": True,
     "display": "Extended RAM Cards",
     "about": "How many 128 KB extended RAM cards are fitted. The default is none. A few programs use "
              "them." + SAME},
    {"name": "kanjiLevel2", "type": "bool", "default": True, "sync": True, "display": "Level 2 Kanji ROM",
     "about": "Whether the machine has the level 2 kanji ROM (KANJI2.ROM), which holds the less common "
              "kanji. On by default; without it those kanji show as blanks." + SAME},
    {"name": "dictionaryRom", "type": "bool", "default": False, "sync": True, "display": "Dictionary ROM",
     "about": "Whether the machine has the dictionary ROM (N88JISHO.ROM) of the later models, used by "
              "their kana to kanji conversion. Off by default." + SAME},
    {"name": "startTime", "type": "datetime", "default": 567993600, "sync": True, "display": "Start Time",
     "about": "The date and time the machine's calendar clock shows when it is switched on, exactly as "
              "written here: the clock holds a local time and has no time zone. The default is "
              "1988-01-01 00:00:00. The clock then runs with the emulated time, not with the time on "
              "your computer." + SAME},
    {"name": "rhythmSamples", "type": "bool", "default": False, "sync": True, "display": "Rhythm Samples",
     "about": "Whether the drums of the YM2608 (Sound Board II) play. They are recordings that no ROM of the PC-8801 holds: with the MAME Sound Generator they come from the YM2608's drum ROM as MAME dumps it (ym2608_adpcm_rom.bin, 8 KiB), with fmgen from the six 2608_*.WAV files PC-88 and PC-98 emulators share. Off by default, as the drums are then silent, which needs no file." + SAME},
    {"name": "luaMemoryEvents", "type": "bool", "default": False, "sync": True, "display": "Lua Memory Events",
     "about": "Lets Lua scripts stop at memory either Z80 runs, reads or writes (event.on_bus_exec, on_bus_read and "
              "on_bus_write), by the address the CPU uses (scope Main CPU or Sub CPU), and change the values "
              "read or written. It uses a copy of QUASI88 that checks every instruction and memory access, a "
              "little slower, so it is off by default. The machine runs the same either way, so a movie plays "
              "back the same with any value. Takes effect when the machine starts."},
    {"name": "volumeFM", "type": "int", "default": 100, "min": 0, "max": 100, "display": "FM Volume",
     "about": "The FM music of the sound chip, in per cent. The default, 100, is QUASI88's." + SOUND_ONLY},
    {"name": "volumeSSG", "type": "int", "default": 20, "min": 0, "max": 100, "display": "SSG Volume",
     "about": "The SSG part of the sound chip, which most games use for sound effects, in per cent. "
              "The default, 20, is QUASI88's." + SOUND_ONLY},
    {"name": "volumeBeep", "type": "int", "default": 60, "min": 0, "max": 100, "display": "Beep Volume",
     "about": "The machine's beeper, in per cent. The default, 60, is QUASI88's." + SOUND_ONLY},
    {"name": "volumeRhythm", "type": "int", "default": 100, "min": 0, "max": 200, "display": "Rhythm Volume",
     "about": "The drums of the YM2608 (Sound Board II), in per cent. The default, 100, is QUASI88's." + SOUND_ONLY},
    {"name": "volumeADPCM", "type": "int", "default": 100, "min": 0, "max": 200, "display": "ADPCM Volume",
     "about": "The ADPCM samples of the YM2608 (Sound Board II), in per cent. The default, 100, is QUASI88's." + SOUND_ONLY},
    {"name": "volumeFmgen", "type": "int", "default": 100, "min": 0, "max": 100, "display": "fmgen Volume",
     "about": "The whole sound chip when the Sound Generator is fmgen, in per cent. The default, 100, is "
              "QUASI88's." + SOUND_ONLY},
]

# ---------------------------------------------------------------- firmware
#
# Mounted in the guest under its id, which is the name QUASI88 looks for
# (pc88/memory.c rom_list). The sizes are the ROMs'.
FIRMWARE = [
    ("pc88.rom", 90112, "PC88.ROM (combined ROMs)", "The BASIC and disk unit ROMs of one PC-8801mkIISR or later that you own, in the one file M88 and QUASI88 read (PC88.ROM, 88 KiB): N88-BASIC, its four extension banks, the top of N-BASIC and the disk unit's ROM. A dump with the whole of N-BASIC (112 KiB) also runs the N mode.", ["pc88"], {"setting": "romFiles", "is": "PC88.ROM"}),
    ("n88.rom", 32768, "N88-BASIC ROM", "The main ROM of a PC-8801mkIISR or later that you own: N88-BASIC (32 KiB, N88.ROM).", ["n88.rom", "n88"], SEPARATE),
    ("n88_0.rom", 8192, "N88-BASIC V2 ROM, bank 0", "The first of the four 8 KiB banks of N88-BASIC V2's fourth ROM (N88_0.ROM or N88EXT0.ROM).", ["n88_0", "n88ext0"], SEPARATE),
    ("n88_1.rom", 8192, "N88-BASIC V2 ROM, bank 1", "The second of the four 8 KiB banks of N88-BASIC V2's fourth ROM (N88_1.ROM or N88EXT1.ROM).", ["n88_1", "n88ext1"], SEPARATE),
    ("n88_2.rom", 8192, "N88-BASIC V2 ROM, bank 2", "The third of the four 8 KiB banks of N88-BASIC V2's fourth ROM (N88_2.ROM or N88EXT2.ROM).", ["n88_2", "n88ext2"], SEPARATE),
    ("n88_3.rom", 8192, "N88-BASIC V2 ROM, bank 3", "The fourth of the four 8 KiB banks of N88-BASIC V2's fourth ROM (N88_3.ROM or N88EXT3.ROM).", ["n88_3", "n88ext3"], SEPARATE),
    ("n88n.rom", 32768, "N-BASIC ROM", "The N-BASIC ROM of the same machine (32 KiB, N88N.ROM or N80.ROM), for the N mode.", ["n88n", "n80"], SEPARATE),
    ("disk.rom", 8192, "Disk unit ROM", "The ROM of the floppy disk unit's own CPU (DISK.ROM or N88SUB.ROM, 2 or 8 KiB).", ["disk", "n88sub"], SEPARATE),
    ("kanji1.rom", 131072, "Level 1 kanji ROM", "The level 1 kanji ROM (128 KiB, KANJI1.ROM or N88KNJ1.ROM). The machine's text font is taken from it too.", ["kanji1", "knj1"], None),
    ("kanji2.rom", 131072, "Level 2 kanji ROM", "The level 2 kanji ROM (128 KiB, KANJI2.ROM or N88KNJ2.ROM).", ["kanji2", "knj2"], {"setting": "kanjiLevel2", "is": True}),
    ("n88jisho.rom", 524288, "Dictionary ROM", "The dictionary ROM of the later models (512 KiB, N88JISHO.ROM or JISYO.ROM).", ["jisho", "jisyo"], {"setting": "dictionaryRom", "is": True}),
    ("ym2608_adpcm_rom.bin", 8192, "YM2608 drum ROM", "The drum ROM inside the YM2608 sound chip, as MAME dumps it (8 KiB, in MAME's ym2608.zip). Used for the drums of Sound Board II when the Sound Generator is MAME.", ["ym2608"], {"all": [SB2, RHYTHM, {"setting": "soundGenerator", "is": "MAME"}]}),
    ("2608_bd.wav", 0, "YM2608 drum sample (bass drum)", "One of the six drum recordings of the YM2608 sound chip, as the WAV files PC-88 and PC-98 emulators share (2608_BD.WAV). Used for the drums of Sound Board II when the Sound Generator is fmgen.", ["2608_bd."], {"all": [SB2, RHYTHM, {"setting": "soundGenerator", "is": "fmgen"}]}),
    ("2608_sd.wav", 0, "YM2608 drum sample (snare drum)", "One of the six drum recordings of the YM2608 sound chip, as the WAV files PC-88 and PC-98 emulators share (2608_SD.WAV). Used for the drums of Sound Board II when the Sound Generator is fmgen.", ["2608_sd."], {"all": [SB2, RHYTHM, {"setting": "soundGenerator", "is": "fmgen"}]}),
    ("2608_top.wav", 0, "YM2608 drum sample (top cymbal)", "One of the six drum recordings of the YM2608 sound chip, as the WAV files PC-88 and PC-98 emulators share (2608_TOP.WAV). Used for the drums of Sound Board II when the Sound Generator is fmgen.", ["2608_top."], {"all": [SB2, RHYTHM, {"setting": "soundGenerator", "is": "fmgen"}]}),
    ("2608_hh.wav", 0, "YM2608 drum sample (hi-hat)", "One of the six drum recordings of the YM2608 sound chip, as the WAV files PC-88 and PC-98 emulators share (2608_HH.WAV). Used for the drums of Sound Board II when the Sound Generator is fmgen.", ["2608_hh."], {"all": [SB2, RHYTHM, {"setting": "soundGenerator", "is": "fmgen"}]}),
    ("2608_tom.wav", 0, "YM2608 drum sample (tom-tom)", "One of the six drum recordings of the YM2608 sound chip, as the WAV files PC-88 and PC-98 emulators share (2608_TOM.WAV). Used for the drums of Sound Board II when the Sound Generator is fmgen.", ["2608_tom."], {"all": [SB2, RHYTHM, {"setting": "soundGenerator", "is": "fmgen"}]}),
    ("2608_rim.wav", 0, "YM2608 drum sample (rim shot)", "One of the six drum recordings of the YM2608 sound chip, as the WAV files PC-88 and PC-98 emulators share (2608_RIM.WAV). Used for the drums of Sound Board II when the Sound Generator is fmgen.", ["2608_rim."], {"all": [SB2, RHYTHM, {"setting": "soundGenerator", "is": "fmgen"}]}),
]

SLOTS = [
    {"id": "floppy", "title": "Floppy disks", "min": 0, "max": -1, "formats": ["d88", "88d"],
     "help": "Floppy disk images in the D88 format (.d88, .88d). A D88 file may hold several disks. The "
             "first disk of the first file is in drive 1 when the machine starts, and the second file, or "
             "the first file's second disk, in drive 2. The order here is the order the Previous, Next and "
             "Swap Floppy Disk inputs go through, every disk of each file in turn. What the machine writes to a "
             "disk stays for the session and can be exported as save data."},
    {"id": "tape", "title": "Cassette tapes", "min": 0, "max": -1, "formats": ["t88", "cmt"],
     "help": "Cassette tape images (.t88, or QUASI88's raw .cmt). The first is in the data recorder when the machine starts; the Tape Previous, Next, Swap and Eject inputs change it, in the order listed here, and Tape Rewind winds it back to the start. A game on tape is loaded from BASIC (for example with LOAD or CLOAD, or MON and L). What the machine saves to tape goes into tape-save.cmt, which can be exported as save data."},
]

CODE_TEXTS = {
    "error.no_room_for_buffers": "There is no room in the core's memory for the picture or the sound.",
    "error.start_failed": "QUASI88 could not start the machine. Check that the firmware is there.",
    "error.no_firmware": "The firmware file %1 is missing.",
}

# ------------------------------------------------------------------ write


def text_key(kind, name, suffix=""):
    s = []
    for ch in name:
        if ch.isupper() and s and s[-1] != "_":
            s.append("_")
        s.append(ch.lower() if ch.isalnum() else "_")
    k = kind + "." + "".join(s).strip("_")
    return k + suffix


def main():
    texts = dict(CODE_TEXTS)
    buttons = [k["name"] for k in K] + [o[0] for o in OTHER]
    if len(set(buttons)) != len(buttons):
        sys.exit("two controls share a name")
    # None leaves a control to Chimera's rule, as the NP2kai core does: no
    # letter is the first letter of the name's last word, no header the letter
    mnemonics = {k["name"]: k["mnem"] for k in K if k["mnem"] is not None}
    headers = {k["name"]: k["header"] for k in K if k["header"] is not None}
    for o in OTHER:
        if o[2] is not None:
            mnemonics[o[0]] = o[2]
        if o[1] is not None:
            headers[o[0]] = o[1]

    def shown(b):
        return headers.get(b) or mnemonics.get(b) or b.split()[-1][0]

    # a header may not be another column's of the same player (Chimera's
    # contract tests refuse it): the console's columns, then player 1's.
    # Only the headers given are compared: a column headed by its letter is
    # one of many on a computer keyboard, as in the NP2kai core.
    for group in ([b for b in buttons if not b.startswith("P1 ")], [b for b in buttons if b.startswith("P1 ")]):
        seen = {}
        for b in group:
            if b not in headers:
                continue
            if headers[b] in seen:
                sys.exit("%s and %s share the header %s" % (seen[headers[b]], b, headers[b]))
            seen[headers[b]] = b

    settings = []
    for s in SETTINGS:
        d = {kk: v for kk, v in s.items() if kk not in ("display", "about")}
        d["display"] = text_key("setting", s["name"])
        d["description"] = text_key("setting", s["name"], ".about")
        texts[d["display"]] = s["display"]
        texts[d["description"]] = s["about"]
        settings.append(d)

    firmware = []
    for fid, size, disp, about, hints, req in FIRMWARE:
        k = "firmware." + fid.replace(".", "_")
        texts[k] = disp
        texts[k + ".about"] = about
        e = {"id": fid, "display": k, "description": k + ".about", "size": size, "name": fid, "nameHints": hints}
        if req:
            e["requiredWhen"] = req
        firmware.append(e)

    config = {
        "coreName": "QUASI88",
        "systemId": "PC88",
        "systemNames": {"PC88": "NEC PC-8801"},
        "author": "Showzoh Fukunaga (QUASI88), bubio (the SDL2 port); Chimera port by ShinobiWannabe and contributors",
        "url": "https://github.com/ShinobiWannabe/chimera-core-quasi88",
        "romFile": "rom",
        "deterministic": True,
        "runsOn": ["windows-x86_64", "linux-x86_64", "macos-x86_64"],
        "memoryLayoutMiB": [64, 16, 16, 16, 128],
        "video": {
            "_comment": "QUASI88 draws the 640x400 screen; 200-line modes are drawn line doubled. "
                        "One frame is one VSYNC, at QUASI88's 55.4 Hz.",
            "width": 640, "height": 400, "virtualWidth": 640, "virtualHeight": 400,
            "vsyncNumerator": 277, "vsyncDenominator": 5, "getBgra": "GetVideoBgra",
        },
        "audio": {
            "_comment": "44320 Hz is exactly 800 samples a frame at 55.4 frames a second.",
            "samplesPerFrame": 800, "channels": 2, "rate": 44320, "get": "GetAudio",
        },
        "input": {
            "name": CONTROLLER,
            "_comment": "Index order is the wire format (waterbox/q88-input.h, written by tools/gen-config.py): "
                        "the keyboard row by row, the cursor keys, the number pad, then the joystick, the mouse "
                        "buttons, the drives and Reset. Wider than 64, so everything rides the SetButton channel.",
            "buttons": buttons,
            "mnemonics": mnemonics,
            "headers": headers,
            "axes": AXES,
            "tastudioColumns": TASTUDIO,
        },
        "extensions": ["d88", "88d", "t88", "cmt"],
        "settings": settings,
        "firmware": firmware,
    }

    slots = []
    for s in SLOTS:
        d = dict(s)
        d["title"] = "slot." + s["id"]
        d["help"] = "slot." + s["id"] + ".help"
        texts[d["title"]] = s["title"]
        texts[d["help"]] = s["help"]
        slots.append(d)

    def dump(path, obj):
        with open(path, "w", encoding="utf-8", newline="\n") as f:
            json.dump(obj, f, indent=2, ensure_ascii=False)
            f.write("\n")

    dump(os.path.join(WB, "waterbox.config"), config)
    dump(os.path.join(WB, "file_slots.json"), {
        "_comment": "The kinds of file a project of this core holds. Written by tools/gen-config.py.",
        "slots": slots})
    binds = {k["name"]: k["bind"] for k in K}
    for o in OTHER:
        binds[o[0]] = o[3]
    dump(os.path.join(WB, "default_keybinds.json"), {
        "_comment": ["Default bindings for the PC-8801: each key on the PC key in its place, GRPH on Alt,",
                     "KANA on Ctrl+ScrollLock, the joystick on the game pad, the mouse on the mouse.",
                     "KETTEI, HENKAN, PC, ZENKAKU, keypad =, the drives and Reset ship unbound.",
                     "Written by tools/gen-config.py."],
        "AllTrollers": {CONTROLLER: binds},
        "AllTrollersAutoFire": {CONTROLLER: {}},
        "AllTrollersAnalog": {CONTROLLER: {
            "Mouse X": {"Value": "RMouse X", "Mult": 1.0, "Deadzone": 0.0},
            "Mouse Y": {"Value": "RMouse Y", "Mult": 1.0, "Deadzone": 0.0}}},
    })
    os.makedirs(os.path.join(WB, "languages"), exist_ok=True)
    dump(os.path.join(WB, "languages", "en.json"), {"texts": dict(sorted(texts.items()))})

    # the driver's table
    lines = ["/* q88-input.h - the controls in wire order. Written by tools/gen-config.py; do not edit. */",
             "#ifndef Q88_INPUT_H", "#define Q88_INPUT_H", "",
             "/* X(name, kind, a, b): kind KEY pulls the matrix bits of row a of",
             " * Q88_KEY_MASKS (a mask a port, ports 00h-0Eh) while held; LOCK toggles",
             " * the bits b of port a on each press; JOY pulls bit a of the controller",
             " * port; the rest are actions. */",
             "#define Q88_BUTTONS(X) \\"]
    rows = []
    for k in K:
        if k["name"] in LOCKS:
            port, bit = LOCKS[k["name"]]
            lines.append('\tX("%s", LOCK, 0x%02x, 0x%02x) \\' % (k["name"], port, 1 << bit))
            continue
        masks = [0] * 15
        for port, bit in k["bits"]:
            masks[port] |= 1 << bit
        lines.append('\tX("%s", KEY, %d, 0) \\' % (k["name"], len(rows)))
        rows.append("\t{ %s }, /* %s */ \\" % (", ".join("0x%02x" % m for m in masks), k["name"].replace("*/", "")))
    for name, _, _, _, kind, a in OTHER:
        lines.append('\tX("%s", %s, %d, 0) \\' % (name, kind, a))
    lines[-1] = lines[-1][:-2]
    lines += ["", "/* the matrix bits each key pulls low, ports 00h-0Eh */", "#define Q88_KEY_MASKS { \\"]
    lines += [r.replace("/* ", "/* ").replace(" */ \\", " */ \\") for r in rows]
    lines += ["}"]
    lines += ["", "#define Q88_AXIS_MOUSE_X 0", "#define Q88_AXIS_MOUSE_Y 1", "#define Q88_AXIS_COUNT 2", "",
              "#endif", ""]
    with open(os.path.join(WB, "q88-input.h"), "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(lines))
    print("gen-config: %d buttons, %d settings, %d firmware, %d texts" % (len(buttons), len(settings), len(firmware), len(texts)))


if __name__ == "__main__":
    main()
