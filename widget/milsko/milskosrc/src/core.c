#include <Mw/Milsko.h>

#include "../external/stb_ds.h"

#ifdef USE_COCOA
#define PERIODIC
#endif

#define RESIZE_ON_TICK

#define DRAW(handle) \
	{ \
		MwLLBeginDraw(handle->lowlevel); \
\
		handle->bgcolor = NULL; \
		MwDispatch(handle, draw); \
		if(handle->draw_inject != NULL) handle->draw_inject(handle); \
		MwDispatchUserHandler(handle, MwNdrawHandler, NULL); \
\
		MwLLEndDraw(handle->lowlevel); \
	}

#define RESIZE(handle) \
	{ \
		int RESIZE_i; \
		MwDispatchUserHandler(handle, MwNresizeHandler, NULL); \
		for(RESIZE_i = 0; RESIZE_i < arrlen(handle->children); RESIZE_i++) { \
			MwDispatch(handle->children[RESIZE_i], parent_resize); \
		} \
		MwDispatch(handle, resize); \
	}

static void MWAPI     MwVaListApply_Internal(MwWidget handle, va_list va, int only_early);
static MwWidget MWAPI MwCreateWidget_Internal(MwClass widget_class, const char* name, MwWidget parent, int x, int y, unsigned int width, unsigned int height, int do_prop, va_list prop);

static void lldrawhandler(MwLL handle, void* data) {
	MwWidget h = (MwWidget)handle->common.user;
#ifdef PERIODIC
	MwWidget top;
#endif

	(void)data;

#ifdef PERIODIC
	top = h;
	while(top != NULL) {
		if(top->top_step) {
			break;
		}

		top = top->parent;
	}

	if(top == NULL) {
		DRAW(h); /* fallback */
	} else {
		int i;

		for(i = 0; i < arrlen(top->draw_queue); i++) {
			if(top->draw_queue[i] == h) break;
		}
		if(i == arrlen(top->draw_queue)) arrput(top->draw_queue, h);
	}
#else
	DRAW(h);
#endif
}

static void lluphandler(MwLL handle, void* data) {
	MwWidget h = (MwWidget)handle->common.user;
	MwMouse* p = data;
	if(MwGetInteger(h, MwNdisabled) == 1) return;

	if(p->button == MwMOUSE_LEFT) h->pressed = 0;
	h->mouse_point.x = p->point.x;
	h->mouse_point.y = p->point.y;

	if(p->button == MwMOUSE_LEFT) MwDispatch(h, click);
	MwDispatch3(h, mouse_up, data);
	MwDispatchUserHandler(h, MwNmouseUpHandler, data);
}

static void lldownhandler(MwLL handle, void* data) {
	MwWidget h = (MwWidget)handle->common.user;
	MwMouse* p = data;
	if(MwGetInteger(h, MwNdisabled) == 1) return;

	if(p->button == MwMOUSE_LEFT) h->pressed = 1;
	h->mouse_point.x = p->point.x;
	h->mouse_point.y = p->point.y;

	MwDispatch3(h, mouse_down, data);
	MwDispatchUserHandler(h, MwNmouseDownHandler, data);
}

static void llresizehandler(MwLL handle, void* data) {
	MwWidget h = (MwWidget)handle->common.user;
#ifdef RESIZE_ON_TICK
	MwWidget top;
#endif

	(void)data;

#ifdef RESIZE_ON_TICK
	top = h;
	while(top != NULL) {
		if(top->top_step) {
			break;
		}

		top = top->parent;
	}

	if(top == NULL) {
		RESIZE(h); /* fallback */
	} else {
		int i;

		for(i = 0; i < arrlen(top->resize_queue); i++) {
			if(top->resize_queue[i] == h) break;
		}
		if(i == arrlen(top->resize_queue)) arrput(top->resize_queue, h);
	}
#else
	RESIZE(h);
#endif
}

static void llclosehandler(MwLL handle, void* data) {
	MwWidget h = (MwWidget)handle->common.user;
	int	 n;

	(void)data;

	if((n = MwGetInteger(h, MwNmain)) != MwDEFAULT && n) {
		while(h != NULL) {
			h->close = 1;
			MwDispatchUserHandler(h, MwNcloseHandler, NULL);
			h = h->parent;
		}
	} else {
		h->close = 1;
		MwDispatchUserHandler(h, MwNcloseHandler, NULL);
	}
}

static void llmovehandler(MwLL handle, void* data) {
	MwWidget h	 = (MwWidget)handle->common.user;
	MwPoint* p	 = data;
	h->mouse_point.x = p->x;
	h->mouse_point.y = p->y;

	MwDispatch(h, mouse_move);
	MwDispatchUserHandler(h, MwNmouseMoveHandler, data);
}

static void llkeyhandler(MwLL handle, void* data) {
	MwWidget h   = (MwWidget)handle->common.user;
	int	 key = *(int*)data;
	if(MwGetInteger(h, MwNdisabled) == 1) return;

	MwDispatch3(h, key, key);
	MwDispatchUserHandler(h, MwNkeyHandler, data);
}

