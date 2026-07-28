#include <Mw/Milsko.h>
#include <Mw/Widget/OpenGL.h>

#include <time.h>

MwWidget   window, opengl, ldate, ltime;
time_t	   last = 0;
struct tm* tm;
int	   br, bg, bb;
int	   fr, fg, fb;

double deg2rad(double deg) {
	return (360.0 - deg) / 180.0 * M_PI;
}

void MWAPI tick(MwWidget handle, void* user, void* call) {
	time_t t = time(NULL);
	int    i;
	double rad;
	int    render = 0;
	int    w      = MwGetInteger(opengl, MwNwidth);
	int    h      = MwGetInteger(opengl, MwNheight);

	(void)handle;
	(void)user;
	(void)call;

	if(last != t) {
		char* wday[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
		char* mon[]  = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

		char buf[512];
		tm = localtime(&t);

		MwStringPrintIntoBuffer(buf, 512, "%s %02d %s", mon[tm->tm_mon], tm->tm_mday, wday[tm->tm_wday]);
		MwSetText(ldate, MwNtext, buf);

		MwStringPrintIntoBuffer(buf, 512, "%02d:%02d %s", tm->tm_hour % 12, tm->tm_min, tm->tm_hour >= 12 ? "PM" : "AM");
		MwSetText(ltime, MwNtext, buf);

		render = 1;

		last = t;
	}

	MwOpenGLMakeCurrent(opengl);

	glClearColor(br / 255.0, bg / 255.0, bb / 255.0, 1);
	glClear(GL_COLOR_BUFFER_BIT);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(-1, 1, -1, 1, 0, 100);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	glLineWidth(2);
	for(i = 0; i < 12; i++) {
		rad = deg2rad(30.0 * i);
		glPointSize((i % 3) ? 2 : 5);
		glBegin(GL_POINTS);
		glColor3f(fr / 255.0, fg / 255.0, fb / 255.0);
		glVertex2f(cos(rad) * 0.85, sin(rad) * 0.85);
		glEnd();
	}

	rad = deg2rad((tm->tm_hour % 12) * 30 + (tm->tm_min / 2) - 90);
	glBegin(GL_LINES);
	glVertex2f(0, 0);
	glVertex2f(cos(rad) * 0.3, sin(rad) * 0.3);
	glEnd();

	rad = deg2rad(tm->tm_min * 6 - 90);
	glBegin(GL_LINES);
	glVertex2f(0, 0);
	glVertex2f(cos(rad) * 0.5, sin(rad) * 0.5);
	glEnd();

	if(render && w > 0 && h > 0) {
		unsigned char* buffer = malloc(w * h * 4);
		MwLLPixmap     px;
		int	       j;

		glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, buffer);
		for(i = 0; i < h / 2; i++) {
			for(j = 0; j < w * 4; j++) {
				unsigned char b = buffer[i * w * 4 + j];

				buffer[i * w * 4 + j]		= buffer[(h - i - 1) * w * 4 + j];
				buffer[(h - i - 1) * w * 4 + j] = b;
			}
		}

		px = MwLoadRaw(window, buffer, w, h);
		MwVaApply(window,
			  MwNiconPixmap, px,
			  NULL);
		MwLLDestroyPixmap(px);

		free(buffer);
	}

	MwOpenGLSwapBuffer(opengl);
}

void MWAPI resize(MwWidget handle, void* user, void* call) {
	int w = MwGetInteger(handle, MwNwidth);
	int h = MwGetInteger(handle, MwNheight);

	(void)user;
	(void)call;

	MwVaApply(opengl,
		  MwNwidth, w / 5 * 2,
		  MwNheight, h,
		  NULL);

	MwVaApply(ldate,
		  MwNx, w / 5 * 2,
		  MwNwidth, w - w / 5 * 2,
		  MwNheight, h / 2,
		  NULL);

	MwVaApply(ltime,
		  MwNx, w / 5 * 2,
		  MwNy, h / 2,
		  MwNwidth, w - w / 5 * 2,
		  MwNheight, h / 2,
		  NULL);

	glViewport(0, 0, w / 5 * 2, h);
}

int main() {
	MwLLColor bgcolor, fgcolor;

	MwLibraryInit();

	window = MwVaCreateWidget(MwWindowClass, "main", NULL, MwDEFAULT, MwDEFAULT, 250, 100,
				  MwNtitle, "clock",
				  NULL);

	bgcolor = MwParseColor(window, MwGetText(window, MwNbackground));
	MwColorGet(bgcolor, &br, &bg, &bb);

	fgcolor = MwParseColor(window, MwGetText(window, MwNforeground));
	MwColorGet(fgcolor, &fr, &fg, &fb);

	opengl = MwCreateWidget(MwOpenGLClass, "clock", window, 0, 0, 100, 100);

	ldate = MwVaCreateWidget(MwLabelClass, "date", window, 100, 0, 150, 50,
				 MwNbold, 1,
				 NULL);
	ltime = MwVaCreateWidget(MwLabelClass, "time", window, 100, 50, 150, 50,
				 MwNbold, 1,
				 NULL);

	MwAddUserHandler(window, MwNtickHandler, tick, NULL);
	MwAddUserHandler(window, MwNresizeHandler, resize, NULL);

	resize(window, NULL, NULL);
	tick(window, NULL, NULL);

	MwLoop(window);

	MwLLFreeColor(fgcolor);
	MwLLFreeColor(bgcolor);
}
