#include <string.h>
#include <stdio.h>
#include "render/text.h"
#include "render/shader.h"
#include "util/io.h"
#include "math/mathlib.h"

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

#define SIZE 32
#define ASIZE 34 // atlas size, +2 for padding

// defines the constant offset to the aa value
// increase to sharpen, decrease (down to 0) to smooth/blur
#define AA_BIAS 0.5

GLuint text_shader, text_vao, text_vbo;
mat4x4 projection;

void ex_font_init()
{
  text_shader = ex_shader("text.glsl");

  glGenVertexArrays(1, &text_vao);
  glGenBuffers(1, &text_vbo);

  glBindVertexArray(text_vao);
  glBindBuffer(GL_ARRAY_BUFFER, text_vbo);
  glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * 24, NULL, GL_DYNAMIC_DRAW);

  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(GLfloat)*4, (GLvoid*)0);
  glEnableVertexAttribArray(0);

  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(GLfloat)*4, (GLvoid*)(2 * sizeof(GLfloat)));
  glEnableVertexAttribArray(1);

  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindVertexArray(0);

  mat4x4_ortho(projection, 0.0f, 1280.0f, 0.0f, 720.0f, 0.0f, 1.0f);
}

ex_font_t* ex_font_load(const char *path, const char *letters)
{
  // load the ttf data
  size_t length;
  uint8_t *data = (uint8_t*)ex_io_read(path, "rb", &length);
  if (!data) {
    printf("[TEXT] Failed generating font %s\n", path);
    return NULL;
  }

  // process the ttf data
  stbtt_fontinfo font;
  stbtt_InitFont(&font, (const uint8_t*)data, stbtt_GetFontOffsetForIndex(data,0));

  size_t count      = strlen(letters);
  size_t atlas_size = ceil(sqrt(count))*ASIZE;
  size_t byte_count = sizeof(float)*(atlas_size*atlas_size)*3;
  float *atlas      = malloc(byte_count);
  memset(atlas, 0.0f, byte_count);

  ex_font_t *f = malloc(sizeof(ex_font_t));
  f->uv        = malloc(sizeof(float)*count*12);
  f->metrics   = malloc(sizeof(ex_metrics_t)*count);

  ex_metrics_t metrics;
  char *character = (char*)letters;
  size_t x = 1, y = 1, index = 0;
  while (*character != '\0') {
    char c = *character++;
    float *bitmap = ex_msdf_glyph(&font, ex_utf8(&c), SIZE, SIZE, &metrics);

    // blit msdf bitmap to atlas
    for (int i=0; i<SIZE; i++) {
      size_t pixel = 3*((y*atlas_size)+x+(i*atlas_size));
      memcpy(&atlas[pixel], &bitmap[3*(i*SIZE)], sizeof(float)*SIZE*3);
    }

    // store metrics for each glyph
    memcpy(&f->metrics[index], &metrics, sizeof(ex_metrics_t));

    // pre-calculate texture coordinates
    float x0 = x/(float)atlas_size;
    float y0 = y/(float)atlas_size;
    float x1 = x0+(SIZE/(float)atlas_size);
    float y1 = y0+(SIZE/(float)atlas_size);
    float uvs[] = {
      x0, y0,
      x0, y1,
      x1, y1,
      x0, y0,
      x1, y1,
      x1, y0
    };
    memcpy(&f->uv[index*12], &uvs[0], sizeof(float)*12);
    f->indices[index] = c;
    index++;

    // advance to next tile in atlas
    x += ASIZE;
    if (x >= atlas_size) {
      x = 0;
      y += ASIZE;
    }

    free(bitmap);
  }

  glGenTextures(1, &f->texture);
  glBindTexture(GL_TEXTURE_2D, f->texture);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, 0);

  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB32F, atlas_size, atlas_size, 0, GL_RGB, GL_FLOAT, atlas);

  glBindTexture(GL_TEXTURE_2D, 0);

  free(data);
  free(atlas);
  printf("[TEXT] Done generating msdf atlas for font %s\n", path);

  return f;
}

void ex_font_dbg(ex_font_t *f)
{
  char *str = "Heljo World! this is a test";
  // float w = 16.0f, h = 16.0f;
  float w = 128.0f, h = 128.0f;
  double t = (double)SDL_GetPerformanceCounter() / (double)SDL_GetPerformanceFrequency();
  w += 112.0f * cos(t * 0.5);
  h += 112.0f * cos(t * 0.5);

  glUseProgram(text_shader);
  glBindVertexArray(text_vao);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, f->texture);
  glUniform1i(ex_uniform(text_shader, "u_texture"), 0);
  glUniform1f(ex_uniform(text_shader, "u_scale"), (w/SIZE) * AA_BIAS);
  glUniformMatrix4fv(ex_uniform(text_shader, "u_projection"), 1, GL_FALSE, projection[0]);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glViewport(0, 0, 1280, 720);

  float x = 32.0f, y = 0.0f, sy = 32.0f;
  char *character = (char*)str;
  while (*character != '\0') {
    char c = *character++;

    if (c == ' ') {
      x += w/4;
      continue;
    }

    // find uv array index for char c
    size_t index = 0, iuv = 0;
    for (int i=0; i<MAX_GLYPH; i++) {
      if (f->indices[i] == c) {
        index = i;
        iuv   = i*12;
        break;
      }
    }

    y = sy;
    y -= 16-((f->metrics[index].iy1 - f->metrics[index].iy0)/2);
    y += f->metrics[index].iy0;
    y *= (h/SIZE);

    GLfloat vertices[] = {
      // pos     // uv
      x  , y+h,  f->uv[iuv+0],  f->uv[iuv+1],
      x  , y  ,  f->uv[iuv+2],  f->uv[iuv+3],
      x+w, y  ,  f->uv[iuv+4],  f->uv[iuv+5],
      x  , y+h,  f->uv[iuv+6],  f->uv[iuv+7],
      x+w, y  ,  f->uv[iuv+8],  f->uv[iuv+9],
      x+w, y+h,  f->uv[iuv+10], f->uv[iuv+11]
    };

    glBindBuffer(GL_ARRAY_BUFFER, text_vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glDrawArrays(GL_TRIANGLES, 0, 6);

    x += f->metrics[index].advance*(w/SIZE);

    // if (*character != '\0') {
      // x += stbtt_GetGlyphKernAdvance(&font, stbtt_FindGlyphIndex(&font, c), stbtt_FindGlyphIndex(&font, *character));
    // }
  }

  glBindVertexArray(0);
  glBindTexture(GL_TEXTURE_2D, 0);
}