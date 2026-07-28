#include <Mw/Milsko.h>

#include <sys/poll.h>
#include <sys/mman.h>

#include "../../../external/stb_ds.h"

wayland_call_table_t wl_call_tbl;
#ifdef MW_VULKAN
MwBool MwWaylandVulkan = MwFALSE;
#endif

MwBool MwWaylandCairoOnly = MwFALSE;

static pthread_mutex_t destroyedWidgetsTableMutex;
/*
 * So Wayland, bless its soul; it keeps using callbacks LONG after they should not only be destroyed but the widget doesn't even exist anymore. Naturally, this causes use after free. so we fight fire with fire in the worst code i've ever written: by storing the freed pointers here, we disallow wayland from ever using them again. if something else is created that takes this slot, we remove it from the table.
 *
 * To illustrate how bad this code is: if Israel and Palestine found out I was doing this then the Israel/Palestine conflict would be solved because both world leaders would come to the conclusion that this code is the worst thing ever.
 */
static MwLL* destroyedWidgetsTable;

MwBool MwLLWaylandWidgetIsDestroyed(MwLL self) {
	int i;
	pthread_mutex_lock(&destroyedWidgetsTableMutex);
	for(i = 0; i < arrlen(destroyedWidgetsTable); i++) {
		if(self == destroyedWidgetsTable[i]) {
			pthread_mutex_unlock(&destroyedWidgetsTableMutex);
			return MwTRUE;
		}
	}
	pthread_mutex_unlock(&destroyedWidgetsTableMutex);
	return MwFALSE;
}
void MwLLWaylandWidgetUndestroy(MwLL self) {
	int i;
	pthread_mutex_lock(&destroyedWidgetsTableMutex);
	for(i = 0; i < arrlen(destroyedWidgetsTable); i++) {
		if(self == destroyedWidgetsTable[i]) {
			arrdel(destroyedWidgetsTable, i);
			break;
		}
	}
	pthread_mutex_unlock(&destroyedWidgetsTableMutex);
	return;
}

static int event_loop(MwLL handle);

/* Recursively dispatch a resize event to a widget and its children */
static void recursive_dispatch_resize(MwLL handle) {
	MwWidget h = (MwWidget)handle->common.user;
	if(h) {
		int i;
		for(i = 0; i < arrlen(h->children); i++) {
			MwDispatch(h->children[i], resize);
			if(arrlen(h->children[i]->children) > 0) {
				recursive_dispatch_resize(h->children[i]->lowlevel);
			}
		}
	}
};

static void recursive_render(MwLL handle) {
	int i;
	WIDGET_CHECK(handle);

	for(i = 0; i < arrlen(((MwWidget)handle->common.user)->children); i++) recursive_render(((MwWidget)handle->common.user)->children[i]->lowlevel);

	MwLLForceRender(handle);
}

/* Flush Wayland events */
void MwLLWaylandFlush(MwLL handle) {
	struct pollfd pfd;
	int	      rc;

	pfd.fd	   = wl_display_get_fd(handle->wayland.display);
	pfd.events = POLLIN;
	while(MwTRUE) {
		rc = wl_display_flush(handle->wayland.display);
		if(errno != EAGAIN || rc != -1) {
			break;
		}

		if(poll(&pfd, 1, -1) == -1) {
			break;
		}
	}
}

/* Get the registered interface from r, or NULL if it doesn't currently have it. */
#define WAYLAND_GET_INTERFACE(handle, inter) shget(handle.wl_protocol_map, inter##_interface.name)

/* Standard Wayland event loop. */
static int event_loop(MwLL handle) {
	struct pollfd	     fd;
	struct timespec	     timeout;
	struct _MwLLWayland* wayland = &handle->wayland;

	timeout.tv_nsec = 1;
	timeout.tv_sec	= 0;

	if(wayland->display == NULL) {
		return 0;
	}
	fd.fd	  = wl_display_get_fd(wayland->display);
	fd.events = POLLIN;

	/* If an error other than EAGAIN happens, we have likely been disconnected from the Wayland session */
	while(wl_display_flush(wayland->display) == -1) {
		if(errno != EAGAIN) {
			return MwFALSE;
		}

		while(poll(&fd, 1, -1) == -1) {
			if(errno != EINTR && errno != EAGAIN) {
				wl_display_cancel_read(wayland->display);

				MwLLDispatch(handle, close, NULL);
				return 0;
			}
		}
	}

	wl_display_prepare_read(handle->wayland.display);
	/* Condition where no events are being sent. */
	if(!poll(&fd, 1, timeout.tv_nsec)) {
		wl_display_cancel_read(wayland->display);

		if(wayland->configured) {
			MwLLWaylandBufferUpdate(handle, &wayland->framebuffer);
			if(wayland->type == MwLL_WAYLAND_TOPLEVEL)
				MwLLWaylandBufferUpdate(handle, &wayland->backbuffer);
		}
		return 0;
	}
	wl_display_read_events(wayland->display);
	if(wl_display_dispatch_pending(wayland->display) < 0) {
		wl_display_cancel_read(wayland->display);
	}
	return 1;
}

/* make the fuck sure that we're configurd */
void MwLLWaylandHangUntilConfigured(MwLL handle) {
	while(!handle->wayland.configured) {
		if(wl_display_roundtrip(handle->wayland.display) == -1) {
			printf("roundtrip failed: %d\n", wl_display_get_error(handle->wayland.display));
			raise(SIGTRAP);
			return;
		}
		event_loop(handle);
	}
}

/* `xdg_toplevel.configure` callback */
static void xdg_toplevel_configure(void*		data,
				   struct xdg_toplevel* xdg_toplevel,
				   MwI32 width, MwI32 height,
				   struct wl_array* states) {
	MwLL self = data;
	(void)states;
	(void)xdg_toplevel;

	/* Yes, somehow this can get called before  */
	if(!self->wayland.configured) {
		return;
	}

	if(width == 0 || height == 0) {
		width  = self->wayland.ww;
		height = self->wayland.wh;
		/* if it's still 0 then bail */
		if(width == 0 || height == 0) {
			return;
		}
	}

	if(self->wayland.type == MwLL_WAYLAND_TOPLEVEL) {
		if(width < 50) width = 50;
		if(height < 50) height = 50;
	}

	MwLLWaylandRegionInvalidate(self);
	self->wayland.ww = width;
	self->wayland.wh = height;
	// if(self->wayland.type == MwLL_WAYLAND_TOPLEVEL) {
	// 	if(!self->wayland.has_decorations && self->wayland.toplevel->maxim_state) {
	// 		self->wayland.ww -= CSD_BORDER_FRAME_LEFT + CSD_BORDER_FRAME_RIGHT;
	// 		self->wayland.wh -= CSD_BORDER_FRAME_TOP + CSD_BORDER_FRAME_BOTTOM;
	// 	}
	// }

	if(self->wayland.resizing == 0) {
		xdg_surface_set_window_geometry(self->wayland.toplevel->xdg_surface, 0, 0, self->wayland.ww, self->wayland.wh);

		MwLLWaylandBackbufferDestroy(&self->wayland);
		MwLLWaylandBackbufferSetup(&self->wayland);
		MwLLWaylandFramebufferDestroy(&self->wayland);
		MwLLWaylandFramebufferSetup(&self->wayland);

		MwLLWaylandRegionSetup(self);
		MwLLDispatch(self, resize, NULL);

		recursive_dispatch_resize(self);

		if(self->wayland.pointer_constrained) {
			zwp_locked_pointer_v1_set_cursor_position_hint(self->wayland.locked_pointer, 0, CSD_BORDER_FRAME_TOP);
			wl_surface_commit(self->wayland.framebuffer.surface);
		}
		self->wayland.resizing = 1;
	}
};

/* `xdg_surface.close` callback */
static void xdg_toplevel_close(
    void* data, struct xdg_toplevel* xdg_toplevel) {
	MwLL self = data;
	(void)xdg_toplevel;
	WAYLAND_EVENT_OP_START(self);
	MwLLDispatch(self, close, NULL);
	WAYLAND_EVENT_OP_END(self);
};

/* `xdg_surface.configure` callback */
static void xdg_surface_configure(
    void* data, struct xdg_surface* xdg_surface, MwU32 serial) {
	MwLL self = data;

	xdg_surface_ack_configure(xdg_surface, serial);

	MwLLDispatch(self, draw, NULL);

	if(self->wayland.configured) {
		MwLLWaylandBufferUpdate(self, &self->wayland.framebuffer);
		MwLLWaylandBufferUpdate(self, &self->wayland.backbuffer);
	}

	self->wayland.configured = MwTRUE;
}

void MwLLWaylandBufferUpdate(MwLL self, struct _MwLLWaylandShmBuffer* buffer) {
	if(self->wayland.configured) {
		memcpy(buffer->buf, buffer->buf_back, buffer->buf_size);
		if(buffer->surface) {
			// Yes this is needed every time, it's how we fix weston.
			if(self->wayland.configured)
				wl_surface_attach(buffer->surface, buffer->shm_buffer, 0, 0);
			wl_surface_commit(buffer->surface);
		}
	}
}

