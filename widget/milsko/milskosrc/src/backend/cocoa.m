/*
 * some implementation notes to know:
 * - Mac really does not like immediate mode drawing, there was some trick I tried where I rendered to a bitmap context but it was very slow. So instead we just convert it to a commands system which gets cleared out whenever MacOS Actually draws the window.
 * - We override NSApplication to intercept sendEvent and know when an event has been sent. When it is, doPending is sent. The appropriate "pending" function then returns whether that's set (before resetting it).
 * - We want the application to be *functional* on old MacOS, at a minimum of 10.4. There's obviously points where newer functions will have to be used, and in that case we use ObjC's "respondsToSelector" function to know if we can call the newer function.
 */

#include <Mw/Milsko.h>
#include "../../external/stb_ds.h"
#include "Mw/LowLevel/Cocoa.h"

#ifdef MW_VULKAN
#include <vulkan/vulkan_core.h>
#include <vulkan/vulkan_metal.h>
#endif

#ifdef __clang__
#pragma clang push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#endif

/*
 * GNUStep has the modal session be a global singleton and once we assign it we can't do any more. This breaks several widgets.
 * I barely care about supporting this so we'll just not support that, sorry.
 */
#ifndef __APPLE__
static bool canAssignModalSession = MwTRUE;
#else
#define canAssignModalSession 1
#endif

/* Coordinate flipping function (global, rect) */
static NSRect rectFlip(NSRect originFrame) {
	NSScreen* zeroScreen	   = [[NSScreen screens] objectAtIndex:0];
	double	  screenHeight	   = [zeroScreen frame].size.height;
	double	  originY	   = originFrame.origin.y;
	double	  frameHeight	   = originFrame.size.height;
	double	  destinationY	   = screenHeight - (originY + frameHeight);
	NSRect	  destinationFrame = originFrame;
	destinationFrame.origin.y  = destinationY;
	return destinationFrame;
}

/* Coordinate flipping function (global, point) */
static inline NSPoint pointFlip(NSPoint point) {
	return rectFlip(NSMakeRect(point.x, point.y, 0, 0)).origin;
}

/* Coordinate flipping function (local to view; rect) */
static NSRect localRectFlip(NSRect originFrame, NSView* view) {
	float  viewHeight = [view bounds].size.height;
	NSRect destinationFrame =
	    NSMakeRect(originFrame.origin.x,
		       [view bounds].size.height -
			   (originFrame.origin.y + originFrame.size.height),
		       originFrame.size.width, originFrame.size.height);
	return destinationFrame;
}

/* Coordinate flipping function (local to view; point) */
static inline NSPoint localPointFlip(NSPoint point, NSView* view) {
	return localRectFlip(NSMakeRect(point.x, point.y, 0, 0), view).origin;
}

/* Recursively dispatch a key event to a widget and its children */
static void recursive_dispatch_key(MwLL handle, int* k) {
	MwWidget h = (MwWidget)handle->common.user;
	MwLLDispatch(handle, key, k);
	if(h) {
		int i;
		for(i = 0; i < arrlen(h->children); i++) {
			MwLLDispatch(h->children[i]->lowlevel, key, k);
			if(arrlen(h->children[i]->children) > 0) {
				recursive_dispatch_key(h->children[i]->lowlevel, k);
			}
		}
	}
};
/* Recursively dispatch a key released event to a widget and its children */
static void recursive_dispatch_key_released(MwLL handle, int* k) {
	MwWidget h = (MwWidget)handle->common.user;
	MwLLDispatch(handle, key_released, k);
	if(h) {
		int i;
		for(i = 0; i < arrlen(h->children); i++) {
			MwLLDispatch(h->children[i]->lowlevel, key_released, k);
			if(arrlen(h->children[i]->children) > 0) {
				recursive_dispatch_key_released(h->children[i]->lowlevel, k);
			}
		}
	}
};

@implementation MilskoCocoaApplication
- (void)sendEvent:(NSEvent*)event {
	self->doPending = MwTRUE;
	[super sendEvent:event];
}
- (MwBool)pending {
	if(self->doPending) {
		self->doPending = MwFALSE;
		return MwTRUE;
	}
	return MwFALSE;
}
@end

@implementation MilskoCocoaView
- (id)initWithFrame:(NSRect)frame {
	[super initWithFrame:frame];
	self->commands = NULL;
	return self;
}

- (void)setChild {
	self->_child = MwTRUE;
}
- (void)destroy {
	arrfree(self->commands);
};

