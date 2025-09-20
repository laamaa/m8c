# m8c

## Introduction

*m8c* is a remote display client for the Dirtywave M8 Tracker. It mirrors the M8’s display, enables
keyboard/gamepad control, and can route audio to your computer—useful for recording, streaming, larger screens,
or alternative input methods. The application is cross‑platform and can be built on Linux, Windows (MSYS2/MINGW64), and
macOS.

The [Dirtywave M8 Tracker](https://dirtywave.com/products/m8-tracker) is a portable sequencer and synthesizer featuring
8 tracks of assignable instruments (using engines like FM, waveform synthesis, virtual analog, sample playback, and MIDI output). 
It is inspired by the Game Boy tracker [Little Sound DJ](https://www.littlesounddj.com/lsd/index.php).

m8c works with the official M8 hardware over USB. It also supports the [M8 Headless](https://github.com/Dirtywave/M8HeadlessFirmware)
firmware running on a [Teensy](https://www.pjrc.com/teensy/) microcontroller. If you enjoy the M8 and its
tracker workflow, please support [Dirtywave](https://dirtywave.com/) by purchasing the hardware. You can check
availability [here](https://dirtywave.com/products/m8-tracker-model-02). You can also
support the creator, [Trash80](https://github.com/trash80) via [Patreon](https://www.patreon.com/trash80).

Many thanks to:

* Trash80: For the great M8 hardware and the original fonts that were converted to a bitmap for use in the program.
* driedfruit: For a wonderful little routine to blit inline bitmap fonts, [SDL_inprint](https://github.com/driedfruit/SDL_inprint/)
* marcinbor85: For the slip handling routine, https://github.com/marcinbor85/slip
* turbolent: For the great Golang-based g0m8 application, which I used as reference on how the M8 serial protocol works.
* *Everyone who's contributed to m8c!*

-------

## Installation

### Quick Start

1. Download the prebuilt binary for your platform from the [releases section](https://github.com/laamaa/m8c/releases/)
2. Connect your M8 or Teensy (with headless firmware) to your computer
3. Run the program—it should automatically detect your device

### Windows

There are prebuilt binaries available in the [releases section](https://github.com/laamaa/m8c/releases/) for Windows.

When running the program for the first time on Windows, Windows Defender may show a warning about an unrecognized app.
Click "More info" and then "Run anyway" to proceed.

### macOS

There are prebuilt binaries available in the [releases section](https://github.com/laamaa/m8c/releases/) for recent
versions of macOS.

When running the program for the first time on macOS, it may not open as it is from an Unidentified Developer. You need
to open it from the Applications Folder via Control+Click > Open then select Open from the popup menu.

### Linux

There are packages available for NixOS, an AppImage for easy installation, or you can build the program from source.

#### AppImage

An AppImage is available for Linux in the [releases section](https://github.com/laamaa/m8c/releases/). To use it:

1. Download the `.AppImage` file from the releases
2. Make it executable: `chmod +x m8c-*.AppImage`
3. Run it: `./m8c-*.AppImage`

The AppImage is portable and doesn't require installation - just download and run.

#### NixOS

```sh
nix-env -iA m8c -f https://github.com/laamaa/m8c/archive/refs/heads/main.tar.gz
```

Or if you're using flakes and the nix command, you can run the app directly with:

```sh
nix run github:laamaa/m8c
```

### Building from source code

#### Install dependencies

You will need git, gcc, pkg-config, make and the development headers for SDL3 and libserialport.

##### Linux (Apt/Ubuntu)

For Ubuntu 25.04 and later, SDL3 packages are available in the official repositories:

```sh
sudo apt update && sudo apt install -y git gcc pkg-config make libserialport-dev libsdl3-dev
```

For older Ubuntu versions, there is no official SDL3 package yet in the Ubuntu repositories.
You'll likely need to build the library
yourself. https://github.com/libsdl-org/SDL/blob/main/docs/README-cmake.md#building-sdl-on-unix

##### Windows (MSYS2/MINGW64)

This assumes you have [MSYS2](https://www.msys2.org/) installed:

```sh
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-pkg-config mingw-w64-x86_64-make mingw-w64-x86_64-SDL3 mingw-w64-x86_64-libserialport
```

##### macOS

This assumes you have [installed brew](https://docs.brew.sh/Installation)

```sh
brew update && brew install git gcc make sdl3 libserialport pkg-config
```

#### Download source code

```sh
mkdir code && cd code
git clone https://github.com/laamaa/m8c.git
```

#### Build the program

```sh
cd m8c
make
```

#### Start the program

Connect the M8 or Teensy (with headless firmware) to your computer and start the program. It should automatically detect
your device.

```sh
./m8c
```

If the stars are aligned correctly, you should see the M8 screen.

#### Choosing a preferred device

When you have multiple M8 devices connected and you want to choose a specific one or launch m8c multiple times, you can
get the list of devices by running

```sh
./m8c --list
```

Example output:

```
2024-02-25 18:39:27.806 m8c[99838:4295527] INFO: Found M8 device: /dev/cu.usbmodem124709801
2024-02-25 18:39:27.807 m8c[99838:4295527] INFO: Found M8 device: /dev/cu.usbmodem121136001
```

And you can specify the preferred device by using

```sh
./m8c --dev /dev/cu.usbmodem124709801
```

-----------

## Keyboard mappings

Default keys for controlling the program:

* ↑ = up
* ↓ = down
* ← = left
* → = right
* z / left shift = shift
* x / space = play
* a / left alt = opt
* s / left ctrl = edit

Additional controls:

* Alt+Enter = toggle full screen / windowed
* Alt+F4 = quit program
* Delete = opt+edit (deletes a row)
* Esc = toggle keyjazz on/off
* r / select+start+opt+edit = reset display (if glitches appear on the screen, use this)
* F1 = open config editor
* F2 = toggle in-app log overlay
* F12 = toggle audio routing on / off

### Keyjazz

Keyjazz allows to enter notes with keyboard, oldschool tracker-style. The layout is two octaves, starting from keys Z
and Q.
When keyjazz is active, regular a/s/z/x keys are disabled. The base octave can be adjusted with numpad star/divide keys
and the velocity can be set

* Numpad asterisk (*): increase base octave
* Numpad divide (/): decrease base octave
* Numpad plus (+): increase velocity by 10
* Numpad minus (-): decrease velocity by 10
* Holding the ALT key while changing velocity increases/decreases the value in steps of 1 instead of the default.

## Gamepads

The program uses SDL's game controller system, which should make it work automatically with most gamepads. On startup,
the program tries to load a SDL game controller database named `gamecontrollerdb.txt` from the same directory as the
config file. If your joypad doesn't work out of the box, you might need to create custom bindings to this file, for
example with [SDL2 Gamepad Tool](https://generalarcade.com/gamepadtool/).

### Gamepad Configuration

To configure your gamepad:

1. Download the [SDL2 Gamepad Tool](https://generalarcade.com/gamepadtool/)
2. Connect your gamepad and open the tool
3. Create custom bindings and save them to `gamecontrollerdb.txt`
4. Place the file in the same directory as your `config.ini` file

## Audio

m8c supports audio routing from the M8 device to your computer's audio output.

### Audio Controls

- **Toggle audio routing:** F12 (default) or configure `key_toggle_audio` in config
- **Audio buffer size:** Configure `audio_buffer_size` in config (0 = SDL default)
- **Audio device:** Configure `audio_device_name` in config for specific device selection

### Platform-specific Notes

- **macOS:** Grant microphone permission for audio routing to work
- **Linux:** May require additional audio permissions or configuration
- **Windows:** Should work with standard audio drivers

## Config

### Settings menu (in-app)

You can change the most common options without editing `config.ini` using the in-app settings overlay.

- **How to open:**
    - Keyboard: press F1.
    - Gamepad: hold the Back/Select button for about 2 seconds.
- **How to navigate:**
    - Move: Up/Down arrows or D‑pad.
    - Activate/enter: Enter/Space or South/A.
    - Adjust values (sliders/integers): Left/Right arrows or D‑pad left/right.
    - Back/close: Esc or F1; on gamepad use East/B or Back.
    - While remapping inputs, the menu will prompt you; press the desired key/button or move an axis. Use Esc/B/Back to
      cancel a capture.

Changes take effect immediately; use Save if you want them persisted to disk.

### Settings file (config.ini)

Application settings and keyboard/game controller bindings can be configured via `config.ini`.

The keyboard configuration uses SDL Scancodes, a reference list can be found
at https://wiki.libsdl.org/SDL2/SDLScancodeLookup

If the file does not exist, it will be created in one of these locations:

* Windows: `C:\Users\<username>\AppData\Roaming\m8c\config.ini`
* Linux: `/home/<username>/.local/share/m8c/config.ini`
* macOS: `/Users/<username>/Library/Application Support/m8c/config.ini`

You can choose to load an alternate configuration with the `--config` command line option. Example:

```sh
m8c --config alternate_config.ini
```

This looks for a config file with the given name in the same directory as the default config. If you specify a config
file that does not exist, a new default config file with the specified name will be created, which you can then edit.

### Log overlay

An in-app log overlay is available for platforms where reading console output is inconvenient.

- Default toggle key: F2. You can change it in `config.ini` under `[keyboard]` using `key_toggle_log=<SDL_SCANCODE>`.
- The overlay shows recent `SDL_Log*` messages.
- Long lines are wrapped to fit; the view tails the most recent output.

Enjoy making some nice music!

-----------

## FAQ

### Permission Issues

* When starting the program, something like the following appears and the program does not start:

```sh
$ ./m8c
INFO: Looking for USB serial devices.
INFO: Found M8 in /dev/ttyACM1.
INFO: Opening port.
ERROR: Error: Failed: Permission denied
```

This is likely caused because the user running m8c does not have permission to use the serial port. The easiest way to
fix this is to add the current user to a group with permission to use the serial port.

On Linux systems, look at the permissions on the serial port shown on the line that says "Found M8 in":

```sh
$ ls -la /dev/ttyACM1
crw-rw---- 1 root dialout 166, 0 Jan  8 14:51 /dev/ttyACM0
```

In this case the serial port is owned by the user 'root' and the group 'dialout'. Both the user and the group have
read/write permissions. To add a user to the group, run this command, replacing 'dialout' with the group shown on your
own system:

```sh
sudo adduser $USER dialout
```

You may need to log out and back in or even fully reboot the system for this change to take effect, but this will
hopefully fix the problem. Please see [this issue for more details](https://github.com/laamaa/m8c/issues/20).

### Device Not Found

* The program starts but shows "No M8 device found":
    - Ensure your M8 or Teensy is connected via USB
        - If using a Teensy, check that the headless firmware is properly installed
    - Try running with `--list` to see detected devices
    - On Linux, verify USB permissions (see permission issues above)

### Audio Issues

* No audio output:
    - Check that audio routing is enabled (F12)
    - Verify audio device selection in config
    - On macOS, ensure microphone permission is granted
