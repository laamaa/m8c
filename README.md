# m8c

m8c is a Linux client for Dirtywave M8 tracker's headless mode. It is intended for use with lower end systems like RPi zero. The application should be cross-platform ready and can be built in Mac OS and Windows (with MSYS2/MINGW64).

Please note that routing the headless M8 USB audio isn't in the scope of this program -- if this is needed, it can be achieved with tools like jackd, alsa\_in and alsa\_out for example. Check out the guide in file AUDIOGUIDE.md for some instructions on routing the audio.

Many thanks to:

Trash80 for the great M8 hardware and the original font (stealth57.ttf) that was converted to a bitmap for use in the progam.

driedfruit for a wonderful little routine to blit inline bitmap fonts, https://github.com/driedfruit/SDL_inprint/

marcinbor85 for the slip handling routine, https://github.com/marcinbor85/slip

turbolent for the great Golang-based g0m8 application, which I used as reference on how the M8 serial protocol works.

Disclaimer: I'm not a coder and hardly understand C, use at your own risk :)

-------

## Installation

These instructions are tested with Raspberry Pi 3 B+ and Raspberry Pi OS with desktop (March 4 2021 release), but should apply for other Debian/Ubuntu flavors as well.

The instructions assume that you already have a working Linux desktop installation with an internet connection.

Open Terminal and run the following commands:

### Install required packages

```
sudo apt update && sudo apt install -y git gcc make libsdl2-dev libserialport-dev
```

### Download source code

```
mkdir code && cd code
git clone https://github.com/laamaa/m8c.git
 ```

### Build the program

```
cd m8c
make && sudo make install
 ```

### Start the program

Connect the Teensy to your computer and start the program. It should automatically detect your device.

```
m8c
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

Keyjazz allows to enter notes with keyboard, oldschool tracker-style. The layout is two octaves, starting from keys Z and Q.
When keyjazz is active, regular a/s/z/x keys are disabled.

There is also a so-called gamepad support, currently it's working only if there's only one pad connected and it's a Retrobit NES-style controller, since the settings are hardcoded.

Enjoy making some nice music!

-----------

### Bonus: improve performance on the Raspberry Pi
Enabling the experimental GL Driver with Full KMS can boost the program's performance a bit.

The driver can be enabled with ```sudo raspi-config``` and selecting "Advanced options" -> "GL Driver" -> "GL (Full KMS)" and rebooting.

Please note that with some configurations (for example, composite video) this can lead to not getting video output at all. If that happens, you can delete the row ```dtoverlay=vc4-kms-v3d``` in bottom of /boot/config.txt.

Further performance improvement can be achieved by not using X11 and running the program directly in framebuffer console, but this might require doing a custom build of SDL.