/* Toplevel setup function */
static void setup_toplevel(MwLL r, int x, int y) {
	r->wayland.type	    = MwLL_WAYLAND_TOPLEVEL;
	r->wayland.toplevel = malloc(sizeof(struct _MwLLWaylandTopLevel));
	r->wayland.x	    = x;
	r->wayland.y	    = y;

	MwLLWaylandSetupCallbacks(&r->wayland);

	/* Connect to the Wayland compositor */
	if(!r->wayland.display) {
		r->wayland.display = wl_display_connect(NULL);
		if(r->wayland.display == NULL) {
			printf("wayland: failed to create display\n");
			raise(SIGTRAP);
			return;
		}
	}

	/* Do a roundtrip to ensure all interfaces are setup. */
	r->wayland.registry = wl_display_get_registry(r->wayland.display);
	wl_registry_add_listener(r->wayland.registry, &r->wayland.registry_listener, r);
	if(wl_display_roundtrip(r->wayland.display) == -1) {
		printf("roundtrip failed\n");
		raise(SIGTRAP);
		return;
	}

	/* check now if we have decorations so that everything else can act accordingly */
	if(shget(r->wayland.wl_protocol_map, zxdg_decoration_manager_v1_interface.name) != NULL) {
		r->wayland.has_decorations = MwTRUE;
	} else {
		r->wayland.do_csd = MwTRUE;
	}

	r->wayland.xkb_context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);

	/* Create a wl_surface, a xdg_surface and a xdg_toplevel */
	r->wayland.framebuffer.surface = wl_compositor_create_surface(r->wayland.compositor);
	r->wayland.backbuffer.surface  = wl_compositor_create_surface(r->wayland.compositor);
	r->wayland.toplevel->ssurface  = wl_subcompositor_get_subsurface(r->wayland.toplevel->scompositor, r->wayland.framebuffer.surface, r->wayland.backbuffer.surface);
	wl_subsurface_set_desync(r->wayland.toplevel->ssurface);

	r->wayland.toplevel->xdg_surface =
	    xdg_wm_base_get_xdg_surface(WAYLAND_GET_INTERFACE(r->wayland, xdg_wm_base)->context, r->wayland.backbuffer.surface);
	r->wayland.toplevel->xdg_top_level = xdg_surface_get_toplevel(r->wayland.toplevel->xdg_surface);

	/* setup mandatory listeners */
	r->wayland.toplevel->xdg_surface_listener.configure = xdg_surface_configure;

	r->wayland.toplevel->xdg_toplevel_listener.configure = xdg_toplevel_configure;
	r->wayland.toplevel->xdg_toplevel_listener.close     = xdg_toplevel_close;

	xdg_surface_add_listener(r->wayland.toplevel->xdg_surface, &r->wayland.toplevel->xdg_surface_listener, r);
	xdg_toplevel_add_listener(r->wayland.toplevel->xdg_top_level, &r->wayland.toplevel->xdg_toplevel_listener,
				  r);

	xdg_toplevel_set_title(r->wayland.toplevel->xdg_top_level, "Milsko App");
	xdg_toplevel_set_app_id(r->wayland.toplevel->xdg_top_level, "MilskoWaylandApp");

	/* Perform the initial commit and wait for the first configure event */
	wl_surface_commit(r->wayland.backbuffer.surface);
	wl_surface_commit(r->wayland.framebuffer.surface);

	while(!r->wayland.configured) {
		if(wl_display_roundtrip(r->wayland.display) == -1) {
			printf("roundtrip failed\n");
			raise(SIGTRAP);
			return;
		}
	}

	/* setup decorations if we can */
	if(r->wayland.has_decorations) {
		zxdg_decoration_manager_v1_context_t* dec = WAYLAND_GET_INTERFACE(r->wayland, zxdg_decoration_manager_v1)->context;

		dec->decoration = zxdg_decoration_manager_v1_get_toplevel_decoration(
		    dec->manager, r->wayland.toplevel->xdg_top_level);

		zxdg_toplevel_decoration_v1_set_mode(
		    dec->decoration, ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
	} else {
		wl_subsurface_set_position(r->wayland.toplevel->ssurface, r->wayland.do_csd ? CSD_BORDER_FRAME_LEFT : 0, r->wayland.do_csd ? CSD_BORDER_FRAME_TOP : 0);
	}

	MwLLWaylandFramebufferSetup(&r->wayland);
	MwLLWaylandBackbufferSetup(&r->wayland);
}

/* Toplevel destroy function */
static void destroy_toplevel(MwLL r) {
	if(shget(r->wayland.wl_protocol_map, zxdg_decoration_manager_v1_interface.name) != NULL) {
		zxdg_decoration_manager_v1_context_t* dec = WAYLAND_GET_INTERFACE(r->wayland, zxdg_decoration_manager_v1)->context;
		zxdg_toplevel_decoration_v1_destroy(dec->decoration);
		zxdg_decoration_manager_v1_destroy(dec->manager);
	}

	MwLLWaylandFramebufferDestroy(&r->wayland);
	MwLLWaylandBackbufferDestroy(&r->wayland);

	xdg_toplevel_destroy(r->wayland.toplevel->xdg_top_level);

	xdg_surface_destroy(r->wayland.toplevel->xdg_surface);

	wl_compositor_destroy(r->wayland.compositor);

	xkb_keymap_unref(r->wayland.xkb_keymap);

	xkb_state_unref(r->wayland.xkb_state);

	xkb_context_unref(r->wayland.xkb_context);

	wl_pointer_destroy(r->wayland.pointer);
	wl_keyboard_destroy(r->wayland.keyboard);

	if(r->common.user) {
		MwWidget w = r->common.user;
		int	 i;
		for(i = 0; i < arrlen(w->children); i++) {
			if(w->children[i]->lowlevel->wayland.type == MwLL_WAYLAND_SUBLEVEL) {
				MwLL child = w->children[i]->lowlevel;
				wl_subsurface_destroy(child->wayland.sublevel->subsurface);
				child->wayland.sublevel->subsurface = NULL;
			}
		}
	}

	wl_surface_destroy(r->wayland.framebuffer.surface);

	free(r->wayland.toplevel);

	r->wayland.configured = MwFALSE;
}

/* Sublevel setup function */
static void setup_sublevel(MwLL parent, MwLL r, int x, int y) {
	struct wl_compositor* compositor     = parent->wayland.compositor;
	struct wl_surface*    parent_surface = parent->wayland.framebuffer.surface;

	r->wayland.sublevel	    = malloc(sizeof(struct _MwLLWaylandSublevel));
	r->wayland.sublevel->parent = parent;

	r->wayland.type = MwLL_WAYLAND_SUBLEVEL;

	r->wayland.display = parent->wayland.display;

	MwLLWaylandSetupCallbacks(&r->wayland);

	r->wayland.registry = wl_display_get_registry(parent->wayland.display);
	r->wayland.display  = parent->wayland.display;

	if(parent->wayland.type == MwLL_WAYLAND_TOPLEVEL) {
		r->wayland.sublevel->xdg_surface = parent->wayland.toplevel->xdg_surface;
	} else {
		r->wayland.sublevel->xdg_surface = parent->wayland.sublevel->xdg_surface;
	}

	wl_registry_add_listener(r->wayland.registry, &r->wayland.registry_listener, r);
	if(wl_display_roundtrip(r->wayland.display) == -1) {
		printf("roundtrip failed: %d\n", wl_display_get_error(r->wayland.display));
		raise(SIGTRAP);
		return;
	}

	r->wayland.framebuffer.surface = wl_compositor_create_surface(compositor);

	r->wayland.sublevel->subsurface = wl_subcompositor_get_subsurface(r->wayland.sublevel->subcompositor, r->wayland.framebuffer.surface, parent_surface);

	wl_subsurface_set_desync(r->wayland.sublevel->subsurface);
	wl_subsurface_set_position(r->wayland.sublevel->subsurface, x, y);

	if(parent) {
		wl_subsurface_place_above(r->wayland.sublevel->subsurface, parent->wayland.framebuffer.surface);
	}

	r->wayland.xkb_keymap = parent->wayland.xkb_keymap;
	r->wayland.xkb_state  = parent->wayland.xkb_state;

	r->wayland.configured = MwTRUE;

	MwLLWaylandFramebufferSetup(&r->wayland);
	MwLLWaylandBackbufferSetup(&r->wayland);
}

/* Sublevel setup function */
static void destroy_sublevel(MwLL r) {
	MwLLWaylandBackbufferDestroy(&r->wayland);
	MwLLWaylandFramebufferDestroy(&r->wayland);

	wl_subsurface_destroy(r->wayland.sublevel->subsurface);

	free(r->wayland.sublevel);

	r->wayland.configured = MwFALSE;
}

static void popup_configure(void*	      data,
			    struct xdg_popup* xdg_popup,
			    int32_t	      x,
			    int32_t	      y,
			    int32_t	      width,
			    int32_t	      height) {
	MwLL self = data;
	(void)xdg_popup;

	if(width < 50) width = 50;
	if(height < 50) height = 50;
	self->wayland.x	 = x;
	self->wayland.y	 = y;
	self->wayland.ww = width;
	self->wayland.wh = height;
};

static void popup_done(void*		 data,
		       struct xdg_popup* xdg_popup) {
	(void)data;
	(void)xdg_popup;
};

static void popup_repositioned(void*		 data,
			       struct xdg_popup* xdg_popup,
			       uint32_t		 token) {
	MwLL self = data;
	(void)xdg_popup;

	xdg_surface_ack_configure(self->wayland.popup->xdg_surface, token);
};

struct xdg_popup_listener popup_listener = {
    .configure	  = popup_configure,
    .popup_done	  = popup_done,
    .repositioned = popup_repositioned,

};

