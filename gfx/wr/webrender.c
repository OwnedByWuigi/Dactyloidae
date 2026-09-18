#include "webrender.h"

#include <float.h>
#include <math.h>
#include <stdlib.h>

typedef struct {
  wr_rect rect;
  wr_color color;
  wr_transform transform;
  wr_rect clip_rect;
  uint8_t has_clip;
} wr_rect_command;

typedef struct {
  wr_transform transform;
  float opacity;
  size_t clip_depth;
} wr_display_state;

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
  wr_frame frame;
};

static int
wr_size_mul_overflow(size_t a, size_t b)
{
  return b != 0 && a > SIZE_MAX / b;
}

static int
wr_reserve(void** data, size_t* capacity, size_t count, size_t element_size)
{
  size_t new_capacity;
  void* new_data;

  if (count <= *capacity)
    return 1;
  if (element_size == 0 || wr_size_mul_overflow(count, element_size))
    return 0;

  new_capacity = *capacity ? *capacity : 8;
  while (new_capacity < count) {
    if (new_capacity > SIZE_MAX / 2) {
      new_capacity = count;
      break;
    }
    new_capacity *= 2;
  }
  if (wr_size_mul_overflow(new_capacity, element_size))
    return 0;

  new_data = realloc(*data, new_capacity * element_size);
  if (!new_data)
    return 0;

  *data = new_data;
  *capacity = new_capacity;
  return 1;
}

static int
wr_valid_rect(wr_rect rect)
{
  return isfinite(rect.x) && isfinite(rect.y) &&
         isfinite(rect.width) && isfinite(rect.height) &&
         rect.width > 0.0f && rect.height > 0.0f;
}

static int
wr_valid_transform(wr_transform transform)
{
  return isfinite(transform.m11) && isfinite(transform.m12) &&
         isfinite(transform.m21) && isfinite(transform.m22) &&
         isfinite(transform.m31) && isfinite(transform.m32);
}

static wr_transform
wr_identity_transform(void)
{
  wr_transform transform = { 1, 0, 0, 1, 0, 0 };
  return transform;
}

static wr_transform
wr_multiply_transform(wr_transform a, wr_transform b)
{
  wr_transform result;
  result.m11 = a.m11 * b.m11 + a.m21 * b.m12;
  result.m12 = a.m12 * b.m11 + a.m22 * b.m12;
  result.m21 = a.m11 * b.m21 + a.m21 * b.m22;
  result.m22 = a.m12 * b.m21 + a.m22 * b.m22;
  result.m31 = a.m11 * b.m31 + a.m21 * b.m32 + a.m31;
  result.m32 = a.m12 * b.m31 + a.m22 * b.m32 + a.m32;
  return result;
}

static wr_rect
wr_empty_rect(void)
{
  wr_rect rect = { 0, 0, 0, 0 };
  return rect;
}

static float
wr_clamp01(float value)
{
  if (value < 0.0f)
    return 0.0f;
  if (value > 1.0f)
    return 1.0f;
  return value;
}

static wr_color
wr_normalize_color(wr_color color)
{
  color.r = wr_clamp01(color.r);
  color.g = wr_clamp01(color.g);
  color.b = wr_clamp01(color.b);
  color.a = wr_clamp01(color.a);
  return color;
}