static void llkeyrelhandler(MwLL handle, void* data) {
	MwWidget h = (MwWidget)handle->common.user;
	if(MwGetInteger(h, MwNdisabled) == 1) return;

	MwDispatchUserHandler(h, MwNkeyReleaseHandler, data);
}

static void llfocusinhandler(MwLL handle, void* data) {
	MwWidget h = (MwWidget)handle->common.user;
	if(MwGetInteger(h, MwNdisabled) == 1) return;

	MwDispatchUserHandler(h, MwNfocusInHandler, data);
}

static void llfocusouthandler(MwLL handle, void* data) {
	MwWidget h = (MwWidget)handle->common.user;
	if(MwGetInteger(h, MwNdisabled) == 1) return;

	MwDispatchUserHandler(h, MwNfocusOutHandler, data);
}

static void llclipboardhandler(MwLL handle, void* data) {
	MwWidget h = (MwWidget)handle->common.user;
	if(MwGetInteger(h, MwNdisabled) == 1) return;

	MwDispatch3(h, clipboard, data);
	MwDispatchUserHandler(h, MwNclipboardHandler, data);
}

/* if both of them are true
 * 1. widget class is not NULL
 * 2.    parent is NULL
 *    or parent's widget class is NULL
 *
 * most likely this widget is first *visible* widget
 *
 * this macro is not used anywhere else - we could expose it if we have to
 *
 * (nishi)
 */
#define IsFirstVisible(handle) ((handle)->widget_class != NULL && ((handle)->parent == NULL || (handle)->parent->widget_class == NULL))

static void lldarkthemehandler(MwLL handle, void* data) {
	MwWidget h   = (MwWidget)handle->common.user;
	int*	 ptr = data;
	int	 s;

	if(h->widget_class == MwWindowClass && (shgeti(h->integer, MwNdarkTheme) == -1 || ((s = MwGetInteger(h, MwNdarkThemeAutomatic)) != MwDEFAULT && s))) {
		if(MwGetInteger(h, MwNmodernLook)) {
			MwVaApply(h,
				  MwNdarkTheme, *ptr,
				  MwNdarkThemeAutomatic, 1,
				  NULL);

			MwDispatchUserHandler(h, MwNdarkThemeHandler, data);
		}
	}
}

static MwWidget MwCreateWidget_Internal(MwClass widget_class, const char* name, MwWidget parent, int x, int y, unsigned int width, unsigned int height, int do_prop, va_list prop) {
	MwWidget h = malloc(sizeof(*h));

	if(name)
		h->name = MwStringDuplicate(name);
	else
		h->name = NULL;

	h->parent   = parent;
	h->children = NULL;

	if(widget_class != NULL) {
		if((h->lowlevel = MwLLCreate(parent == NULL ? NULL : parent->lowlevel, x, y, width, height)) == NULL) {
			if(h->name) {
				free(h->name);
			}
			free(h);
			return NULL;
		}
	} else {
		h->lowlevel = NULL;
	}
	h->widget_class	  = widget_class;
	h->pressed	  = 0;
	h->close	  = 0;
	h->destroy_queue  = NULL;
	h->prop_event	  = 1;
	h->draw_inject	  = NULL;
	h->destroy_inject = NULL;
	h->tick_list	  = NULL;
	h->destroyed	  = 0;
	h->bgcolor	  = NULL;

	h->berserk   = 0;
	h->last_tick = MwTimeGetTick();

	h->internal = NULL;
	h->opaque   = NULL;
	h->user	    = NULL;

	h->top_step	     = 0;
	h->draw_queue	     = NULL;
	h->resize_queue	     = NULL;
	h->monitored_entries = NULL;

	if(parent == NULL) arrput(h->tick_list, h);

	if(parent == NULL) h->top_step = 1;

	if(h->lowlevel != NULL) {
		h->lowlevel->common.user		  = h;
		h->lowlevel->common.handler->draw	  = lldrawhandler;
		h->lowlevel->common.handler->up		  = lluphandler;
		h->lowlevel->common.handler->down	  = lldownhandler;
		h->lowlevel->common.handler->resize	  = llresizehandler;
		h->lowlevel->common.handler->close	  = llclosehandler;
		h->lowlevel->common.handler->move	  = llmovehandler;
		h->lowlevel->common.handler->key	  = llkeyhandler;
		h->lowlevel->common.handler->key_released = llkeyrelhandler;
		h->lowlevel->common.handler->focus_in	  = llfocusinhandler;
		h->lowlevel->common.handler->focus_out	  = llfocusouthandler;
		h->lowlevel->common.handler->clipboard	  = llclipboardhandler;
		h->lowlevel->common.handler->dark_theme	  = lldarkthemehandler;
	}

	if(parent != NULL) arrput(parent->children, h);

	sh_new_strdup(h->text);
	sh_new_strdup(h->integer);
	sh_new_strdup(h->handler);
	sh_new_strdup(h->data);

	shdefault(h->integer, MwDEFAULT);
	shdefault(h->text, NULL);
	shdefault(h->handler, NULL);
	shdefault(h->data, NULL);

	if(do_prop) MwVaListApply_Internal(h, prop, 1);

	h->prop_event = 0;
	if(MwDispatch2(h, create) != 0) {
		h->widget_class = NULL;
		MwDestroyWidget(h);
		return NULL;
	}
	if(h->widget_class == NULL) {
		MwSetDefault(h);
	}
	h->prop_event = 1;

	if(h->widget_class != NULL && h->widget_class->tick != NULL) {
		MwAddTickList(h);
	}

#if defined(USE_STB_TRUETYPE) || defined(USE_FREETYPE2) || defined(USE_GDI_TEXT)
	if(IsFirstVisible(h)) {
	font_setup:
		h->root_font	     = MwFontLoad(MwTTFData, MwTTFDataSize);
		h->root_boldfont     = MwFontLoad(MwBoldTTFData, MwBoldTTFDataSize);
		h->root_monofont     = MwFontLoad(MwMonospaceTTFData, MwMonospaceTTFDataSize);
		h->root_boldmonofont = MwFontLoad(MwBoldMonospaceTTFData, MwBoldMonospaceTTFDataSize);
		if(!h->root_font) {
#ifdef USE_STB_TRUETYPE
			MwFL_STBTTSetup();
			goto font_setup;
#endif
		}
	} else
#endif
	{
		h->root_font	     = NULL;
		h->root_boldfont     = NULL;
		h->root_monofont     = NULL;
		h->root_boldmonofont = NULL;
	}

	if(h->parent != NULL) MwDispatch4(h->parent, children_update, h, 1);

	return h;
}

