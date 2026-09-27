#include <wr_internal.h>

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
  context->commands[context->command_count].kind = WR_PRIMITIVE_SOLID;
  context->commands[context->command_count].active = 1;
  context->commands[context->command_count].image_key = 0;
  context->commands[context->command_count].tex_rect = (wr_rect){ 0, 0, 1, 1 };
  context->commands[context->command_count].item_id = context->next_item_id++;
  ++context->command_count;
  context->frame_valid = 0;
  return 1;
}
