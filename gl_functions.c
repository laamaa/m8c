#include "gl_functions.h"

#include "SDL_log.h"
#include "SDL_rwops.h"
#include "SDL_stdinc.h"
#include "SDL_timer.h"
#include "SDL_video.h"
#include <SDL2/SDL.h>
#include <SDL_opengl.h>
#include <SDL_opengl_glext.h>

/* Original C++ code from https://github.com/AugustoRuiz/sdl2glsl, ported to C
 */

PFNGLCREATESHADERPROC glCreateShader;
PFNGLSHADERSOURCEPROC glShaderSource;
PFNGLCOMPILESHADERPROC glCompileShader;
PFNGLGETSHADERIVPROC glGetShaderiv;
PFNGLGETSHADERINFOLOGPROC glGetShaderInfoLog;
PFNGLDELETESHADERPROC glDeleteShader;
PFNGLATTACHSHADERPROC glAttachShader;
PFNGLCREATEPROGRAMPROC glCreateProgram;
PFNGLLINKPROGRAMPROC glLinkProgram;
PFNGLVALIDATEPROGRAMPROC glValidateProgram;
PFNGLGETPROGRAMIVPROC glGetProgramiv;
PFNGLGETPROGRAMINFOLOGPROC glGetProgramInfoLog;
PFNGLUSEPROGRAMPROC glUseProgram;
PFNGLUNIFORM1FPROC glUniform1f;
PFNGLUNIFORM2FVPROC glUniform2fv;
PFNGLGETUNIFORMLOCATIONPROC glGetUniformLocation;
PFNGLGETACTIVEATTRIBPROC glGetActiveAttrib;
PFNGLGETACTIVEUNIFORMPROC glGetActiveUniform;
PFNGLENABLEVERTEXATTRIBARRAYPROC glEnableVertexAttribArray;
PFNGLVERTEXATTRIBPOINTERPROC glVertexAttribPointer;

GLint uniform_time;   // Global time

float fTime(void) {

  static Uint64 start = 0;
  static Uint64 frequency = 0;

  if (start == 0) {
    start = SDL_GetPerformanceCounter();
    frequency = SDL_GetPerformanceFrequency();
    return 0.0f;
  }

  Uint64 counter = 0;
  counter = SDL_GetPerformanceCounter();
  Uint64 accumulate = counter - start;
  return (float)accumulate / (float)frequency;
}

char *file_read(const char *filename) {
  SDL_RWops *rw = SDL_RWFromFile(filename, "rb");
  if (rw == NULL)
    return NULL;

  Sint64 res_size = SDL_RWsize(rw);
  char *res = (char *)malloc(res_size + 1);

  Sint64 nb_read_total = 0, nb_read = 1;
  char *buf = res;
  while (nb_read_total < res_size && nb_read != 0) {
    nb_read = SDL_RWread(rw, buf, 1, (res_size - nb_read_total));
    nb_read_total += nb_read;
    buf += nb_read;
  }
  SDL_RWclose(rw);
  if (nb_read_total != res_size) {
    free(res);
    return NULL;
  }

  res[nb_read_total] = '\0';
  return res;
}

int init_gl_extensions() {
  glCreateShader =
      (PFNGLCREATESHADERPROC)SDL_GL_GetProcAddress("glCreateShader");
  glShaderSource =
      (PFNGLSHADERSOURCEPROC)SDL_GL_GetProcAddress("glShaderSource");
  glCompileShader =
      (PFNGLCOMPILESHADERPROC)SDL_GL_GetProcAddress("glCompileShader");
  glGetShaderiv = (PFNGLGETSHADERIVPROC)SDL_GL_GetProcAddress("glGetShaderiv");
  glGetShaderInfoLog =
      (PFNGLGETSHADERINFOLOGPROC)SDL_GL_GetProcAddress("glGetShaderInfoLog");
  glDeleteShader =
      (PFNGLDELETESHADERPROC)SDL_GL_GetProcAddress("glDeleteShader");
  glAttachShader =
      (PFNGLATTACHSHADERPROC)SDL_GL_GetProcAddress("glAttachShader");
  glCreateProgram =
      (PFNGLCREATEPROGRAMPROC)SDL_GL_GetProcAddress("glCreateProgram");
  glLinkProgram = (PFNGLLINKPROGRAMPROC)SDL_GL_GetProcAddress("glLinkProgram");
  glValidateProgram =
      (PFNGLVALIDATEPROGRAMPROC)SDL_GL_GetProcAddress("glValidateProgram");
  glGetProgramiv =
      (PFNGLGETPROGRAMIVPROC)SDL_GL_GetProcAddress("glGetProgramiv");
  glGetProgramInfoLog =
      (PFNGLGETPROGRAMINFOLOGPROC)SDL_GL_GetProcAddress("glGetProgramInfoLog");
  glUseProgram = (PFNGLUSEPROGRAMPROC)SDL_GL_GetProcAddress("glUseProgram");
  glUniform1f = (PFNGLUNIFORM1FPROC)SDL_GL_GetProcAddress("glUniform1f");
  glUniform2fv = (PFNGLUNIFORM2FVPROC)SDL_GL_GetProcAddress("glUniform2fv");
  glGetUniformLocation = (PFNGLGETUNIFORMLOCATIONPROC)SDL_GL_GetProcAddress(
      "glGetUniformLocation");
  glGetActiveAttrib =
      (PFNGLGETACTIVEATTRIBPROC)SDL_GL_GetProcAddress("glGetActiveAttrib");
  glGetActiveUniform =
      (PFNGLGETACTIVEUNIFORMPROC)SDL_GL_GetProcAddress("glGetActiveUniform");
  glEnableVertexAttribArray =
      (PFNGLENABLEVERTEXATTRIBARRAYPROC)SDL_GL_GetProcAddress(
          "glEnableVertexAttribArray");
  glVertexAttribPointer = (PFNGLVERTEXATTRIBPOINTERPROC)SDL_GL_GetProcAddress(
      "glVertexAttribPointer");

  return glCreateShader && glShaderSource && glCompileShader && glGetShaderiv &&
         glGetShaderInfoLog && glDeleteShader && glAttachShader &&
         glCreateProgram && glLinkProgram && glValidateProgram &&
         glGetProgramiv && glGetProgramInfoLog && glUseProgram && glUniform1f &&
         glUniform2fv && glGetUniformLocation && glGetActiveAttrib &&
         glGetActiveUniform && glEnableVertexAttribArray && glVertexAttribPointer;
}

