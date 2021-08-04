#include "fx_piano.h"
#include "SDL_stdinc.h"
#include "render.h"
#include "SDL2/SDL.h"
#include "SDL_render.h"
#include "SDL_timer.h"

int width = 320;
int height = 240;
SDL_Texture *fx_texture;
uint32_t *framebuffer;
uint32_t *prev_buffer;
uint32_t last_ticks;
#define bgcolor 0xA0000000

uint32_t palette[8] = {0xEFFF0000,0xEF00FF00,0xEF0000FF,0xEFFFFF00,0xEFFF00FF,0xEF00FFFF,0xEFFFFFFF,0xEFFFFFFF};

void fx_piano_init(SDL_Texture *output_texture) {
  fx_texture = output_texture;
  SDL_QueryTexture(fx_texture, NULL, NULL, &width, &height);
  framebuffer = malloc(sizeof(uint32_t) * width * height);
  prev_buffer = malloc(sizeof(uint32_t) * width * height);
  SDL_memset4(framebuffer, bgcolor, width*height);
}

void fx_piano_update() {
  //Shift everything up by one row and clear bottom row
  for (int y = 1; y < height; y++) {
    for (int x = 0; x < width; x++) {
      int buffer_pos = x + (y * width);
      // Top rows have a fading alpha value
      if (y<60 && (framebuffer[buffer_pos] >> 24) > 0x04){
        framebuffer[buffer_pos-width] = framebuffer[buffer_pos]-((0x05 << 24));
      } else {
        framebuffer[buffer_pos-width] = framebuffer[buffer_pos];
      }
      }
    }
  
  for (int x = 0; x < width; x++) {
    framebuffer[width*(height-1)+x] = bgcolor;
    framebuffer[x] = bgcolor;
  }

  int active_notes[8];
  for (int i = 0; i < 8; i++) {
    active_notes[i] = get_active_note_from_channel(i);
    
     int last_row = (height-1)*width;
     if (active_notes[i]>0) {
       int pos = last_row+(active_notes[i]*2.5);
       uint32_t color = (framebuffer[pos] + palette[i]) | (0xCF <<24);
       framebuffer[pos++] = color;
       framebuffer[pos++] = color;
       framebuffer[pos] = color | (0x20 << 24);
     }
   }

  SDL_UpdateTexture(fx_texture, NULL, framebuffer,
                    width * sizeof(framebuffer[0]));
};

void fx_piano_destroy() { free(framebuffer); }