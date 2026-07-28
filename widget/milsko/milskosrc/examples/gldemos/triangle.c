#include <Mw/Milsko.h>
#include <Mw/Widget/OpenGL.h>

MwWidget window, opengl, button;
int	 ow, oh;
double	 deg = 0;
double	 dir = 1;

void MWAPI resize(MwWidget handle, void* user_data, void* call_data) {
	unsigned int w, h;

	(void)user_data;
	(void)call_data;

	w = MwGetInteger(handle, MwNwidth);
	h = MwGetInteger(handle, MwNheight);

	MwVaApply(opengl,
		  MwNwidth, (ow = w - 100),
		  MwNheight, (oh = h - 150),
		  NULL);

	MwVaApply(button,
		  MwNy, h - 50 - 50,
		  MwNwidth, ow,
		  MwNheight, 50,
		  NULL);
}

void MWAPI tick(MwWidget handle, void* user_data, void* call_data) {
	(void)handle;
	(void)user_data;
	(void)call_data;

	MwOpenGLMakeCurrent(opengl);

	glViewport(0, 0, ow, oh);
	glClear(GL_COLOR_BUFFER_BIT);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(-1, 1, -1, 1, -1, 1);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	glPushMatrix();
	glRotatef(deg, 0, 0, 1);
	glBegin(GL_TRIANGLES);
	glColor3f(1, 0, 0);
	glVertex2f(0, 0.8);
	glColor3f(0, 1, 0);
	glVertex2f(-0.8, -0.8);
	glColor3f(0, 0, 1);
	glVertex2f(0.8, -0.8);
	glEnd();
	glPopMatrix();

	MwOpenGLSwapBuffer(opengl);

	deg += 120.0 / (1000.0 / MwWaitMS) * dir;
}

void MWAPI activate(MwWidget handle, void* user_data, void* call_data) {
	(void)handle;
	(void)user_data;
	(void)call_data;

	dir = dir == 1 ? -1 : 1;
}

int main() {
	MwLibraryInit();

	window = MwVaCreateWidget(MwWindowClass, "main", NULL, MwDEFAULT, MwDEFAULT, 400, 450,
				  MwNtitle, "hello world",
				  NULL);
	opengl = MwCreateWidget(MwOpenGLClass, "opengl", window, 50, 50, (ow = 300), (oh = 300));
	button = MwVaCreateWidget(MwButtonClass, "button", window, 50, 350, 300, 50,
				  MwNtext, "Reverse",
				  NULL);

	MwAddUserHandler(window, MwNresizeHandler, resize, NULL);
	MwAddUserHandler(window, MwNtickHandler, tick, NULL);
	MwAddUserHandler(button, MwNactivateHandler, activate, NULL);

	MwLoop(window);
}