/* Popup setup function */
static void setup_popup(MwLL r, int x, int y, MwLL parent) {
	MwLL topmost_parent = r->wayland.parent;
	r->wayland.type	    = MwLL_WAYLAND_POPUP;
	r->wayland.x	    = x;
	r->wayland.y	    = y;
	r->wayland.popup    = malloc(sizeof(struct _MwLLWaylandPopup));

	if(parent) {
		MwWidget p = parent->common.user;
		while(topmost_parent->wayland.parent) {
			topmost_parent = topmost_parent->wayland.parent;
		}

		if(!topmost_parent->wayland.has_decorations && topmost_parent->wayland.do_csd) {
			if(p->is_menu) {
				r->wayland.x += round((float)CSD_BORDER_FRAME_LEFT / 2.);
				r->wayland.y += ceil((float)CSD_BORDER_FRAME_TOP / 2.);
			}
		}
	}

	MwLLWaylandSetupCallbacks(&r->wayland);

	/* Connect to the Wayland compositor */
	r->wayland.display = parent->wayland.display;

	/* Do a roundtrip to ensure all interfaces are setup. */
	r->wayland.registry = wl_display_get_registry(r->wayland.display);
	wl_registry_add_listener(r->wayland.registry, &r->wayland.registry_listener, r);
	if(wl_display_roundtrip(r->wayland.display) == -1) {
		printf("roundtrip failed: %d\n", wl_display_get_error(r->wayland.display));
		raise(SIGTRAP);
		return;
	}

	/* check now if we have decorations so that everything else can act accordingly */
	if(shget(r->wayland.wl_protocol_map, zxdg_decoration_manager_v1_interface.name) != NULL) {
		r->wayland.has_decorations = MwTRUE;
	} else {
		r->wayland.do_csd = MwTRUE;
	}

	r->wayland.popup->xdg_wm_base = WAYLAND_GET_INTERFACE(r->wayland, xdg_wm_base)->context;

	r->wayland.popup->xdg_positioner = xdg_wm_base_create_positioner(r->wayland.popup->xdg_wm_base);

	xdg_positioner_set_size(r->wayland.popup->xdg_positioner, r->wayland.ww, r->wayland.wh);
	xdg_positioner_set_anchor_rect(
	    r->wayland.popup->xdg_positioner,
	    topmost_parent->wayland.x, topmost_parent->wayland.y, r->wayland.ww, r->wayland.wh);
	xdg_positioner_set_offset(
	    r->wayland.popup->xdg_positioner,
	    r->wayland.x, r->wayland.y);

	xdg_positioner_set_anchor(r->wayland.popup->xdg_positioner, XDG_POSITIONER_ANCHOR_NONE);
	xdg_positioner_set_gravity(r->wayland.popup->xdg_positioner, XDG_POSITIONER_GRAVITY_NONE);

	r->wayland.xkb_keymap = topmost_parent->wayland.xkb_keymap;
	r->wayland.xkb_state  = topmost_parent->wayland.xkb_state;

	r->wayland.framebuffer.surface = wl_compositor_create_surface(r->wayland.compositor);

	r->wayland.popup->xdg_surface =
	    xdg_wm_base_get_xdg_surface(WAYLAND_GET_INTERFACE(r->wayland, xdg_wm_base)->context, r->wayland.framebuffer.surface);

	if(topmost_parent->wayland.type == MwLL_WAYLAND_TOPLEVEL) {
		r->wayland.popup->xdg_popup = xdg_surface_get_popup(
		    r->wayland.popup->xdg_surface,
		    topmost_parent->wayland.toplevel->xdg_surface,
		    r->wayland.popup->xdg_positioner);
	} else if(topmost_parent->wayland.type == MwLL_WAYLAND_LAYER_SURFACE) {
		r->wayland.popup->xdg_popup = xdg_surface_get_popup(
		    r->wayland.popup->xdg_surface, NULL,
		    r->wayland.popup->xdg_positioner);
		zwlr_layer_surface_v1_get_popup(topmost_parent->wayland.layer_surface->surface, r->wayland.popup->xdg_popup);
	}

	xdg_popup_add_listener(r->wayland.popup->xdg_popup, &popup_listener, r);

	r->wayland.popup->xdg_surface_listener.configure = xdg_surface_configure;
	xdg_surface_add_listener(r->wayland.popup->xdg_surface, &r->wayland.popup->xdg_surface_listener, r);

	if(wl_display_roundtrip(r->wayland.display) == -1) {
		printf("roundtrip failed\n");
		raise(SIGTRAP);
		return;
	}

	MwLLWaylandFramebufferSetup(&r->wayland);
}

/* Popup destroy function */
static void destroy_popup(MwLL r) {

	MwLLWaylandBackbufferDestroy(&r->wayland);
	MwLLWaylandFramebufferDestroy(&r->wayland);

	xdg_surface_destroy(r->wayland.popup->xdg_surface);

	xdg_popup_destroy(r->wayland.popup->xdg_popup);

	xdg_positioner_destroy(r->wayland.popup->xdg_positioner);

	wl_surface_destroy(r->wayland.framebuffer.surface);

	wl_registry_destroy(r->wayland.registry);

	free(r->wayland.popup);

	r->wayland.configured = MwFALSE;
}

static void layer_shell_configure(void*				data,
				  struct zwlr_layer_surface_v1* surface,
				  uint32_t			serial,
				  uint32_t			width,
				  uint32_t			height) {
	MwLL self = data;
	zwlr_layer_surface_v1_ack_configure(surface, serial);

	if(width < 50) width = 50;
	if(height < 50) height = 50;

	self->wayland.ww = width;
	self->wayland.wh = height;

	self->wayland.configured = MwTRUE;

	MwLLWaylandFramebufferSetup(&self->wayland);
	MwLLWaylandRegionSetup(self);
	MwLLDispatch(self, resize, NULL);

	wl_surface_commit(self->wayland.framebuffer.surface);
};

static void layer_shell_done(void*			   data,
			     struct zwlr_layer_surface_v1* zwlr_layer_surface_v1) {
	(void)data;
	(void)zwlr_layer_surface_v1;

	printf("closed\n");
};

static const struct zwlr_layer_surface_v1_listener layer_surface_listener = {
    .configure = layer_shell_configure,
    .closed    = layer_shell_done,
};

/* Layer surface setup function */
static void setup_layer_surface(MwLL r, int x, int y, int width, int height) {
	wayland_protocol_t*	    layer_shell_protocol;
	struct zwlr_layer_shell_v1* layer_shell;
	r->wayland.type		 = MwLL_WAYLAND_LAYER_SURFACE;
	r->wayland.layer_surface = malloc(sizeof(struct _MwLLWaylandLayerSurface));
	r->wayland.x		 = x;
	r->wayland.y		 = y;

	MwLLWaylandSetupCallbacks(&r->wayland);

	/* Connect to the Wayland compositor */
	if(!r->wayland.display) {
		r->wayland.display = wl_display_connect(NULL);
		if(r->wayland.display == NULL) {
			printf("wayland: failed to create display\n");
			raise(SIGTRAP);
			return;
		}
	}

	/* Do a roundtrip to ensure all interfaces are setup. */
	r->wayland.registry = wl_display_get_registry(r->wayland.display);
	wl_registry_add_listener(r->wayland.registry, &r->wayland.registry_listener, r);
	if(wl_display_roundtrip(r->wayland.display) == -1) {
		printf("roundtrip failed\n");
		raise(SIGTRAP);
		return;
	}

	layer_shell_protocol = WAYLAND_GET_INTERFACE(r->wayland, zwlr_layer_shell_v1);
	assert(layer_shell_protocol);

	layer_shell = layer_shell_protocol->context;

	r->wayland.xkb_context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);

	r->wayland.framebuffer.surface = wl_compositor_create_surface(r->wayland.compositor);

	r->wayland.layer_surface->surface = zwlr_layer_shell_v1_get_layer_surface(layer_shell, r->wayland.framebuffer.surface, NULL, ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY, "milsko-surfaces");

	zwlr_layer_surface_v1_set_size(r->wayland.layer_surface->surface, width, height);
	zwlr_layer_surface_v1_set_margin(r->wayland.layer_surface->surface,
					 y, 0, 0, x);
	zwlr_layer_surface_v1_set_anchor(r->wayland.layer_surface->surface, ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP | ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT);
	zwlr_layer_surface_v1_set_exclusive_zone(r->wayland.layer_surface->surface, 1);
	zwlr_layer_surface_v1_set_keyboard_interactivity(
	    r->wayland.layer_surface->surface, ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_ON_DEMAND);

	zwlr_layer_surface_v1_add_listener(r->wayland.layer_surface->surface, &layer_surface_listener, r);

	/* Perform the initial commit and wait for the first configure event */
	wl_surface_commit(r->wayland.framebuffer.surface);

	if(wl_display_roundtrip(r->wayland.display) == -1) {
		printf("roundtrip failed\n");
		raise(SIGTRAP);
		return;
	}
}

/* Toplevel destroy function */
static void destroy_layer_surface(MwLL r) {
	if(shget(r->wayland.wl_protocol_map, zxdg_decoration_manager_v1_interface.name) != NULL) {
		zxdg_decoration_manager_v1_context_t* dec = WAYLAND_GET_INTERFACE(r->wayland, zxdg_decoration_manager_v1)->context;
		zxdg_decoration_manager_v1_destroy(dec->manager);
	}

	wl_surface_destroy(r->wayland.framebuffer.surface);

	wl_compositor_destroy(r->wayland.compositor);

	xkb_keymap_unref(r->wayland.xkb_keymap);

	xkb_state_unref(r->wayland.xkb_state);

	xkb_context_unref(r->wayland.xkb_context);

	wl_registry_destroy(r->wayland.registry);

	r->wayland.configured = MwFALSE;
}

static void destroy_widget(MwLL handle) {
	int i;

	MwLLShow(handle, MwFALSE);

	switch(handle->wayland.type) {
	case MwLL_WAYLAND_TOPLEVEL:
		destroy_toplevel(handle);
		break;
	case MwLL_WAYLAND_SUBLEVEL:
		destroy_sublevel(handle);
		break;
	case MwLL_WAYLAND_POPUP:
		destroy_popup(handle);
		break;
	case MwLL_WAYLAND_LAYER_SURFACE:
		destroy_layer_surface(handle);
		break;
	default:
		printf("Handle with unknown type(%d) tried to be destroyed (%p)\n", handle->wayland.type, handle);
		break;
	}

	if(wl_display_roundtrip(handle->wayland.display) == -1) {
		printf("roundtrip failed\n");
		raise(SIGTRAP);
		return;
	}

	for(i = 0; i < shlen(handle->wayland.wl_protocol_setup_map); i++) {
		void* ctx = shget(handle->wayland.wl_protocol_map, handle->wayland.wl_protocol_setup_map[i].key);

		if(ctx != NULL) {
			handle->wayland.wl_protocol_setup_map[i].value->destroy(&handle->wayland, ctx);
		}
		shdel(handle->wayland.wl_protocol_map, handle->wayland.wl_protocol_setup_map[i].value);
		free(handle->wayland.wl_protocol_setup_map[i].value);
	}

	shfree(handle->wayland.wl_protocol_map);
	shfree(handle->wayland.wl_protocol_setup_map);

	MwLLWaylandFlush(handle);
}

