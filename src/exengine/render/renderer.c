#include "render.h"
#include "math/mathlib.h"

// projections
mat4x4 projection_90deg;

// shaders
GLuint pointfbo_shader    = 0;
GLuint forward_shader     = 0;
GLuint framebuffer_shader = 0;

// buffers
GLuint screenquad_vao, screenquad_vbo;

// framebuffers
typedef struct {
  GLuint fbo, rbo, cbo;
  GLuint vao, vbo;
  size_t width, height;
} ex_framebuffer_t;
ex_framebuffer_t framebuffer;

// some default textures
GLuint default_texture_diffuse;
GLuint default_texture_normal;
GLuint default_texture_specular;

void ex_render_init()
{
  /* -- SHADERS -- */
  pointfbo_shader  = ex_shader("pointfbo.glsl");
  framebuffer_shader = ex_shader("fboshader.glsl");

  // set up a 90 degree projection
  // mostly for omni-directional lights
  float aspect = (float)SHADOW_MAP_SIZE/(float)SHADOW_MAP_SIZE;
  mat4x4_perspective(projection_90deg, rad(90.0f), aspect, 0.1f, EX_POINT_FAR_PLANE);
  /* ------------- */


  /* -- DEFAULTS -- */
  GLuint *textures[] = {
    &default_texture_diffuse,
    &default_texture_normal,
    &default_texture_specular
  };

  char colors[][4] = {
    {0,0,0,255},
    {128,127,255,255},
    {0,0,0,255}
  };

  for (int i=0; i<3; i++) {
    glGenTextures(1, textures[i]);
    glBindTexture(GL_TEXTURE_2D, *textures[i]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, &colors[i][0]);
  }
  glBindTexture(GL_TEXTURE_2D, 0);
  /* -------------- */


  /* -- SCREEN QUAD -- */
  GLfloat vertices[] = {
    // pos         // uv
    -1.0f,  1.0f,  0.0f, 1.0f,
    -1.0f, -1.0f,  0.0f, 0.0f,
     1.0f, -1.0f,  1.0f, 0.0f,
    -1.0f,  1.0f,  0.0f, 1.0f,
     1.0f, -1.0f,  1.0f, 0.0f,
     1.0f,  1.0f,  1.0f, 1.0f
  };

  glGenVertexArrays(1, &screenquad_vao);
  glGenBuffers(1, &screenquad_vbo);
  glBindVertexArray(screenquad_vao);

  // vertices
  glBindBuffer(GL_ARRAY_BUFFER, screenquad_vbo);
  glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), &vertices[0], GL_STATIC_DRAW);

  // position
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(GLfloat)*4, (GLvoid*)0);
  glEnableVertexAttribArray(0);

  // tex coords
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(GLfloat)*4, (GLvoid*)(2 * sizeof(GLfloat)));
  glEnableVertexAttribArray(1);
  glBindVertexArray(0);
  /* ----------------- */


  /* -- FRAMEBUFFER -- */
  glGenFramebuffers(1, &framebuffer.fbo);
  glGenRenderbuffers(1, &framebuffer.rbo);
  glGenTextures(1, &framebuffer.cbo);
  framebuffer.vao = screenquad_vao;
  framebuffer.vbo = screenquad_vbo;

  ex_render_resize(display.width, display.height);
  /* ----------------- */
}

void ex_render(ex_renderer_e renderer, ex_renderable_t *renderables)
{
  /* -- LIGHT DEPTH MAPS -- */
  ex_renderlist_t *point_lights = &renderables->point_lights;
  ex_renderlist_t *models       = &renderables->models;

  // render light depth maps
  glCullFace(GL_BACK);
  ex_shader_use(pointfbo_shader);
  for (int i=0; i<point_lights->count; i++) {
    ex_point_light_t *light = (ex_point_light_t*)point_lights->nodes[i].obj;

    if ((light->dynamic || light->update) && light->is_shadow && light->is_visible) {
      ex_render_point_light_begin(light, pointfbo_shader);

      // render all shadow-casting models
      for (int j=0; j<models->count; j++) {
        ex_model_t *model = (ex_model_t*)models->nodes[j].obj;

        if (model->is_shadow)
          ex_render_model(model, pointfbo_shader);
      }
    }

  }
  /* ---------------------- */


  /* -- RENDER PASS -- */
  switch (renderer) {
    case EX_RENDERER_FORWARD: {
      ex_render_forward(renderables);
      break;
    }
    case EX_RENDERER_DEFERRED: {
      break;
    }
  }
  /* ----------------- */
}

