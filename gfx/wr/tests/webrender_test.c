#include "../wr_util.c"
#include "../wr_context.c"
#include "../wr_display_list.c"
#include "../wr_frame.c"
#include "../wr_frame_packet.c"
#include "../wr_retained.c"

#include <assert.h>

static void
test_retained_frame(void)
{
  wr_context* context = wr_context_create(4);
  const wr_frame* first;
  const wr_frame* cached;
  const wr_frame* culled;

  assert(context);
  wr_context_set_viewport(context, (wr_rect){ 0, 0, 100, 100 });
  assert(wr_display_list_push_rect(context, (wr_rect){ 0, 0, 10, 10 },
                                   (wr_color){ 1, 0, 0, 1 }));
  first = wr_context_build_frame(context);
  cached = wr_context_build_frame(context);
  assert(first == cached);
  assert(cached->quad_count == 1 && cached->batch_count == 1);

  wr_context_set_viewport(context, (wr_rect){ 200, 200, 10, 10 });
  culled = wr_context_build_frame(context);
  assert(culled && culled->quad_count == 0);
  wr_context_destroy(context);
}

static void
test_stacking_and_image(void)
{
  wr_context* context = wr_context_create(4);
  const wr_frame* frame;
  const wr_transform translate = { 1, 0, 0, 1, 20, 0 };

  assert(context);
  wr_context_set_viewport(context, (wr_rect){ 0, 0, 100, 100 });
  assert(wr_context_register_image(context, 7, 64, 64));
  assert(wr_display_list_push_stacking_context(context, translate, 0.5f));
  assert(wr_display_list_push_clip(context, (wr_rect){ 15, 0, 30, 30 }));
  assert(wr_display_list_push_rect(context, (wr_rect){ 0, 0, 10, 10 },
                                   (wr_color){ 1, 0, 0, 1 }));
  wr_display_list_pop_stacking_context(context);
  assert(wr_display_list_push_image(
    context, (wr_rect){ 30, 0, 20, 20 }, 7,
    (wr_rect){ 0, 0, 0.5f, 0.5f }, (wr_color){ 0.25f, 0.5f, 0.75f, 1 }));

  frame = wr_context_build_frame(context);
  assert(frame && frame->quad_count == 2 && frame->batch_count == 2);
  assert(frame->quads[0].color.a == 0.5f);
  assert(frame->batches[0].has_clip);
  assert(frame->batches[0].transform.m31 == 20);
  assert(frame->batches[1].kind == WR_PRIMITIVE_IMAGE);
  assert(frame->batches[1].image_key == 7);
  assert(frame->quads[1].color.r == 0.25f);
  assert(frame->quads[1].color.g == 0.5f);
  assert(frame->quads[1].color.b == 0.75f);
  assert(frame->quads[1].tex_rect.width == 0.5f);
  wr_context_unregister_image(context, 7);
  frame = wr_context_build_frame(context);
  assert(frame && frame->quad_count == 1 && frame->batch_count == 1);
  wr_context_clear(context);
  assert(!wr_display_list_push_image(
    context, (wr_rect){ 30, 0, 20, 20 }, 7,
    (wr_rect){ 0, 0, 0.5f, 0.5f }, (wr_color){ 1, 1, 1, 1 }));
  wr_context_destroy(context);
}