static void clip(MwLL handle) {
	int   i;
	int   x, y, cx, cy, mx, my;
	MwLL  toplevel = handle->wayland.parent;
	MwLL* ws       = NULL;

	arrput(ws, handle);
	while(toplevel) {
		arrput(ws, toplevel);
		if(!toplevel->wayland.parent || toplevel->wayland.is_toplevel) {
			break;
		}
		toplevel = toplevel->wayland.parent;
	}
	if(toplevel) {

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
		/* i seriously have no fucking idea how this works, do not ask me how this really works (nishi) */
		x  = 0;
		y  = 0;
		cx = 0;
		cy = 0;
		mx = toplevel->wayland.ww;
		my = toplevel->wayland.wh;
		// ws is traversed result
		// ws[ws.length - 1] is ignored bc it's toplevel
		for(i = arrlen(ws) - 2; i >= 0; i--) {
			int j;
			int l = 0;

			x += ws[i]->wayland.x;
			y += ws[i]->wayland.y;

			for(j = i - 1; j >= 0; j--) {
				if(ws[j]->wayland.x > cx) l = 1;
				if(ws[j]->wayland.y > cy) l = 1;
				if(l) break;
			}

			if(!l) {
				cx = MAX(cx, ws[i]->wayland.x);
				cy = MAX(cy, ws[i]->wayland.y);
			}

			mx = MIN(mx, x + ws[i]->wayland.ww);
			my = MIN(my, y + ws[i]->wayland.wh);
		}

		if(mx < cx) mx = cx;
		if(my < cy) my = cy;

		arrfree(ws);

		if(mx < cx) mx = cx;
		if(my < cy) my = cy;

		handle->wayland.clipping_rect.x	     = cx - x;
		handle->wayland.clipping_rect.y	     = cy - y;
		handle->wayland.clipping_rect.width  = mx - cx;
		handle->wayland.clipping_rect.height = my - cy;
		MwLLWaylandRegionSetup(handle);

		cairo_reset_clip(handle->wayland.cairo.front_cairo);
		if(handle->wayland.type == MwLL_WAYLAND_SUBLEVEL && !handle->wayland.is_toplevel) {
			cairo_rectangle(handle->wayland.cairo.front_cairo, cx - x, cy - y, mx - cx, my - cy);
			cairo_clip(handle->wayland.cairo.front_cairo);
		}

		if(handle->wayland.is_clipping) {
			cairo_rectangle(handle->wayland.cairo.front_cairo, handle->wayland.clip.x, handle->wayland.clip.y, handle->wayland.clip.width, handle->wayland.clip.height);
			cairo_clip(handle->wayland.cairo.front_cairo);
		}
	}
}

static void wl_logger(const char* fmt, va_list args) {
	vprintf(fmt, args);
	raise(SIGTRAP);
}

static void widget_setup(MwLL r, MwLL parent, int x, int y, int width, int height, enum _MwLLWaylandType ty) {
	/* Wayland does not report global coordinates ever. Compositors are not even expected to have knowledge of this.
	 */
	r->common.coordinate_type = MwCOORDINATE_LOCAL;

	r->common.type = MwLLBackendWayland;

	if(width < 2) width = 2;
	if(height < 2) height = 2;

	if(x == MwDEFAULT) {
		x = 0;
	}
	if(y == MwDEFAULT) {
		y = 0;
	}

	r->wayland.ww	  = width;
	r->wayland.wh	  = height;
	r->wayland.x	  = x;
	r->wayland.y	  = y;
	r->wayland.parent = parent;
	if(MwLLWaylandWidgetIsDestroyed(parent)) {
		r->wayland.valid = MwFALSE;
		return;
	} else {
		r->wayland.valid = MwTRUE;
	}

	if(ty == MwLL_WAYLAND_UNKNOWN) {
		if(parent == NULL) {
			setup_toplevel(r, x, y);
		} else {
			setup_sublevel(parent, r, x, y);
		}
	} else {
		switch(ty) {
		case MwLL_WAYLAND_UNKNOWN:
			break;
		case MwLL_WAYLAND_TOPLEVEL:
			setup_toplevel(r, x, y);
			break;
		case MwLL_WAYLAND_SUBLEVEL:
			setup_sublevel(parent, r, x, y);
			break;
		case MwLL_WAYLAND_POPUP:
			if(parent) {
				setup_popup(r, x, y, parent);
			} else {
				/*
				 * We can't actually have a popup without a parent but this needs to be valid behavior.
				 * If we're down this code path, though, it means we can't create a layer surface.
				 * So we just fake it with a borderless window.
				 */
				setup_toplevel(r, x, y);
				MwLLMakeBorderless(r, MwTRUE);
			}
			break;
		case MwLL_WAYLAND_LAYER_SURFACE:
			setup_layer_surface(r, x, y, width, height);
			break;
		}
	}

	WIDGET_CHECK(r);

	r->wayland.region   = wl_compositor_create_region(r->wayland.compositor);
	r->wayland.o_region = wl_compositor_create_region(r->wayland.compositor);
	MwLLWaylandRegionSetup(r);

	MwLLForceRender(r);
	if(parent != NULL) {
		MwLLForceRender(parent);
	}

	/* If the compositor never sets the keyboard repeat info, we'll copy KDE's default of... */
	r->wayland.keyboard_delay = 600; /* starting at 600ms... */
	r->wayland.keyboard_rate  = 25;	 /* and going every 25ms... */

	if(!r->wayland.has_decorations && r->wayland.do_csd) {
		MwLLBeginDraw(r);
		MwLLEndDraw(r);
	}
}

#ifdef USE_DBUS
static void dark_theme_listener(MwLL handle, MwU32 new_value) {
	handle->wayland.dark_theme = (new_value == 1) ? 1 : 0;

	MwLLDispatch(handle, dark_theme, &handle->wayland.dark_theme);
}

static void detect_dark_theme(MwLL handle) {
	MwU32 value = 0;

	MwLLDBusPortalGet(&wl_call_tbl.dbus, &handle->wayland.dbus, "org.freedesktop.portal.Settings", "org.freedesktop.appearance", "color-scheme", &value);
	handle->wayland.dark_theme = (value == 1) ? 1 : 0;

	MwLLDBusPortalWatch(&wl_call_tbl.dbus, &handle->wayland.dbus, "org.freedesktop.portal.Settings");

	MwLLDispatch(handle, dark_theme, &handle->wayland.dark_theme);
}
#endif

static MwLL MwLLCreateImpl(MwLL parent, int x, int y, int width, int height) {
	MwLL r;
	r = malloc(sizeof(*r));
	memset(r, 0, sizeof(*r));
	MwLLCreateCommon(r);

	if(MwLLWaylandWidgetIsDestroyed(r)) {
		MwLLWaylandWidgetUndestroy(r);
	}

	r->wayland.is_toplevel = parent == NULL;
	r->wayland.is_clipping = 0;

	widget_setup(r, parent, x, y, width, height, MwLL_WAYLAND_UNKNOWN);

#ifdef USE_DBUS
	if(!parent && wl_call_tbl.has_dbus) {
		wl_call_tbl.has_dbus		= MwLLDBusNewContext(&wl_call_tbl.dbus, &r->wayland.dbus);
		r->wayland.dark_theme_detection = MwTRUE;
		r->wayland.dark_theme		= -1;
	}
#endif

	return r;
}

static void MwLLDestroyImpl(MwLL handle) {
	int	       i;
	struct timeval tv;
	int	       select_ret;

	event_loop(handle);
	// wl_display_cancel_read(handle->wayland.display);

	MwLLWaylandFlush(handle);

	handle->wayland.cancelEvent = MwTRUE;

	MwLLDestroyCommon(handle);

	tv.tv_sec  = 0;
	tv.tv_usec = 1000;
	do {
		select_ret = select(1, NULL, NULL, NULL, &tv);
	} while((select_ret == -1) && (errno == EINTR));

	pthread_mutex_destroy(&handle->wayland.eventsMutex);

#ifdef USE_DBUS
	if(!wl_call_tbl.has_dbus) {
		MwLLDBusFreeContext(&wl_call_tbl.dbus, &handle->wayland.dbus);
	}
#endif

	if(handle->wayland.valid) {
		MwLLWaylandBufferDestroy(&handle->wayland.cursor);
		wl_region_destroy(handle->wayland.region);

		if(handle->wayland.supports_zwp) {
			zwp_primary_selection_source_v1_destroy(handle->wayland.clipboard_source.zwp);
		} else {
			wl_data_source_destroy(handle->wayland.clipboard_source.wl);
		}
		if(handle->wayland.icon != NULL) {
			MwLLWaylandBufferDestroy(handle->wayland.icon);
			wl_surface_destroy(handle->wayland.icon->surface);
		}

		destroy_widget(handle);
	} else {
		printf("widget invalid\n");
	}

	pthread_mutex_lock(&destroyedWidgetsTableMutex);
	arrput(destroyedWidgetsTable, handle);
	pthread_mutex_unlock(&destroyedWidgetsTableMutex);

	free(handle);

	// if(topmost_parent->wayland.currentlyHeldWidget == handle) {
	// 	topmost_parent->wayland.currentlyHeldWidget = NULL;
	// }
}