void ex_render_forward(ex_renderable_t *renderables)
{
  /* INIT SOME STUFF */
  // render lists
  ex_renderlist_t *point_lights = &renderables->point_lights;
  ex_renderlist_t *models       = &renderables->models;

  // bind main framebuffer
  glViewport(0, 0, framebuffer.width, framebuffer.height);
  glBindFramebuffer(GL_FRAMEBUFFER, framebuffer.fbo);
  glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glEnable(GL_DEPTH_TEST);
  glEnable(GL_CULL_FACE);
  glCullFace(GL_BACK);

  if (!forward_shader)
    forward_shader = ex_shader("forward.glsl");
  /* --------------- */


  /* FIRST PASS */
  ex_shader_use(forward_shader);

  // send camera vars to shader
  ex_camera_matrices_t *camera = renderables->camera;
  glUniformMatrix4fv(ex_uniform(forward_shader, "u_projection"), 1, GL_FALSE, camera->projection[0]);
  glUniformMatrix4fv(ex_uniform(forward_shader, "u_view"), 1, GL_FALSE, camera->view[0]);
  glUniformMatrix4fv(ex_uniform(forward_shader, "u_inverse_view"), 1, GL_FALSE, camera->inverse_view[0]);

  // first pass is ambient
  glDisable(GL_BLEND);
  glCullFace(GL_BACK);

  // do all non shadow casting lights in a single pass
  char buff[64];
  size_t pcount = 0;
  for (int i=0; i<point_lights->count; i++) {
    ex_point_light_t *light = (ex_point_light_t*)point_lights->nodes[i].obj;

    if (light->is_visible && !light->is_shadow) {
      sprintf(buff, "u_point_lights[%u]", (uint32_t)pcount++);
      ex_render_point_light(light, forward_shader, buff);
    }
  }

  glUniform1i(ex_uniform(forward_shader, "u_point_active"), 0);
  glUniform1i(ex_uniform(forward_shader, "u_point_count"), pcount);
  glUniform1i(ex_uniform(forward_shader, "u_ambient_pass"), 1);
  for (int i=0; i<models->count; i++) {
    ex_model_t *model = (ex_model_t*)models->nodes[i].obj;
    ex_render_model(model, forward_shader);
  }
  glUniform1i(ex_uniform(forward_shader, "u_ambient_pass"), 0);
  glUniform1i(ex_uniform(forward_shader, "u_point_count"), 0);
  /* ------------ */


  /* SECOND PASS */
  // enable blending for second pass onwards
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE);

  // render all shadow casting point lights
  glUniform1i(ex_uniform(forward_shader, "u_point_active"), 1);
  for (int i=0; i<point_lights->count; i++) {
    ex_point_light_t *light = (ex_point_light_t*)point_lights->nodes[i].obj;

    if (!light->is_shadow || !light->is_visible)
      continue;

    ex_render_point_light(light, forward_shader, NULL);
    for (int i=0; i<models->count; i++) {
      ex_model_t *model = (ex_model_t*)models->nodes[i].obj;
      ex_render_model(model, forward_shader);
    }
  }
  glDisable(GL_BLEND);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  /* ----------- */


  /* RENDER TO SCREEN */
  glViewport(0, 0, display.width, display.height);
  glDisable(GL_DEPTH_TEST);
  glEnable(GL_FRAMEBUFFER_SRGB);

  ex_shader_use(framebuffer_shader);

  glBindVertexArray(framebuffer.vao);
  glActiveTexture(GL_TEXTURE0);
  glUniform1i(ex_uniform(framebuffer_shader, "u_texture"), 0);
  glBindTexture(GL_TEXTURE_2D, framebuffer.cbo);
  glDrawArrays(GL_TRIANGLES, 0, 6);
  glBindVertexArray(0);
  glBindTexture(GL_TEXTURE_2D, 0);
  /* ---------------- */
}