- (void)drawRect:(NSRect)dirtyRect {
	NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];
	int		   i;

	[super drawRect:dirtyRect];

	for(i = 0; i < arrlen(self->commands); i++) {
		draw_command cmd = self->commands[i];
		switch(cmd.type) {
		case COCOA_DRAW_COMMAND_POLY:
		{
			int	      n;
			NSBezierPath* path = [NSBezierPath bezierPath];

			NSColor* nscolor =
			    [NSColor colorWithCalibratedRed:cmd.poly.color.red / 255.
						      green:cmd.poly.color.green / 255.
						       blue:cmd.poly.color.blue / 255.
						      alpha:1.0];

			[nscolor setFill];
			for(n = 0; n < cmd.poly.points_count; n++) {
				NSPoint p = localPointFlip(NSMakePoint(cmd.poly.points[n].x, cmd.poly.points[n].y), self);
				p.x += [self bounds].origin.x;
				p.y += [self bounds].origin.y;
				if(n == 0) {
					[path moveToPoint:p];
				} else {
					[path lineToPoint:p];
				}
			}

			[path closePath];
			[path fill];

			free(cmd.poly.points);
		} break;

		case COCOA_DRAW_COMMAND_LINES:
		{
			int	      n;
			NSBezierPath* path = [NSBezierPath bezierPath];

			NSColor* nscolor =
			    [NSColor colorWithCalibratedRed:cmd.lines.color.red / 255.
						      green:cmd.lines.color.green / 255.
						       blue:cmd.lines.color.blue / 255.
						      alpha:1.0];

			[nscolor setFill];
			for(n = 0; n < 2; n++) {
				NSPoint p = localPointFlip(NSMakePoint(cmd.lines.points[n].x, cmd.lines.points[n].y), self);
				p.x += [self bounds].origin.x;
				p.y += [self bounds].origin.y;
				if(n == 0) {
					[path moveToPoint:p];
				} else {
					[path lineToPoint:p];
				}
			}

			[path closePath];
			[path stroke];

			free(cmd.lines.points);
		} break;

		case COCOA_DRAW_COMMAND_PIXMAP:
		{
			NSRect rect = localRectFlip(NSMakeRect(cmd.pixmap.rect.x, cmd.pixmap.rect.y, cmd.pixmap.rect.width, cmd.pixmap.rect.height), self);
			rect.origin.x += [self bounds].origin.x;
			rect.origin.y += [self bounds].origin.y;
			[cmd.pixmap.pixmap->cocoa.real->image
			    drawInRect:rect
			      fromRect:NSZeroRect
			     operation:NSCompositeSourceOver
			      fraction:1.0];
			MwLLDestroyPixmap(cmd.pixmap.pixmap);
		} break;
		}
	}

	arrfree(self->commands);

	[pool release];
}

- (BOOL)isFlipped {
	return NO;
}

- (BOOL)canBecomeKeyView {
	return YES;
}
- (BOOL)acceptsFirstResponder {
	return YES;
}

