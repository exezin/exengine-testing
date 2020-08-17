#include "render/window.h"
#include "input/input.h"
#include "engine.h"
#include "glad/glad.h"
#include <stdio.h>

#define DEFAULT_FLAGS SDL_WINDOW_SHOWN | SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE

ex_window_t display;

int ex_window_init(uint32_t width, uint32_t height, const char *title)
{
  if (SDL_Init(SDL_INIT_VIDEO)) {
    printf("Failed to init SDL\n%s", SDL_GetError());
    return 0;
  }

  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
  SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0);
  SDL_GL_SetAttribute(SDL_GL_ACCELERATED_VISUAL, 1);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
  SDL_GL_SetAttribute(SDL_GL_FRAMEBUFFER_SRGB_CAPABLE, 1);

  // create a window
  display.window = SDL_CreateWindow(title,
                                  SDL_WINDOWPOS_CENTERED,
                                  SDL_WINDOWPOS_CENTERED,
                                  width,
                                  height,
                                  DEFAULT_FLAGS);
  if (!display.window) {
    printf("Failed to open SDL window\n%s", SDL_GetError());
    return 0;
  }

  // attempt to setup GL
  display.context = SDL_GL_CreateContext(display.window);
  if (!gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress)) {
    printf("Failed creating GL context");
    return 0;
  }

  // set vsync
  SDL_GL_SetSwapInterval(0);

  // set viewport etc
  glViewport(0, 0, width, height);
  glEnable(GL_DEPTH_TEST);
  glEnable(GL_CULL_FACE);
  glEnable(GL_BLEND);
  glCullFace(GL_BACK);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glDepthFunc(GL_LEQUAL);
  glEnable(GL_FRAMEBUFFER_SRGB);

  // lock mouse
  SDL_SetRelativeMouseMode(SDL_TRUE);
  SDL_CaptureMouse(SDL_TRUE);
  SDL_SetWindowGrab(display.window, SDL_TRUE);

  display.width = width;
  display.height = height;

  return 1;
}

void ex_window_event(SDL_Event *event)
{
  switch (event->window.event) {
    case SDL_WINDOWEVENT_SIZE_CHANGED:
    case SDL_WINDOWEVENT_RESIZED: {
      display.width = event->window.data1;
      display.height = event->window.data2;
      if (ex_resize_ptr)
        ex_resize_ptr(event->window.data1, event->window.data2);
      break;
    }
  }
}

void ex_window_destroy()
{
  // bye bye
  SDL_GL_DeleteContext(display.context);
  SDL_DestroyWindow(display.window);
  SDL_Quit();
}