void ex_render_model(ex_model_t *model, GLuint shader)
{
  // handle transformations
  if (!model->use_transform && model->instance_count < 2) {
    mat4x4_identity(model->transforms[0]);
    mat4x4_translate_in_place(model->transforms[0], model->position[0], model->position[1], model->position[2]);
    mat4x4_rotate_Y(model->transforms[0], model->transforms[0], rad(model->rotation[1]));
    mat4x4_rotate_X(model->transforms[0], model->transforms[0], rad(model->rotation[0]));
    mat4x4_rotate_Z(model->transforms[0], model->transforms[0], rad(model->rotation[2]));
    mat4x4_scale_aniso(model->transforms[0], model->transforms[0], model->scale, model->scale, model->scale);
  }

  // pass bone data
  GLuint has_skeleton_loc = ex_uniform(shader, "u_has_skeleton");
  glUniform1i(has_skeleton_loc, 0);

  if (model->bones != NULL && model->current_anim != NULL) {
    glUniform1i(has_skeleton_loc, 1);
    glUniformMatrix4fv(ex_uniform(shader, "u_bone_matrix"), model->bones_len, GL_TRUE, &model->skeleton[0][0][0]);
  }

  // update instancing matrix vbo
  if (!model->is_static || model->is_static == 1) {
    glBindBuffer(GL_ARRAY_BUFFER, model->instance_vbo);
    GLvoid *ptr = glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY);
    memcpy(ptr, &model->transforms[0], model->instance_count * sizeof(mat4x4));
    glUnmapBuffer(GL_ARRAY_BUFFER);

    if (model->is_static)
      model->is_static = 2;
  }

  // these remain the same for each mesh
  glUniform1i(ex_uniform(shader, "u_texture"), 4);
  glUniform1i(ex_uniform(shader, "u_spec"), 5);
  glUniform1i(ex_uniform(shader, "u_norm"), 6);

  // render meshes
  for (int i=0; i<EX_MODEL_MAX_MESHES; i++) {
    if (model->meshes[i] == NULL)
      continue;

    glUniform1i(ex_uniform(shader, "u_is_lit"), model->is_lit);
    ex_render_mesh(model->meshes[i], shader, model->instance_count);
  }
}

void ex_render_mesh(ex_mesh_t *mesh, GLuint shader, size_t count)
{
  // bind vao/ebo/tex
  glBindVertexArray(mesh->VAO);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh->EBO);

  // diffuse
  glActiveTexture(GL_TEXTURE4);
  if (mesh->texture < 1)
    glBindTexture(GL_TEXTURE_2D, default_texture_diffuse);
  else
    glBindTexture(GL_TEXTURE_2D, mesh->texture);

  // specular
  glActiveTexture(GL_TEXTURE5);
  if (mesh->texture_spec < 1)
    glBindTexture(GL_TEXTURE_2D, default_texture_specular);
  else
    glBindTexture(GL_TEXTURE_2D, mesh->texture_spec);

  // normal
  glActiveTexture(GL_TEXTURE6);
  if (mesh->texture_norm < 1)
    glBindTexture(GL_TEXTURE_2D, default_texture_normal);
  else
    glBindTexture(GL_TEXTURE_2D, mesh->texture_norm);

  // draw mesh
  glDrawElementsInstanced(GL_TRIANGLES, mesh->icount, GL_UNSIGNED_INT, 0, count);

  // unbind buffers
  glBindTexture(GL_TEXTURE_2D, 0);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
  glBindVertexArray(0);
}

void ex_render_point_light_begin(ex_point_light_t *light, GLuint shader)
{
  // prevent needless updates
  light->update = 0;

  // set up light transforms for cube map
  vec3 temp;
  vec3_add(temp, light->position, (vec3){1.0f, 0.0f, 0.0f});
  mat4x4_look_at(light->transform[0], light->position, temp, (vec3){0.0f, -1.0f, 0.0f});
  mat4x4_mul(light->transform[0], projection_90deg, light->transform[0]);

  vec3_add(temp, light->position, (vec3){-1.0f, 0.0f, 0.0f});
  mat4x4_look_at(light->transform[1], light->position, temp, (vec3){0.0f, -1.0f, 0.0f});
  mat4x4_mul(light->transform[1], projection_90deg, light->transform[1]);

  vec3_add(temp, light->position, (vec3){0.0f, 1.0f, 0.0f});
  mat4x4_look_at(light->transform[2], light->position, temp, (vec3){0.0f, 0.0f, 1.0f});
  mat4x4_mul(light->transform[2], projection_90deg, light->transform[2]);

  vec3_add(temp, light->position, (vec3){0.0f, -1.0f, 0.0f});
  mat4x4_look_at(light->transform[3], light->position, temp, (vec3){0.0f, 0.0f, -1.0f});
  mat4x4_mul(light->transform[3], projection_90deg, light->transform[3]);

  vec3_add(temp, light->position, (vec3){0.0f, 0.0f, 1.0f});
  mat4x4_look_at(light->transform[4], light->position, temp, (vec3){0.0f, -1.0f, 0.0f});
  mat4x4_mul(light->transform[4], projection_90deg, light->transform[4]);

  vec3_add(temp, light->position, (vec3){0.0f, 0.0f, -1.0f});
  mat4x4_look_at(light->transform[5], light->position, temp, (vec3){0.0f, -1.0f, 0.0f});
  mat4x4_mul(light->transform[5], projection_90deg, light->transform[5]);

  // render to depth cube map
  glViewport(0, 0, SHADOW_MAP_SIZE, SHADOW_MAP_SIZE);
  glBindFramebuffer(GL_FRAMEBUFFER, light->depth_map_fbo);

  glClear(GL_DEPTH_BUFFER_BIT);
  glEnable(GL_DEPTH_TEST);

  // pass transform matrices to shader
  glUniformMatrix4fv(ex_uniform(shader, "u_shadow_matrices[0]"), 1, GL_FALSE, *light->transform[0]);
  glUniformMatrix4fv(ex_uniform(shader, "u_shadow_matrices[1]"), 1, GL_FALSE, *light->transform[1]);
  glUniformMatrix4fv(ex_uniform(shader, "u_shadow_matrices[2]"), 1, GL_FALSE, *light->transform[2]);
  glUniformMatrix4fv(ex_uniform(shader, "u_shadow_matrices[3]"), 1, GL_FALSE, *light->transform[3]);
  glUniformMatrix4fv(ex_uniform(shader, "u_shadow_matrices[4]"), 1, GL_FALSE, *light->transform[4]);
  glUniformMatrix4fv(ex_uniform(shader, "u_shadow_matrices[5]"), 1, GL_FALSE, *light->transform[5]);

  glUniform1f(ex_uniform(shader, "u_far_plane"), EX_POINT_FAR_PLANE);
  glUniform3fv(ex_uniform(shader, "u_light_pos"), 1, light->position);
}

