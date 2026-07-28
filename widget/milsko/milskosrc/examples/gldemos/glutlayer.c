#include <Mw/Milsko.h>
#include <Mw/Widget/OpenGL.h>

MwWidget opengl;

static void draw(void);
static void idle(void);
static void reshape(int width, int height);
static void init(void);
static void key(int k);

static void MWAPI tick(MwWidget handle, void* user, void* client) {
	(void)handle;
	(void)user;
	(void)client;

	draw();
	idle();

	MwOpenGLSwapBuffer(opengl);
}

static void MWAPI resize(MwWidget handle, void* user, void* client) {
	int ww, wh;

	(void)user;
	(void)client;

	ww = MwGetInteger(handle, MwNwidth) - 100;
	wh = MwGetInteger(handle, MwNheight) - 100;

	MwVaApply(opengl,
		  MwNwidth, ww,
		  MwNheight, wh,
		  NULL);

	reshape(ww, wh);
}

static void MWAPI key_pressed(MwWidget handle, void* user, void* client) {
	(void)handle;
	(void)user;

	key(*(int*)client);
}

int main() {
	MwWidget window;

	MwLibraryInit();

	window = MwVaCreateWidget(MwWindowClass, "main", NULL, MwDEFAULT, MwDEFAULT, 500, 500,
				  MwNtitle, TITLE,
				  NULL);
	opengl = MwCreateWidget(MwOpenGLClass, "opengl", window, 50, 50, 400, 400);
	if(!opengl) {
		MwMessageBox(NULL, "Unable to create OpenGL window.", "Error", MwMB_ICONERROR);
	} else {
		MwOpenGLMakeCurrent(opengl);

		init();
		reshape(400, 400);

		MwAddUserHandler(window, MwNresizeHandler, resize, NULL);
		MwAddUserHandler(opengl, MwNtickHandler, tick, NULL);

		MwAddTickList(opengl);

		MwAddUserHandler(opengl, MwNkeyHandler, key_pressed, NULL);
	}

	MwLoop(window);
}
