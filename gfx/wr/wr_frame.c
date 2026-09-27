#include <wr_internal.h>

static int
wr_frame_has_image(const wr_context* context, uint32_t key)
{
  size_t i;
  for (i = 0; i < context->image_count; ++i)
    if (context->images[i].key == key)
      return 1;
  return 0;
}

const wr_frame*
wr_context_build_frame(wr_context* context)
{
  size_t i;

  if (!context)
    return NULL;
  if (context->frame_valid)
    return &context->frame;
  context->quad_count = 0;
  context->batch_count = 0;

  for (i = 0; i < context->command_count; ++i) {
    wr_rect_command* command = &context->commands[i];
    wr_gpu_quad* quad;
    wr_gpu_batch* batch;

    if (!command->active ||
        (command->kind == WR_PRIMITIVE_IMAGE &&
         !wr_frame_has_image(context, command->image_key)) ||
        wr_command_occluded(context, i))
      continue;
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
    quad->tex_rect = command->tex_rect;
    quad->image_key = command->image_key;
    quad->item_id = command->item_id;
    quad->active = command->active;
    quad->kind = command->kind;

    if (context->batch_count &&
        context->batches[context->batch_count - 1].kind == command->kind &&
        context->batches[context->batch_count - 1].image_key ==
          command->image_key &&
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
      batch->kind = command->kind;
      batch->image_key = command->image_key;
    }
  }

  context->frame.quads = context->quads;
  context->frame.quad_count = context->quad_count;
  context->frame.batches = context->batches;
  context->frame.batch_count = context->batch_count;
  context->frame_valid = 1;
  return &context->frame;
}
