#include <Mw/Milsko.h>

void MwLLWaylandRegionInvalidate(MwLL handle) {
	if(!handle->wayland.configured) {
		return;
	}
	wl_region_subtract(handle->wayland.region, 0, 0, handle->wayland.ww, handle->wayland.wh);
}
void MwLLWaylandRegionSetup(MwLL handle) {
	int width  = (handle->wayland.clipping_rect.width == 0) ? handle->wayland.ww : handle->wayland.clipping_rect.width;
	int height = (handle->wayland.clipping_rect.height == 0) ? handle->wayland.wh : handle->wayland.clipping_rect.height;

	if(!handle->wayland.has_decorations && handle->wayland.do_csd && handle->wayland.type == MwLL_WAYLAND_TOPLEVEL) {
		width += CSD_BORDER_FRAME_LEFT + CSD_BORDER_FRAME_RIGHT;
		height += CSD_BORDER_FRAME_TOP + CSD_BORDER_FRAME_BOTTOM;
	}
	if(!handle->wayland.configured) {
		return;
	}

	if(!handle->wayland.parent)
		wl_region_add(handle->wayland.o_region, 0, 0, width, height);

	if(handle->wayland.type == MwLL_WAYLAND_POPUP) {
		wl_region_add(handle->wayland.region, 0, 0, width + abs(handle->wayland.x), height + abs(handle->wayland.y));
	} else {
		wl_region_subtract(handle->wayland.region, 0, 0, width + abs(handle->wayland.x), height + abs(handle->wayland.y));
		wl_region_add(handle->wayland.region, handle->wayland.clipping_rect.x, handle->wayland.clipping_rect.y, handle->wayland.clipping_rect.width, handle->wayland.clipping_rect.height);

		if(!handle->wayland.parent)
			wl_region_add(handle->wayland.region, 0, 0, width, height);
		else {
			if(!handle->wayland.parent->wayland.parent) {
				wl_region_add(handle->wayland.region, 0, 0, width, height);
			}
		}
	}

	if(handle->wayland.type == MwLL_WAYLAND_TOPLEVEL) {
		wl_surface_set_opaque_region(handle->wayland.backbuffer.surface, handle->wayland.o_region);
		wl_surface_set_input_region(handle->wayland.backbuffer.surface, handle->wayland.region);
	}
	wl_surface_set_opaque_region(handle->wayland.framebuffer.surface, handle->wayland.o_region);
	wl_surface_set_input_region(handle->wayland.framebuffer.surface, handle->wayland.region);
}