- (void)handleKeyEvent:(NSEvent*)ev ll:(MwLL)ll down:(MwBool)isDown {
	int ch;
	/* todo: consolidate these values into the below switch case. */
	enum {
		kVK_ANSI_A	      = 0x00,
		kVK_ANSI_S	      = 0x01,
		kVK_ANSI_D	      = 0x02,
		kVK_ANSI_F	      = 0x03,
		kVK_ANSI_H	      = 0x04,
		kVK_ANSI_G	      = 0x05,
		kVK_ANSI_Z	      = 0x06,
		kVK_ANSI_X	      = 0x07,
		kVK_ANSI_C	      = 0x08,
		kVK_ANSI_V	      = 0x09,
		kVK_ANSI_B	      = 0x0B,
		kVK_ANSI_Q	      = 0x0C,
		kVK_ANSI_W	      = 0x0D,
		kVK_ANSI_E	      = 0x0E,
		kVK_ANSI_R	      = 0x0F,
		kVK_ANSI_Y	      = 0x10,
		kVK_ANSI_T	      = 0x11,
		kVK_ANSI_1	      = 0x12,
		kVK_ANSI_2	      = 0x13,
		kVK_ANSI_3	      = 0x14,
		kVK_ANSI_4	      = 0x15,
		kVK_ANSI_6	      = 0x16,
		kVK_ANSI_5	      = 0x17,
		kVK_ANSI_Equal	      = 0x18,
		kVK_ANSI_9	      = 0x19,
		kVK_ANSI_7	      = 0x1A,
		kVK_ANSI_Minus	      = 0x1B,
		kVK_ANSI_8	      = 0x1C,
		kVK_ANSI_0	      = 0x1D,
		kVK_ANSI_RightBracket = 0x1E,
		kVK_ANSI_O	      = 0x1F,
		kVK_ANSI_U	      = 0x20,
		kVK_ANSI_LeftBracket  = 0x21,
		kVK_ANSI_I	      = 0x22,
		kVK_ANSI_P	      = 0x23,
		kVK_ANSI_L	      = 0x25,
		kVK_ANSI_J	      = 0x26,
		kVK_ANSI_Quote	      = 0x27,
		kVK_ANSI_K	      = 0x28,
		kVK_ANSI_Semicolon    = 0x29,
		kVK_ANSI_Backslash    = 0x2A,
		kVK_ANSI_Comma	      = 0x2B,
		kVK_ANSI_Slash	      = 0x2C,
		kVK_ANSI_N	      = 0x2D,
		kVK_ANSI_M	      = 0x2E,
		kVK_ANSI_Period	      = 0x2F,
		kVK_ANSI_Grave	      = 0x32,
		kVK_Return	      = 0x24,
		kVK_Space	      = 0x31,
		kVK_Escape	      = 0x35,
		kVK_Shift	      = 0x38,
		kVK_Control	      = 0x3B,
		kVK_RightShift	      = 0x3C,
		kVK_RightControl      = 0x3E,
		kVK_LeftArrow	      = 0x7B,
		kVK_RightArrow	      = 0x7C,
		kVK_DownArrow	      = 0x7D,
		kVK_UpArrow	      = 0x7E
	};
#define KEY_CASE(x, y) \
	case x: \
		ch = y; \
		break;
	switch([ev keyCode]) {
		KEY_CASE(kVK_ANSI_A, 'a')
		KEY_CASE(kVK_ANSI_B, 'b')
		KEY_CASE(kVK_ANSI_C, 'c')
		KEY_CASE(kVK_ANSI_D, 'd')
		KEY_CASE(kVK_ANSI_E, 'e')
		KEY_CASE(kVK_ANSI_F, 'f')
		KEY_CASE(kVK_ANSI_G, 'g')
		KEY_CASE(kVK_ANSI_H, 'h')
		KEY_CASE(kVK_ANSI_I, 'i')
		KEY_CASE(kVK_ANSI_J, 'j')
		KEY_CASE(kVK_ANSI_K, 'k')
		KEY_CASE(kVK_ANSI_L, 'l')
		KEY_CASE(kVK_ANSI_M, 'm')
		KEY_CASE(kVK_ANSI_N, 'n')
		KEY_CASE(kVK_ANSI_O, 'o')
		KEY_CASE(kVK_ANSI_P, 'p')
		KEY_CASE(kVK_ANSI_Q, 'q')
		KEY_CASE(kVK_ANSI_R, 'r')
		KEY_CASE(kVK_ANSI_S, 's')
		KEY_CASE(kVK_ANSI_T, 't')
		KEY_CASE(kVK_ANSI_U, 'u')
		KEY_CASE(kVK_ANSI_V, 'v')
		KEY_CASE(kVK_ANSI_W, 'w')
		KEY_CASE(kVK_ANSI_X, 'x')
		KEY_CASE(kVK_ANSI_Y, 'y')
		KEY_CASE(kVK_ANSI_Z, 'z')
		KEY_CASE(kVK_ANSI_0, '0')
		KEY_CASE(kVK_ANSI_1, '1')
		KEY_CASE(kVK_ANSI_2, '2')
		KEY_CASE(kVK_ANSI_3, '3')
		KEY_CASE(kVK_ANSI_4, '4')
		KEY_CASE(kVK_ANSI_5, '5')
		KEY_CASE(kVK_ANSI_6, '6')
		KEY_CASE(kVK_ANSI_7, '7')
		KEY_CASE(kVK_ANSI_8, '8')
		KEY_CASE(kVK_ANSI_9, '9')
		KEY_CASE(kVK_ANSI_Quote, '\"')
		KEY_CASE(kVK_ANSI_Grave, '`')
		KEY_CASE(kVK_ANSI_Backslash, '/')
		KEY_CASE(kVK_ANSI_Comma, ',')
		KEY_CASE(kVK_ANSI_Equal, '=')
		KEY_CASE(kVK_Escape, MwKEY_ESCAPE)
		KEY_CASE(kVK_ANSI_LeftBracket, '[')
		KEY_CASE(kVK_ANSI_Minus, '-')
		KEY_CASE(kVK_ANSI_Period, '.')
		KEY_CASE(kVK_Return, MwKEY_ENTER)
		KEY_CASE(kVK_ANSI_RightBracket, ']')
		KEY_CASE(kVK_ANSI_Semicolon, ';')
		KEY_CASE(kVK_ANSI_Slash, '\\')
		KEY_CASE(kVK_Space, ' ')
		KEY_CASE(kVK_Control, MwKEY_CONTROL)
		KEY_CASE(kVK_RightControl, MwKEY_CONTROL)
		KEY_CASE(kVK_Shift, MwKEY_LEFTSHIFT)
		KEY_CASE(kVK_RightShift, MwKEY_RIGHTSHIFT)
		KEY_CASE(kVK_DownArrow, MwKEY_DOWN)
		KEY_CASE(kVK_LeftArrow, MwKEY_LEFT)
		KEY_CASE(kVK_RightArrow, MwKEY_RIGHT)
		KEY_CASE(kVK_UpArrow, MwKEY_UP)
	}
	if(isDown) {
		recursive_dispatch_key(ll, &ch);
	} else {
		recursive_dispatch_key_released(ll, &ch);
	}
	[ll->cocoa.real nudge];
}
- (void)mouseEntered:(NSEvent*)ev {
	MwLL h = [((MilskoFakePointer*)[[self subviews] objectAtIndex:0]) pointer];
	MwLLDispatch(h, focus_in, NULL);
	[h->cocoa.real pushCursor];
	[super mouseEntered:ev];
}
- (void)mouseExited:(NSEvent*)ev {
	MwLL h = [((MilskoFakePointer*)[[self subviews] objectAtIndex:0]) pointer];
	MwLLDispatch(h, focus_out, NULL);
	[h->cocoa.real popCursor];
	[super mouseExited:ev];
}
- (void)keyDown:(NSEvent*)ev {
	MwLL h = [((MilskoFakePointer*)[[self subviews] objectAtIndex:0]) pointer];
	[self handleKeyEvent:ev ll:h down:MwTRUE];
	[super keyDown:ev];
}
- (void)keyUp:(NSEvent*)ev {
	MwLL h = [((MilskoFakePointer*)[[self subviews] objectAtIndex:0]) pointer];
	[self handleKeyEvent:ev ll:h down:MwFALSE];
	[super keyUp:ev];
}

