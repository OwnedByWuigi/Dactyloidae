#include <wr_internal.h>

static size_t
wr_image_index(const wr_context* context, uint32_t key)
{
  size_t i;
  for (i = 0; context && i < context->image_count; ++i)
    if (context->images[i].key == key)
      return i;
  return SIZE_MAX;
}

static size_t
wr_item_index(const wr_context* context, wr_item_id item)
{
  size_t i;
  for (i = 0; context && i < context->command_count; ++i)
    if (context->commands[i].active && context->commands[i].item_id == item)
      return i;
  return SIZE_MAX;
}

static int
wr_rect_contains(wr_rect outer, wr_rect inner)
{
  return inner.x >= outer.x && inner.y >= outer.y &&
         inner.x + inner.width <= outer.x + outer.width &&
         inner.y + inner.height <= outer.y + outer.height;
}

static int
wr_append(wr_context* context, wr_item_id item, wr_rect rect, wr_color color,
          uint8_t kind, uint32_t image_key, wr_rect tex_rect)
{
  wr_rect clip = context->clip_depth
    ? context->clip_stack[context->clip_depth - 1] : context->viewport;
  if (!context || !wr_valid_rect(rect) ||
      (kind == WR_PRIMITIVE_IMAGE &&
       (wr_image_index(context, image_key) == SIZE_MAX ||
        tex_rect.width == 0.0f || tex_rect.height == 0.0f)))
    return 0;
  if (!wr_reserve((void**)&context->commands, &context->command_capacity,
                  context->command_count + 1, sizeof(*context->commands)))
    return 0;
  if (!item) {
    item = context->next_item_id++;
    if (!item)
      item = context->next_item_id++;
  }
  if (wr_item_index(context, item) != SIZE_MAX)
    return 0;
  context->commands[context->command_count] = (wr_rect_command){
    rect, wr_normalize_color(color), context->transform, clip,
    context->clip_depth ? 1 : 0, kind, 1, image_key, tex_rect, item };
  context->commands[context->command_count].color.a *= context->opacity;
  ++context->command_count;
  context->frame_valid = 0;
  return 1;
}

int
wr_context_register_image(wr_context* context, uint32_t key,
                          uint32_t width, uint32_t height)
{
  size_t index;
  if (!context || !key || !width || !height)
    return 0;
  index = wr_image_index(context, key);
  if (index != SIZE_MAX) {
    context->images[index].width = width;
    context->images[index].height = height;
    return 1;
  }
  if (!wr_reserve((void**)&context->images, &context->image_capacity,
                  context->image_count + 1, sizeof(*context->images)))
    return 0;
  context->images[context->image_count++] = (wr_image_resource){ key, width, height };
  return 1;
}

void
wr_context_unregister_image(wr_context* context, uint32_t key)
{
  size_t index = wr_image_index(context, key);
  if (index == SIZE_MAX)
    return;
  context->images[index] = context->images[--context->image_count];
  context->frame_valid = 0;
}

void
wr_context_clear_images(wr_context* context)
{
  if (context) {
    context->image_count = 0;
    context->frame_valid = 0;
  }
}

int
wr_context_begin_transaction(wr_context* context)
{
  wr_scene_transaction transaction = { 0, NULL };
  if (!context || !wr_reserve((void**)&context->transactions,
      &context->transaction_capacity, context->transaction_depth + 1,
      sizeof(*context->transactions)))
    return 0;
  transaction.command_count = context->command_count;
  if (transaction.command_count) {
    transaction.commands = malloc(transaction.command_count * sizeof(*transaction.commands));
    if (!transaction.commands)
      return 0;
    memcpy(transaction.commands, context->commands,
           transaction.command_count * sizeof(*transaction.commands));
  }
  context->transactions[context->transaction_depth++] = transaction;
  return 1;
}

int
wr_context_commit_transaction(wr_context* context)
{
  if (!context || !context->transaction_depth)
    return 0;
  free(context->transactions[--context->transaction_depth].commands);
  context->frame_valid = 0;
  return 1;
}

void
wr_context_abort_transaction(wr_context* context)
{
  wr_scene_transaction transaction;
  if (!context || !context->transaction_depth)
    return;
  transaction = context->transactions[--context->transaction_depth];
  if (transaction.command_count)
    memcpy(context->commands, transaction.commands,
           transaction.command_count * sizeof(*context->commands));
  context->command_count = transaction.command_count;
  free(transaction.commands);
  context->frame_valid = 0;
}

