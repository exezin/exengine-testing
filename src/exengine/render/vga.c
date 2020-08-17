#include "vga.h"
#include "vga_font.h"
#include "shader.h"
#include "window.h"
#include "math/mathlib.h"
#include <stdio.h>

#define EX_VGA_FONT_WIDTH  8
#define EX_VGA_FONT_HEIGHT 16
#define EX_VGA_WIDTH  256
#define EX_VGA_HEIGHT 256

uint32_t *ex_vga_data = NULL;
uint32_t ex_vga_fg = 0xFFFFFFFF, ex_vga_bg = 0x00000000;
size_t ex_vga_len = 0;
GLuint vga_texture, vga_shader, vga_vao, vga_vbo;
GLfloat vga_vertices[24];
mat4x4 vga_projection;

void ex_vga_init()
{
  if (ex_vga_data) {
    free(ex_vga_data);
  } else {
    // gen texture
    glGenTextures(1, &vga_texture);
    glBindTexture(GL_TEXTURE_2D, vga_texture);
    glTexImage2D(GL_TEXTURE_2D, 0,
      GL_RGBA, EX_VGA_WIDTH, EX_VGA_HEIGHT, 0,
      GL_RGBA, GL_UNSIGNED_INT_8_8_8_8_REV, NULL);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);

    // load shader
    vga_shader = ex_shader("vga.glsl");

    // set up vao, vbo etc
    float w = EX_VGA_WIDTH;
    float h = EX_VGA_HEIGHT;
    float u0 = 0.0f, v0 = 0.0f;
    float u1 = 1.0, v1 = 1.0;
    GLfloat v[] = {
      // pos          // uv
      0.0f,   0.0f+h, u0,  v1,
      0.0f,   0.0f,   u0,  v0,
      0.0f+w, 0.0f,   u1,  v0,
      0.0f,   0.0f+h, u0,  v1,
      0.0f+w, 0.0f,   u1,  v0,
      0.0f+w, 0.0f+h, u1,  v1
    };
    memcpy(vga_vertices, v, sizeof(GLfloat)*24);

    glGenVertexArrays(1, &vga_vao);
    glGenBuffers(1, &vga_vbo);

    uint32_t stride = sizeof(GLfloat) * 4;

    glBindVertexArray(vga_vao);
    glBindBuffer(GL_ARRAY_BUFFER, vga_vbo);

    glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat)*24, vga_vertices, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride, (GLvoid*)0);

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, (GLvoid*)(sizeof(GLfloat) * 2));

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
  }

  // calculate vga data len
  ex_vga_len = EX_VGA_FONT_WIDTH * EX_VGA_FONT_HEIGHT;
  ex_vga_len *= EX_VGA_WIDTH * EX_VGA_HEIGHT;

  // malloc a large buffer for the pixel data
  ex_vga_data = malloc(sizeof(uint32_t) * ex_vga_len);
  memset(ex_vga_data, 0, sizeof(uint32_t) * ex_vga_len);
}

void ex_vga_print(size_t x, size_t y, const char *str)
{
  x *= EX_VGA_FONT_WIDTH;
  y *= EX_VGA_FONT_HEIGHT;

  uint8_t *font = ex_vga_font_array;
  for (int i=0; i<strlen(str); i++) {
    size_t c = (str[i]*16);

    if (c > EX_VGA_FONT_DATA_LEN)
      c = 0;

    if (x >= EX_VGA_WIDTH * EX_VGA_FONT_WIDTH) {
      x = 0;
      y += EX_VGA_FONT_HEIGHT;
    }
    if (y >= EX_VGA_HEIGHT * EX_VGA_FONT_HEIGHT)
      y = 0;

    for (int j=0; j<16; j++) {
      size_t p = (EX_VGA_WIDTH * y) + x;
      p += (EX_VGA_WIDTH * j);
      uint8_t byte = font[c+j];

      for (int k=0; k<8; k++) {
        int color = (byte >> k) & 0x01;
        if (color)
          ex_vga_data[p+(8-k)] = ex_vga_fg;
        else
          ex_vga_data[p+(8-k)] = ex_vga_bg;
      }
    }

    x += EX_VGA_FONT_WIDTH;
  }
}

void ex_vga_render()
{
  mat4x4_ortho(vga_projection, 0.0f, display.width, display.height, 0.0f, -1.0f, 1.0f);
  glViewport(0, 0, display.width, display.height);

  glBindTexture(GL_TEXTURE_2D, vga_texture);
  glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0,
    EX_VGA_WIDTH, EX_VGA_HEIGHT,
    GL_RGBA, GL_UNSIGNED_INT_8_8_8_8_REV, ex_vga_data);

  glUseProgram(vga_shader);
  glBindVertexArray(vga_vao);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, vga_texture);
  glUniform1i(ex_uniform(vga_shader, "u_texture"), 0);
  glUniformMatrix4fv(ex_uniform(vga_shader, "u_projection"), 1, GL_FALSE, vga_projection[0]);
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_CULL_FACE);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glDrawArrays(GL_TRIANGLES, 0, 6);
  glDisable(GL_BLEND);

  glBindVertexArray(0);
  glBindTexture(GL_TEXTURE_2D, 0);
}

void ex_vga_clear()
{
  memset(ex_vga_data, 0, sizeof(uint32_t) * ex_vga_len);
}

void ex_vga_setfg(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
  ex_vga_fg = r;
  ex_vga_fg |= ((uint32_t)g) << 8;
  ex_vga_fg |= ((uint32_t)b) << 16;
  ex_vga_fg |= ((uint32_t)a) << 24;
}

void ex_vga_setbg(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
  ex_vga_bg = r;
  ex_vga_bg |= ((uint32_t)g) << 8;
  ex_vga_bg |= ((uint32_t)b) << 16;
  ex_vga_bg |= ((uint32_t)a) << 24;
}

void ex_vga_destroy()
{
  if (ex_vga_data) {
    free(ex_vga_data);

    glDeleteVertexArrays(1, &vga_vao);
    glDeleteBuffers(1, &vga_vbo);
  }
}