static void
test_scene_transactions(void)
{
  wr_context* context = wr_context_create(2);
  const wr_frame* frame;

  assert(context);
  wr_context_set_viewport(context, (wr_rect){ 0, 0, 100, 100 });
  assert(wr_display_list_push_rect(context, (wr_rect){ 0, 0, 10, 10 },
                                   (wr_color){ 1, 0, 0, 1 }));
  frame = wr_context_build_frame(context);
  assert(frame && frame->quad_count == 1);

  assert(wr_context_begin_transaction(context));
  assert(wr_display_list_push_rect(context, (wr_rect){ 20, 0, 10, 10 },
                                   (wr_color){ 0, 1, 0, 1 }));
  wr_context_abort_transaction(context);
  frame = wr_context_build_frame(context);
  assert(frame && frame->quad_count == 1);

  assert(wr_context_begin_transaction(context));
  assert(wr_display_list_push_rect(context, (wr_rect){ 20, 0, 10, 10 },
                                   (wr_color){ 0, 1, 0, 1 }));
  assert(wr_context_begin_transaction(context));
  assert(wr_display_list_push_rect(context, (wr_rect){ 40, 0, 10, 10 },
                                   (wr_color){ 0, 0, 1, 1 }));
  wr_context_abort_transaction(context);
  frame = wr_context_build_frame(context);
  assert(frame && frame->quad_count == 2);
  assert(wr_context_commit_transaction(context));
  frame = wr_context_build_frame(context);
  assert(frame && frame->quad_count == 2);

  assert(!wr_context_commit_transaction(context));
  wr_context_destroy(context);
}

static void
test_multiple_image_resources(void)
{
  wr_context* context = wr_context_create(4);
  const wr_frame* frame;

  assert(context);
  wr_context_set_viewport(context, (wr_rect){ 0, 0, 100, 100 });
  assert(wr_context_register_image(context, 11, 32, 32));
  assert(wr_context_register_image(context, 12, 64, 64));
  assert(wr_display_list_push_image(
    context, (wr_rect){ 0, 0, 20, 20 }, 11,
    (wr_rect){ 0, 0, 1, 1 }, (wr_color){ 1, 1, 1, 1 }));
  assert(wr_display_list_push_image(
    context, (wr_rect){ 20, 0, 20, 20 }, 12,
    (wr_rect){ 0, 0, 1, 1 }, (wr_color){ 1, 1, 1, 1 }));

  frame = wr_context_build_frame(context);
  assert(frame && frame->quad_count == 2 && frame->batch_count == 2);
  assert(frame->batches[0].image_key == 11);
  assert(frame->batches[1].image_key == 12);
  wr_context_clear_images(context);
  frame = wr_context_build_frame(context);
  assert(frame && frame->quad_count == 0 && frame->batch_count == 0);
  wr_context_destroy(context);
}

static void
test_stable_item_updates(void)
{
  wr_context* context = wr_context_create(4);
  const wr_frame* frame;
  const wr_transform identity = { 1, 0, 0, 1, 0, 0 };
  wr_item_id item;

  assert(context);
  wr_context_set_viewport(context, (wr_rect){ 0, 0, 100, 100 });
  item = wr_display_list_push_rect_with_id(
    context, 42, (wr_rect){ 0, 0, 10, 10 }, (wr_color){ 1, 0, 0, 1 });
  assert(item == 42);
  assert(wr_display_list_push_rect_with_id(
    context, 42, (wr_rect){ 20, 0, 10, 10 }, (wr_color){ 0, 1, 0, 1 }) == 0);

  assert(wr_context_begin_transaction(context));
  assert(wr_display_list_update_rect(
    context, item, (wr_rect){ 20, 0, 10, 10 },
    (wr_color){ 0, 1, 0, 1 }, identity));
  wr_context_abort_transaction(context);
  frame = wr_context_build_frame(context);
  assert(frame && frame->quad_count == 1);
  assert(frame->quads[0].rect.x == 0);

  assert(wr_context_begin_transaction(context));
  assert(wr_display_list_update_rect(
    context, item, (wr_rect){ 20, 0, 10, 10 },
    (wr_color){ 0, 1, 0, 1 }, identity));
  assert(wr_context_commit_transaction(context));
  frame = wr_context_build_frame(context);
  assert(frame && frame->quad_count == 1);
  assert(frame->quads[0].rect.x == 20);
  assert(frame->quads[0].item_id == 42);

  assert(wr_context_remove_item(context, item));
  frame = wr_context_build_frame(context);
  assert(frame && frame->quad_count == 0);
  wr_context_destroy(context);
}