int
wr_context_remove_item(wr_context* context, wr_item_id item)
{
  size_t index = wr_item_index(context, item);
  if (index == SIZE_MAX)
    return 0;
  context->commands[index].active = 0;
  context->frame_valid = 0;
  return 1;
}

wr_item_id
wr_display_list_push_rect_with_id(wr_context* context, wr_item_id item,
                                   wr_rect rect, wr_color color)
{
  return wr_append(context, item, rect, color, WR_PRIMITIVE_SOLID, 0,
                   (wr_rect){ 0, 0, 1, 1 }) ? item : 0;
}

int
wr_display_list_update_rect(wr_context* context, wr_item_id item, wr_rect rect,
                            wr_color color, wr_transform transform)
{
  size_t index = wr_item_index(context, item);
  if (index == SIZE_MAX || !wr_valid_rect(rect) || !wr_valid_transform(transform))
    return 0;
  context->commands[index].rect = rect;
  context->commands[index].color = wr_normalize_color(color);
  context->commands[index].color.a *= context->opacity;
  context->commands[index].transform = transform;
  context->frame_valid = 0;
  return 1;
}

int
wr_display_list_push_image(wr_context* context, wr_rect rect, uint32_t key,
                           wr_rect tex_rect, wr_color tint)
{ return wr_append(context, 0, rect, tint, WR_PRIMITIVE_IMAGE, key, tex_rect); }

wr_item_id
wr_display_list_push_image_with_id(wr_context* context, wr_item_id item,
                                   wr_rect rect, uint32_t key, wr_rect tex,
                                   wr_color tint)
{ return wr_append(context, item, rect, tint, WR_PRIMITIVE_IMAGE, key, tex) ? item : 0; }

int
wr_display_list_push_linear_gradient(wr_context* context, wr_rect rect,
                                     wr_color start, wr_color end, int horizontal)
{
  unsigned i;
  for (i = 0; i < 32; ++i) {
    float a = (float)i / 32.0f, b = (float)(i + 1) / 32.0f;
    wr_rect slice = rect;
    wr_color color = { start.r + (end.r-start.r)*(a+b)*.5f,
      start.g + (end.g-start.g)*(a+b)*.5f, start.b + (end.b-start.b)*(a+b)*.5f,
      start.a + (end.a-start.a)*(a+b)*.5f };
    if (horizontal) { slice.x += rect.width*a; slice.width = rect.width*(b-a); }
    else { slice.y += rect.height*a; slice.height = rect.height*(b-a); }
    if (!wr_append(context, 0, slice, color, WR_PRIMITIVE_SOLID, 0,
                   (wr_rect){ 0, 0, 1, 1 })) return 0;
  }
  return 1;
}

int
wr_display_list_push_rounded_rect(wr_context* context, wr_rect rect,
                                  wr_color color, float radius)
{
  unsigned i;
  (void)radius;
  for (i = 0; i < 16; ++i) {
    wr_rect slice = { rect.x, rect.y + rect.height * i / 16.0f,
                      rect.width, rect.height / 16.0f };
    if (!wr_append(context, 0, slice, color, WR_PRIMITIVE_SOLID, 0,
                   (wr_rect){ 0, 0, 1, 1 }))
      return 0;
  }
  return 1;
}

int
wr_display_list_push_glyph_run(wr_context* context, uint32_t key,
                               const wr_glyph* glyphs, size_t count, wr_color color)
{
  size_t i;
  for (i = 0; glyphs && i < count; ++i)
    if (!wr_append(context, 0, glyphs[i].rect, color, WR_PRIMITIVE_IMAGE,
                   key, glyphs[i].tex_rect)) return 0;
  return glyphs && count != 0;
}

int
wr_command_occluded(const wr_context* context, size_t index)
{
  size_t i;
  wr_rect bounds = wr_transform_bounds(context->commands[index].rect,
                                       context->commands[index].transform);
  for (i = index + 1; i < context->command_count; ++i) {
    const wr_rect_command* covering = &context->commands[i];
    wr_rect cover;
    if (!covering->active || covering->kind != WR_PRIMITIVE_SOLID ||
        covering->color.a != 1.0f) continue;
    cover = wr_transform_bounds(covering->rect, covering->transform);
    if (wr_rect_contains(cover, bounds)) return 1;
  }
  return 0;
}