- (void)mouseMoved:(NSEvent*)ev {
	MwMouse mouse;
	NSPoint mousePoint = [ev locationInWindow];
	MwLL this	   = [((MilskoFakePointer*)[[self subviews] objectAtIndex:0]) pointer];
	mouse.button	   = MwMOUSE_LEFT;
	mouse.point.x	   = mousePoint.x;
	mouse.point.y	   = mousePoint.y;
	MwLLDispatch(this, move, &mouse);
	[super mouseMoved:ev];
}
- (BOOL)mouseDownCanMoveWindow {
	return NO;
}

- (void)mouseDown:(NSEvent*)ev {
	MwMouse mouse;
	NSPoint mousePoint = [ev locationInWindow];
	MwLL this	   = [((MilskoFakePointer*)[[self subviews] objectAtIndex:0]) pointer];
	mousePoint.y	   = [[ev window] frame].size.height - mousePoint.y;
	mouse.button	   = MwMOUSE_LEFT;
	mouse.point.x	   = mousePoint.x;
	mouse.point.y	   = mousePoint.y;
	MwLLDispatch(this, down, &mouse);
	[super mouseDown:ev];
}

- (void)mouseUp:(NSEvent*)ev {
	MwMouse mouse;
	NSPoint mousePoint = [ev locationInWindow];
	MwLL this	   = [((MilskoFakePointer*)[[self subviews] objectAtIndex:0]) pointer];
	mousePoint.y	   = [[ev window] frame].size.height - mousePoint.y;
	mouse.button	   = MwMOUSE_LEFT;
	mouse.point.x	   = mousePoint.x;
	mouse.point.y	   = mousePoint.y;
	MwLLDispatch(this, up, &mouse);
	[super mouseUp:ev];
}
- (void)rightMouseDown:(NSEvent*)ev {
	MwMouse mouse;
	NSPoint mousePoint = [ev locationInWindow];
	MwLL this	   = [((MilskoFakePointer*)[[self subviews] objectAtIndex:0]) pointer];
	mousePoint.y	   = [[ev window] frame].size.height - mousePoint.y;
	mouse.button	   = MwMOUSE_RIGHT;
	mouse.point.x	   = mousePoint.x;
	mouse.point.y	   = mousePoint.y;
	MwLLDispatch(this, down, &mouse);
	[super rightMouseDown:ev];
}

- (void)rightMouseUp:(NSEvent*)ev {
	MwMouse mouse;
	NSPoint mousePoint = [ev locationInWindow];
	MwLL this	   = [((MilskoFakePointer*)[[self subviews] objectAtIndex:0]) pointer];
	mousePoint.y	   = [[ev window] frame].size.height - mousePoint.y;
	mouse.button	   = MwMOUSE_RIGHT;
	mouse.point.x	   = mousePoint.x;
	mouse.point.y	   = mousePoint.y;
	MwLLDispatch(this, up, &mouse);
	[super rightMouseUp:ev];
}

@end

@implementation MilskoCocoaApplicationDelegate
- (MilskoCocoaApplicationDelegate*)initWithAppl:(MilskoCocoaApplication*)_appl {
	self->appl = _appl;
	return self;
}
@end

@implementation MilskoCocoaWindowDelegate

- (NSSize)windowWillResize:(NSWindow*)win toSize:(NSSize)frameSize;
{
	int	   i;
	NSUInteger placeholder = [[[win contentView] subviews] count];
	for(i = 0; i < placeholder; i++) {
		NSView* ptr = [[[win contentView] subviews] objectAtIndex:i];
		if([ptr isKindOfClass:[MilskoFakePointer class]]) {
			MwLL h = [(MilskoFakePointer*)ptr pointer];
			MwLLDispatch(h, resize, NULL);
			break;
		}
	}
	return frameSize;
}

- (void)windowDidBecomeMain:(NSNotification*)notification {
	if([[[w contentView] subviews] count] != 0) {
		MwLL h = [((MilskoFakePointer*)[[[w contentView] subviews]
		    objectAtIndex:0]) pointer];
		MwLLDispatch(h, focus_in, NULL);
	}
	[self->w makeKeyAndOrderFront:nil];
}

- (MilskoCocoaWindowDelegate*)initWithWin:(NSWindow*)win {
	self->w = win;
	return self;
}

- (void)windowWillClose:(NSNotification*)notification {
	if([[[w contentView] subviews] count] != 0) {
		MwLL h = [((MilskoFakePointer*)[[[w contentView] subviews]
		    objectAtIndex:0]) pointer];
		MwLLDispatch(h, close, NULL);
		h->cocoa.real->isClosing = MwTRUE;
	}
}

@end

@implementation MilskoFakePointer
- (void)setPointer:(void*)pointer {
	[self setFrame:*(NSRect*)&pointer];
	self->ptr = pointer;
};
- (void*)pointer {
	return self->ptr;
};

- (void)drawRect:(NSRect)dirtyRect {
	/* explicitly do nothing */
	(void)dirtyRect;
}

- (void)destroy {
}
@end

@implementation MilskoCocoaWindow
- (BOOL)canBecomeKeyWindow {
	return true;
}
- (BOOL)canBecomeMainWindow {
	return true;
}

@end

