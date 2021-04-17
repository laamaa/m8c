# m8c

m8c is a Linux client for Dirtywave M8 tracker's headless mode. It is intended for use with lower end systems like RPi zero.

To build, install gcc, make, libsdl2-dev and libsdl2-ttf-dev and run make in the source directory.
This will build a binary that can be launched with ./m8c /dev/ttyACM0 (or whatever your M8 shows up as in the dev system)

Disclaimer: I'm not a coder and hardly understand C, use at your own risk.

Many thanks to turbolent for the great Golang-based g0m8 application, which I used as reference on how the M8 serial protocol works.