MwWidget MwCreateWidget(MwClass widget_class, const char* name, MwWidget parent, int x, int y, unsigned int width, unsigned int height) {
	va_list dummy;

	memset(&dummy, 0, sizeof(dummy));

	return MwCreateWidget_Internal(widget_class, name, parent, x, y, width, height, 0, dummy);
}

MwWidget MwVaCreateWidget(MwClass widget_class, const char* name, MwWidget parent, int x, int y, unsigned int width, unsigned int height, ...) {
	MwWidget h;
	va_list	 va;

	va_start(va, height);
	h = MwVaListCreateWidget(widget_class, name, parent, x, y, width, height, va);
	va_end(va);

	return h;
}

MwWidget MwVaListCreateWidget(MwClass widget_class, const char* name, MwWidget parent, int x, int y, unsigned int width, unsigned int height, va_list va) {
	MwWidget h;
	va_list	 va2;

#ifdef va_copy
	va_copy(va2, va);
#else
	va2 = va;
#endif

	h = MwCreateWidget_Internal(widget_class, name, parent, x, y, width, height, 1, va);
	MwVaListApply(h, va2);

	va_end(va2);

	return h;
}

void MwFreeWidget(MwWidget handle) {
	int	 i;
	MwWidget root = handle;
	MwWidget w;

	w = handle;
	while(w != NULL) {
		if(w->top_step) break;
		w = w->parent;
	}

	if(w != NULL) {
		for(i = 0; i < arrlen(w->draw_queue); i++) {
			if(w->draw_queue[i] == handle) {
				arrdel(w->draw_queue, i);
				i--;
			}
		}
	}

	handle->destroyed = 0;
	if(handle->destroy_inject != NULL) handle->destroy_inject(handle);
	MwDispatch(handle, destroy);

	for(i = 0; i < arrlen(handle->children); i++) {
		MwFreeWidget(handle->children[i]);
	}
	arrfree(handle->children);

	while(root->parent != NULL) root = root->parent;
	for(i = 0; i < arrlen(root->tick_list); i++) {
		if(handle == root->tick_list[i]) {
			arrdel(root->tick_list, i);
			i--;
		}
	}

	if(handle->name) free(handle->name);

	if(handle->lowlevel != NULL) MwLLDestroy(handle->lowlevel);

	shfree(handle->integer);

	for(i = 0; i < shlen(handle->text); i++) {
		free(handle->text[i].value);
		handle->text[i].value = NULL;
	}
	shfree(handle->text);
	for(i = 0; i < shlen(handle->handler); i++) {
		arrfree(handle->handler[i].value);
	}
	shfree(handle->handler);
	shfree(handle->data);

	arrfree(handle->destroy_queue);
	arrfree(handle->tick_list);

	arrfree(handle->draw_queue);
	arrfree(handle->resize_queue);

	if(handle->root_font != NULL) MwFontFree(handle->root_font);
	if(handle->root_boldfont != NULL) MwFontFree(handle->root_boldfont);
	if(handle->root_monofont != NULL) MwFontFree(handle->root_monofont);
	if(handle->root_boldmonofont != NULL) MwFontFree(handle->root_boldmonofont);

	free(handle);
}

static void destroy_children(MwWidget handle) {
	int i;
	for(i = 0; i < arrlen(handle->children); i++) {
		destroy_children(handle->children[i]);
	}
	handle->destroyed = 1;
}

