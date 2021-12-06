# m8c

m8c is a client for Dirtywave M8 tracker's headless mode. The application should be cross-platform ready and can be built in Linux, Windows (with MSYS2/MINGW64) and Mac OS.

Please note that routing the headless M8 USB audio isn't in the scope of this program -- if this is needed, it can be achieved with tools like jackd, alsa\_in and alsa\_out for example. Check out the guide in file AUDIOGUIDE.md for some instructions on routing the audio.

Many thanks to:

Trash80 for the great M8 hardware and the original font (stealth57.ttf) that was converted to a bitmap for use in the progam.

driedfruit for a wonderful little routine to blit inline bitmap fonts, https://github.com/driedfruit/SDL_inprint/

marcinbor85 for the slip handling routine, https://github.com/marcinbor85/slip

turbolent for the great Golang-based g0m8 application, which I used as reference on how the M8 serial protocol works.

Disclaimer: I'm not a coder and hardly understand C, use at your own risk :)

-------

## Installation

These instructions are tested with Raspberry Pi 3 B+ and Raspberry Pi OS with desktop (March 4 2021 release), but should apply for other Debian/Ubuntu flavors as well. The begining on the build process on OSX is slightly different at the start, and then the same once packages are installed.

The instructions assume that you already have a working Linux desktop installation with an internet connection.

Open Terminal and run the following commands:

### Install required packages (Raspberry Pi, Linux)

```
sudo apt update && sudo apt install -y git gcc make libsdl2-dev libserialport-dev
```
### Install required packages (OSX)

This assumes you have [installed brew](https://docs.brew.sh/Installation)

```
brew update && brew install -y git gcc make sdl2 libserialport
```
### Download source code (All)

```
mkdir code && cd code
git clone https://github.com/laamaa/m8c.git
 ```

### Build the program

```
cd m8c
make
 ```

### Start the program

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
* a / left shift = select
* s / space = start
* z / left alt = opt
* x / left ctrl = edit

Additional controls:
* Alt + enter = toggle full screen / windowed
* Alt + F4 = quit program
* Delete = opt+edit (deletes a row)
* Esc = toggle keyjazz on/off 
* r / select+start+opt+edit = reset display (if glitches appear on the screen, use this)
* F12 = cycle visual fx
* F11 = enable/disable gl shader

Keyjazz allows to enter notes with keyboard, oldschool tracker-style. The layout is two octaves, starting from keys Z and Q.
When keyjazz is active, regular a/s/z/x keys are disabled.

## Gamepads

The program uses SDL's game controller system, which should make it work automagically with most gamepads.

## GL Shaders

There is a possibility to use a GL shader to spice up your headless experience. You can enable/disable it by pressing F11.

If you wish to customize the shader, you can edit crt.fragment / std.vertex files or add new ones and edit initialize_sdl() in render.c accordingly.

Enjoy making some nice music!

### Config

Keyboard and game controller bindings can be configured via `config.ini`.

If not found, the file will be created in one of these locations:
Windows: C:\Users\<username>\AppData\Roaming\m8c\config.ini
Linux: /home/<username>/.local/share/m8c/config.ini
MacOS: /Users/<username>/Library/Application Support/m8c/config.ini

See the `config.ini.sample` file to see the available options.

-----------

## FAQ

* When starting the program, something like the following appears and the program does not start:
```
$ ./m8c
INFO: Looking for USB serial devices.
INFO: Found M8 in /dev/ttyACM1.
INFO: Opening port.
Aborted (core dumped)
```

This is likely an issue with the version of libserialport included in your Linux distribution and you need to build the library yourself. Please see [this issue for a workaround](https://github.com/laamaa/m8c/issues/20).

-----------

### Bonus content: quickly install m8c locally with nix

``` sh
nix-env -iA m8c-stable -f https://github.com/laamaa/m8c/archive/refs/heads/main.tar.gz
```
