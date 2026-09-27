#ifndef gfx_webrender_h
#define gfx_webrender_h

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct wr_context wr_context;
typedef uint64_t wr_item_id;

typedef struct {
  uint32_t key;
  uint32_t width;
  uint32_t height;
} wr_image_resource;

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
  wr_rect tex_rect;
} wr_glyph;

typedef enum {
  WR_PRIMITIVE_SOLID = 0,
  WR_PRIMITIVE_IMAGE = 1
} wr_primitive_kind;

typedef struct {
  wr_rect rect;
  wr_color color;
  wr_rect tex_rect;
  uint32_t image_key;
  wr_item_id item_id;
  uint8_t kind;
  uint8_t active;
} wr_gpu_quad;

typedef struct {
  uint32_t first_quad;
  uint32_t quad_count;
  wr_color color;
  wr_transform transform;
  wr_rect clip_rect;
  uint8_t has_clip;
  uint8_t kind;
  uint32_t image_key;
} wr_gpu_batch;

typedef struct {
  const wr_gpu_quad* quads;
  size_t quad_count;
  const wr_gpu_batch* batches;
  size_t batch_count;
} wr_frame;

typedef struct {
  uint32_t version;
  uint32_t quad_count;
  uint32_t batch_count;
} wr_frame_packet_header;

#define WR_FRAME_PACKET_VERSION 1

/* Creates a retained display-list context. */
wr_context* wr_context_create(size_t initial_capacity);
void wr_context_destroy(wr_context* context);

/* Removes every item from the retained display list. */
void wr_context_clear(wr_context* context);

/* Clears transient image resources without releasing retained allocations. */
void wr_context_clear_images(wr_context* context);

/* Begins an atomic retained-scene update. Transactions may be nested. */
int wr_context_begin_transaction(wr_context* context);

/* Commits the innermost retained-scene update. */
int wr_context_commit_transaction(wr_context* context);

/* Rolls back the innermost retained-scene update. */
void wr_context_abort_transaction(wr_context* context);

/* Removes a retained display-list item by stable item ID. */
int wr_context_remove_item(wr_context* context, wr_item_id item_id);

/* Sets the viewport used for coarse CPU-side culling. */
void wr_context_set_viewport(wr_context* context, wr_rect viewport);

/* Sets the transform applied to subsequently recorded primitives. */
void wr_context_set_transform(wr_context* context, wr_transform transform);

/* Sets the opacity applied to subsequently recorded primitives. */
void wr_context_set_opacity(wr_context* context, float opacity);

/* Registers or updates an image resource referenced by display-list commands. */
int wr_context_register_image(wr_context* context, uint32_t image_key,
                              uint32_t width, uint32_t height);

/* Removes an image resource and invalidates frames referencing it. */
void wr_context_unregister_image(wr_context* context, uint32_t image_key);

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

/* Adds a solid rectangle with a caller-supplied stable item ID. */
wr_item_id wr_display_list_push_rect_with_id(wr_context* context,
                                             wr_item_id item_id,
                                             wr_rect rect, wr_color color);

/* Updates an existing solid rectangle without rebuilding the scene. */
int wr_display_list_update_rect(wr_context* context, wr_item_id item_id,
                                wr_rect rect, wr_color color,
                                wr_transform transform);

/* Adds a GPU-tessellated linear gradient. */
int wr_display_list_push_linear_gradient(wr_context* context, wr_rect rect,
                                         wr_color start, wr_color end,
                                         int horizontal);

/* Adds a GPU-tessellated rounded rectangle. */
int wr_display_list_push_rounded_rect(wr_context* context, wr_rect rect,
                                      wr_color color, float radius);

/* Explicit transform form for display-list builders. */
int wr_display_list_push_transformed_rect(wr_context* context, wr_rect rect,
                                          wr_color color,
                                          wr_transform transform);

/* Adds an image primitive resolved by the compositor using image_key. */
int wr_display_list_push_image(wr_context* context, wr_rect rect,
                               uint32_t image_key, wr_rect tex_rect,
                               wr_color tint);

/* Adds an image with a caller-supplied stable item ID. */
wr_item_id wr_display_list_push_image_with_id(wr_context* context,
                                              wr_item_id item_id,
                                              wr_rect rect,
                                              uint32_t image_key,
                                              wr_rect tex_rect,
                                              wr_color tint);

/* Records glyph quads sourced from a registered image atlas. */
int wr_display_list_push_glyph_run(wr_context* context, uint32_t image_key,
                                   const wr_glyph* glyphs, size_t glyph_count,
                                   wr_color color);

/* Builds the GPU-ready stream from the retained display list. */
const wr_frame* wr_context_build_frame(wr_context* context);

/* Returns the byte size required for a versioned, flat frame packet. */
size_t wr_frame_packet_size(const wr_frame* frame);

/* Serializes a frame into a flat buffer suitable for thread/IPC transfer. */
int wr_frame_serialize(const wr_frame* frame, void* buffer, size_t buffer_size);

#ifdef __cplusplus
}
#endif

#endif /* gfx_webrender_h */