void MwDestroyWidget(MwWidget handle) {
	if(handle->destroyed) return;

	if(handle->parent != NULL) {
		int i;
		for(i = 0; i < arrlen(handle->parent->destroy_queue); i++) {
			if(handle->parent->destroy_queue[i] == handle) break;
		}
		if(i == arrlen(handle->parent->destroy_queue)) {
			arrput(handle->parent->destroy_queue, handle);
		}

		MwDispatch4(handle->parent, children_update, handle, 0);
	}
	destroy_children(handle);
	handle->destroyed = 1;
}

void MwVaWidgetExecute(MwWidget handle, const char* func_name, void* out, ...) {
	va_list args;
	va_start(args, out);
	MwVaListWidgetExecute(handle, func_name, out, args);
	va_end(args);
}

void MwVaListWidgetExecute(MwWidget handle, const char* func_name, void* out, va_list va) {
	if(handle->widget_class->execute != NULL) {
		handle->widget_class->execute(handle, func_name, out, va);
	}
}

static void clean_destroy_queue(MwWidget handle) {
	int i, j;
	for(i = 0; i < arrlen(handle->destroy_queue); i++) {
		MwWidget w = handle->destroy_queue[i];

		MwFreeWidget(w);
	}
	for(i = 0; i < arrlen(handle->destroy_queue); i++) {
		for(j = 0; j < arrlen(handle->children); j++) {
			if(handle->children[j] == handle->destroy_queue[i]) {
				arrdel(handle->children, j);
				break;
			}
		}
	}
	arrfree(handle->destroy_queue);
}

static void MwAfterStep(MwWidget handle) {
	int i;

#ifdef PERIODIC
	if(handle->top_step) {
		for(i = 0; i < arrlen(handle->draw_queue); i++) {
			DRAW(handle->draw_queue[i]);
		}

		arrfree(handle->draw_queue);
	}
#endif

#ifdef RESIZE_ON_TICK
	if(handle->top_step) {
		for(i = 0; i < arrlen(handle->resize_queue); i++) {
			RESIZE(handle->resize_queue[i]);
		}

		arrfree(handle->resize_queue);
	}
#endif

	if(arrlen(handle->tick_list) > 0 && (MwTimeGetTick() - handle->last_tick) >= (MwGetInteger(handle, MwNwaitMS) == MwDEFAULT ? MwWaitMS : MwGetInteger(handle, MwNwaitMS))) {
		for(i = 0; i < arrlen(handle->tick_list); i++) {
			MwDispatch(handle->tick_list[i], tick);
			MwDispatchUserHandler(handle->tick_list[i], MwNtickHandler, NULL);
		}

		handle->last_tick = MwTimeGetTick();
	}
}

int MwStep(MwWidget handle) {
	int	  i;
	MwWidget* widgets = NULL;
	for(i = 0; i < arrlen(handle->children); i++) {
		arrput(widgets, handle->children[i]);
	}
	for(i = 0; i < arrlen(widgets); i++) MwStep(widgets[i]);

	arrfree(widgets);

	handle->prop_event = 0;

	while(handle->lowlevel != NULL && MwLLPending(handle->lowlevel))
		MwLLNextEvent(handle->lowlevel);

	handle->prop_event = 1;

	if(handle->top_step) MwAfterStep(handle);

	clean_destroy_queue(handle);
	if(handle->parent == NULL && handle->destroyed) {
		MwFreeWidget(handle);
		return 1;
	}
	return 0;
}

int MwPending(MwWidget handle) {
	int i;
	for(i = 0; i < arrlen(handle->children); i++) {
		if(MwPending(handle->children[i])) return 1;
	}
	return (arrlen(handle->tick_list) > 0 && (MwTimeGetTick() - handle->last_tick) >= (MwGetInteger(handle, MwNwaitMS) == MwDEFAULT ? MwWaitMS : MwGetInteger(handle, MwNwaitMS))) || (arrlen(handle->destroy_queue) > 0) || (handle->widget_class == NULL ? 0 : MwLLPending(handle->lowlevel));
}

void MwLoop(MwWidget handle) {
	long tick = MwTimeGetTick();
	long wait = MwGetInteger(handle, MwNwaitMS);
	long over = 0;
	if(wait == MwDEFAULT) wait = MwWaitMS;
	while(!handle->close) {
		int  v = 0;
		long t, t2;
		long more;
		while(MwPending(handle)) {
			if((v = MwStep(handle)) != 0) break;
		}
		if(v != 0) break;

		more = over % (wait / 2);
		t    = (tick + wait - more) - (t2 = MwTimeGetTick());
		if(t > 0) {
			if(!handle->berserk) MwTimeSleep(t);
			tick = MwTimeGetTick();
			over -= more;
		} else {
			tick = t2;
			over += -t;
		}
	}
}

static void force_render_all(MwWidget handle) {
	int i;
	for(i = 0; i < arrlen(handle->children); i++) {
		force_render_all(handle->children[i]);
	}
	if(handle->lowlevel != NULL) MwForceRender(handle);
}

