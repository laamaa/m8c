#Set all your object files (the object files of all the .c files in your project, e.g. main.o my_sub_functions.o )
OBJ = src/main.o src/serial.o src/slip.o src/command.o src/render.o src/ini.o src/config.o src/input.o src/gamecontrollers.o src/fx_cube.o src/usb.o src/audio.o src/usb_audio.o src/ringbuffer.o src/inprint2.o src/command_queue.o

#Set any dependant header files so that if they are edited they cause a complete re-compile (e.g. main.h some_subfunctions.h some_definitions_file.h ), or leave blank
DEPS = src/serial.h src/slip.h src/command.h src/render.h src/ini.h src/config.h src/input.h src/gamecontrollers.h src/fx_cube.h src/audio.h src/ringbuffer.h src/inline_font.h src/command_queue.h

#Any special libraries you are using in your project (e.g. -lbcm2835 -lrt `pkg-config --libs gtk+-3.0` ), or leave blank
INCLUDES = $(shell pkg-config --libs sdl2 libserialport | sed 's/-mwindows//')

#Set any compiler flags you want to use (e.g. -I/usr/include/somefolder `pkg-config --cflags gtk+-3.0` ), or leave blank
local_CFLAGS = $(CFLAGS) $(shell pkg-config --cflags sdl2 libserialport) -Wall -Wextra -O2 -pipe -I.

#Set the compiler you are using ( gcc for C or g++ for C++ )
CC = gcc

#Set the filename extensiton of your C files (e.g. .c or .cpp )
EXTENSION = .c

SOURCE_DIR = src/

#define a rule that applies to all files ending in the .o suffix, which says that the .o file depends upon the .c version of the file and all the .h files included in the DEPS macro.  Compile each object file
%.o: %$(EXTENSION) $(DEPS)
	$(CC) -c -o $@ $< $(local_CFLAGS)

#Combine them into the output file
#Set your desired exe output file name here
m8c: $(OBJ)
	$(CC) -o $@ $^ $(local_CFLAGS) $(INCLUDES)

libusb: INCLUDES = $(shell pkg-config --libs sdl2 libusb-1.0)
libusb: local_CFLAGS = $(CFLAGS) $(shell pkg-config --cflags sdl2 libusb-1.0) -Wall -O2 -pipe -I. -DUSE_LIBUSB=1
libusb: m8c

#Cleanup
.PHONY: clean

clean:
	rm -f src/*.o *~ m8c

# PREFIX is environment variable, but if it is not set, then set default value
ifeq ($(PREFIX),)
    PREFIX := /usr/local
endif

install: m8c
	install -d $(DESTDIR)$(PREFIX)/bin/
	install -m 755 m8c $(DESTDIR)$(PREFIX)/bin/