@implementation MilskoCocoaPixmap
+ (MilskoCocoaPixmap*)newWithWidth:(int)width height:(int)height {
	MilskoCocoaPixmap* p = [MilskoCocoaPixmap alloc];

	p->width  = width;
	p->height = height;
	p->image  = NULL;

	p->buf = malloc(width * height * 4);

	p->rep =
	    [[NSBitmapImageRep alloc] initWithBitmapDataPlanes:&p->buf
						    pixelsWide:(int)width
						    pixelsHigh:(int)height
						 bitsPerSample:8
					       samplesPerPixel:4
						      hasAlpha:YES
						      isPlanar:NO
						colorSpaceName:NSDeviceRGBColorSpace
						   bytesPerRow:(int)width * 4
						  bitsPerPixel:32];
	assert(p->rep);

	p->image = [[NSImage alloc] initWithSize:NSMakeSize(width, height)];
	assert(p->image);
	[p->image addRepresentation:p->rep];

	return p;
}
- (void)updateWithData:(unsigned char*)_data {
	memcpy(self->buf, _data, width * height * 4);
	if(self->rep) {
		[self->image removeRepresentation:self->rep];
		[self->rep release];
	}
	self->rep =
	    [[NSBitmapImageRep alloc] initWithBitmapDataPlanes:&self->buf
						    pixelsWide:(int)width
						    pixelsHigh:(int)height
						 bitsPerSample:8
					       samplesPerPixel:4
						      hasAlpha:YES
						      isPlanar:NO
						colorSpaceName:NSDeviceRGBColorSpace
						   bytesPerRow:(int)width * 4
						  bitsPerPixel:32];
	assert(self->rep);
	[self->image addRepresentation:self->rep];
}
- (void)destroy {
	free(self->buf);
	[self->image removeRepresentation:self->rep];
	[self->image release];
	[self->rep release];
	self->image = NULL;
	self->rep   = NULL;
}
- (NSImage*)image {
	return self->image;
}
@end

@implementation MilskoCocoa
- (void)sendClipboardEvent {
	/* Either uncomment this and make it use respondsToSelector and/or find out what the api proceeding NSPasteboard is. */
	/*NSAutoreleasePool* pool	      = [[NSAutoreleasePool alloc] init];
	NSPasteboard*	   pasteboard = [NSPasteboard generalPasteboard];
	NSArray*	   items      = @[
		       @"public.utf8-plain-text",
		       @"public.utf16-external-plain-text",
		       @"com.apple.traditional-mac-plain-text",
	];
	MwLL this = self->handle.pointer;

	if([pasteboard canReadItemWithDataConformingToTypes:items]) {
		char*  data = NULL;
		size_t size = 0;
		for(NSPasteboardItem* item in [pasteboard pasteboardItems]) {
			for(NSString* it in items) {
				NSString* itemData = [item
	stringForType:(NSString*)it]; if(itemData != NULL) { if(strHash != 0 &&
	strHash != [itemData hash]) { char* text = malloc([itemData length]);
						strncpy(text, [itemData UTF8String],
	[itemData length]); MwLLDispatch(this, clipboard, text); printf("%s -> %p\n",
	text, this); free(text);
					}
					strHash = [itemData hash];
				}
			}
		}
	}

	[pool release];*/
}

- (void)destroy {
	[self->handle release];
	[self->window release];
}

- (MwLL)getParent {
	return self->parent;
}

- (NSView*)getView {
	return view;
}
- (NSWindow*)getWindow {
	return window;
}

- (void)nudge {
	MwLL	 topmost_parent = [self->handle pointer];
	MwWidget h;

	while(topmost_parent->cocoa.real->parent) topmost_parent = topmost_parent->cocoa.real->parent;

	if(!topmost_parent->cocoa.real->isClosing)
		self->_eventsPending = MwTRUE;

	[topmost_parent->cocoa.real->view setNeedsDisplay:true];
	MwLLDispatch(topmost_parent, draw, NULL);

	h = (MwWidget)topmost_parent->common.user;
	if(h) {
		int i;
		for(i = 0; i < arrlen(h->children); i++) {
			[h->children[i]->lowlevel->cocoa.real->view setNeedsDisplay:true];
			MwLLDispatch(h->children[i]->lowlevel, draw, NULL);
		}
	}

	// MwLL p		     = self->parent;
	// self->_eventsPending = MwTRUE;

	// while(p) {
	// 	MwLLDispatch(p, draw, NULL);
	// 	p = p->cocoa.real->parent;
	// }
}

- (void)pushCursor {
	[self->cursor push];
}
- (void)popCursor {
	[self->cursor pop];
}

- (MilskoFakePointer*)getHandle {
	return handle;
}

@end

