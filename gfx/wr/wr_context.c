#include <wr_internal.h>

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
  context->next_item_id = 1;

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
  free(context->images);
  for (size_t i = 0; i < context->transaction_depth; ++i)
    free(context->transactions[i].commands);
  free(context->transactions);
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
  context->frame_valid = 0;
  while (context->transaction_depth)
    free(context->transactions[--context->transaction_depth].commands);
}

void
wr_context_set_viewport(wr_context* context, wr_rect viewport)
{
  if (context && wr_valid_rect(viewport))
    context->viewport = viewport;
  if (context)
    context->frame_valid = 0;
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
    context->frame_valid = 0;
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