void ex_render_point_light(ex_point_light_t *light, GLuint shader, const char *prefix)
{
  if (light->is_shadow) {
    glUniform1i(ex_uniform(shader, "u_point_light.is_shadow"), 1);

    glUniform1i(ex_uniform(shader, "u_point_depth"), 7);

    glActiveTexture(GL_TEXTURE7);
    glBindTexture(GL_TEXTURE_CUBE_MAP, light->depth_map);
  } else if (prefix != NULL) {
    char buff[64];
    sprintf(buff, "%s.is_shadow", prefix);
    glUniform1i(ex_uniform(shader, buff), 0);
  }

  if (prefix != NULL) {
    char buff[64];
    sprintf(buff, "%s.far", prefix);
    glUniform1f(ex_uniform(shader,  buff), EX_POINT_FAR_PLANE);
    sprintf(buff, "%s.position", prefix);
    glUniform3fv(ex_uniform(shader, buff), 1, light->position);
    sprintf(buff, "%s.color", prefix);
    glUniform3fv(ex_uniform(shader, buff), 1, light->color);
  } else {
    glUniform1i(ex_uniform(shader,  "u_point_active"), 1);
    glUniform1f(ex_uniform(shader,  "u_point_light.far"), EX_POINT_FAR_PLANE);
    glUniform3fv(ex_uniform(shader, "u_point_light.position"), 1, light->position);
    glUniform3fv(ex_uniform(shader, "u_point_light.color"), 1, light->color);
  }
}

void ex_render_resize(size_t width, size_t height)
{
  /* -- resze framebuffer -- */
  glBindFramebuffer(GL_FRAMEBUFFER, framebuffer.fbo);

  framebuffer.width = display.width;
  framebuffer.height = display.height;

  glBindTexture(GL_TEXTURE_2D, framebuffer.cbo);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB10_A2, framebuffer.width, framebuffer.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_BORDER);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
  GLfloat border[] = {1.0, 1.0, 1.0, 1.0};
  glTexParameterfv(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_BORDER_COLOR, border);
  glBindTexture(GL_TEXTURE_2D, 0);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, framebuffer.cbo, 0);

  // depth buffer
  glBindRenderbuffer(GL_RENDERBUFFER, framebuffer.rbo);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, framebuffer.width, framebuffer.height);
  glBindRenderbuffer(GL_RENDERBUFFER, 0);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, framebuffer.rbo);

  // test framebuffer
  if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    printf("Error! Framebuffer is not complete\n");
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  /* ----------------- */
}

void ex_render_destroy()
{
  glDeleteRenderbuffers(1, &framebuffer.rbo);
  glDeleteFramebuffers(1, &framebuffer.fbo);
  glDeleteTextures(1, &framebuffer.cbo);

  glDeleteBuffers(1, &screenquad_vbo);
  glDeleteVertexArrays(1, &screenquad_vao);
}