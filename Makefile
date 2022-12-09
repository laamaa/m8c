#Set all your object files (the object files of all the .c files in your project, e.g. main.o my_sub_functions.o )
OBJ = main.o serial.o slip.o command.o render.o ini.o config.o input.o font.o fx_cube.o

#Set any dependant header files so that if they are edited they cause a complete re-compile (e.g. main.h some_subfunctions.h some_definitions_file.h ), or leave blank
DEPS = serial.h slip.h command.h render.h ini.h config.h input.h fx_cube.h

#Any special libraries you are using in your project (e.g. -lbcm2835 -lrt `pkg-config --libs gtk+-3.0` ), or leave blank
INCLUDES = $(shell pkg-config --libs sdl2 libserialport)



#Set any compiler flags you want to use (e.g. -I/usr/include/somefolder `pkg-config --cflags gtk+-3.0` ), or leave blank
local_CFLAGS = $(CFLAGS) $(shell pkg-config --cflags sdl2 libserialport) -Wall -O2 -pipe -I.

#Set the compiler you are using ( gcc for C or g++ for C++ )
CC = gcc

#Set the filename extensiton of your C files (e.g. .c or .cpp )
EXTENSION = .c

#define a rule that applies to all files ending in the .o suffix, which says that the .o file depends upon the .c version of the file and all the .h files included in the DEPS macro.  Compile each object file
%.o: %$(EXTENSION) $(DEPS)
	$(CC) -c -o $@ $< $(local_CFLAGS)

#Combine them into the output file
#Set your desired exe output file name here
m8c: $(OBJ)
	$(CC) -o $@ $^ $(local_CFLAGS) $(INCLUDES)

font.c: inline_font.h
	@echo "#include <SDL.h>" > $@-tmp1
	@cat inline_font.h >> $@-tmp1
	@cat inprint2.c > $@-tmp2
	@sed '/#include/d' $@-tmp2 >> $@-tmp1
	@rm $@-tmp2
	@mv $@-tmp1 $@
	@echo "[~cat] inline_font.h inprint2.c > font.c"
#	$(CC) -c -o font.o font.c $(local_CFLAGS)

#Cleanup
.PHONY: clean

clean:
	rm -f *.o *~ m8c *~ font.c

# PREFIX is environment variable, but if it is not set, then set default value
ifeq ($(PREFIX),)
    PREFIX := /usr/local
endif

install: m8c
	install -d $(DESTDIR)$(PREFIX)/bin/
	install -m 755 m8c $(DESTDIR)$(PREFIX)/bin/
