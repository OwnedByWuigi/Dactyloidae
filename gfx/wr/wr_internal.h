#ifndef GFX_WR_INTERNAL_H
#define GFX_WR_INTERNAL_H

#include <webrender.h>
#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  wr_rect rect;
  wr_color color;
  wr_transform transform;
  wr_rect clip_rect;
  uint8_t has_clip;
  uint8_t kind;
  uint8_t active;
  uint32_t image_key;
  wr_rect tex_rect;
  wr_item_id item_id;
} wr_rect_command;

typedef struct {
  wr_transform transform;
  float opacity;
  size_t clip_depth;
} wr_display_state;

typedef struct {
  size_t command_count;
  wr_rect_command* commands;
} wr_scene_transaction;

struct wr_context {
  wr_rect_command* commands;
  size_t command_count;
  size_t command_capacity;

  wr_gpu_quad* quads;
  size_t quad_count;
  size_t quad_capacity;

  wr_gpu_batch* batches;
  size_t batch_count;
  size_t batch_capacity;

  wr_rect viewport;
  wr_transform transform;
  float opacity;
  wr_rect* clip_stack;
  size_t clip_depth;
  size_t clip_capacity;
  wr_display_state* state_stack;
  size_t state_depth;
  size_t state_capacity;
  wr_item_id next_item_id;
  wr_image_resource* images;
  size_t image_count;
  size_t image_capacity;
  wr_scene_transaction* transactions;
  size_t transaction_depth;
  size_t transaction_capacity;
  int frame_valid;
  wr_frame frame;
};

int wr_size_mul_overflow(size_t, size_t);
int wr_reserve(void**, size_t*, size_t, size_t);
int wr_valid_rect(wr_rect);
int wr_valid_transform(wr_transform);
wr_transform wr_identity_transform(void);
wr_transform wr_multiply_transform(wr_transform, wr_transform);
wr_rect wr_empty_rect(void);
float wr_clamp01(float);
wr_color wr_normalize_color(wr_color);
int wr_colors_equal(wr_color, wr_color);
int wr_rects_equal(wr_rect, wr_rect);
int wr_intersects(wr_rect, wr_rect);
wr_rect wr_intersect_rect(wr_rect, wr_rect);
wr_rect wr_transform_bounds(wr_rect, wr_transform);
int wr_command_occluded(const wr_context*, size_t);
#endif