static void MwLLGetXYWHImpl(MwLL handle, int* x, int* y, unsigned int* w, unsigned int* h) {
	if(!handle->wayland.valid) {
		*x = 0;
		*y = 0;
		*w = 1;
		*h = 1;
		return;
	}
	*x = handle->wayland.x;
	*y = handle->wayland.y;
	*w = handle->wayland.ww;
	*h = handle->wayland.wh;
}

static void MwLLSetXYImpl(MwLL handle, int x, int y) {
	WIDGET_CHECK(handle);
	MwLLWaylandRegionInvalidate(handle);
	handle->wayland.x = x;
	handle->wayland.y = y;

	switch(handle->wayland.type) {
	case MwLL_WAYLAND_TOPLEVEL:
		/* toplevels cannot be moved */
		break;
	case MwLL_WAYLAND_SUBLEVEL:
		wl_subsurface_set_position(handle->wayland.sublevel->subsurface, x, y);
		break;
	case MwLL_WAYLAND_POPUP:
		xdg_positioner_set_anchor_rect(handle->wayland.popup->xdg_positioner, handle->wayland.parent->wayland.x, handle->wayland.parent->wayland.y, handle->wayland.ww, handle->wayland.wh);
		xdg_positioner_set_offset(handle->wayland.popup->xdg_positioner, x, y);
		xdg_popup_reposition(handle->wayland.popup->xdg_popup, handle->wayland.popup->xdg_positioner, 0);
		xdg_surface_set_window_geometry(handle->wayland.popup->xdg_surface, 0, 0, handle->wayland.ww, handle->wayland.wh);
		break;
	case MwLL_WAYLAND_LAYER_SURFACE:
		zwlr_layer_surface_v1_set_margin(handle->wayland.layer_surface->surface,
						 y, 0, 0, x);
		break;
	case MwLL_WAYLAND_UNKNOWN:
		break;
	}
	MwLLWaylandRegionSetup(handle);

	if(handle->wayland.type != MwLL_WAYLAND_TOPLEVEL) recursive_render(handle);

	MwLLDispatch(handle, draw, NULL);
}

static void actually_set_wh(MwLL handle) {
	MwLLWaylandRegionInvalidate(handle);

	if(handle->wayland.type == MwLL_WAYLAND_TOPLEVEL && handle->wayland.configured) {
		xdg_surface_set_window_geometry(handle->wayland.toplevel->xdg_surface, 0, 0, handle->wayland.ww, handle->wayland.wh);
	}

	if(handle->wayland.type == MwLL_WAYLAND_SUBLEVEL && handle->wayland.configured && !handle->wayland.dispatching_resize) {
		handle->wayland.dispatching_resize = MwTRUE;
		MwLLDispatch(handle, resize, NULL);
		handle->wayland.dispatching_resize = MwFALSE;
	}

	if(handle->wayland.type == MwLL_WAYLAND_POPUP) {
		destroy_widget(handle);
		MwLLWaylandFlush(handle);
		setup_popup(handle, handle->wayland.x, handle->wayland.y, handle->wayland.parent);
	}

	MwLLWaylandRegionSetup(handle);
	if(handle->wayland.type == MwLL_WAYLAND_SUBLEVEL) recursive_render(handle);

	MwLLWaylandFramebufferDestroy(&handle->wayland);
	MwLLWaylandFramebufferSetup(&handle->wayland);
	MwLLWaylandBackbufferDestroy(&handle->wayland);
	MwLLWaylandBackbufferSetup(&handle->wayland);
	MwLLDispatch(handle, draw, NULL);
}

static void MwLLSetWHImpl(MwLL handle, int w, int h) {
	WIDGET_CHECK(handle);

	if(handle->wayland.ww == w && handle->wayland.wh == h) {
		return;
	}

	/* Prevent an integer underflow when the w/h is too low */
	if(handle->wayland.type == MwLL_WAYLAND_TOPLEVEL) {
		if(w < 50) w = 50;
		if(h < 50) h = 50;
	}

	handle->wayland.ww = w;
	handle->wayland.wh = h;

	if(!handle->wayland.setting_wh) {
		handle->wayland.setting_wh = 1;
	}

	handle->wayland.events_pending = 1;
}

typedef struct {
	double r, g, b;
} float_color;
static float_color lerp_color(float_color a, float_color b, float t) {
	return (float_color){
	    a.r + (b.r - a.r) * t,
	    a.g + (b.g - a.g) * t,
	    a.b + (b.b - a.b) * t};
}
const float_color one = {.2627451, .37254902, .49411765};
const float_color two = {.05490196, .17254902, .30588235};

static void MwLLBeginDrawImpl(MwLL handle) {
	WIDGET_CHECK(handle);
	cairo_save(handle->wayland.cairo.front_cairo);
	cairo_set_source_rgba(handle->wayland.cairo.front_cairo, 0, 0, 0, 0);
	cairo_set_operator(handle->wayland.cairo.front_cairo, CAIRO_OPERATOR_SOURCE);
	cairo_rectangle(handle->wayland.cairo.front_cairo, 0, 0, handle->wayland.ww, handle->wayland.wh);
	cairo_fill(handle->wayland.cairo.front_cairo);
	cairo_restore(handle->wayland.cairo.front_cairo);

	if(handle->wayland.type != MwLL_WAYLAND_TOPLEVEL) {
		handle->wayland.cairo.selected_cairo = handle->wayland.cairo.front_cairo;
		return;
	}

	if(!handle->wayland.has_decorations && handle->wayland.do_csd) {
		int    y;
		MwU32  w = handle->wayland.ww + CSD_BORDER_FRAME_LEFT + CSD_BORDER_FRAME_RIGHT;
		MwU32  h = handle->wayland.wh + CSD_BORDER_FRAME_TOP + CSD_BORDER_FRAME_BOTTOM;
		MwRect r;
		handle->wayland.cairo.selected_cairo = handle->wayland.cairo.back_cairo;

#define DO_BOX(x, y, col, w, h) \
	cairo_rectangle(handle->wayland.cairo.back_cairo, x, y, w, h); \
	cairo_set_source_rgb(handle->wayland.cairo.back_cairo, col, col, col); \
	cairo_fill(handle->wayland.cairo.back_cairo);

#define DO_LINE(off, col) \
	cairo_move_to(handle->wayland.cairo.back_cairo, off, off); \
	cairo_line_to(handle->wayland.cairo.back_cairo, off, h - (off)); \
	cairo_set_source_rgb(handle->wayland.cairo.back_cairo, col, col, col); \
	cairo_stroke(handle->wayland.cairo.back_cairo); \
	cairo_move_to(handle->wayland.cairo.back_cairo, off, off); \
	cairo_line_to(handle->wayland.cairo.back_cairo, w - (off), off); \
	cairo_set_source_rgb(handle->wayland.cairo.back_cairo, col, col, col); \
	cairo_stroke(handle->wayland.cairo.back_cairo);

		/* border */
		cairo_set_antialias(handle->wayland.cairo.back_cairo, CAIRO_ANTIALIAS_NONE);

		DO_BOX(0, 0, 0.14901961, w, h);
		DO_BOX(1, 1, 0.55686275, w - 2, h - 2);
		DO_BOX(2, 2, 0.82352941, w - 4, h - 4);
		DO_BOX(3, 3, 0.93333333, w - 6, h - 6);
		DO_BOX(4, 4, 1, w - 8, h - 8);
		DO_BOX(4, 4, 0.14901961, w - CSD_BORDER_FRAME_LEFT - 4, h - CSD_BORDER_FRAME_BOTTOM - 4);
		cairo_set_line_width(handle->wayland.cairo.back_cairo, 1.0);

		DO_LINE(1, 1)
		DO_LINE(2, 0.93333333)
		DO_LINE(3, 0.82352941)
		DO_LINE(4, 0.55686275)
		DO_LINE(5, 0.14901961)

		cairo_set_line_width(handle->wayland.cairo.back_cairo, 2.0);
		for(y = 1; y < CSD_BORDER_FRAME_TOP - CSD_BORDER_FRAME_LEFT; y++) {
			float_color c = lerp_color(one, two, (double)y / (double)(CSD_BORDER_FRAME_TOP - CSD_BORDER_FRAME_LEFT));

			cairo_set_source_rgb(handle->wayland.cairo.back_cairo, c.r, c.g, c.b);

			cairo_move_to(handle->wayland.cairo.back_cairo, CSD_BORDER_FRAME_LEFT, y + CSD_BORDER_FRAME_LEFT);
			cairo_line_to(handle->wayland.cairo.back_cairo, w - CSD_BORDER_FRAME_RIGHT, y + CSD_BORDER_FRAME_LEFT);
			cairo_stroke(handle->wayland.cairo.back_cairo);
		}

		/* icon */
		r.x	 = CSD_BORDER_FRAME_LEFT + 1;
		r.y	 = CSD_BORDER_FRAME_RIGHT + 1;
		r.width	 = 15;
		r.height = 15;
		DO_BOX(5, 5, 1, 18, 18);
		DO_BOX(6, 6, 0.35294118, 17, 17);
		DO_BOX(6, 6, 0.82352941, 16, 16);
		handle->wayland.cairo.selected_cairo = handle->wayland.cairo.back_cairo;
		if(handle->wayland.icon_pixmap) {
			MwLLDrawPixmap(handle, &r, handle->wayland.icon_pixmap);
		}

		/* close */
		DO_BOX(w - 19 - CSD_BORDER_FRAME_LEFT, CSD_BORDER_FRAME_BOTTOM + 1, 1, 18, 18);
		DO_BOX(w - 18 - CSD_BORDER_FRAME_LEFT, CSD_BORDER_FRAME_BOTTOM + 2, 0.35294118, 17, 17);
		DO_BOX(w - 18 - CSD_BORDER_FRAME_LEFT, CSD_BORDER_FRAME_BOTTOM + 2, 0.82352941, 16, 16);
		DO_BOX(w - 18 - CSD_BORDER_FRAME_LEFT + 5, CSD_BORDER_FRAME_BOTTOM + 7, 0, 6, 6);
		DO_BOX(w - 18 - CSD_BORDER_FRAME_LEFT + 6, CSD_BORDER_FRAME_BOTTOM + 8, 0.82352941, 4, 4);

		/* maxim */
		DO_BOX(w - 37 - CSD_BORDER_FRAME_LEFT, CSD_BORDER_FRAME_BOTTOM + 1, 1, 18, 18);
		DO_BOX(w - 36 - CSD_BORDER_FRAME_LEFT, CSD_BORDER_FRAME_BOTTOM + 2, 0.35294118, 17, 17);
		DO_BOX(w - 36 - CSD_BORDER_FRAME_LEFT, CSD_BORDER_FRAME_BOTTOM + 2, 0.82352941, 16, 16);
		DO_BOX(w - 36 - CSD_BORDER_FRAME_LEFT + 5, CSD_BORDER_FRAME_BOTTOM + 7, 0, 6, 6);
		DO_BOX(w - 36 - CSD_BORDER_FRAME_LEFT, CSD_BORDER_FRAME_BOTTOM + 9, 0.82352941, 16, 2);
		DO_BOX(w - 36 - CSD_BORDER_FRAME_LEFT + 7, CSD_BORDER_FRAME_BOTTOM + 4, 0.82352941, 2, 14);
		DO_BOX(w - 36 - CSD_BORDER_FRAME_LEFT + 6, CSD_BORDER_FRAME_BOTTOM + 8, 0.82352941, 4, 4);

		/* title */
		if(strlen(handle->wayland.title) != 0) {
			int	       y, x;
			int	       i = 0, sx = 0, sy = 0;
			int	       tw, th;
			unsigned char* px;
			MwLLPixmap     p;
			tw = strlen(handle->wayland.title) * 7;
			th = 14;
			px = malloc(tw * th * 4);
			assert(px);

			memset(px, 0, tw * th * 4);

			while(handle->wayland.title[i]) {
				int out;
				i += MwUTF8ToUTF32(handle->wayland.title + i, &out);

				if(out > 0xff) {
					out = 0;
				}

				for(y = 0; y < 14; y++) {
					for(x = 0; x < 7; x++) {
						unsigned char* ppx = &px[((sy + y) * tw + sx + x) * 4];
						int	       col = MwFontData[out].data[y] & (1 << ((7 - 1) - x)) ? 255 : 0;
						ppx[0]		   = col;
						ppx[1]		   = col;
						ppx[2]		   = col;
						ppx[3]		   = col;
					}
				}
				sx += 7;
			}

			p	 = MwLLCreatePixmap(handle, px, tw, th);
			r.x	 = 24;
			r.y	 = 5 + 2;
			r.width	 = tw;
			r.height = th;

			MwLLDrawPixmap(handle, &r, p);
			MwLLDestroyPixmap(p);
			free(px);
		}
		MwLLWaylandBufferUpdate(handle, &handle->wayland.backbuffer);
	}
	handle->wayland.cairo.selected_cairo = handle->wayland.cairo.front_cairo;
}