static MwLL MwLLCreateImpl(MwLL parent, int x, int y, int width, int height) {
	MwLL		   r;
	NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];
	MilskoCocoa*	   c	= [MilskoCocoa alloc];
	r			= malloc(sizeof(*r));

	MwLLCreateCommon(r);

	[c retain];
	c->application = (MilskoCocoaApplication*)[MilskoCocoaApplication sharedApplication];
	[c->application retain];

	/*
	 * MacOS doesn't really have a "default" window position, you're actually
	 * meant to center the window yourself, so if the user passes MwDEFAULT
	 * respond appropriately. Also for child windows just have it be 0.
	 */
	if(x == MwDEFAULT) {
		x = parent ? 0 : ([[NSScreen mainScreen] frame].size.width / 2.) - (width / 2.);
	}
	if(y == MwDEFAULT) {
		y = parent ? 0 : ([[NSScreen mainScreen] frame].size.height / 2.) - (height / 2.);
	}
	c->rect = NSMakeRect(x, y, width, height);

	if(parent == NULL) {
		c->window = [[NSWindow alloc]
		    initWithContentRect:rectFlip(c->rect)
			      styleMask:(NSTitledWindowMask |
					 NSClosableWindowMask | NSMiniaturizableWindowMask |
					 NSResizableWindowMask)
				backing:NSBackingStoreBuffered
				  defer:NO];
		[c->window
		    setDelegate:[[MilskoCocoaWindowDelegate alloc] initWithWin:c->window]];
		[c->window retain];

		[c->window makeKeyAndOrderFront:c->application];
		[c->window makeFirstResponder:c->view];

		c->view = [[MilskoCocoaView alloc] initWithFrame:c->rect];
		[c->view retain];
		[c->window setContentView:c->view];

		/* In order for our application to be front and center when launched, we need to use this function introduced in 10.5. There's no function that works on 10.4 or below as far as I know because what you're actually supposed to do is use a feature of XCode project files to modify the settings of the compiled app to set the activation policy. */
		if([c->application respondsToSelector:@selector(setActivationPolicy:)]) {
			[c->application
			    setActivationPolicy:0 /* NSApplicationActivationPolicyRegular */];
		}

		[c->application activateIgnoringOtherApps:MwTRUE];
		[c->application setDelegate:[[MilskoCocoaApplicationDelegate alloc]
						initWithAppl:c->application]];
		[c->application retain];

		[c->view addTrackingRect:c->rect owner:c->view userData:NULL assumeInside:MwFALSE];
		if(canAssignModalSession)
			c->modalSession = [c->application beginModalSessionForWindow:c->window];

#ifndef __APPLE__
		canAssignModalSession = MwFALSE;
#endif

	} else {
		MilskoCocoa* p	  = parent->cocoa.real;
		NSRect	     rect = localRectFlip(c->rect, p->view);

		c->window = p->window;
		c->view	  = [[MilskoCocoaView alloc] initWithFrame:rect];
		[c->view setBounds:rect];
		[c->view retain];
		[c->view setChild];

		[c->view addTrackingRect:c->rect owner:c->view userData:NULL assumeInside:MwFALSE];

		[p->view addSubview:c->view];

		c->modalSession = p->modalSession;
	}
	[c->window setAcceptsMouseMovedEvents:MwTRUE];

	c->handle = [[MilskoFakePointer alloc] initWithFrame:NSMakeRect(0, 0, 1, 1)];
	[c->handle retain];
	[c->handle setPointer:r];
	[c->view addSubview:c->handle];

	c->parent = parent;

	c->_forceRender = MwTRUE;
	c->strHash	= 0;

	c->cursorPixmap = NULL;

	[c->application finishLaunching];

	c->pointerLocked = MwFALSE;

	[pool release];

	c->isClosing = MwFALSE;

	r->cocoa.real = c;

	return r;
}

static void MwLLDestroyImpl(MwLL handle) {
	MilskoCocoa* h = handle->cocoa.real;

	[h destroy];

	MwLLDestroyCommon(handle);

	free(handle);
}

static void MwLLBeginDrawImpl(MwLL handle) {
	(void)handle;
}

static void MwLLEndDrawImpl(MwLL handle) {
	(void)handle;
	// [handle->cocoa.real->view displayRect:[handle->cocoa.real->window frame]];
}

static void MwLLPolygonImpl(MwLL handle, MwPoint* points, int points_count,
			    MwLLColor color) {
	draw_command poly;
	poly.type = COCOA_DRAW_COMMAND_POLY;

	poly.poly.color	       = color->common;
	poly.poly.points_count = points_count;

	poly.poly.points = malloc(sizeof(MwPoint) * points_count);
	memcpy(poly.poly.points, points, sizeof(MwPoint) * points_count);

	arrpush(handle->cocoa.real->view->commands, poly);

	[handle->cocoa.real nudge];
}

static void MwLLLineImpl(MwLL handle, MwPoint* points, MwLLColor color) {
	draw_command lines;
	lines.type = COCOA_DRAW_COMMAND_LINES;

	lines.lines.color = color->common;

	lines.lines.points = malloc(sizeof(MwPoint) * 2);
	memcpy(lines.lines.points, points, sizeof(MwPoint) * 2);

	arrpush(handle->cocoa.real->view->commands, lines);

	[handle->cocoa.real nudge];
}

static MwLLColor MwLLAllocColorImpl(MwLL handle, int r, int g, int b) {
	MwLLColor c = malloc(sizeof(*c));
	MwLLColorUpdate(handle, c, r, g, b);
	return c;
}

static void MwLLColorUpdateImpl(MwLL handle, MwLLColor c, int r, int g, int b) {
	(void)handle;

	c->common.red	= r;
	c->common.green = g;
	c->common.blue	= b;
}

static void MwLLGetXYWHImpl(MwLL handle, int* x, int* y, unsigned int* w,
			    unsigned int* h) {
	MilskoCocoa* self = handle->cocoa.real;
	NSRect	     frame;
	if(self->parent) {
		frame = [self->view frame];
	} else {
		frame = [self->window frame];
	}

	if(!self->parent) {
		frame = rectFlip(frame);
	} else {
		frame = localRectFlip(frame, self->parent->cocoa.real->view);
	}

	*x = frame.origin.x;
	*y = frame.origin.y;

	*w = frame.size.width;
	*h = frame.size.height;
}

