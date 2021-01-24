/* octree
  A simple octree implementation for
  storing arbitrary data.

  Used for collision optimizations,
  render culling etc.
*/

#ifndef EX_OCTREE_H
#define EX_OCTREE_H

#include "glad/glad.h"

#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include "util/list.h"
#include "math/mathlib.h" 

#define EX_OCTREE_DEFAULT_MIN_SIZE 5.0f
extern int ex_octree_min_size;

typedef enum {
  OBJ_TYPE_UINT,
  OBJ_TYPE_INT,
  OBJ_TYPE_BYTE,
  OBJ_TYPE_FLOAT,
  OBJ_TYPE_DOUBLE,
  OBJ_TYPE_NULL
} ex_octree_obj_type;

typedef struct {
  union {
    uint32_t data_uint;
    int32_t  data_int;
    uint8_t  data_byte;
    float    data_float;
    double   data_double;
  };
  rect_t box;
} ex_octree_obj_t;

typedef struct {
  void *data;
  size_t len;
} ex_octree_data_t;

typedef struct ex_octree_t ex_octree_t;
struct ex_octree_t {
  rect_t region;
  ex_octree_t *children[8];
  int max_life, cur_life;
  ex_list_t *obj_list;
  // flags etc
  uint8_t rendered  : 1;
  uint8_t built     : 1;
  uint8_t first     : 1;
  uint8_t data_type : 5;
  // data
  size_t  data_len;
  union {
    uint32_t *data_uint;
    int32_t  *data_int;
    uint8_t  *data_byte;
    float    *data_float;
    double   *data_double;
  };
  // debug render stuffs
  GLuint vbo, vao, ebo;
  int player_inside;
};

/**
 * [ex_octree_new defines a new octree]
 * @param  type [the data type to store]
 * @return      [the new octree]
 */
ex_octree_t* ex_octree_new(uint8_t type);

/**
 * [ex_octree_init init the tree via the given object list]
 * @param o       [the octree to init]
 * @param region  [the max region]
 * @param objects [list of objects to add]
 */
void ex_octree_init(ex_octree_t *o, rect_t region, ex_list_t *objects);

/**
 * [ex_octree_build]
 * @param o [the octree to rebuild]
 */
void ex_octree_build(ex_octree_t *o);

/**
 * [ex_octree_finalize migrates the data to flat arrays for speed]
 * @param o [the octree to finalize]
 */
void ex_octree_finalize(ex_octree_t *o);

/**
 * [ex_octree_reset cleans up the octree and sets it to its default state]
 * @param  o [the octree to clean]
 * @return   [the empty octree]
 */
ex_octree_t* ex_octree_reset(ex_octree_t *o);

/**
 * [ex_octree_get_colliding_count]
 * @param o      [the octree to check]
 * @param bounds [the bounds to check]
 * @param count  [the amount of colliding entries]
 */
void ex_octree_get_colliding_count(ex_octree_t *o, rect_t *bounds, int *count);

/**
 * [ex_octree_get_colliding get all colliding entry data]
 * @param o         [the octree to check]
 * @param bounds    [the bounds to check]
 * @param data_list [the output data]
 * @param index     [should be 0]
 */
void ex_octree_get_colliding(ex_octree_t *o, rect_t *bounds, ex_octree_data_t *data_list, int *index);

/**
 * [ex_octree_render debug render]
 * @param o [the octree to render]
 */
void ex_octree_render(ex_octree_t *o);

/**
 * [ex_octree_data_ptr]
 * @param o []
 */
static inline void* ex_octree_data_ptr(ex_octree_t *o) {
  switch (o->data_type) {
    case OBJ_TYPE_UINT:
      if (o->data_uint != NULL)
        return o->data_uint;
      break;
    case OBJ_TYPE_INT:
      if (o->data_int != NULL)
        return o->data_int;
      break;
    case OBJ_TYPE_BYTE:
      if (o->data_byte != NULL)
        return o->data_byte;
      break;
    case OBJ_TYPE_FLOAT:
      if (o->data_float != NULL)
        return o->data_float;
      break;
    case OBJ_TYPE_DOUBLE:
      if (o->data_double != NULL)
        return o->data_double;
      break;
    default:
      return NULL;
      break;
  }

  return NULL;
}

/**
 * [ex_rect_new defines a new 3d rect]
 * @param  min [the min position]
 * @param  max [the max position]
 * @return     [the new rect]
 */
static inline rect_t ex_rect_new(vec3 min, vec3 max) {
  rect_t r;
  memcpy(r.min, min, sizeof(vec3));
  memcpy(r.max, max, sizeof(vec3));
  return r;
};

static inline float ex_squared(float v) { return v * v; };
static inline int ex_rect_intersect_sphere(rect_t r, vec3 pos, float radius) {
  float dist = radius * radius;
  if (pos[0] < r.min[0]) dist -= ex_squared(pos[0] - r.min[0]);
  else if (pos[0] > r.max[0]) dist -= ex_squared(pos[0] - r.max[0]);
  if (pos[1] < r.min[1]) dist -= ex_squared(pos[1] - r.min[1]);
  else if (pos[1] > r.max[1]) dist -= ex_squared(pos[1] - r.max[1]);
  if (pos[2] < r.min[2]) dist -= ex_squared(pos[2] - r.min[2]);
  else if (pos[2] > r.max[2]) dist -= ex_squared(pos[2] - r.max[2]);
  return dist > 0;
};

static inline int ex_aabb_aabb(rect_t a, rect_t b) {
  return (a.min[0] <= b.max[0] &&
          a.max[0] >= b.min[0] &&
          a.min[1] <= b.max[1] &&
          a.max[1] >= b.min[1] &&
          a.min[2] <= b.max[2] &&
          a.max[2] >= b.min[2]);
};

static inline int ex_aabb_inside(rect_t outer, rect_t inner) {
  return (outer.min[0] <= inner.min[0] &&
          outer.max[0] >= inner.max[0] &&
          outer.min[1] <= inner.min[1] &&
          outer.max[1] >= inner.max[1] &&
          outer.min[2] <= inner.min[2] &&
          outer.max[2] >= inner.max[2]);
};

static inline rect_t ex_rect_from_triangle(vec3 tri[3]) {
  rect_t box;

  vec3_min(box.min, tri[0], tri[1]);
  vec3_min(box.min, box.min, tri[2]);

  vec3_max(box.max, tri[0], tri[1]);
  vec3_max(box.max, box.max, tri[2]);

  return box;
};


#endif // EX_OCTREE_H