static void MwLLEndDrawImpl(MwLL handle) {
	if(handle->wayland.configured) {
		if(handle->wayland.type == MwLL_WAYLAND_TOPLEVEL) {
			MwLLWaylandBufferUpdate(handle, &handle->wayland.backbuffer);
		}
		MwLLWaylandBufferUpdate(handle, &handle->wayland.framebuffer);
	}
}

static void MwLLPolygonImpl(MwLL handle, MwPoint* points, int points_count, MwLLColor color) {
	WIDGET_CHECK(handle);

	clip(handle);

	MwLLCairoPolygon(handle->wayland.cairo, points, points_count, color);

	handle->wayland.events_pending = 1;
}

static void MwLLLineImpl(MwLL handle, MwPoint* points, MwLLColor color) {
	int i;
	WIDGET_CHECK(handle);

	clip(handle);

	MwLLCairoLine(handle->wayland.cairo, points, color);

	handle->wayland.events_pending = 1;
}

static MwLLColor MwLLAllocColorImpl(MwLL handle, int r, int g, int b) {
	MwLLColor c = malloc(sizeof(*c));
	(void)handle;

	c->common.red	= r;
	c->common.blue	= b;
	c->common.green = g;

	return c;
}

static void MwLLColorUpdateImpl(MwLL handle, MwLLColor c, int r, int g, int b) {
	(void)handle;
	c->common.red	= r;
	c->common.blue	= b;
	c->common.green = g;
}

static void MwLLFreeColorImpl(MwLLColor color) {
	free(color);
}

static int MwLLPendingImpl(MwLL handle) {
	struct timespec timeout = {
	    .tv_nsec = 1,
	    .tv_sec  = 0,
	};
	struct pollfd fd = {
	    .fd	    = wl_display_get_fd(handle->wayland.display),
	    .events = POLLOUT,
	};
	int pending = 0;

	if(MwLLWaylandWidgetIsDestroyed(handle) || !handle->wayland.valid) {
		return 0;
	}

	handle->wayland.resizing = 0;

	if(handle->wayland.setting_wh) {
		actually_set_wh(handle);
		handle->wayland.setting_wh = 0;
	}

	handle->wayland.end_time = MwTimeGetTick();

	if(handle->wayland.holding_key) {
		float elapsed = ((float)(handle->wayland.end_time - (float)handle->wayland.start_time));
		if(elapsed >= handle->wayland.next_elapsed) {
			MwLL topmost_parent = handle;
			while(topmost_parent->wayland.parent) {
				MwLLDispatch(topmost_parent, key, &handle->wayland.last_pressed_key);
				topmost_parent = topmost_parent->wayland.parent;
			}
			handle->wayland.next_elapsed = elapsed + handle->wayland.keyboard_rate;
		}
	}

#ifdef USE_DBUS
	if(wl_call_tbl.has_dbus) {
		if(handle->wayland.dark_theme_detection) {

			handle->wayland.dark_theme_detection = MwFALSE;
			detect_dark_theme(handle);
			MwLLDispatch(handle, draw, NULL);
		} else {
			MwLLDBusPortalPoll(&wl_call_tbl.dbus, &handle->wayland.dbus, handle, "org.freedesktop.portal.Settings", "org.freedesktop.appearance", "color-scheme", &dark_theme_listener);
		}
	}
#endif

	if(!handle->wayland.did_initial_resize) {
		recursive_dispatch_resize(handle);
		handle->wayland.did_initial_resize = 1;
	}

	if(!MwWaylandVulkan) {
		wl_display_prepare_read(handle->wayland.display);
		if(!poll(&fd, 1, timeout.tv_nsec)) {
			wl_display_cancel_read(handle->wayland.display);
		} else {
			wl_display_read_events(handle->wayland.display);
			if((pending = wl_display_dispatch_pending(handle->wayland.display)) < 0) {
				wl_display_cancel_read(handle->wayland.display);
			}
		}
	} else {
		if((pending = wl_display_dispatch_pending(handle->wayland.display)) < 0) {
			wl_display_cancel_read(handle->wayland.display);
		}
		return pending;
	}

	MwLLWaylandBufferUpdate(handle, &handle->wayland.framebuffer);
	if(handle->wayland.type == MwLL_WAYLAND_TOPLEVEL) {
		MwLLWaylandBufferUpdate(handle, &handle->wayland.backbuffer);
	}

	return handle->wayland.force_render || handle->wayland.events_pending || pending;
}

static void MwLLNextEventImpl(MwLL handle) {
	WIDGET_CHECK(handle);
	if(!MwWaylandVulkan) {
		if(handle->wayland.did_event_loop_early) {
			handle->wayland.did_event_loop_early = MwFALSE;
		} else {
			event_loop(handle);
		}
	}

	if(handle->wayland.force_render) {
		MwLLDispatch(handle, draw, NULL);
		if(handle->wayland.configured) MwLLWaylandBufferUpdate(handle, &handle->wayland.framebuffer);
		handle->wayland.force_render = 0;
	}
	if(handle->wayland.events_pending) {
		handle->wayland.events_pending = 0;
	}
}

static void MwLLSetTitleImpl(MwLL handle, const char* title) {
	WIDGET_CHECK(handle);
	if(handle->wayland.type == MwLL_WAYLAND_TOPLEVEL) {
		xdg_toplevel_set_title(handle->wayland.toplevel->xdg_top_level, title);
	}
	if(!handle->wayland.has_decorations && handle->wayland.do_csd) {
		strncpy(handle->wayland.title, title, 255);
		MwLLBeginDraw(handle);
		MwLLEndDraw(handle);

		xdg_toplevel_set_min_size(handle->wayland.toplevel->xdg_top_level, (strlen(handle->wayland.title) * 8) + 64, 64);
	}
}

static MwLLPixmap MwLLCreatePixmapImpl(MwLL handle, unsigned char* data, int width, int height) {
	return MwLLCairoCreatePixmap(handle->wayland.cairo, data, width, height);
}

static void MwLLPixmapUpdateImpl(MwLLPixmap r) {
	MwLLCairoPixmapUpdate(r);
}

static void MwLLDestroyPixmapImpl(MwLLPixmap pixmap) {
	MwLLCairoDestroyPixmap(pixmap);
}