static void MwLLSetXYImpl(MwLL handle, int x, int y) {
	MilskoCocoa* self = handle->cocoa.real;
	NSRect	     frame;
	if(self->parent) {
		frame = [self->view frame];
	} else {
		frame = [self->window frame];
	}

	frame.origin.x = x;
	frame.origin.y = y;

	if(!self->parent) {
		frame = rectFlip(frame);
		[self->window setFrame:frame display:MwTRUE animate:false];
	} else {
		frame = localRectFlip(frame, self->parent->cocoa.real->view);
		[self->view setFrame:frame];
		[self->view setBounds:frame];
	}
	MwLLForceRender(handle);
}

static void MwLLSetWHImpl(MwLL handle, int w, int h) {
	MilskoCocoa* self = handle->cocoa.real;
	NSRect	     frame;
	if(self->parent) {
		frame = [self->view frame];
	} else {
		frame = [self->window frame];
	}

	frame.size.width  = w;
	frame.size.height = h;

	self->rect = frame;

	[self->view setFrameSize:frame.size];
	if(self->parent) {
		[self->view setFrame:frame];
		[self->view setBounds:frame];
	} else {
		[self->window setFrame:frame display:YES animate:false];
	}
	MwLLForceRender(handle);
}

static void MwLLFreeColorImpl(MwLLColor color) {
	free(color);
}

static int MwLLPendingImpl(MwLL handle) {
	MilskoCocoa*	   self	     = handle->cocoa.real;
	NSAutoreleasePool* pool	     = [[NSAutoreleasePool alloc] init];
	MwBool		   isPending = [self->application pending];

	if(self->modalSession) {
		[self->application runModalSession:self->modalSession];
	}
	[pool release];
	return !self->isClosing && (self->_forceRender || self->_eventsPending || isPending);
}

static void MwLLNextEventImpl(MwLL handle) {
	MilskoCocoa* self = handle->cocoa.real;
	MwLL	     h	  = [self->handle pointer];
	if(self->_forceRender) {
		MwLLDispatch(h, draw, NULL);
		self->_forceRender = MwFALSE;
	}
	if(self->_eventsPending) {
		self->_eventsPending = MwFALSE;
	}
	[self sendClipboardEvent];

#ifdef __APPLE__
	if(self->pointerLocked && [self->window isMainWindow] && self->lastEvent) {
		struct CGPoint pos;
		pos.x = [self->window frame].origin.x + [self->window frame].size.width / 2;
		pos.y = [self->window frame].origin.y + [self->window frame].size.height / 2;

		CGWarpMouseCursorPosition(pos);
	}
#endif

	[self->application updateWindows];
}

static void MwLLSetTitleImpl(MwLL handle, const char* title) {
	NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];
	[handle->cocoa.real->window
	    setTitleWithRepresentedFilename:[NSString stringWithUTF8String:title]];
	[pool release];
}

static MwLLPixmap MwLLCreatePixmapImpl(MwLL handle, unsigned char* data,
				       int width, int height) {
	MwLLPixmap r = malloc(sizeof(*r));

	(void)handle;
	memset(r, 0, sizeof(*r));

	r->common.raw = malloc(4 * width * height);
	memcpy(r->common.raw, data, 4 * width * height);

	r->common.width	 = width;
	r->common.height = height;

	r->cocoa.real = [MilskoCocoaPixmap newWithWidth:width height:height];

	MwLLPixmapUpdate(r);
	return r;
}

static void MwLLPixmapUpdateImpl(MwLLPixmap pixmap) {
	MilskoCocoaPixmap* self = pixmap->cocoa.real;
	memcpy(self->buf, pixmap->common.raw, pixmap->common.width * pixmap->common.height * 4);
	[pixmap->cocoa.real updateWithData:pixmap->common.raw];
}

static void MwLLDestroyPixmapImpl(MwLLPixmap pixmap) {
	free(pixmap->common.raw);
	[pixmap->cocoa.real destroy];
	[pixmap->cocoa.real release];
	free(pixmap);
}

static void MwLLDrawPixmapImpl(MwLL handle, MwRect* rect, MwLLPixmap pixmap) {
	draw_command pix;
	pix.type = COCOA_DRAW_COMMAND_PIXMAP;

	pix.pixmap.rect = *rect;

	/* copy the pixmap into a new object that we'll free later */
	pix.pixmap.pixmap = MwLLCreatePixmap(handle, pixmap->common.raw, pixmap->common.width, pixmap->common.height);

	arrpush(handle->cocoa.real->view->commands, pix);

	[handle->cocoa.real nudge];
}

static void MwLLSetIconImpl(MwLL handle, MwLLPixmap pixmap) {
	[handle->cocoa.real->application setApplicationIconImage:[pixmap->cocoa.real image]];
}

static void MwLLForceRenderImpl(MwLL handle) {
	handle->cocoa.real->_forceRender = MwTRUE;
}