static void
test_opaque_occlusion(void)
{
  wr_context* context = wr_context_create(2);
  const wr_frame* frame;

  assert(context);
  wr_context_set_viewport(context, (wr_rect){ 0, 0, 100, 100 });
  assert(wr_display_list_push_rect(
    context, (wr_rect){ 0, 0, 20, 20 }, (wr_color){ 1, 0, 0, 1 }));
  assert(wr_display_list_push_rect(
    context, (wr_rect){ 0, 0, 20, 20 }, (wr_color){ 0, 0, 1, 1 }));
  frame = wr_context_build_frame(context);
  assert(frame && frame->quad_count == 1 && frame->batch_count == 1);
  assert(frame->quads[0].color.b == 1.0f);
  wr_context_destroy(context);
}

static void
test_gpu_tessellated_primitives(void)
{
  wr_context* context = wr_context_create(64);
  const wr_frame* frame;

  assert(context);
  wr_context_set_viewport(context, (wr_rect){ 0, 0, 100, 100 });
  assert(wr_display_list_push_linear_gradient(
    context, (wr_rect){ 0, 0, 40, 20 },
    (wr_color){ 1, 0, 0, 1 }, (wr_color){ 0, 0, 1, 1 }, 1));
  assert(wr_display_list_push_rounded_rect(
    context, (wr_rect){ 0, 20, 40, 20 }, (wr_color){ 0, 1, 0, 1 }, 8));
  frame = wr_context_build_frame(context);
  assert(frame && frame->quad_count == 48);
  wr_context_destroy(context);
}

static void
test_glyph_run(void)
{
  wr_context* context = wr_context_create(4);
  const wr_frame* frame;
  const wr_glyph glyphs[2] = {
    { { 0, 0, 8, 12 }, { 0, 0, 0.25f, 1 } },
    { { 8, 0, 8, 12 }, { 0.25f, 0, 0.25f, 1 } }
  };

  assert(context);
  wr_context_set_viewport(context, (wr_rect){ 0, 0, 100, 100 });
  assert(wr_context_register_image(context, 21, 256, 256));
  assert(wr_display_list_push_glyph_run(
    context, 21, glyphs, 2, (wr_color){ 1, 1, 1, 1 }));
  frame = wr_context_build_frame(context);
  assert(frame && frame->quad_count == 2 && frame->batch_count == 1);
  assert(frame->batches[0].kind == WR_PRIMITIVE_IMAGE);
  wr_context_destroy(context);
}

static void
test_frame_packet(void)
{
  wr_context* context = wr_context_create(2);
  const wr_frame* frame;
  size_t packet_size;
  unsigned char* packet;
  wr_frame_packet_header header;

  assert(context);
  wr_context_set_viewport(context, (wr_rect){ 0, 0, 100, 100 });
  assert(wr_display_list_push_rect(
    context, (wr_rect){ 0, 0, 10, 10 }, (wr_color){ 1, 0, 0, 1 }));
  frame = wr_context_build_frame(context);
  packet_size = wr_frame_packet_size(frame);
  assert(packet_size > sizeof(header));
  packet = (unsigned char*)malloc(packet_size);
  assert(packet);
  assert(wr_frame_serialize(frame, packet, packet_size));
  memcpy(&header, packet, sizeof(header));
  assert(header.version == WR_FRAME_PACKET_VERSION);
  assert(header.quad_count == 1 && header.batch_count == 1);
  free(packet);
  wr_context_destroy(context);
}

int
main(void)
{
  test_retained_frame();
  test_stacking_and_image();
  test_scene_transactions();
  test_multiple_image_resources();
  test_stable_item_updates();
  test_opaque_occlusion();
  test_gpu_tessellated_primitives();
  test_glyph_run();
  test_frame_packet();
  return 0;
}
