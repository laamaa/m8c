# m8c

## Introduction

The [Dirtywave M8 Tracker](https://dirtywave.com/products/m8-tracker) is a portable sequencer and synthesizer, featuring 8 tracks of assignable instruments such as FM, waveform synthesis, virtual analog, sample playback, and MIDI output. It is powered by a [Teensy](https://www.pjrc.com/teensy/) micro-controller and inspired by the Gameboy tracker [Little Sound DJ](https://www.littlesounddj.com/lsd/index.php).

While Dirtywave makes new batches of units available on a regular basis, M8 is sometimes sold out due to the worldwide chip shortage and high demand of the unit. To fill this gap and and to allow users to freely test this wonderful tracker, [Timothy Lamb](https://github.com/trash80) was kind enough to make the [M8 Headless](https://github.com/Dirtywave/M8HeadlessFirmware) available to everyone.

If you like the M8 and you gel with the tracker workflow, please support [Dirtywave](https://dirtywave.com/) by purchasing the actual unit. You can check its availability [here](https://dirtywave.com/products/m8-tracker). Meanwhile, you can also subscribe to Timothy Lamb's [Patreon](https://www.patreon.com/trash80).

*m8c* is a client for Dirtywave M8 tracker's headless mode. The application should be cross-platform ready and can be built in Linux, Windows (with MSYS2/MINGW64) and Mac OS.

Many thanks to:

* Trash80 for the great M8 hardware and the original font (stealth57.ttf) that was converted to a bitmap for use in the progam.
* driedfruit for a wonderful little routine to blit inline bitmap fonts, https://github.com/driedfruit/SDL_inprint/
* marcinbor85 for the slip handling routine, https://github.com/marcinbor85/slip
* turbolent for the great Golang-based g0m8 application, which I used as reference on how the M8 serial protocol works.
* *Everyone who's contributed to m8c!*

Disclaimer: I'm not a coder and hardly understand C, use at your own risk :)

-------

## Installation

### Windows / MacOS

There are prebuilt binaries available in the [releases section](https://github.com/laamaa/m8c/releases/) for Windows and recent versions of MacOS.

### Linux

There are packages available for Fedora Linux and NixOS, or you can build the program from source.

#### Fedora
``` sh
sudo dnf copr enable laamaa/m8c
sudo dnf install m8c
```

#### NixOS
``` sh
nix-env -iA m8c-stable -f https://github.com/laamaa/m8c/archive/refs/heads/main.tar.gz
```

### Building from source code

#### Install dependencies

You will need git, gcc, pkg-config, make and the development headers for libsdl2 and libserialport.

##### Linux (Ubuntu)
```
sudo apt update && sudo apt install -y git gcc pkg-config make libsdl2-dev libserialport-dev
```

##### MacOS

This assumes you have [installed brew](https://docs.brew.sh/Installation)
```
brew update && brew install git gcc make sdl2 libserialport pkg-config
```

#### Download source code

```
mkdir code && cd code
git clone https://github.com/laamaa/m8c.git
```

#### Build the program

```
cd m8c
make
```

#### Start the program

Connect the M8 or Teensy (with headless firmware) to your computer and start the program. It should automatically detect your device.

```
./m8c
```

If the stars are aligned correctly, you should see the M8 screen.

-----------

## Keyboard mappings

Keys for controlling the progam:

* Up arrow = up
* Down arrow = down
* Left arrow = left
* Right arrow = right
* z / left shift = shift
* x / space = play
* a / left alt = opt
* s / left ctrl = edit

Additional controls:
* Alt + enter = toggle full screen / windowed
* Alt + F4 = quit program
* Delete = opt+edit (deletes a row)
* Esc = toggle keyjazz on/off 
* r / select+start+opt+edit = reset display (if glitches appear on the screen, use this)

### Keyjazz
Keyjazz allows to enter notes with keyboard, oldschool tracker-style. The layout is two octaves, starting from keys Z and Q.
When keyjazz is active, regular a/s/z/x keys are disabled. The base octave can be adjusted with numpad star/divide keys and the velocity can be set 

* Numpad asterisk (\*): increase base octave
* Numpad divide (/): decrease base ooctave
* Numpad plus (+): increase velocity
* Numpad minus (-): decrease velocity

## Gamepads

The program uses SDL's game controller system, which should make it work automagically with most gamepads. On startup, the program tries to load a SDL game controller database named gamecontrollerdb.txt from the same directory as the config file. If your joypad doesn't work out of the box, you might need to create custom bindings to this file, for example with [SDL2 Gamepad Tool](https://generalarcade.com/gamepadtool/).

## Audio

Experimental audio routing support can be enabled by setting the config value `"audio_enabled"` to `"true"`. The audio buffer size can also be tweaked from the config file for possible lower latencies.
If the right audio device is not picked up by default, you can use a specific audio device by using `"audio_output_device"` config parameter.

## Config

Application settings and keyboard/game controller bindings can be configured via `config.ini`.

If not found, the file will be created in one of these locations:
* Windows: `C:\Users\<username>\AppData\Roaming\m8c\config.ini`
* Linux: `/home/<username>/.local/share/m8c/config.ini`
* MacOS: `/Users/<username>/Library/Application Support/m8c/config.ini`

See the `config.ini.sample` file to see the available options.

Enjoy making some nice music!

-----------

## FAQ

* When starting the program, something like the following appears and the program does not start:
```
$ ./m8c
INFO: Looking for USB serial devices.
INFO: Found M8 in /dev/ttyACM1.
INFO: Opening port.
ERROR: Error: Failed: Permission denied
```

This is likely caused because the user running m8c does not have permission to use the serial port. The eaiest way to fix this is to add the current user to a group with permission to use the serial port.

On Linux systems, look at the permissions on the serial port shown on the line that says "Found M8 in":
```
$ ls -la /dev/ttyACM1
crw-rw---- 1 root dialout 166, 0 Jan  8 14:51 /dev/ttyACM0
```

In this case the serial port is owned by the user 'root' and the group 'dialout'. Both the user and the group have read/write permissions. To add a user to the group, run this command, replacing 'dialout' with the group shown on your own system:

``` sh
sudo adduser $USER dialout
```

You may need to log out and back in or even fully reboot the system for this change to take effect, but this will hopefully fix the problem. Please see [this issue for more details](https://github.com/laamaa/m8c/issues/20).



