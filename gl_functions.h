#ifndef GL_FUNCTIONS_H_
#define GL_FUNCTIONS_H_

#include <SDL2/SDL.h>
#include <SDL_opengl.h>
#include <SDL_opengl_glext.h>

int init_gl_extensions();
GLuint compile_program(const char *vtx_file, const char *frag_file);
void present_backbuffer(SDL_Renderer *renderer, SDL_Window *win,
                        SDL_Texture *backBuffer, GLuint program_id);
void query_gl_vars(GLuint program_id);

#endif