#Set all your object files (the object files of all the .c files in your project, e.g. main.o my_sub_functions.o )
OBJ = src/main.o \
src/audio.o \
src/command.o \
src/config.o \
src/fx_cube.o \
src/gamecontrollers.o \
src/ini.o \
src/inprint2.o \
src/input.o \
src/backends/queue.o \
src/render.o \
src/backends/rtmidi.o \
src/backends/ringbuffer.o \
src/backends/serialport.o \
src/backends/slip.o \
src/backends/usb.o \
src/backends/usb_audio.o \

#Set any dependant header files so that if they are edited they cause a complete re-compile (e.g. main.h some_subfunctions.h some_definitions_file.h ), or leave blank
DEPS = src/audio.h \
src/command.h \
src/config.h \
src/fx_cube.h \
src/gamecontrollers.h \
src/ini.h \
src/input.h \
src/render.h \
src/backends/ringbuffer.h \
src/backends/rtmidi.h \
src/backends/queue.h \
src/backends/serialport.h \
src/backends/slip.h \
src/fonts/inline_font.h

#Any special libraries you are using in your project (e.g. -lbcm2835 -lrt `pkg-config --libs gtk+-3.0` ), or leave blank
INCLUDES = $(shell pkg-config --libs sdl3 libserialport | sed 's/-mwindows//')

#Set any compiler flags you want to use (e.g. -I/usr/include/somefolder `pkg-config --cflags gtk+-3.0` ), or leave blank
local_CFLAGS = $(CFLAGS) $(shell pkg-config --cflags sdl3 libserialport) -DUSE_LIBSERIALPORT -Wall -Wextra -O2 -pipe -I. -DNDEBUG

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

libusb: INCLUDES = $(shell pkg-config --libs sdl3 libusb-1.0)
libusb: local_CFLAGS = $(CFLAGS) $(shell pkg-config --cflags sdl3 libusb-1.0) -Wall -Wextra -O2 -pipe -I. -DUSE_LIBUSB=1 -DNDEBUG
libusb: m8c

rtmidi: INCLUDES = $(shell pkg-config --libs sdl3 rtmidi)
rtmidi: local_CFLAGS = $(CFLAGS) $(shell pkg-config --cflags sdl3 rtmidi) -Wall -Wextra -O2 -pipe -I. -DUSE_RTMIDI -DNDEBUG
rtmidi: m8c

#Cleanup
.PHONY: clean

clean:
	rm -f src/*.o src/backends/*.o *~ m8c

# PREFIX is environment variable, but if it is not set, then set default value
ifeq ($(PREFIX),)
    PREFIX := /usr/local
endif

install: m8c
	install -d $(DESTDIR)$(PREFIX)/bin/
	install -m 755 m8c $(DESTDIR)$(PREFIX)/bin/