void MwSetInteger(MwWidget handle, const char* key, int n) {
	int xy;
	int wh = 0;
	if((xy = ((strcmp(key, MwNx) == 0 || strcmp(key, MwNy) == 0) ? 1 : 0)) || (wh = ((strcmp(key, MwNwidth) == 0 || strcmp(key, MwNheight) == 0)) ? 1 : 0)) {
		int	     x, y;
		unsigned int w, h;

		MwLLGetXYWH(handle->lowlevel, &x, &y, &w, &h);
		if(strcmp(key, MwNx) == 0) x = n;
		if(strcmp(key, MwNy) == 0) y = n;
		if(strcmp(key, MwNwidth) == 0) w = n;
		if(strcmp(key, MwNheight) == 0) h = n;
		if(xy) MwLLSetXY(handle->lowlevel, x, y);
		if(wh) MwLLSetWH(handle->lowlevel, w, h);
	} else {
		shput(handle->integer, key, n);
	}
	if(handle->prop_event) {
		char** keys = NULL;

		arrput(keys, (char*)key);
		arrput(keys, NULL);
		MwDispatch3(handle, props_change, keys);
		arrfree(keys);

		MwDispatch3(handle, prop_change, key);
	}

	if(handle->parent != NULL && handle->parent->prop_event) {
		if(handle->parent != NULL) MwDispatch4(handle->parent, children_prop_change, handle, key);
	}

	if(strcmp(key, MwNforceInverted) == 0) {
		MwForceRender(handle);
	}
	if(strcmp(key, MwNmodernLook) == 0 || strcmp(key, MwNdarkTheme) == 0 || strcmp(key, MwNbitmapFont) == 0) {
		force_render_all(handle);
	}

	if(strcmp(key, MwNdarkTheme) == 0) {
		MwWidget h = handle;

		shdel(h->integer, MwNdarkThemeAutomatic);

		if(h->widget_class != MwWindowClass) {
			while(h->parent != NULL && h->parent->widget_class != MwWindowClass) h = h->parent;
		}

		if(h->lowlevel != NULL) MwLLSetDarkTheme(h->lowlevel, n);
	}
}

void MwSetText(MwWidget handle, const char* key, const char* value) {
	if(handle->widget_class == MwWindowClass && strcmp(key, MwNtitle) == 0) {
		MwLLSetTitle(handle->lowlevel, value);
	} else {
		char* v = value == NULL ? NULL : MwStringDuplicate(value);

		if(shgeti(handle->text, key) != -1) free(shget(handle->text, key));

		if(value != NULL) {
			shput(handle->text, key, v);
		} else {
			shdel(handle->text, key);
		}
	}
	if(handle->prop_event) {
		char** keys = NULL;

		arrput(keys, (char*)key);
		arrput(keys, NULL);
		MwDispatch3(handle, props_change, keys);
		arrfree(keys);

		MwDispatch3(handle, prop_change, key);
	}

	if(handle->parent != NULL && handle->parent->prop_event) {
		if(handle->parent != NULL) MwDispatch4(handle->parent, children_prop_change, handle, key);
	}

	if(strcmp(key, MwNbackground) == 0 || strcmp(key, MwNforeground) == 0 || strcmp(key, MwNsubBackground) == 0 || strcmp(key, MwNsubForeground) == 0) {
		MwForceRender(handle);
	}
}

void MwSetVoid(MwWidget handle, const char* key, void* value) {
	if(IsFirstVisible(handle) && strcmp(key, MwNiconPixmap) == 0) {
		MwLLSetIcon(handle->lowlevel, value);
	} else if(strcmp(key, MwNsizeHints) == 0) {
		MwSizeHints* sz = value;

		MwLLBeginStateChange(handle->lowlevel);
		MwLLSetSizeHints(handle->lowlevel, sz->min_width, sz->min_height, sz->max_width, sz->max_height);
		MwLLEndStateChange(handle->lowlevel);
	} else {
		shput(handle->data, key, value);
	}
	if(handle->prop_event) {
		char** keys = NULL;

		arrput(keys, (char*)key);
		arrput(keys, NULL);
		MwDispatch3(handle, props_change, keys);
		arrfree(keys);

		MwDispatch3(handle, prop_change, key);
	}

	if(handle->parent != NULL && handle->parent->prop_event) {
		if(handle->parent != NULL) MwDispatch4(handle->parent, children_prop_change, handle, key);
	}
}

static int inherit_integer(MwWidget handle, const char* key, int default_v) {
	int v;

	if(handle->parent != NULL && (v = MwGetInteger(handle->parent, key)) != MwDEFAULT) {
		return v;
	}
	return default_v;
}