static void MwLLDrawPixmapImpl(MwLL handle, MwRect* rect, MwLLPixmap pixmap) {
	WIDGET_CHECK(handle);

	clip(handle);

	MwLLCairoDrawPixmap(handle->wayland.cairo, rect, pixmap);

	MwLLForceRender(handle);
	wl_surface_damage(handle->wayland.framebuffer.surface, 0, 0, handle->wayland.ww, handle->wayland.wh);
}
static void MwLLSetIconImpl(MwLL handle, MwLLPixmap pixmap) {
	WIDGET_CHECK(handle);
	if(handle->wayland.type == MwLL_WAYLAND_TOPLEVEL) {
		if(WAYLAND_GET_INTERFACE(handle->wayland, xdg_toplevel_icon_manager_v1) != NULL) {
			struct xdg_toplevel_icon_manager_v1* icon_manager = WAYLAND_GET_INTERFACE(handle->wayland, xdg_toplevel_icon_manager_v1)->context;
			struct xdg_toplevel_icon_v1*	     icon	  = xdg_toplevel_icon_manager_v1_create_icon(icon_manager);
			MwU64				     size	  = pixmap->common.width * pixmap->common.height * 4;
			int				     i		  = 0;
			int				     line	  = (pixmap->common.width < pixmap->common.height) ? pixmap->common.height : pixmap->common.width;

			if(handle->wayland.icon == NULL) {
				handle->wayland.icon = malloc(sizeof(struct _MwLLWaylandShmBuffer));
				memset(handle->wayland.icon, 0, sizeof(struct _MwLLWaylandShmBuffer));
				handle->wayland.icon->shm = handle->wayland.framebuffer.shm;
			}

			if(handle->wayland.icon->setup) {
				MwLLWaylandBufferDestroy(handle->wayland.icon);
				wl_surface_destroy(handle->wayland.icon->surface);
			}
			handle->wayland.icon->surface = wl_compositor_create_surface(handle->wayland.compositor);

			MwLLWaylandBufferSetup(handle->wayland.icon, line, line);

			wl_surface_attach(handle->wayland.icon->surface, handle->wayland.icon->shm_buffer, 0, 0);
			wl_surface_commit(handle->wayland.icon->surface);

			for(; i < size; i += 2) {
				handle->wayland.icon->buf_back[i]     = pixmap->common.raw[i + 2];
				handle->wayland.icon->buf_back[i + 1] = pixmap->common.raw[i + 1];
				handle->wayland.icon->buf_back[i + 2] = pixmap->common.raw[i + 0];
				handle->wayland.icon->buf_back[i + 3] = 255;
			}

			if(handle->wayland.configured) MwLLWaylandBufferUpdate(handle, handle->wayland.icon);

			xdg_toplevel_icon_v1_add_buffer(icon, handle->wayland.icon->shm_buffer_back, 1);

			xdg_toplevel_icon_manager_v1_set_icon(icon_manager, handle->wayland.toplevel->xdg_top_level, icon);
		}
	}
	if(!handle->wayland.has_decorations && handle->wayland.do_csd) {
		if(handle->wayland.icon_pixmap) {
			MwLLDestroyPixmap(handle->wayland.icon_pixmap);
		}
		handle->wayland.icon_pixmap = MwLLCreatePixmap(handle, pixmap->common.raw, pixmap->common.width, pixmap->common.height);

		actually_set_wh(handle);
		actually_set_wh(handle);
		MwLLBeginDraw(handle);
		MwLLEndDraw(handle);
	}
}

static void MwLLForceRenderImpl(MwLL handle) {
	WIDGET_CHECK(handle);
	wl_surface_damage(handle->wayland.framebuffer.surface, 0, 0, handle->wayland.ww, handle->wayland.wh);

	handle->wayland.force_render = MwTRUE;
	if(handle->wayland.parent) {
		handle->wayland.parent->wayland.force_render = MwTRUE;
	}
}

static void MwLLSetCursorImpl(MwLL handle, MwCursor* image, MwCursor* mask) {
	int x, y, xs, ys;

	WIDGET_CHECK(handle);

	if(handle->wayland.cursor.setup) {
		MwLLWaylandBufferDestroy(&handle->wayland.cursor);
		wl_surface_destroy(handle->wayland.cursor.surface);
	}
	MwLLWaylandBufferSetup(&handle->wayland.cursor, mask->width, mask->height);
	memset(handle->wayland.cursor.buf_back, 0, handle->wayland.cursor.buf_size);

	xs = -mask->x + image->x;
	ys = mask->height + mask->y;
	ys = image->height + image->y - ys;

	if(ys < 0) ys = -ys;

	for(y = 0; y < mask->height; y++) {
		unsigned int d = mask->data[y];
		for(x = mask->width - 1; x >= 0; x--) {
			int idx = ((y * mask->width) + x) * 4;

			if(d & 1) {
				handle->wayland.cursor.buf_back[idx + 3] = 255;
			};
			d = d >> 1;
		}
	}
	for(y = 0; y < image->height; y++) {
		unsigned int d = image->data[y];
		for(x = image->width - 1; x >= 0; x--) {
			int px	= 0;
			int idx = (((y + ys) * mask->width) + (x + xs)) * 4;

			if(d & 1) {
				px = 255;
			};

			handle->wayland.cursor.buf_back[idx]	 = px;
			handle->wayland.cursor.buf_back[idx + 1] = px;
			handle->wayland.cursor.buf_back[idx + 2] = px;
			d					 = d >> 1;
		}
	}

	handle->wayland.cursor.surface = wl_compositor_create_surface(handle->wayland.compositor);
	wl_surface_attach(handle->wayland.cursor.surface, handle->wayland.cursor.shm_buffer, 0, 0);
	wl_surface_commit(handle->wayland.cursor.surface);
	MwLLWaylandBufferUpdate(handle, &handle->wayland.cursor);
}

static void MwLLDetachImpl(MwLL handle, MwPoint* point) {
	MwLL p = handle->wayland.parent;
	int  x = 0, y = 0;
	WIDGET_CHECK(handle);
	while(p != NULL) {
		if(p->wayland.type != MwLL_WAYLAND_TOPLEVEL) {
			x += p->wayland.x;
			y += p->wayland.y;
		} else {
			break;
		}
		p = p->wayland.parent;
	}

	handle->wayland.changing     = MwTRUE;
	handle->wayland.detatching   = MwTRUE;
	handle->wayland.detach_point = *point;
	handle->wayland.detach_point.x += x;
	handle->wayland.detach_point.y += y;
}

static void MwLLShowImpl(MwLL handle, int show) {
	if(!handle->wayland.configured) {
		return;
	}
	WIDGET_CHECK(handle);
	/* Some guy on a mailing list said that "abusing" wl_surface_attach for this purpose is bad? This is documented behavior so please I beg of you let me know if there's a compositor that actually has a problem with this. */
	if(handle->wayland.framebuffer.surface) {
		wl_surface_attach(handle->wayland.framebuffer.surface, show ? handle->wayland.framebuffer.shm_buffer : NULL, 0, 0);
		wl_surface_commit(handle->wayland.framebuffer.surface);
	}
	if(handle->wayland.backbuffer.surface) {
		wl_surface_attach(handle->wayland.backbuffer.surface, show ? handle->wayland.backbuffer.shm_buffer : NULL, 0, 0);
		wl_surface_commit(handle->wayland.backbuffer.surface);
	}
}

static void MwLLMakePopupImpl(MwLL handle, MwLL parent) {
	(void)handle;
	(void)parent;
	/* Wayland doesn't have "popups" in the Milsko sense per se. xdg_popup is closer to ToolWindow and as such is what we use there. So just like the Mac backend, this is just left alone. */
}

static void MwLLSetSizeHintsImpl(MwLL handle, int minx, int miny, int maxx, int maxy) {
	WIDGET_CHECK(handle);
	if(handle->wayland.type == MwLL_WAYLAND_TOPLEVEL) {
		xdg_toplevel_set_min_size(handle->wayland.toplevel->xdg_top_level, minx, miny);
		xdg_toplevel_set_max_size(handle->wayland.toplevel->xdg_top_level, maxx, maxy);

		wl_surface_commit(handle->wayland.framebuffer.surface);
		wl_surface_commit(handle->wayland.backbuffer.surface);
	}
}

