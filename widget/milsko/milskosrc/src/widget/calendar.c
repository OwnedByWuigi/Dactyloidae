#include <Mw/Milsko.h>

static unsigned int font[] = {
    0x00000000, 0x00401084, 0x0000014a, 0x00afabea, 0x01fa7cbf, 0x01111111,
    0x0126c8a2, 0x00000084, 0x00821088, 0x00221082, 0x015711d5, 0x00427c84,
    0x00220000, 0x00007c00, 0x00400000, 0x00111110, 0x00e9d72e, 0x00421084,
    0x01f2222e, 0x00e8b22e, 0x008fa54c, 0x01f87c3f, 0x00e8bc2e, 0x0042221f,
    0x00e8ba2e, 0x00e87a2e, 0x00020080, 0x00220080, 0x00820888, 0x000f83e0,
    0x00222082, 0x0042222e, 0x00ead72e, 0x011fc544, 0x00f8be2f, 0x00e8862e,
    0x00f8c62f, 0x01f0fc3f, 0x0010bc3f, 0x00e8e42e, 0x0118fe31, 0x00e2108e,
    0x00e8c210, 0x01149d31, 0x01f08421, 0x0118d771, 0x011cd671, 0x00e8c62e,
    0x0010be2f, 0x01ecc62e, 0x0114be2f, 0x00f8383e, 0x0042109f, 0x00e8c631,
    0x00454631, 0x00aad6b5, 0x01151151, 0x00421151, 0x01f1111f, 0x00e1084e,
    0x01041041, 0x00e4210e, 0x00000144, 0x01f00000, 0x00000082, 0x0164a4c0,
    0x00749c20, 0x00e085c0, 0x00e4b908, 0x0060bd26, 0x0042388c, 0x00c8724c,
    0x00a51842, 0x00420080, 0x00621004, 0x00a32842, 0x00421086, 0x015ab800,
    0x00949800, 0x0064a4c0, 0x0013a4c0, 0x008724c0, 0x00108da0, 0x0064104c,
    0x00c23880, 0x0164a520, 0x00452800, 0x00aad400, 0x00a22800, 0x00111140,
    0x00e221c0, 0x00c2088c, 0x00421084, 0x00622086, 0x000022a2};

static int wcreate(MwWidget handle) {
	MwSetDefault(handle);

	MwSetInteger(handle, MwNyear, 1970);
	MwSetInteger(handle, MwNmonth, 0);
	MwSetInteger(handle, MwNdate, 1);
	MwSetInteger(handle, MwNday, 3);
	MwSetInteger(handle, MwNscale, 1);
	return 0;
}

static MwLLPixmap blit(MwWidget handle, const char* str, MwLLColor bg, MwLLColor fg, int scale) {
	int	       i;
	unsigned char* buf;
	MwLLPixmap     px;
	int	       w = (5 * strlen(str) + (strlen(str) - 1)) * scale;
	int	       h = 5 * scale;
	int	       y, x, s;
	int	       r, g, b;

	MwColorGet(bg, &r, &g, &b);

	buf = malloc(w * h * 4);
	for(i = 0; i < w * h; i++) {
		buf[4 * i + 0] = r;
		buf[4 * i + 1] = g;
		buf[4 * i + 2] = b;
		buf[4 * i + 3] = 255;
	}

	MwColorGet(fg, &r, &g, &b);

	for(y = 0; y < h; y++) {
		for(i = 0; str[i] != 0; i++) {
			unsigned int n = font[str[i] - 0x20];

			n = n >> (5 * (y / scale));

			for(x = 0; x < 5; x++) {
				for(s = 0; s < scale; s++) {
					if(n & 1) {
						unsigned char* opx = &buf[(y * w + i * (6 * scale) + x * scale + s) * 4];

						opx[0] = r;
						opx[1] = g;
						opx[2] = b;
						opx[3] = 255;
					}
				}

				n = n >> 1;
			}
		}
	}

	px = MwLoadRaw(handle, buf, w, h);

	free(buf);

	return px;
}