int MwGetInteger(MwWidget handle, const char* key) {
	if(strcmp(key, MwNx) == 0 || strcmp(key, MwNy) == 0 || strcmp(key, MwNwidth) == 0 || strcmp(key, MwNheight) == 0) {
		int	     x, y;
		unsigned int w, h;

		MwLLGetXYWH(handle->lowlevel, &x, &y, &w, &h);

		if(strcmp(key, MwNx) == 0) return x;
		if(strcmp(key, MwNy) == 0) return y;
		if(strcmp(key, MwNwidth) == 0) return w;
		if(strcmp(key, MwNheight) == 0) return h;
		return MwDEFAULT;
	} else {
		if(shgeti(handle->integer, key) == -1) {
#if defined(USE_STB_TRUETYPE) || defined(USE_FREETYPE2) || defined(USE_GDI_TEXT)
			if(strcmp(key, MwNbitmapFont) == 0) return inherit_integer(handle, key, 0);
#else
			if(strcmp(key, MwNbitmapFont) == 0) return inherit_integer(handle, key, 1);
#endif
#ifdef USE_CLASSIC_THEME
			if(strcmp(key, MwNmodernLook) == 0) return inherit_integer(handle, key, 0);
#else
			if(strcmp(key, MwNmodernLook) == 0) return inherit_integer(handle, key, MwLLDoModern(handle->lowlevel));
#endif
			if(strcmp(key, MwNdarkTheme) == 0) return inherit_integer(handle, key, 0);
			if(strcmp(key, MwNuseMonospace) == 0) return inherit_integer(handle, key, 0);
			if(strcmp(key, MwNdisabled) == 0) return inherit_integer(handle, key, 0);
		}
		return shget(handle->integer, key);
	}
}

const char* MwGetText(MwWidget handle, const char* key) {
	if((shgeti(handle->text, key) == -1 || strcmp(shget(handle->text, key), "DEFAULT") == 0) && (strcmp(key, MwNbackground) == 0 || strcmp(key, MwNforeground) == 0 || strcmp(key, MwNsubBackground) == 0 || strcmp(key, MwNsubForeground) == 0 || strcmp(key, MwNtitleBackground) == 0 || strcmp(key, MwNtitleForeground) == 0)) {
		const char* v = NULL;
		if(shgeti(handle->text, key) != -1 && strcmp(shget(handle->text, key), "DEFAULT") != 0) {
			MwWidget h = handle->parent;
			while(h != NULL) {
				if((v = MwGetText(h, key)) != NULL) break;
				h = h->parent;
			}
		}
		if(v == NULL) {
			if(MwGetInteger(handle, MwNdarkTheme)) {
				if(strcmp(key, MwNbackground) == 0) return MwDefaultDarkBackground;
				if(strcmp(key, MwNforeground) == 0) return MwDefaultDarkForeground;
				if(strcmp(key, MwNsubBackground) == 0) return MwDefaultDarkSubBackground;
				if(strcmp(key, MwNsubForeground) == 0) return MwDefaultDarkSubForeground;
				if(strcmp(key, MwNtitleBackground) == 0) return MwDefaultDarkTitleBackground;
				if(strcmp(key, MwNtitleForeground) == 0) return MwDefaultDarkTitleForeground;
			} else {
				if(strcmp(key, MwNbackground) == 0) return MwDefaultBackground;
				if(strcmp(key, MwNforeground) == 0) return MwDefaultForeground;
				if(strcmp(key, MwNsubBackground) == 0) return MwDefaultSubBackground;
				if(strcmp(key, MwNsubForeground) == 0) return MwDefaultSubForeground;
				if(strcmp(key, MwNtitleBackground) == 0) return MwDefaultTitleBackground;
				if(strcmp(key, MwNtitleForeground) == 0) return MwDefaultTitleForeground;
			}
		}
		return v;
	}

	return shget(handle->text, key);
}

#if defined(USE_STB_TRUETYPE) || defined(USE_FREETYPE2) || defined(USE_GDI_TEXT)
static void* inherit_void(MwWidget handle, const char* key) {
	void* v;

	if(handle->parent != NULL && (v = MwGetVoid(handle->parent, key)) != NULL) {
		return v;
	}
	return NULL;
}
#endif

void* MwGetVoid(MwWidget handle, const char* key) {
	void* v = shget(handle->data, key);

	if(v != NULL) return v;

#if defined(USE_STB_TRUETYPE) || defined(USE_FREETYPE2) || defined(USE_GDI_TEXT)
	if(strcmp(key, MwNfont) == 0 || strcmp(key, MwNboldFont) == 0 || strcmp(key, MwNmonospaceFont) == 0 || strcmp(key, MwNboldMonospaceFont) == 0) {
		v = inherit_void(handle, key);

		if(v == NULL) {
			MwWidget w = handle;

			while(w != NULL && v == NULL) {
				if(strcmp(key, MwNfont) == 0) {
					v = w->root_font;
				} else if(strcmp(key, MwNboldFont) == 0) {
					v = w->root_boldfont;
				} else if(strcmp(key, MwNmonospaceFont) == 0) {
					v = w->root_monofont;
				} else if(strcmp(key, MwNboldMonospaceFont) == 0) {
					v = w->root_boldmonofont;
				}

				w = w->parent;
			}
		}
	}
#endif

	return v;
}

void MwVaApply(MwWidget handle, ...) {
	va_list va;

	va_start(va, handle);
	MwVaListApply(handle, va);
	va_end(va);
}

