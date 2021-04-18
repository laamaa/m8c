# m8c

m8c is a Linux client for Dirtywave M8 tracker's headless mode. It is intended for use with lower end systems like RPi zero.

Please note that routing the headless M8 USB audio isn't in the scope of this program -- if this is needed, it can be achieved with tools like jackd, alsa\_in and alsa\_out for example.

Many thanks to turbolent for the great Golang-based g0m8 application, which I used as reference on how the M8 serial protocol works.

Disclaimer: I'm not a coder and hardly understand C, use at your own risk :)

## Installation

These instructions are tested with Raspberry Pi 3 B+ and Raspberry Pi OS with desktop (March 4 2021 release), but should apply for other Debian/Ubuntu flavors as well.

The instructions assume that you already have a working Linux desktop installation with an internet connection.

Open Terminal and run the following commands:

### Install required packages

>
```
sudo apt update && sudo apt install -y git gcc make libsdl2-dev libsdl2-ttf-dev
```

### Download source code
>
```
mkdir code && cd code
git clone https://github.com/laamaa/m8c.git
 ```

### Build the program
>
```
cd m8c
make && sudo make install
 ```

### Find out the correct device name

Connect the Teensy to your computer and look up the device name:
>
```
sudo dmesg | grep ttyACM
```

This should output something like:

```
pi@raspberrypi:~/code/m8c $ sudo dmesg | grep ttyACM
[    8.129649] cdc_acm 1-1.2:1.0: ttyACM0: USB ACM device
```
Note the ttyACM0 part -- this is the device name.

### Start the program
```
m8c /dev/<YOUR DEVICE NAME>
```


for example:

```
m8c /dev/ttyACM0
```

If the stars are aligned correctly, you should see the M8 screen.

Keys for controlling the progam:

* Up arrow / i = up
* Down arrow / k = down
* Left arrow / j = left
* Right arrow / l = right
* a = select
* s = start
* z = opt
* x = edit

You can toggle full screen mode with ALT+Enter and quit the program with CTRL+q.

There is also a so-called gamepad support, currently it's working only if there's only one pad connected and it's a Retrobit NES-style controller, since the settings are hardcoded.

Enjoy making some nice music!

-----------

### Bonus: improve performance on the Raspberry Pi
Enabling the experimental GL Driver with Full KMS can boost the program's performance a bit.

The driver can be enabled with ```sudo raspi-config``` and selecting "Advanced options" -> "GL Driver" -> "GL (Full KMS)" and rebooting.

Please note that with some configurations (for example, composite video) this can lead to not getting video output at all. If that happens, you can delete the row ```dtoverlay=vc4-kms-v3d``` in bottom of /boot/config.txt.