static void MwLLSetCursorImpl(MwLL handle, MwCursor* image, MwCursor* mask) {
	MilskoCocoa*   self = handle->cocoa.real;
	int	       y, x, ys, xs;
	unsigned char* di = malloc(image->width * image->height * 4);
	memset(di, 0, image->width * image->height * 4);

	if(self->cursorPixmap) {
		[self->cursorPixmap destroy];
	}
	self->cursorPixmap =
	    [MilskoCocoaPixmap newWithWidth:image->width
				     height:image->height];

	xs = -mask->x + image->x;
	ys = MwCursorDataHeight + mask->y;
	ys = MwCursorDataHeight + image->y - ys;

	for(y = 0; y < mask->height; y++) {
		unsigned int d = mask->data[y];
		for(x = mask->width - 1; x >= 0; x--) {
			int px	= 0;
			int idx = ((y * mask->width) + x) * 4;

			if(d & 1) {
				di[idx + 3] = 255;
			};
			d = d >> 1;
		}
	}
	for(y = 0; y < image->height; y++) {
		unsigned int d = image->data[y];
		for(x = image->width - 1; x >= 0; x--) {
			int px	= 0;
			int idx = ((y * image->width) + x) * 4;

			if(d & 1) {
				px = 255;
			};

			di[idx]	    = px;
			di[idx + 1] = px;
			di[idx + 2] = px;
			d	    = d >> 1;
		}
	}

	[self->cursorPixmap updateWithData:di];

	self->cursor = [[NSCursor alloc]
	    initWithImage:[self->cursorPixmap image]
		  hotSpot:NSMakePoint(image->x, image->y + image->height)];
	[self->cursor retain];

	[self->cursor pop];
	[self->cursor push];

	free(di);
}

static void MwLLDetachImpl(MwLL handle, MwPoint* point) {
	(void)handle;
	(void)point;
}

static void MwLLShowImpl(MwLL handle, int show) {
	(void)show;
}

static void MwLLMakePopupImpl(MwLL handle, MwLL parent) {
	(void)parent;
}

static void MwLLSetSizeHintsImpl(MwLL handle, int minx, int miny, int maxx,
				 int maxy) {
	MilskoCocoa* self = handle->cocoa.real;
	[self->window setMinSize:NSMakeSize(minx, miny)];
	[self->window setMaxSize:NSMakeSize(maxx, maxy)];
}

static void MwLLMakeBorderlessImpl(MwLL handle, int toggle) {
	MilskoCocoa* self = handle->cocoa.real;
	MwU32	     mask = [self->window styleMask];
	if(toggle) {
		mask ^= NSBorderlessWindowMask;
		mask |= NSTitledWindowMask;
	} else {
		mask |= NSBorderlessWindowMask;
		mask ^= NSTitledWindowMask;
	}
	[self->window initWithContentRect:self->rect
				styleMask:mask
				  backing:NSBackingStoreBuffered
				    defer:NO];
}

static void MwLLFocusImpl(MwLL handle) {
	[handle->cocoa.real->application activateIgnoringOtherApps:MwTRUE];
}

static void MwLLGrabPointerImpl(MwLL handle, int toggle) {
	handle->cocoa.real->pointerLocked = toggle;
}

static void MwLLSetClipboardImpl(MwLL handle, const char* text, int clipboard_type) {
	(void)handle;
	(void)text;
	(void)clipboard_type;
}

static void MwLLGetClipboardImpl(MwLL handle, int clipboard_type) {
	(void)handle;
	(void)clipboard_type;
}

static void MwLLMakeToolWindowImpl(MwLL handle) {
	/* If my understand of what a "tool window" usually is is correct then I
	 * highly doubt the Mac OS has this and if they did they probably outright
	 * removed it along time ago is this kind of conflicts with modern UX. So
	 * we'll just make it borderless idgaf */
	MwLLMakeBorderlessImpl(handle, 1);
}

static void MwLLGetCursorCoordImpl(MwLL handle, MwPoint* point) {
	NSPoint p = [NSEvent mouseLocation];
	point->x  = p.x;
	point->y  = p.y;
}

static void MwLLGetScreenSizeImpl(MwLL handle, MwRect* rect) {
	MilskoCocoa* self   = handle->cocoa.real;
	NSScreen*    screen = [self->window screen];
	rect->x		    = [screen frame].origin.x;
	rect->y		    = [screen frame].origin.y;
	rect->width	    = [screen frame].size.width;
	rect->height	    = [screen frame].size.height;
}

static void MwLLBeginStateChangeImpl(MwLL handle) {
	MwLLShow(handle, 0);
}

static void MwLLEndStateChangeImpl(MwLL handle) {
	MwLLShow(handle, 1);
}

static void MwLLSetDarkThemeImpl(MwLL handle, int toggle) {
	(void)handle;
	(void)toggle;
}

static MwBool MwLLDoModernImpl(MwLL handle) {
	(void)handle;
	return MwTRUE;
}

static void MwLLRaiseImpl(MwLL handle) {
	(void)handle;
}

static void MwLLClipImpl(MwLL handle, MwRect* rect) {
	(void)handle;
	(void)rect;
}

static int MwLLCocoaCallInitImpl(void) {
#ifndef __APPLE__
	printf("Using GNUStep Backend\n");
#endif

	[MilskoCocoaApplication sharedApplication];

	return 0;
}

#ifdef MW_VULKAN
VkMetalSurfaceCreateInfoEXT MwCocoaGetMetalSurfaceCreateInfo(MwWidget handle) {
	VkMetalSurfaceCreateInfoEXT createInfo = {
	    .sType  = VK_STRUCTURE_TYPE_METAL_SURFACE_CREATE_INFO_EXT,
	    .pNext  = NULL,
	    .flags  = 0,
	    .pLayer = (CAMetalLayer*)[handle->lowlevel->cocoa.real->view layer],
	};
	return createInfo;
};
#endif

#include "call.c"
CALL(Cocoa);