static void draw(MwWidget handle) {
	MwLLColor   c	 = MwParseColor(handle, MwGetText(handle, MwNbackground));
	MwLLColor   cf	 = MwParseColor(handle, MwGetText(handle, MwNforeground));
	MwLLColor   cb	 = MwParseColor(handle, "#fff");
	MwLLColor   cb_g = MwParseColor(handle, "#666");
	MwLLColor   ct	 = MwParseColor(handle, "#000");
	MwRect	    r, r2, r3;
	MwPoint	    p[6];
	int	    h  = 32 * MwGetInteger(handle, MwNscale);
	int	    pw = h / 16;	  /* parallelogram width */
	int	    ph = pw * 3;	  /* parallelogram height */
	int	    gh = h / 32;	  /* gray area height */
	int	    sh = gh * 5 + gh * 2; /* shift height for year */
	char	    buf[5];
	const char* months[] = {
	    "JAN",
	    "FEB",
	    "MAR",
	    "APR",
	    "MAY",
	    "JUN",
	    "JUL",
	    "AUG",
	    "SEP",
	    "OCT",
	    "NOV",
	    "DEC"};
	const char* days[] = {
	    "SUN",
	    "MON",
	    "TUE",
	    "WED",
	    "THU",
	    "FRI",
	    "SAT"};
	MwLLPixmap px;

	r.x	 = 0;
	r.y	 = 0;
	r.width	 = MwGetInteger(handle, MwNwidth);
	r.height = MwGetInteger(handle, MwNheight);
	MwDrawRect(handle, &r, c);

	p[0].x = (r.width - h) / 2;
	p[0].y = (r.height - h - sh) / 2;
	p[1].x = p[0].x;
	p[1].y = p[0].y + h + gh;
	p[2].x = p[1].x + gh * 3;
	p[2].y = p[1].y + gh * 3;
	p[3].x = p[1].x + h + gh * 7;
	p[3].y = p[2].y;
	p[4].x = p[3].x;
	p[4].y = p[0].y + gh * 7;
	p[5].x = p[0].x + h;
	p[5].y = p[0].y;
	MwLLPolygon(handle->lowlevel, p, 6, cb_g);

	p[0].x = (r.width - h) / 2;
	p[0].y = (r.height - h - sh) / 2;
	p[1].x = p[0].x;
	p[1].y = p[0].y + h + gh;
	p[2].x = p[1].x + gh * 2;
	p[2].y = p[1].y + gh * 2;
	p[3].x = p[1].x + h + gh * 3;
	p[3].y = p[2].y;
	p[4].x = p[3].x;
	p[4].y = p[0].y + gh * 3;
	p[5].x = p[0].x + h;
	p[5].y = p[0].y;
	MwLLPolygon(handle->lowlevel, p, 6, ct);

	r2.width  = h;
	r2.height = h - ph - gh;
	r2.x	  = (r.width - r2.width) / 2;
	r2.y	  = (r.height - r2.height - ph - gh - sh) / 2;
	MwDrawRect(handle, &r2, cb);

	r3.x	  = r2.x;
	r3.y	  = r2.y + r2.height;
	r3.width  = r2.width;
	r3.height = ph + gh;
	MwDrawRect(handle, &r3, cb_g);

	r3.y += r3.height;
	r3.height = gh;
	MwDrawRect(handle, &r3, cb);

	p[0].x = r2.x;
	p[0].y = r2.y + r2.height;
	p[1].x = r2.x - pw;
	p[1].y = r2.y + r2.height + ph;
	p[2].x = r2.x + r2.width - pw;
	p[2].y = r2.y + r2.height + ph;
	p[3].x = r2.x + r2.width;
	p[3].y = r2.y + r2.height;
	MwLLPolygon(handle->lowlevel, p, 4, cb);

	p[0].x = r.width / 2;
	p[0].y = (r.height - sh) / 2;
	MwStringPrintIntoBuffer(buf, 3, "%d", MwGetInteger(handle, MwNdate));
	MwDrawText(handle, MwFLBuildFont(MwFLFlagBold), p, buf, MwALIGNMENT_CENTER, ct);

	r3 = r2;

	px	  = blit(handle, days[MwGetInteger(handle, MwNday)], cb, ct, gh);
	r2	  = r3;
	r2.width  = px->common.width;
	r2.height = px->common.height;
	r2.x	  = (r.width - r2.width) / 2;
	r2.y	  = r2.y + gh * 2;
	MwLLDrawPixmap(handle->lowlevel, &r2, px);
	MwLLDestroyPixmap(px);

	px	  = blit(handle, months[MwGetInteger(handle, MwNmonth)], cb, ct, gh);
	r2	  = r3;
	r2.width  = px->common.width;
	r2.height = px->common.height;
	r2.x	  = (r.width - r2.width) / 2;
	r2.y	  = r2.y + h - gh * 2 - r2.height;
	MwLLDrawPixmap(handle->lowlevel, &r2, px);
	MwLLDestroyPixmap(px);

	MwStringPrintIntoBuffer(buf, 5, "%d", MwGetInteger(handle, MwNyear));

	px	  = blit(handle, buf, c, cf, gh);
	r2	  = r3;
	r2.width  = px->common.width;
	r2.height = px->common.height;
	r2.x	  = (r.width - r2.width) / 2;
	r2.y	  = r2.y + h + gh * 6;
	MwLLDrawPixmap(handle->lowlevel, &r2, px);
	MwLLDestroyPixmap(px);

	MwLLFreeColor(cf);
	MwLLFreeColor(ct);
	MwLLFreeColor(cb_g);
	MwLLFreeColor(cb);
	MwLLFreeColor(c);
}

static void prop_change(MwWidget handle, const char* key) {
	if(strcmp(key, MwNmonth) == 0 || strcmp(key, MwNdate) == 0 || strcmp(key, MwNday) == 0) MwForceRender(handle);
}

MwClassRec MwCalendarClassRec = {
    wcreate,	 /* create */
    NULL,	 /* destroy */
    draw,	 /* draw */
    NULL,	 /* click */
    NULL,	 /* parent_resize */
    prop_change, /* prop_change */
    NULL,	 /* mouse_move */
    NULL,	 /* mouse_up */
    NULL,	 /* mouse_down */
    NULL,	 /* key */
    NULL,	 /* execute */
    NULL,	 /* tick */
    NULL,	 /* resize */
    NULL,	 /* children_update */
    NULL,	 /* children_prop_change */
    NULL,	 /* clipboard */
    NULL,	 /* props_change */
    NULL,
    NULL,
    NULL};
MwClass MwCalendarClass = &MwCalendarClassRec;
