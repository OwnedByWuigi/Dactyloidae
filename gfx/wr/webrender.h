#ifndef gfx_webrender_h
#define gfx_webrender_h

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct wr_context wr_context;

typedef struct {
  float x;
  float y;
  float width;
  float height;
} wr_rect;

typedef struct {
  float r;
  float g;
  float b;
  float a;
} wr_color;

/* A 2D affine transform in the same layout used by gfx::Matrix. */
typedef struct {
  float m11;
  float m12;
  float m21;
  float m22;
  float m31;
  float m32;
} wr_transform;

typedef struct {
  wr_rect rect;
  wr_color color;
} wr_gpu_quad;

typedef struct {
  uint32_t first_quad;
  uint32_t quad_count;
  wr_color color;
  wr_transform transform;
  wr_rect clip_rect;
  uint8_t has_clip;
} wr_gpu_batch;

typedef struct {
  const wr_gpu_quad* quads;
  size_t quad_count;
  const wr_gpu_batch* batches;
  size_t batch_count;
} wr_frame;

/* Creates a retained display-list context. */
wr_context* wr_context_create(size_t initial_capacity);
void wr_context_destroy(wr_context* context);

/* Removes every item from the retained display list. */
void wr_context_clear(wr_context* context);

/* Sets the viewport used for coarse CPU-side culling. */
void wr_context_set_viewport(wr_context* context, wr_rect viewport);

/* Sets the transform applied to subsequently recorded primitives. */
void wr_context_set_transform(wr_context* context, wr_transform transform);

/* Sets the opacity applied to subsequently recorded primitives. */
void wr_context_set_opacity(wr_context* context, float opacity);

/* Records a retained stacking-context boundary and restores it on pop. */
int wr_display_list_push_stacking_context(wr_context* context,
                                          wr_transform transform,
                                          float opacity);
void wr_display_list_pop_stacking_context(wr_context* context);

/* Pushes/pops a retained primitive clip. Clips are intersected in order. */
int wr_display_list_push_clip(wr_context* context, wr_rect clip);
void wr_display_list_pop_clip(wr_context* context);

/* Adds an opaque or translucent solid rectangle to the display list. */
int wr_display_list_push_rect(wr_context* context, wr_rect rect,
                              wr_color color);

/* Explicit transform form for display-list builders. */
int wr_display_list_push_transformed_rect(wr_context* context, wr_rect rect,
                                          wr_color color,
                                          wr_transform transform);

/* Builds the GPU-ready stream from the retained display list. */
const wr_frame* wr_context_build_frame(wr_context* context);

#ifdef __cplusplus
}
#endif

#endif /* gfx_webrender_h */
