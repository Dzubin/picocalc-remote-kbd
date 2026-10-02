# Changelog

All notable changes to PicoCalc Remote Keyboard/Mouse.
Format loosely follows [Keep a Changelog](https://keepachangelog.com/);
the version here matches `REMOTE_KBD_VERSION` in `version.h`.

## [0.01A] - Unreleased

First version: the `remote/` library, the desktop relay, the example program
and the cursor demo. See `README.md` for what it does and `INTEGRATION.md` for
how to use it in your own PicoCalc program.

Build outputs are named without the `picocalc-` prefix, since the chip or system
at the end of the name already says what they are for: `remote-kbd-example-<chip>.uf2`,
`remote-kbd-cursor-<chip>.uf2` and `remote-kbd-relay-Windows.exe` /
`remote-kbd-relay-Linux`.
