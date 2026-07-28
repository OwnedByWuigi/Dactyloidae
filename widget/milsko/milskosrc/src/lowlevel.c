#include <Mw/Milsko.h>

MwLL (*MwLLCreate)(MwLL parent, int x, int y, int width, int height) = NULL;
void (*MwLLDestroy)(MwLL handle)				     = NULL;

void (*MwLLPolygon)(MwLL handle, MwPoint* points, int points_count, MwLLColor color) = NULL;
void (*MwLLLine)(MwLL handle, MwPoint* points, MwLLColor color)			     = NULL;

void (*MwLLBeginDraw)(MwLL handle) = NULL;
void (*MwLLEndDraw)(MwLL handle)   = NULL;

MwLLColor (*MwLLAllocColor)(MwLL handle, int r, int g, int b)	       = NULL;
void (*MwLLColorUpdate)(MwLL handle, MwLLColor c, int r, int g, int b) = NULL;
void (*MwLLFreeColor)(MwLLColor color)				       = NULL;

void (*MwLLGetXYWH)(MwLL handle, int* x, int* y, unsigned int* w, unsigned int* h) = NULL;
void (*MwLLSetXY)(MwLL handle, int x, int y)					   = NULL;
void (*MwLLSetWH)(MwLL handle, int w, int h)					   = NULL;

void (*MwLLSetTitle)(MwLL handle, const char* title) = NULL;

int (*MwLLPending)(MwLL handle)	   = NULL;
void (*MwLLNextEvent)(MwLL handle) = NULL;

MwLLPixmap (*MwLLCreatePixmap)(MwLL handle, unsigned char* data, int width, int height) = NULL;
void (*MwLLPixmapUpdate)(MwLLPixmap pixmap)						= NULL;
void (*MwLLDestroyPixmap)(MwLLPixmap pixmap)						= NULL;
void (*MwLLDrawPixmap)(MwLL handle, MwRect* rect, MwLLPixmap pixmap)			= NULL;
void (*MwLLSetIcon)(MwLL handle, MwLLPixmap pixmap)					= NULL;

void (*MwLLForceRender)(MwLL handle) = NULL;

void (*MwLLSetCursor)(MwLL handle, MwCursor* image, MwCursor* mask) = NULL;
void (*MwLLDetach)(MwLL handle, MwPoint* point)			    = NULL;
void (*MwLLShow)(MwLL handle, int show)				    = NULL;

void (*MwLLSetSizeHints)(MwLL handle, int minx, int miny, int maxx, int maxy) = NULL;
void (*MwLLMakeBorderless)(MwLL handle, int toggle)			      = NULL;
void (*MwLLMakeToolWindow)(MwLL handle)					      = NULL;
void (*MwLLMakePopup)(MwLL handle, MwLL parent)				      = NULL;

void (*MwLLBeginStateChange)(MwLL handle) = NULL;
void (*MwLLEndStateChange)(MwLL handle)	  = NULL;

void (*MwLLFocus)(MwLL handle)			 = NULL;
void (*MwLLGrabPointer)(MwLL handle, int toggle) = NULL;

void (*MwLLSetClipboard)(MwLL handle, const char* text, int clipboard_type) = NULL;
void (*MwLLGetClipboard)(MwLL, int clipboard_type)			    = NULL;

void (*MwLLGetCursorCoord)(MwLL handle, MwPoint* point) = NULL;
void (*MwLLGetScreenSize)(MwLL handle, MwRect* rect)	= NULL;

void (*MwLLSetDarkTheme)(MwLL handle, int toggle) = NULL;

MwBool (*MwLLDoModern)(MwLL handle) = NULL;

void (*MwLLRaise)(MwLL handle) = NULL;

void (*MwLLClip)(MwLL handle, MwRect* rect) = NULL;

void MwLLCreateCommon(MwLL handle) {
	handle->common.handler = malloc(sizeof(*handle->common.handler));
	memset(handle->common.handler, 0, sizeof(*handle->common.handler));

	handle->common.supports_transparency = 0;
}