static int
wr_colors_equal(wr_color a, wr_color b)
{
  return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

static int
wr_rects_equal(wr_rect a, wr_rect b)
{
  return a.x == b.x && a.y == b.y &&
         a.width == b.width && a.height == b.height;
}

static int
wr_intersects(wr_rect a, wr_rect b)
{
  return a.x < b.x + b.width && a.x + a.width > b.x &&
         a.y < b.y + b.height && a.y + a.height > b.y;
}

static wr_rect
wr_intersect_rect(wr_rect a, wr_rect b)
{
  float left = a.x > b.x ? a.x : b.x;
  float top = a.y > b.y ? a.y : b.y;
  float right = a.x + a.width < b.x + b.width ?
                a.x + a.width : b.x + b.width;
  float bottom = a.y + a.height < b.y + b.height ?
                 a.y + a.height : b.y + b.height;
  wr_rect result = { left, top, right - left, bottom - top };
  return result;
}

static wr_rect
wr_transform_bounds(wr_rect rect, wr_transform transform)
{
  float x1 = rect.x * transform.m11 + rect.y * transform.m21 + transform.m31;
  float y1 = rect.x * transform.m12 + rect.y * transform.m22 + transform.m32;
  float x2 = (rect.x + rect.width) * transform.m11 +
             rect.y * transform.m21 + transform.m31;
  float y2 = (rect.x + rect.width) * transform.m12 +
             rect.y * transform.m22 + transform.m32;
  float x3 = rect.x * transform.m11 +
             (rect.y + rect.height) * transform.m21 + transform.m31;
  float y3 = rect.x * transform.m12 +
             (rect.y + rect.height) * transform.m22 + transform.m32;
  float x4 = (rect.x + rect.width) * transform.m11 +
             (rect.y + rect.height) * transform.m21 + transform.m31;
  float y4 = (rect.x + rect.width) * transform.m12 +
             (rect.y + rect.height) * transform.m22 + transform.m32;
  float left = fminf(fminf(x1, x2), fminf(x3, x4));
  float right = fmaxf(fmaxf(x1, x2), fmaxf(x3, x4));
  float top = fminf(fminf(y1, y2), fminf(y3, y4));
  float bottom = fmaxf(fmaxf(y1, y2), fmaxf(y3, y4));
  wr_rect result = { left, top, right - left, bottom - top };
  return result;
}

wr_context*
wr_context_create(size_t initial_capacity)
{
  wr_context* context = (wr_context*)calloc(1, sizeof(*context));
  if (!context)
    return NULL;

  context->viewport.x = -FLT_MAX;
  context->viewport.y = -FLT_MAX;
  context->viewport.width = FLT_MAX;
  context->viewport.height = FLT_MAX;
  context->transform = wr_identity_transform();
  context->opacity = 1.0f;

  if (initial_capacity &&
      !wr_reserve((void**)&context->commands, &context->command_capacity,
                  initial_capacity, sizeof(*context->commands))) {
    wr_context_destroy(context);
    return NULL;
  }
  return context;
}

void
wr_context_destroy(wr_context* context)
{
  if (!context)
    return;
  free(context->commands);
  free(context->quads);
  free(context->batches);
  free(context->clip_stack);
  free(context->state_stack);
  free(context);
}

void
wr_context_clear(wr_context* context)
{
  if (!context)
    return;
  context->command_count = 0;
  context->quad_count = 0;
  context->batch_count = 0;
  context->clip_depth = 0;
  context->state_depth = 0;
}

void
wr_context_set_viewport(wr_context* context, wr_rect viewport)
{
  if (context && wr_valid_rect(viewport))
    context->viewport = viewport;
}

void
wr_context_set_transform(wr_context* context, wr_transform transform)
{
  if (context && wr_valid_transform(transform))
    context->transform = transform;
}

void
wr_context_set_opacity(wr_context* context, float opacity)
{
  if (context && isfinite(opacity)) {
    context->opacity = wr_clamp01(opacity);
  }
}

int
wr_display_list_push_stacking_context(wr_context* context,
                                      wr_transform transform,
                                      float opacity)
{
  wr_display_state state;

  if (!context || !wr_valid_transform(transform) || !isfinite(opacity) ||
      context->state_depth == SIZE_MAX)
    return 0;
  if (!wr_reserve((void**)&context->state_stack, &context->state_capacity,
                  context->state_depth + 1, sizeof(*context->state_stack)))
    return 0;

  state.transform = context->transform;
  state.opacity = context->opacity;
  state.clip_depth = context->clip_depth;
  context->state_stack[context->state_depth++] = state;
  context->transform = wr_multiply_transform(context->transform, transform);
  context->opacity = wr_clamp01(context->opacity * wr_clamp01(opacity));
  return 1;
}

void
wr_display_list_pop_stacking_context(wr_context* context)
{
  wr_display_state state;

  if (!context || !context->state_depth)
    return;
  state = context->state_stack[--context->state_depth];
  context->transform = state.transform;
  context->opacity = state.opacity;
  context->clip_depth = state.clip_depth;
}

int
wr_display_list_push_clip(wr_context* context, wr_rect clip)
{
  wr_rect effective_clip;

  if (!context || !wr_valid_rect(clip) ||
      context->clip_depth == SIZE_MAX)
    return 0;
  if (!wr_reserve((void**)&context->clip_stack, &context->clip_capacity,
                  context->clip_depth + 1, sizeof(*context->clip_stack)))
    return 0;

  effective_clip = clip;
  if (context->clip_depth)
    effective_clip = wr_intersect_rect(
      context->clip_stack[context->clip_depth - 1], clip);
  context->clip_stack[context->clip_depth++] = effective_clip;
  return 1;
}

void
wr_display_list_pop_clip(wr_context* context)
{
  if (context && context->clip_depth)
    --context->clip_depth;
}

int
wr_display_list_push_rect(wr_context* context, wr_rect rect, wr_color color)
{
  return context ? wr_display_list_push_transformed_rect(
                     context, rect, color, context->transform) : 0;
}

int
wr_display_list_push_transformed_rect(wr_context* context, wr_rect rect,
                                      wr_color color, wr_transform transform)
{
  wr_rect clip_rect = context && context->clip_depth ?
                      context->clip_stack[context->clip_depth - 1] :
                      context ? context->viewport : wr_empty_rect();
  uint8_t has_clip = context && context->clip_depth ? 1 : 0;

  if (!context || !wr_valid_rect(rect) || !wr_valid_transform(transform))
    return 0;
  if (context->command_count == SIZE_MAX ||
      context->command_count >= UINT32_MAX)
    return 0;
  if (!wr_reserve((void**)&context->commands, &context->command_capacity,
                  context->command_count + 1, sizeof(*context->commands)))
    return 0;

  context->commands[context->command_count].rect = rect;
  color = wr_normalize_color(color);
  color.a *= context->opacity;
  context->commands[context->command_count].color = color;
  context->commands[context->command_count].transform = transform;
  context->commands[context->command_count].clip_rect = clip_rect;
  context->commands[context->command_count].has_clip = has_clip;
  ++context->command_count;
  return 1;
}

const wr_frame*
wr_context_build_frame(wr_context* context)
{
  size_t i;

  if (!context)
    return NULL;
  context->quad_count = 0;
  context->batch_count = 0;

  for (i = 0; i < context->command_count; ++i) {
    wr_rect_command* command = &context->commands[i];
    wr_gpu_quad* quad;
    wr_gpu_batch* batch;

    if (!wr_intersects(wr_transform_bounds(command->rect,
                                           command->transform),
                       context->viewport) ||
        (command->clip_rect.width <= 0.0f ||
         command->clip_rect.height <= 0.0f))
      continue;
    if (context->quad_count == SIZE_MAX ||
        context->quad_count >= UINT32_MAX ||
        context->batch_count == SIZE_MAX ||
        context->batch_count >= UINT32_MAX)
      return NULL;
    if (!wr_reserve((void**)&context->quads, &context->quad_capacity,
                    context->quad_count + 1, sizeof(*context->quads)) ||
        !wr_reserve((void**)&context->batches, &context->batch_capacity,
                    context->batch_count + 1, sizeof(*context->batches)))
      return NULL;

    quad = &context->quads[context->quad_count++];
    quad->rect = command->rect;
    quad->color = command->color;

    if (context->batch_count &&
        wr_colors_equal(context->batches[context->batch_count - 1].color,
                        command->color) &&
        context->batches[context->batch_count - 1].transform.m11 ==
          command->transform.m11 &&
        context->batches[context->batch_count - 1].transform.m12 ==
          command->transform.m12 &&
        context->batches[context->batch_count - 1].transform.m21 ==
          command->transform.m21 &&
        context->batches[context->batch_count - 1].transform.m22 ==
          command->transform.m22 &&
        context->batches[context->batch_count - 1].transform.m31 ==
          command->transform.m31 &&
        context->batches[context->batch_count - 1].transform.m32 ==
          command->transform.m32 &&
        context->batches[context->batch_count - 1].has_clip ==
          command->has_clip &&
        (!command->has_clip ||
         wr_rects_equal(context->batches[context->batch_count - 1].clip_rect,
                        command->clip_rect))) {
      batch = &context->batches[context->batch_count - 1];
      ++batch->quad_count;
    } else {
      batch = &context->batches[context->batch_count++];
      batch->first_quad = (uint32_t)(context->quad_count - 1);
      batch->quad_count = 1;
      batch->color = command->color;
      batch->transform = command->transform;
      batch->clip_rect = command->clip_rect;
      batch->has_clip = command->has_clip;
    }
  }

  context->frame.quads = context->quads;
  context->frame.quad_count = context->quad_count;
  context->frame.batches = context->batches;
  context->frame.batch_count = context->batch_count;
  return &context->frame;
}
