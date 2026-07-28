#include <Mw/Milsko.h>
#include <Mw/Widget/OpenGL.h>

#include <stdlib.h>

MwWidget opengl;

typedef struct triangle {
	int    points[3 * 2];
	double r;
	double g;
	double b;
} triangle_t;

triangle_t* t	  = NULL;
int	    ct	  = 0;
int	    click = 0;
int	    mx, my;

static void MWAPI tick(MwWidget handle, void* user, void* call) {
	int i;
	int w = MwGetInteger(opengl, MwNwidth);
	int h = MwGetInteger(opengl, MwNheight);

	(void)handle;
	(void)user;
	(void)call;

	MwOpenGLMakeCurrent(opengl);

	glClear(GL_COLOR_BUFFER_BIT);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, w, h, 0, -1, 1);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	for(i = 0; i < ct + (click == 2 ? 1 : 0); i++) {
		int j;
		glBegin(GL_TRIANGLES);
		glColor3d(t[i].r, t[i].g, t[i].b);
		for(j = 0; j < 3; j++) {
			glVertex2d(t[i].points[j * 2 + 0], t[i].points[j * 2 + 1]);
		}
		glEnd();
	}

	MwOpenGLSwapBuffer(opengl);
}

static void MWAPI mouse(MwWidget handle, void* user, void* call) {
	MwMouse* mouse = call;

	(void)handle;
	(void)user;

	if(mouse->button == MwMOUSE_LEFT) {
		t[ct].points[click * 2 + 0] = mouse->point.x;
		t[ct].points[click * 2 + 1] = mouse->point.y;
		click++;
		if(click == 1) {
			t[ct].r = rand() % 65536;
			t[ct].g = rand() % 65536;
			t[ct].b = rand() % 65536;

			t[ct].r /= 65535;
			t[ct].g /= 65535;
			t[ct].b /= 65535;
		} else if(click == 2) {
			t[ct].points[2 * 2 + 0] = mx;
			t[ct].points[2 * 2 + 1] = my;
		} else if(click == 3) {
			triangle_t* old = t;
			int	    i;

			click = 0;
			ct++;

			t = malloc(sizeof(*t) * (ct + 1));
			for(i = 0; i < ct; i++) t[i] = old[i];
			free(old);
		}
	} else if(mouse->button == MwMOUSE_RIGHT) {
		if(click > 0) {
			click = 0;
		} else if(ct > 0) {
			ct--;
		}
	}
}

static void MWAPI mouse_move(MwWidget handle, void* user, void* call) {
	MwPoint* point = call;

	(void)handle;
	(void)user;

	mx = point->x;
	my = point->y;

	if(click == 2) {
		t[ct].points[2 * 2 + 0] = point->x;
		t[ct].points[2 * 2 + 1] = point->y;
	}
}

int main() {
	MwSizeHints hints;
	MwWidget    window, viewport;

	MwLibraryInit();

	window	 = MwVaCreateWidget(MwWindowClass, "main", NULL, MwDEFAULT, MwDEFAULT, 640, 480,
				    MwNtitle, "tripaint",
				    NULL);
	viewport = MwCreateWidget(MwViewportClass, "viewport", window, 5, 5, 630, 470 - 16 - 5);

	hints.min_width = hints.max_width = 640;
	hints.min_height = hints.max_height = 480;
	MwVaApply(window, MwNsizeHints, &hints, NULL);

	MwVaCreateWidget(MwLabelClass, "label", window, 5, 470 - 16 + 5, 630, 16,
			 MwNtext, "Press left click to draw triangle, right click to undo",
			 NULL);

	t = malloc(sizeof(*t));

	opengl = MwCreateWidget(MwOpenGLClass, "opengl", viewport, 0, 0, 1024, 768);

	MwAddUserHandler(window, MwNtickHandler, tick, NULL);
	MwAddUserHandler(opengl, MwNtickHandler, tick, NULL);
	MwAddUserHandler(opengl, MwNmouseDownHandler, mouse, NULL);
	MwAddUserHandler(opengl, MwNmouseMoveHandler, mouse_move, NULL);

	MwLoop(window);
}