void MwLLDestroyCommon(MwLL handle) {
	free(handle->common.handler);
}

#ifdef USE_DBUS
MwBool MwLLDBusFuncSetup(MwLLDBusFuncTable* tbl) {
#ifdef __APPLE__
	tbl->lib = MwDynamicOpen("libdbus-1.dylib");
#else
	tbl->lib = MwDynamicOpen("libdbus-1.so");
#endif
	if(!tbl->lib) {
		fprintf(stderr, "[WARNING] dbus library (libdbus-1.so) not found, will not be able to check for any XDG settings.\n");
		return MwFALSE;
	}

#define DBUS_FUNC(x) \
	tbl->x = MwDynamicSymbol(tbl->lib, #x); \
	if(!tbl->x) { \
		fprintf(stderr, "[WARNING] dbus function %s not found, will not be able to check for any XDG settings.\n", #x); \
		dlclose(tbl->lib); \
		return MwFALSE; \
	}

	DBUS_FUNC(dbus_error_init);
	DBUS_FUNC(dbus_bus_get);
	DBUS_FUNC(dbus_bus_add_match);
	DBUS_FUNC(dbus_error_is_set);
	DBUS_FUNC(dbus_error_free);
	DBUS_FUNC(dbus_message_new_method_call);
	DBUS_FUNC(dbus_message_is_signal);
	DBUS_FUNC(dbus_message_iter_init_append);
	DBUS_FUNC(dbus_message_iter_append_basic);
	DBUS_FUNC(dbus_connection_flush);
	DBUS_FUNC(dbus_connection_pop_message);
	DBUS_FUNC(dbus_connection_read_write);
	DBUS_FUNC(dbus_connection_send_with_reply_and_block);
	DBUS_FUNC(dbus_message_unref);
	DBUS_FUNC(dbus_message_iter_init);
	DBUS_FUNC(dbus_message_iter_get_arg_type);
	DBUS_FUNC(dbus_message_iter_recurse);
	DBUS_FUNC(dbus_message_iter_next);
	DBUS_FUNC(dbus_connection_unref);
	DBUS_FUNC(dbus_message_iter_get_basic);

	return MwTRUE;
}
#endif

#ifdef USE_DBUS
MwBool MwLLDBusNewContext(MwLLDBusFuncTable* tbl, MwLLDBusContext* ctx) {
	if(!tbl->lib) {
		return MwFALSE;
	}

	tbl->dbus_error_init(&ctx->dbus_err);

	ctx->dbus_conn = tbl->dbus_bus_get(DBUS_BUS_SESSION, &ctx->dbus_err);
	if(tbl->dbus_error_is_set(&ctx->dbus_err)) {
		fprintf(stderr, "[WARNING] Could not initialize dbus: Connection error: %s\n", ctx->dbus_err.message);
		tbl->dbus_error_free(&ctx->dbus_err);
		return MwFALSE;
	}
	if(!ctx->dbus_conn) {
		fprintf(stderr, "[WARNING] Could not initialize dbus: Failed to connect to session bus\n");
		return MwFALSE;
	}
	return MwTRUE;
};

MWDECL MwBool MwLLDBusPortalGet(MwLLDBusFuncTable* tbl, MwLLDBusContext* ctx, const char* portal, const char* namespace, const char* key, void* out) {
	DBusMessageIter dbus_args, dbus_variant, dbus_inner_variant;
	char		arg_type;
	if(!ctx->dbus_conn) {
		return MwFALSE;
	}

	ctx->dbus_msg = tbl->dbus_message_new_method_call(
	    "org.freedesktop.portal.Desktop",
	    "/org/freedesktop/portal/desktop",
	    portal,
	    "Read");
	if(!ctx->dbus_msg) {
		fprintf(stderr, "[WARNING] Couldn't get %s::%s: Failed to create message\n", namespace, key);
		return MwFALSE;
	}

	tbl->dbus_message_iter_init_append(ctx->dbus_msg, &dbus_args);
	tbl->dbus_message_iter_append_basic(&dbus_args, 's', &namespace);
	tbl->dbus_message_iter_append_basic(&dbus_args, 's', &key);

	ctx->dbus_reply = tbl->dbus_connection_send_with_reply_and_block(ctx->dbus_conn, ctx->dbus_msg, 100, &ctx->dbus_err);

	if(tbl->dbus_error_is_set(&ctx->dbus_err)) {
		fprintf(stderr, "[WARNING] Couldn't get %s::%s: Send error: %s\n", namespace, key, ctx->dbus_err.message);
		tbl->dbus_error_free(&ctx->dbus_err);
		return MwFALSE;
	}
	if(!ctx->dbus_reply) {
		fprintf(stderr, "[WARNING] Couldn't get %s::%s: No reply received\n", namespace, key);
		return MwFALSE;
	}

	if(!tbl->dbus_message_iter_init(ctx->dbus_reply, &dbus_args)) {
		fprintf(stderr, "[WARNING] Couldn't get %s::%s: Reply has no arguments\n", namespace, key);
		tbl->dbus_message_unref(ctx->dbus_reply);
		return MwFALSE;
	}

	if((arg_type = tbl->dbus_message_iter_get_arg_type(&dbus_args)) != 'v') {
		fprintf(stderr, "[WARNING] Couldn't get %s::%s: Expected outer variant, got: %c\n", namespace, key, arg_type);
		tbl->dbus_message_unref(ctx->dbus_reply);
		return MwFALSE;
	}
	tbl->dbus_message_iter_recurse(&dbus_args, &dbus_variant);

	/* Some portals wrap the value in a second variant */
	arg_type = tbl->dbus_message_iter_get_arg_type(&dbus_variant);
	if(arg_type == 'v') {
		tbl->dbus_message_iter_recurse(&dbus_variant, &dbus_inner_variant);
		if((arg_type = tbl->dbus_message_iter_get_arg_type(&dbus_inner_variant)) != 'u') {
			fprintf(stderr, "[WARNING] Signal reply for %s::SettingChanged: Expected 'u' (uint32) for 'value', got: %c\n", portal, arg_type);
			tbl->dbus_message_unref(ctx->dbus_reply);
			return MwFALSE;
		}
		tbl->dbus_message_iter_get_basic(&dbus_inner_variant, out);
	} else if(arg_type == 'u') {
		tbl->dbus_message_iter_get_basic(&dbus_variant, out);
	} else {
		fprintf(stderr, "[WARNING] Couldn't get %s::%s: Expected variant or string, got: %c\n", namespace, key, arg_type);
		tbl->dbus_message_unref(ctx->dbus_reply);
		return MwFALSE;
	}
	tbl->dbus_message_unref(ctx->dbus_msg);

	return MwTRUE;
}

MWDECL MwBool MwLLDBusPortalWatch(MwLLDBusFuncTable* tbl, MwLLDBusContext* ctx, const char* portal) {
	char filter_string[2048];
	if(!ctx->dbus_conn) {
		return MwFALSE;
	}

	MwStringPrintIntoBuffer(filter_string, sizeof(filter_string), "type='%s',interface=%s", "signal", portal);

	tbl->dbus_bus_add_match(ctx->dbus_conn, filter_string, &ctx->dbus_err);
	tbl->dbus_connection_flush(ctx->dbus_conn);

	return MwTRUE;
}

/* Technically this will swallow all other results, so this is not usable multiple times.
   TODO: Use a hashmap instead */
MWDECL MwBool MwLLDBusPortalPoll(MwLLDBusFuncTable* tbl, MwLLDBusContext* ctx, MwLL handle, const char* portal, const char* namespace, const char* key, MwLLDBusPortalPollListener listener) {
	DBusMessageIter args, msg_value, msg_value_inner;
	const char*	msg_namespace;
	const char*	msg_key;
	MwU32		msg_value_content;
	char		arg_type;

	if(!ctx->dbus_conn) {
		return MwFALSE;
	}

	tbl->dbus_connection_read_write(ctx->dbus_conn, 0);
	tbl->dbus_connection_read_write(ctx->dbus_conn, 0);
	ctx->dbus_reply = tbl->dbus_connection_pop_message(ctx->dbus_conn);

	if(tbl->dbus_error_is_set(&ctx->dbus_err)) {
		fprintf(stderr, "[WARNING] Signal reply for %s::SettingChanged: Poll error: %s\n", portal, ctx->dbus_err.message);
		tbl->dbus_error_free(&ctx->dbus_err);
		return MwFALSE;
	}
	if(!ctx->dbus_reply) {
		return MwFALSE;
	}

	/* check if the message is a signal from the correct interface and with the correct name */
	if(tbl->dbus_message_is_signal(ctx->dbus_reply, portal, "SettingChanged")) {
		/* read the parameters */
		if(!tbl->dbus_message_iter_init(ctx->dbus_reply, &args))
			fprintf(stderr, "[WARNING] Signal reply for %s::SettingChanged has no arguments\n", portal);
		else {
			if((arg_type = tbl->dbus_message_iter_get_arg_type(&args)) != 's') {
				fprintf(stderr, "[WARNING] Signal reply for %s::SettingChanged: Expected 's' (string) for 'namespace', got: %c\n", portal, arg_type);
				tbl->dbus_message_unref(ctx->dbus_reply);
				return MwFALSE;
			}
			tbl->dbus_message_iter_get_basic(&args, &msg_namespace);

			tbl->dbus_message_iter_next(&args);
			if((arg_type = tbl->dbus_message_iter_get_arg_type(&args)) != 's') {
				fprintf(stderr, "[WARNING] Signal reply for %s::SettingChanged: Expected 's' (string) for 'key', got: %c\n", portal, arg_type);
				tbl->dbus_message_unref(ctx->dbus_reply);
				return MwFALSE;
			}
			tbl->dbus_message_iter_get_basic(&args, &msg_key);

			/* Check that key and namespace match */
			if(strcmp(msg_namespace, namespace) == 0 && strcmp(msg_key, key) == 0) {
				/* Assuming the value is a basic type */
				tbl->dbus_message_iter_next(&args);

				if((arg_type = tbl->dbus_message_iter_get_arg_type(&args)) != 'v') {
					fprintf(stderr, "[WARNING] Couldn't get %s::%s: Expected outer variant, got: %c\n", namespace, key, arg_type);
					tbl->dbus_message_unref(ctx->dbus_reply);
					return MwFALSE;
				}
				tbl->dbus_message_iter_recurse(&args, &msg_value);

				/* Some portals wrap the value in a second variant */
				arg_type = tbl->dbus_message_iter_get_arg_type(&msg_value);
				if(arg_type == 'v') {
					tbl->dbus_message_iter_recurse(&msg_value, &msg_value_inner);
					if((arg_type = tbl->dbus_message_iter_get_arg_type(&msg_value_inner)) != 'u') {
						fprintf(stderr, "[WARNING] Signal reply for %s::SettingChanged: Expected 'u' (uint32) for 'value', got: %c\n", portal, arg_type);
						tbl->dbus_message_unref(ctx->dbus_reply);
						return MwFALSE;
					}
					tbl->dbus_message_iter_get_basic(&msg_value_inner, &msg_value_content);
				} else if(arg_type == 'u') {
					tbl->dbus_message_iter_get_basic(&msg_value, &msg_value_content);
				} else {
					fprintf(stderr, "[WARNING] Couldn't get %s::%s: Expected variant or string, got: %c\n", namespace, key, arg_type);
					tbl->dbus_message_unref(ctx->dbus_reply);
					return MwFALSE;
				}

				listener(handle, msg_value_content);
			}
		}
	}

	/* free the message */
	tbl->dbus_message_unref(ctx->dbus_reply);
	return MwTRUE;
}

void MwLLDBusFreeContext(MwLLDBusFuncTable* tbl, MwLLDBusContext* ctx) {
	if(!tbl->lib) {
		return;
	}
	if(ctx->dbus_reply) tbl->dbus_message_unref(ctx->dbus_reply);
	if(ctx->dbus_conn) tbl->dbus_connection_unref(ctx->dbus_conn);
};
#endif