static void MwVaListApply_Internal(MwWidget handle, va_list va, int only_early) {
	char*  key;
	int    x = MwDEFAULT, y = MwDEFAULT, w = MwDEFAULT, h = MwDEFAULT;
	char** keys = NULL;
	int    old  = handle->prop_event;
	int    i;

	handle->prop_event = 0;
	while((key = va_arg(va, char*)) != NULL) {
		if(!only_early || key[1] == 'E') {
			arrput(keys, key);
		}

		if(key[0] == 'I') {
			int n = va_arg(va, int);
			if(only_early && key[1] != 'E') continue;

			if(strcmp(key, MwNx) == 0) {
				x = n;
			} else if(strcmp(key, MwNy) == 0) {
				y = n;
			} else if(strcmp(key, MwNwidth) == 0) {
				w = n;
			} else if(strcmp(key, MwNheight) == 0) {
				h = n;
			} else {
				MwSetInteger(handle, key, n);
			}
		} else if(key[0] == 'S') {
			char* t = va_arg(va, char*);
			if(only_early && key[1] != 'E') continue;

			MwSetText(handle, key, t);
		} else if(key[0] == 'C') {
			MwUserHandler h = va_arg(va, MwUserHandler);
			if(only_early && key[1] != 'E') continue;

			MwAddUserHandler(handle, key, h, NULL);
		} else if(key[0] == 'V') {
			void* v = va_arg(va, void*);
			if(only_early && key[1] != 'E') continue;
			MwSetVoid(handle, key, v);
		}
	}
	if(x != MwDEFAULT && y != MwDEFAULT) {
		MwLLSetXY(handle->lowlevel, x, y);
		if(handle->prop_event) {
			MwDispatch3(handle, prop_change, MwNx);
			MwDispatch3(handle, prop_change, MwNy);
		}
	} else {
		if(x != MwDEFAULT) {
			MwSetInteger(handle, MwNx, x);
		} else if(y != MwDEFAULT) {
			MwSetInteger(handle, MwNy, y);
		}
	}
	if(w != MwDEFAULT && h != MwDEFAULT) {
		MwLLSetWH(handle->lowlevel, w, h);
		if(handle->prop_event) {
			MwDispatch3(handle, prop_change, MwNwidth);
			MwDispatch3(handle, prop_change, MwNheight);
		}
	} else {
		if(w != MwDEFAULT) {
			MwSetInteger(handle, MwNwidth, w);
		} else if(h != MwDEFAULT) {
			MwSetInteger(handle, MwNheight, h);
		}
	}
	handle->prop_event = old;

	arrput(keys, NULL);

	if(handle->prop_event) {
		for(i = 0; keys[i] != NULL; i++) {
			MwDispatch3(handle, prop_change, keys[i]);
		}

		MwDispatch3(handle, props_change, keys);
	}

	arrfree(keys);
}

void MwVaListApply(MwWidget handle, va_list va) {
	MwVaListApply_Internal(handle, va, 0);
}

void MwSetDefault(MwWidget handle) {
	if(handle->lowlevel != NULL) MwLLSetCursor(handle->lowlevel, &MwCursorDefault, &MwCursorDefaultMask);
}

void MwHideCursor(MwWidget handle) {
	MwLLSetCursor(handle->lowlevel, &MwCursorHidden, &MwCursorHiddenMask);
}

void MwDispatchUserHandler(MwWidget handle, const char* key, void* handler_data) {
	int ind = shgeti(handle->handler, key);
	int p	= handle->prop_event;
	int i;

	if(ind == -1) return;
	if(handle->destroyed) return;

	if(!p) handle->prop_event = 1;
	for(i = 0; i < arrlen(handle->handler[ind].value); i++) {
		handle->handler[ind].value[i].handler(handle, handle->handler[ind].value[i].user_data, handler_data);
	}
	if(!p) handle->prop_event = 0;
}

void MwAddUserHandler(MwWidget handle, const char* key, MwUserHandler handler, void* user_data) {
	int		   ind;
	MwUserHandlerArray a;
	a.handler   = handler;
	a.user_data = user_data;

	ind = shgeti(handle->handler, key);
	if(ind == -1) {
		MwUserHandlerKeyValue e;
		e.key	= (char*)key;
		e.value = NULL;

		arrput(e.value, a);
		shputs(handle->handler, e);
	} else {
		arrput(handle->handler[ind].value, a);
	}
}

static MwErrorHandler error_handler   = NULL;
static void*	      error_user_data = NULL;

void MwSetErrorHandler(MwErrorHandler handler, void* user_data) {
	error_handler	= handler;
	error_user_data = user_data;
}

void MwDispatchError(int code, const char* message) {
	if(error_handler != NULL) {
		error_handler(code, message, error_user_data);
	} else {
#ifdef _WIN32
		char buffer[1024];
		MwStringPrintIntoBuffer(buffer, 1024, "Error: %s\r\nCode : %d\r\n\r\nMilsko is exiting.", message, code);
		MessageBox(NULL, buffer, "Error", MB_ICONERROR | MB_OK);
#else
		fprintf(stderr, "Error: %s\nCode : %d\n\nMilsko is exiting.", message, code);
#endif
		exit(1);
	}
}

#undef MwForceRender
void MwForceRender(MwWidget handle) {
	MwLLForceRender(handle->lowlevel);
}

#undef MwForceRender2
void MwForceRender2(MwWidget handle, void* ptr) {
	(void)ptr;

	MwForceRender(handle);
}

void MwForceRender_Internal(MwWidget handle) {
	MwForceRender(handle);
}