static void MwLLMakeBorderlessImpl(MwLL handle, int toggle) {
	WIDGET_CHECK(handle);
	if(handle->wayland.type == MwLL_WAYLAND_TOPLEVEL) {
		if(WAYLAND_GET_INTERFACE(handle->wayland, zxdg_decoration_manager_v1) != NULL) {
			zxdg_decoration_manager_v1_context_t* dec = WAYLAND_GET_INTERFACE(handle->wayland, zxdg_decoration_manager_v1)->context;

			if(!dec->decoration) {
				dec->decoration = zxdg_decoration_manager_v1_get_toplevel_decoration(
				    dec->manager, handle->wayland.toplevel->xdg_top_level);
			}

			zxdg_toplevel_decoration_v1_unset_mode(dec->decoration);
			zxdg_toplevel_decoration_v1_set_mode(
			    dec->decoration, toggle ? ZXDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE : ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
		} else {
			handle->wayland.do_csd = !toggle;
			wl_subsurface_set_position(handle->wayland.toplevel->ssurface, handle->wayland.do_csd ? CSD_BORDER_FRAME_LEFT : 0, handle->wayland.do_csd ? CSD_BORDER_FRAME_TOP : 0);
		}
	}
}

static void MwLLFocusImpl(MwLL handle) {
	(void)handle;
	printf("[WARNING] MwLLFocus not supported on Wayland\n");
}

static void MwLLGrabPointerImpl(MwLL handle, int toggle) {
	MwLL topmost_parent = handle;
	WIDGET_CHECK(handle);
	while(topmost_parent->wayland.parent) topmost_parent = topmost_parent->wayland.parent;
	if(handle->wayland.pointer_constraints && handle->wayland.relative_pointer_manager) {
		if(toggle) {
			topmost_parent->wayland.locked_pointer = zwp_pointer_constraints_v1_lock_pointer(topmost_parent->wayland.pointer_constraints, topmost_parent->wayland.backbuffer.surface, topmost_parent->wayland.pointer, topmost_parent->wayland.region, ZWP_POINTER_CONSTRAINTS_V1_LIFETIME_PERSISTENT);
			wl_surface_commit(topmost_parent->wayland.backbuffer.surface);
			zwp_locked_pointer_v1_set_cursor_position_hint(topmost_parent->wayland.locked_pointer, 0, CSD_BORDER_FRAME_TOP);
			handle->wayland.relative_pointer = zwp_relative_pointer_manager_v1_get_relative_pointer(handle->wayland.relative_pointer_manager, handle->wayland.pointer);
			zwp_relative_pointer_v1_add_listener(handle->wayland.relative_pointer, &MwLLWaylandRelativePointerListener, topmost_parent);
		} else {
			if(topmost_parent->wayland.locked_pointer) {
				zwp_locked_pointer_v1_destroy(topmost_parent->wayland.locked_pointer);
				topmost_parent->wayland.locked_pointer = NULL;
			}
			if(topmost_parent->wayland.relative_pointer) {
				zwp_relative_pointer_v1_destroy(topmost_parent->wayland.relative_pointer);
				topmost_parent->wayland.relative_pointer = NULL;
			}
		}
		handle->wayland.do_lock_pointer = toggle;
	} else {
		printf("[WARNING] MwLLGrabPointer not supported in Wayland session, zwp_pointer_constraints_v1 and relative_pointer_manager_v1 are required.\n");
	}
}

static void MwLLSetClipboardImpl(MwLL handle, const char* text, int clipboard_type) {
	int i;
	WIDGET_CHECK(handle);

	if(handle->wayland.clipboard_buffer != NULL) {
		free(handle->wayland.clipboard_buffer);
	}
	handle->wayland.clipboard_buffer = malloc(strlen(text));
	strcpy(handle->wayland.clipboard_buffer, text);

	if(clipboard_type == MwCLIPBOARD_PRIMARY) {
		if(handle->wayland.supports_zwp) {
			for(i = 0; i < arrlen(handle->wayland.clipboard_devices_zwp); i++) {
				wl_clipboard_device_context_t* device = handle->wayland.clipboard_devices_zwp[i];
				zwp_primary_selection_device_v1_set_selection(device->device.zwp, handle->wayland.clipboard_source.zwp, handle->wayland.keyboard_serial);
			}
		} else {
			printf("[WARNING] Primary clipboard requested for MwLLSetClipboard, but this Wayland compositor doesn't support it.\n");
		}
	} else {
		for(i = 0; i < arrlen(handle->wayland.clipboard_devices_wl); i++) {
			wl_clipboard_device_context_t* device = handle->wayland.clipboard_devices_wl[i];
			wl_data_device_set_selection(device->device.wl, handle->wayland.clipboard_source.wl, handle->wayland.keyboard_serial);
		}
	}
}

static void MwLLGetClipboardImpl(MwLL handle, int clipboard_type) {
	int i;
	WIDGET_CHECK(handle);
	if(clipboard_type == MwCLIPBOARD_PRIMARY) {
		if(handle->wayland.supports_zwp) {
			for(i = 0; i < arrlen(handle->wayland.clipboard_devices_zwp); i++) {
				MwLLWaylandClipboardRead(
				    handle->wayland.clipboard_devices_zwp[i], clipboard_type);
			}
		} else {
			printf("[WARNING] Primary clipboard requested for MwLLSetClipboard, but this Wayland compositor doesn't support it.\n");
		}
	} else {
		for(i = 0; i < arrlen(handle->wayland.clipboard_devices_wl); i++) {
			MwLLWaylandClipboardRead(
			    handle->wayland.clipboard_devices_wl[i], clipboard_type);
		}
	}
	return;
}

static void MwLLMakeToolWindowImpl(MwLL handle) {
	MwBool can_do_layer_surface = MwFALSE;

	WIDGET_CHECK(handle);

	can_do_layer_surface = (WAYLAND_GET_INTERFACE(handle->wayland, zwlr_layer_shell_v1) != NULL);

	if(!handle->wayland.parent && can_do_layer_surface) {
		handle->wayland.type_to_be = MwLL_WAYLAND_LAYER_SURFACE;
	} else {
		handle->wayland.type_to_be = MwLL_WAYLAND_POPUP;
	}

	handle->wayland.changing = MwTRUE;
}

static void MwLLGetCursorCoordImpl(MwLL handle, MwPoint* point) {
	MwLL topmost_parent = handle;

	WIDGET_CHECK(handle);
	while(topmost_parent->wayland.parent) topmost_parent = topmost_parent->wayland.parent;

	*point = topmost_parent->wayland.cur_mouse_pos;
}

static void MwLLGetScreenSizeImpl(MwLL handle, MwRect* rect) {
	WIDGET_CHECK(handle);

	rect->x	     = 0;
	rect->y	     = 0;
	rect->width  = handle->wayland.mw;
	rect->height = handle->wayland.mh;
}

static void MwLLBeginStateChangeImpl(MwLL handle) {
	(void)handle;
	/* no-op */
}

static void MwLLEndStateChangeImpl(MwLL handle) {
	MwLL topmost_parent;
	int  x, y;
	WIDGET_CHECK(handle);

	if(!handle->wayland.changing) {
		return;
	}
	handle->wayland.changing = MwFALSE;

	x = handle->wayland.detatching ? handle->wayland.detach_point.x : handle->wayland.x;
	y = handle->wayland.detatching ? handle->wayland.detach_point.y : handle->wayland.y;

	topmost_parent = handle->wayland.parent;

	while(topmost_parent) {
		if(topmost_parent->wayland.type != MwLL_WAYLAND_TOPLEVEL) {
			topmost_parent = topmost_parent->wayland.parent;
		} else {
			break;
		}
	}

	destroy_widget(handle);

	if(wl_display_roundtrip(handle->wayland.display) == -1) {
		printf("roundtrip failed\n");
		raise(SIGTRAP);
		return;
	}

	MwLLWaylandFlush(handle);

	widget_setup(handle, handle->wayland.parent, x, y, handle->wayland.ww, handle->wayland.wh, handle->wayland.type_to_be);

	handle->wayland.is_toplevel = MwTRUE;

	if(!handle->wayland.parent)
		handle->wayland.dark_theme_detection = MwTRUE;

	if(handle->common.user) {
		MwWidget w = handle->common.user;
		int	 i;
		for(i = 0; i < arrlen(w->children); i++) {
			if(w->children[i]->lowlevel->wayland.type == MwLL_WAYLAND_SUBLEVEL) {
				MwLL child			     = w->children[i]->lowlevel;
				child->wayland.sublevel->xdg_surface = handle->wayland.popup->xdg_surface;
				if(child->wayland.sublevel->subsurface)
					wl_subsurface_destroy(child->wayland.sublevel->subsurface);
				MwLLWaylandFlush(handle);

				child->wayland.sublevel->subsurface = wl_subcompositor_get_subsurface(child->wayland.sublevel->subcompositor, child->wayland.framebuffer.surface, handle->wayland.framebuffer.surface);
				wl_subsurface_set_desync(child->wayland.sublevel->subsurface);
				wl_subsurface_set_position(child->wayland.sublevel->subsurface, child->wayland.x, child->wayland.y);

				wl_subsurface_place_above(child->wayland.sublevel->subsurface, handle->wayland.framebuffer.surface);
				MwLLEndStateChangeImpl(w->children[i]->lowlevel);

				if(topmost_parent) {
					topmost_parent->wayland.currentlyHeldWidget = NULL;
				}
			}
		}
	}

	recursive_render(handle);

	handle->wayland.events_pending = 1;

	handle->wayland.detatching = MwFALSE;
}

static void MwLLSetDarkThemeImpl(MwLL handle, int toggle) {
	(void)toggle;

	/* Not Really what's supposed to happen with this function, but take this opprutunity to do the handlers that will force everything to redraw. */
	MwLLDispatch(handle, resize, NULL);
	MwLLDispatch(handle, draw, NULL);
}

static MwBool MwLLDoModernImpl(MwLL handle) {
	(void)handle;
	return MwTRUE;
}

static void MwLLRaiseImpl(MwLL handle) {
	(void)handle;
	/* 	WIDGET_CHECK(handle);

	if(handle->wayland.type == MwLL_WAYLAND_SUBLEVEL) {
		MwLL topmost_parent = handle;
		int  children_num;
		while(topmost_parent->wayland.parent) topmost_parent = topmost_parent->wayland.parent;
		children_num = arrlen(((MwWidget)topmost_parent->common.user)->children);

		if(children_num > 1) {
			printf("%d\n", children_num);
			MwWidget last_child = ((MwWidget)topmost_parent->common.user)->children[children_num - 1];
			wl_subsurface_place_above(handle->wayland.sublevel->subsurface, last_child->lowlevel->wayland.framebuffer.surface);
		}
		}*/
}

static void MwLLClipImpl(MwLL handle, MwRect* rect) {
	if(rect == NULL) {
		handle->wayland.is_clipping = MwFALSE;
	} else {
		handle->wayland.clip	    = *rect;
		handle->wayland.is_clipping = MwTRUE;
	}
}

static int MwLLWaylandCallInitImpl(void) {
#ifdef __linux__
	MwBool loadWayland = MwFALSE;
	if(getenv("MILSKO_BACKEND")) {
		loadWayland |=
		    (strcmp(getenv("MILSKO_BACKEND"), "wayland") == 0);
	} else if(getenv("WAYLAND_DISPLAY")) {
		loadWayland |= (getenv("WAYLAND_DISPLAY") != NULL);
	}

#ifdef MW_OPENGL
	if(getenv("WSLENV") || getenv("WSL_DISTRO_NAME")) {
		loadWayland = 0;
	}
#endif

	if(loadWayland) {
		if(wayland_load_funcs()) {
			return 1;
		}
		wl_log_set_handler_client(wl_logger);

		return 0;
	}
#endif
	return 1;
}

#include "../call.c"
CALL(Wayland);
