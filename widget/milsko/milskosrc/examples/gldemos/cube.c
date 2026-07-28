#define TITLE "cube"
#include "glutlayer.c"

#if defined(__APPLE__)
#include <OpenGL/gl.h>
#include <OpenGL/glu.h>
#else
#include <GL/gl.h>
#include <GL/glu.h>
#endif

static double deg = 0;

static void draw(void) {
	int i;
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glPushMatrix();
	glRotatef(deg, 1, 0, 0);
	glRotatef(deg, 0, 1, 0);
	glRotatef(deg, 0, 0, 1);

	for(i = 0; i < 6; i++) {
		if(i == 0)
			glColor3f(1, 0, 0);
		if(i == 1)
			glColor3f(0, 1, 0);
		if(i == 2)
			glColor3f(1, 1, 0);
		if(i == 3)
			glColor3f(0, 0, 1);
		if(i == 4)
			glColor3f(1, 0, 1);
		if(i == 5)
			glColor3f(0, 1, 1);

		glBegin(GL_QUADS);
		glNormal3f(0, 1, 0);
		glVertex3f(-1, 1, -1);
		glVertex3f(-1, 1, 1);
		glVertex3f(1, 1, 1);
		glVertex3f(1, 1, -1);
		glEnd();

		if(i < 3)
			glRotatef(90, 0, 0, 1);
		if(i == 3)
			glRotatef(90, 1, 0, 0);
		if(i == 4)
			glRotatef(180, 0, 0, 1);
	}
	glPopMatrix();
}

static void idle(void) {
	deg += 1.0;
}

static void reshape(int width, int height) {
	GLfloat lpos[4];

	lpos[0] = 1;
	lpos[1] = 1;
	lpos[2] = 1;
	lpos[3] = 0;

	glViewport(0, 0, width, height);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluPerspective(60, (double)width / height, 1, 100);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	gluLookAt(2, 2, 2, 0, 0, 0, 0, 1, 0);

	glLightfv(GL_LIGHT0, GL_POSITION, lpos);
}

static void init(void) {
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_LIGHTING);
	glEnable(GL_LIGHT0);
	glEnable(GL_COLOR_MATERIAL);
	glEnable(GL_CULL_FACE);
	glEnable(GL_NORMALIZE);

	glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
}

static void key(int k) {
	(void)k;
}