void MwForceRender2_Internal(MwWidget handle, void* ptr) {
	(void)ptr;

	MwForceRender(handle);
}

void MwAddTickList(MwWidget handle) {
	MwWidget root = handle;
	while(root->parent != NULL) root = root->parent;

	arrput(root->tick_list, handle);
}

void MwFocus(MwWidget handle) {
	MwLLFocus(handle->lowlevel);
}

void MwGrabPointer(MwWidget handle, int toggle) {
	if(toggle) MwFocus(handle);
	MwLLGrabPointer(handle->lowlevel, toggle);
}

void MwSetDarkTheme(MwWidget handle, int toggle) {
	MwVaApply(handle,
		  MwNdarkTheme, toggle,
		  NULL);
}

MwWidget MwGetParent(MwWidget handle) {
	return handle->parent;
}

typedef int (*call_t)(void);
int MwLibraryInit(void) {
	call_t calls[] = {
#ifdef USE_WAYLAND
	    NULL, /* set in a bit */
#endif
#ifdef USE_X11
	    MwLLX11CallInit,
#endif
#ifdef USE_COCOA
	    MwLLCocoaCallInit,
#endif
#ifdef USE_GDI
	    MwLLGDICallInit,
#endif
#ifdef CLASSIC_MAC_OS
	    MwLLClassicMacOSCallInit,
#endif
#ifdef USE_HAIKU
	    MwLLHaikuCallInit,
#endif
	    NULL};
	int i;

#ifdef _WIN32
	timeBeginPeriod(10);
#endif

#ifdef USE_WAYLAND
	if(MwWaylandCairoOnly) {
		calls[0] = MwLLCairoCallInit;
	} else {
		calls[0] = MwLLWaylandCallInit;
	}
#endif

	MwColorTableInit();

	MwFLSetup();

	for(i = 0; calls[i] != NULL; i++) {
		if(calls[i]() == 0) return 0;
	}

#ifdef __APPLE__
	void MwLLNoCocoaErrorMessage(void);
	MwLLNoCocoaErrorMessage();
#else
	printf("[WARNING] No suitable GUI backend found! Enabled:"
#ifdef USE_COCOA
	       " Cocoa"
#endif
#ifdef USE_WAYLAND
	       " Wayland"
#endif
#ifdef USE_X11
	       " X11"
#endif
#ifdef USE_GDI
	       " GDI"
#endif
#ifdef CLASSIC_MAC_OS
	       " ClassicMacOS"
#endif
	       "\n");
#endif
	return 1;
}

void MwShow(MwWidget handle, int toggle) {
	MwLLShow(handle->lowlevel, toggle);
}

void MwReparent(MwWidget handle, MwWidget new_parent) {
	if(handle->parent != NULL) {
		int i;
		for(i = 0; i < arrlen(handle->parent->children); i++) {
			if(handle->parent->children[i] == handle) {
				arrdel(handle->parent->children, i);
				break;
			}
		}

		MwDispatch4(handle->parent, children_update, handle, 0);
	}

	handle->parent = new_parent;
	if(new_parent != NULL) {
		arrput(new_parent->children, handle);

		MwDispatch4(handle->parent, children_update, handle, 1);
	}

	if(new_parent == NULL) {
		handle->top_step = 1;
		arrput(handle->tick_list, handle);
	} else {
		handle->top_step = 0;
		arrfree(handle->tick_list);
	}

	MwForceRender(handle);
}

MwClass MwGetClass(MwWidget handle) {
	return handle->widget_class;
}

const char* MwGetName(MwWidget handle) {
	return handle->name;
}

MwWidget* MwGetChildren(MwWidget handle) {
	MwWidget* c = malloc(sizeof(*c) * (arrlen(handle->children) + 1));
	int	  i;

	for(i = 0; i < arrlen(handle->children); i++) {
		c[i] = handle->children[i];
	}
	c[arrlen(handle->children)] = 0;

	return c;
}

void MwGetCursorCoord(MwWidget handle, MwPoint* point) {
	MwLLGetCursorCoord(handle->lowlevel, point);
}

void MwGetScreenSize(MwWidget handle, MwRect* rect) {
	MwLLGetScreenSize(handle->lowlevel, rect);
}

int MwGetCoordinateType(MwWidget handle) {
	if(handle->parent == NULL || handle->parent->lowlevel == NULL) {
		return handle->lowlevel->common.coordinate_type;
	} else {
		return MwCOORDINATE_LOCAL;
	}
}

void MwGetClipboard(MwWidget handle, int clipboard_type) {
	MwLLGetClipboard(handle->lowlevel, clipboard_type);
}

void MwSetUser(MwWidget handle, void* user) {
	handle->user = user;
}

void* MwGetUser(MwWidget handle) {
	return handle->user;
}

void MwGetFrame(MwWidget handle, MwRect* rect) {
	rect->x	     = MwGetInteger(handle, MwNx);
	rect->y	     = MwGetInteger(handle, MwNy);
	rect->width  = MwGetInteger(handle, MwNwidth);
	rect->height = MwGetInteger(handle, MwNheight);
}

const char* MwVersionString = MwVERSION;