GLuint compile_shader(const char *source, GLuint shaderType) {
  SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "Compiling shader: \n %s\n",
               source);
  // Create ID for shader
  GLuint result = glCreateShader(shaderType);
  // Define shader text
  glShaderSource(result, 1, &source, NULL);
  // Compile shader
  glCompileShader(result);

  // Check vertex shader for errors
  GLint shaderCompiled = GL_FALSE;
  glGetShaderiv(result, GL_COMPILE_STATUS, &shaderCompiled);
  if (shaderCompiled != GL_TRUE) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Shader compile error: %d\n",
                 result);
    GLint logLength;
    glGetShaderiv(result, GL_INFO_LOG_LENGTH, &logLength);
    if (logLength > 0) {
      GLchar *log = (GLchar *)malloc(logLength);
      glGetShaderInfoLog(result, logLength, &logLength, log);
      SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Shader compile log: %s\n",
                   log);
      free(log);
    }
    glDeleteShader(result);
    result = 0;
  } else {
    SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION,
                 "Shader compiled succesfully. Id = %d", result);
  }
  return result;
}

GLuint compile_program(const char *vtx_file, const char *frag_file) {
  GLuint program_id = 0;
  GLuint vtxShaderId, fragShaderId;

  program_id = glCreateProgram();

  vtxShaderId = compile_shader(file_read(vtx_file), GL_VERTEX_SHADER);
  fragShaderId = compile_shader(file_read(frag_file), GL_FRAGMENT_SHADER);

  if (vtxShaderId && fragShaderId) {
    // Associate shader with program
    glAttachShader(program_id, vtxShaderId);
    glAttachShader(program_id, fragShaderId);
    glLinkProgram(program_id);
    glValidateProgram(program_id);

    // Check the status of the compile/link
    GLint log_len;
    glGetProgramiv(program_id, GL_INFO_LOG_LENGTH, &log_len);
    if (log_len > 0) {
      char *log = (char *)malloc(log_len * sizeof(char));
      // Show any errors as appropriate
      glGetProgramInfoLog(program_id, log_len, &log_len, log);
      SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "Prog Info Log:\n%s", log);
      free(log);
    }
  }
  if (vtxShaderId) {
    glDeleteShader(vtxShaderId);
  }
  if (fragShaderId) {
    glDeleteShader(fragShaderId);
  }
  return program_id;
}

void present_backbuffer(SDL_Renderer *renderer, SDL_Window *win,
                        SDL_Texture *backBuffer, GLuint program_id) {
  GLint oldprogram_id;

  // Detach the texture
  SDL_SetRenderTarget(renderer, NULL);
  SDL_RenderClear(renderer);

  SDL_GL_BindTexture(backBuffer, NULL, NULL);
  if (program_id != 0) {
    glUniform1f(uniform_time, fTime()/1000.0);
    glGetIntegerv(GL_CURRENT_PROGRAM, &oldprogram_id);
    glUseProgram(program_id);
  }

  GLfloat minx, miny, maxx, maxy;
  GLfloat minu, maxu, minv, maxv;

  int win_width, win_height;

  SDL_GetWindowSize(win, &win_width, &win_height);

  minx = 0.0f;
  miny = 0.0f;
  maxx = win_width;
  maxy = win_height;

  minu = 0.0f;
  maxu = 1.0f;
  minv = 0.0f;
  maxv = 1.0f;

  int outputWidth, outputHeight;

  SDL_QueryTexture(backBuffer, NULL, NULL, &outputWidth, &outputHeight);

  if (program_id != 0) {
    GLint rubyTextureSize = glGetUniformLocation(program_id, "rubyTextureSize");
    GLint uniform_time = glGetUniformLocation(program_id,"time");
    GLfloat outputSize[2] = {outputWidth, outputHeight};
    glUniform2fv(rubyTextureSize, 1, outputSize);
    glUniform1f(uniform_time,SDL_GetTicks()/1000.0);
  }

  glBegin(GL_TRIANGLE_STRIP);
  glTexCoord2f(minu, minv);
  glVertex2f(minx, miny);
  glTexCoord2f(maxu, minv);
  glVertex2f(maxx, miny);
  glTexCoord2f(minu, maxv);
  glVertex2f(minx, maxy);
  glTexCoord2f(maxu, maxv);
  glVertex2f(maxx, maxy);
  glEnd();
  SDL_GL_SwapWindow(win);

  if (program_id != 0) {
    glUseProgram(oldprogram_id);
  }
}
