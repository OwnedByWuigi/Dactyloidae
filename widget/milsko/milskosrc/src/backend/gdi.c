#include <Mw/Milsko.h>

typedef struct userdata {
	MwLL  ll;
	POINT min;
	POINT max;
	int   min_set;
	int   max_set;
} userdata_t;

static struct symtbl {
	void* lib_dwmapi;

	HRESULT(WINAPI* DwmSetWindowAttribute)(HWND hwnd, DWORD dwAttribute, LPCVOID pvAttribute, DWORD cbAttribute);
} wsymtbl;

static int is_plgblt_reliable = 0;

#define DwmSetWindowAttribute wsymtbl.DwmSetWindowAttribute

/* Dark theme detection for classic Windows: check if the user has set their theme to be dark. */
static void detect_darktheme_classictheme(MwLL handle) {
	/* Returns NULL on Windows 10+, use this to our advantage to check if the value is supported. */
	if(GetSysColorBrush(COLOR_MENU)) {
		DWORD col = GetSysColor(COLOR_MENU);
		int   t	  = (GetRValue(col) <= 75 && GetGValue(col) <= 75 && GetBValue(col) <= 75);
		MwLLDispatch(handle, dark_theme, &t);
	}
}

/* Dark theme detection for modern Windows: use the registry to check if dark mode is enabled. */
static void detect_darktheme(MwLL handle) {
	DWORD dw;
	DWORD sz = sizeof(dw);
	int   err, t;
	HKEY  hkey;
	DWORD type;

	err = RegOpenKeyEx(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize", 0, KEY_QUERY_VALUE, &hkey);
	if(err != ERROR_SUCCESS) {
		detect_darktheme_classictheme(handle);
		return;
	}

	err = RegQueryValueEx(hkey, "AppsUseLightTheme", NULL, &type, (PBYTE)&dw, &sz);
	RegCloseKey(hkey);
	if(err != ERROR_SUCCESS || type != REG_DWORD) {
		detect_darktheme_classictheme(handle);
		return;
	}

	t = dw ? 0 : 1;

	/* no don't do this here (nishi) */
#if 0
	if(t && DwmSetWindowAttribute != NULL) {
		LPARAM style = GetWindowLongPtr(handle->gdi.hWnd, GWL_STYLE);
		if(!(style & WS_CHILD)) MwLLSetDarkTheme(handle, 1);
	}
#endif

	MwLLDispatch(handle, dark_theme, &t);
}

static LRESULT CALLBACK wndproc(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp) {
	userdata_t* u = (userdata_t*)GetWindowLongPtr(hWnd, GWLP_USERDATA);

	if(u == NULL) return DefWindowProc(hWnd, msg, wp, lp);

	switch(msg) {
	case WM_PAINT:
	{
		PAINTSTRUCT ps;
		RECT	    rc;
		HBITMAP	    hbmp;
		HDC	    dc, hbdc;

		u->ll->gdi.force_render = 0;

		if(u->ll->common.copy_buffer) {
			GetClientRect(hWnd, &rc);

			dc   = GetDC(hWnd);
			hbmp = CreateCompatibleBitmap(dc, rc.right - rc.left, rc.bottom - rc.top);
			hbdc = CreateCompatibleDC(dc);
			SelectObject(hbdc, hbmp);
			ReleaseDC(hWnd, dc);

			u->ll->gdi.hDC = hbdc;
			MwLLDispatch(u->ll, draw, NULL);

			dc = BeginPaint(hWnd, &ps);
			StretchBlt(dc, 0, 0, rc.right - rc.left, rc.bottom - rc.top, hbdc, 0, 0, rc.right - rc.left, rc.bottom - rc.top, SRCCOPY);
			EndPaint(hWnd, &ps);

			DeleteDC(hbdc);
			DeleteObject(hbmp);
		} else {
			u->ll->gdi.hDC = BeginPaint(hWnd, &ps);
			MwLLDispatch(u->ll, draw, NULL);
			EndPaint(hWnd, &ps);
		}
		break;
	}
	case WM_LBUTTONDOWN:
	case WM_MBUTTONDOWN:
	case WM_RBUTTONDOWN:
	{
		MwMouse p;
		p.point.x = LOWORD(lp);
		p.point.y = HIWORD(lp);
		if(msg == WM_LBUTTONDOWN) {
			p.button = MwMOUSE_LEFT;
		} else if(msg == WM_MBUTTONDOWN) {
			p.button = MwMOUSE_MIDDLE;
		} else if(msg == WM_RBUTTONDOWN) {
			p.button = MwMOUSE_RIGHT;
		}

		SetCapture(hWnd);
		SetFocus(hWnd);
		MwLLDispatch(u->ll, down, &p);
		break;
	}
	case WM_LBUTTONUP:
	case WM_MBUTTONUP:
	case WM_RBUTTONUP:
	{
		MwMouse p;
		p.point.x = LOWORD(lp);
		p.point.y = HIWORD(lp);
		if(msg == WM_LBUTTONUP) {
			p.button = MwMOUSE_LEFT;
		} else if(msg == WM_MBUTTONUP) {
			p.button = MwMOUSE_MIDDLE;
		} else if(msg == WM_RBUTTONUP) {
			p.button = MwMOUSE_RIGHT;
		}

		SetCapture(NULL);
		MwLLDispatch(u->ll, up, &p);
		break;
	}
	case WM_MOUSEWHEEL:
	{
		int	d = GET_WHEEL_DELTA_WPARAM(wp);
		MwMouse p;
		p.point.x = LOWORD(lp);
		p.point.y = HIWORD(lp);

		if(d > 0) {
			p.button = MwMOUSE_WHEELUP;
		} else if(d < 0) {
			p.button = MwMOUSE_WHEELDOWN;
		}

		MwLLDispatch(u->ll, down, &p);
		MwLLDispatch(u->ll, up, &p);
		break;
	}
	case WM_MOUSEMOVE:
	{
		MwPoint p;
		p.x = LOWORD(lp);
		p.y = HIWORD(lp);

		if(u->ll->gdi.grabbed) {
			RECT rc;

			GetClientRect(hWnd, &rc);
			p.x -= (rc.right - rc.left) / 2;
			p.y -= (rc.bottom - rc.top) / 2;
		}

		MwLLDispatch(u->ll, move, &p);

		if(u->ll->gdi.grabbed && (p.x != 0 || p.y != 0)) {
			RECT  rc;
			POINT p;

			GetClientRect(hWnd, &rc);

			p.x = (rc.right - rc.left) / 2;
			p.y = (rc.bottom - rc.top) / 2;

			MapWindowPoints(hWnd, HWND_DESKTOP, &p, 1);

			SetCursorPos(p.x, p.y);
		}
		break;
	}
	case WM_SIZE:
	{
		MwLLDispatch(u->ll, resize, NULL);
		break;
	}
	case WM_ERASEBKGND:
	{
		return 1;
	}
	case WM_NCHITTEST:
	{
		LPARAM style = GetWindowLongPtr(hWnd, GWL_STYLE);
		if(style & WS_CHILD) return HTCLIENT;
		return DefWindowProc(hWnd, msg, wp, lp);
	}
	case WM_DESTROY:
	{
		PostQuitMessage(0);
		break;
	}
	case WM_CLOSE:
	{
		MwLLDispatch(u->ll, close, NULL);
		break;
	}
	case WM_CHAR:
	case WM_SYSCHAR:
	{
		int	  n    = wp;
		const int base = 'A' - 1;

		if(n != 0x1b && n <= 0x1f) {
			n = (n + base) | MwKEY_CONTROL_FLAG;
			if(!(GetKeyState(VK_LSHIFT) || GetKeyState(VK_RSHIFT))) n += 0x20;
		}
		if(HIBYTE(VkKeyScan(wp)) & 2) n |= MwKEY_CONTROL_FLAG;
		if(msg == WM_SYSCHAR) n |= MwKEY_ALT_FLAG;

		if((0x20 <= n && n <= 0x7f) || (n & MwKEY_FLAG)) MwLLDispatch(u->ll, key, &n);
		break;
	}
	case WM_SETFOCUS:
	{
		MwLLDispatch(u->ll, focus_in, NULL);
		break;
	}
	case WM_KILLFOCUS:
	{
		MwLLDispatch(u->ll, focus_out, NULL);
		break;
	}
	case WM_KEYDOWN:
	case WM_KEYUP:
	case WM_SYSKEYDOWN:
	case WM_SYSKEYUP:
	{
		int n = -1;

		if(wp == VK_MENU && msg == WM_KEYUP) return 0;
		switch(wp) {
		case VK_LEFT:
			n = MwKEY_LEFT;
			break;
		case VK_RIGHT:
			n = MwKEY_RIGHT;
			break;
		case VK_UP:
			n = MwKEY_UP;
			break;
		case VK_DOWN:
			n = MwKEY_DOWN;
			break;
		case VK_RETURN:
			n = MwKEY_ENTER;
			break;
		case VK_BACK:
			n = MwKEY_BACKSPACE;
			break;
		case VK_ESCAPE:
			n = MwKEY_ESCAPE;
			break;
		case VK_LSHIFT:
			n = MwKEY_LEFTSHIFT;
			break;
		case VK_RSHIFT:
			n = MwKEY_RIGHTSHIFT;
			break;
		case VK_MENU:
			n = MwKEY_ALT;
			break;
		case VK_CONTROL:
			n = MwKEY_CONTROL;
			break;
		}

		if((msg == WM_SYSKEYDOWN || msg == WM_SYSKEYUP) && n != -1 && wp != VK_MENU) {
			n |= MwKEY_ALT_FLAG;
		}
		if(n != -1) {
			if(msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN) {
				MwLLDispatch(u->ll, key, &n);
			} else {
				MwLLDispatch(u->ll, key_released, &n);
			}
		}
		break;
	}
	case WM_GETMINMAXINFO:
	{
		if(u->min_set || u->max_set) {
			LPARAM	    style = GetWindowLongPtr(hWnd, GWL_STYLE);
			MINMAXINFO* mmi	  = (MINMAXINFO*)lp;
			if(u->min_set) {
				RECT rc;

				rc.left	  = 0;
				rc.top	  = 0;
				rc.right  = u->min.x;
				rc.bottom = u->min.y;
				AdjustWindowRect(&rc, style, FALSE);

				mmi->ptMinTrackSize.x = rc.right - rc.left;
				mmi->ptMinTrackSize.y = rc.bottom - rc.top;
			}
			if(u->max_set) {
				RECT rc;

				rc.left	  = 0;
				rc.top	  = 0;
				rc.right  = u->min.x;
				rc.bottom = u->min.y;
				AdjustWindowRect(&rc, style, FALSE);

				mmi->ptMaxTrackSize.x = rc.right - rc.left;
				mmi->ptMaxTrackSize.y = rc.bottom - rc.top;
			}
		} else {
			return DefWindowProc(hWnd, msg, wp, lp);
		}
		break;
	}
	case WM_SETCURSOR:
	{
		if(LOWORD(lp) != HTCLIENT) return DefWindowProc(hWnd, msg, wp, lp);
		if(u->ll->gdi.cursor != NULL) SetCursor(u->ll->gdi.cursor);
		break;
	}
	case WM_USER:
	{
		InvalidateRect(hWnd, NULL, FALSE);
		UpdateWindow(hWnd);
		break;
	}
	case WM_WININICHANGE:
	{
		char*  s     = (char*)lp;
		LPARAM style = GetWindowLongPtr(hWnd, GWL_STYLE);

		if(!(style & WS_CHILD)) {
			if(s != NULL && strcmp(s, "ImmersiveColorSet") == 0) detect_darktheme(u->ll);
		}
		break;
	}
	default:
	{
		return DefWindowProc(hWnd, msg, wp, lp);
	}
	}
	return 0;
}

static MwLL MwLLCreateImpl(MwLL parent, int x, int y, int width, int height) {
	MwLL	    r = malloc(sizeof(*r));
	userdata_t* u = malloc(sizeof(*u));
	WNDCLASSEX  wc;

	memset(&wc, 0, sizeof(wc));
	wc.cbSize	 = sizeof(WNDCLASSEX);
	wc.style	 = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc	 = wndproc;
	wc.cbClsExtra	 = 0;
	wc.cbWndExtra	 = 0;
	wc.hInstance	 = (HINSTANCE)GetModuleHandle(NULL);
	wc.lpszClassName = "milsko";
	wc.lpszMenuName	 = NULL;
	wc.hCursor	 = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = GetSysColorBrush(COLOR_MENU);
	wc.hIcon	 = LoadIcon(NULL, IDI_WINLOGO);
	wc.hIconSm	 = NULL;

	MwLLCreateCommon(r);

	RegisterClassEx(&wc);

	r->common.copy_buffer = 1;
	r->common.type	      = MwLLBackendGDI;

	r->gdi.get_clipboard = 0;
	if(parent == NULL) r->gdi.get_darktheme = 1;
	r->gdi.force_render = 0;
	r->gdi.grabbed	    = 0;
	r->gdi.hWnd	    = CreateWindow("milsko", "Milsko", parent == NULL ? (WS_OVERLAPPEDWINDOW) : (WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS), x == MwDEFAULT ? CW_USEDEFAULT : x, y == MwDEFAULT ? CW_USEDEFAULT : y, width, height, parent == NULL ? NULL : parent->gdi.hWnd, 0, wc.hInstance, NULL);
	r->gdi.hInstance    = wc.hInstance;
	r->gdi.cursor	    = NULL;
	r->gdi.icon	    = NULL;

	r->gdi.clip = NULL;

	u->ll	   = r;
	u->min_set = 0;
	u->max_set = 0;
	SetWindowLongPtr(r->gdi.hWnd, GWLP_USERDATA, (LPARAM)u);

	if(parent == NULL) {
		RECT rc;

		rc.left	  = 0;
		rc.top	  = 0;
		rc.right  = width;
		rc.bottom = height;
		AdjustWindowRect(&rc, GetWindowLongPtr(r->gdi.hWnd, GWL_STYLE), FALSE);
		SetWindowPos(r->gdi.hWnd, NULL, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, SWP_NOMOVE);

		ShowWindow(r->gdi.hWnd, SW_NORMAL);
		UpdateWindow(r->gdi.hWnd);

		InvalidateRect(r->gdi.hWnd, NULL, FALSE);
	}

	SetWindowPos(r->gdi.hWnd, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);

	return r;
}

static void MwLLDestroyImpl(MwLL handle) {
	MwLLDestroyCommon(handle);

	/* for safety */
	SetWindowLongPtr(handle->gdi.hWnd, GWLP_USERDATA, (LPARAM)NULL);
	DestroyWindow(handle->gdi.hWnd);

	if(handle->gdi.clip != NULL) DeleteObject(handle->gdi.clip);

	if(handle->gdi.cursor != NULL) DestroyCursor(handle->gdi.cursor);

	free(handle);
}

static void MwLLBeginDrawImpl(MwLL handle) {
	(void)handle;
}

static void MwLLEndDrawImpl(MwLL handle) {
	(void)handle;
}

static void MwLLPolygonImpl(MwLL handle, MwPoint* points, int points_count, MwLLColor color) {
	POINT* p   = malloc(sizeof(*p) * points_count);
	HPEN   pen = CreatePen(PS_NULL, 0, RGB(0, 0, 0));
	int    i;

	for(i = 0; i < points_count; i++) {
		p[i].x = points[i].x;
		p[i].y = points[i].y;
	}
	SelectObject(handle->gdi.hDC, color->gdi.brush);
	SelectObject(handle->gdi.hDC, pen);
	SetPolyFillMode(handle->gdi.hDC, ALTERNATE);
	Polygon(handle->gdi.hDC, p, points_count);

	DeleteObject(pen);

	free(p);
}

static void MwLLLineImpl(MwLL handle, MwPoint* points, MwLLColor color) {
	HPEN pen = CreatePen(PS_SOLID, 1, RGB(color->common.red, color->common.green, color->common.blue));
	RECT rc;

	rc.left	  = points[1].x;
	rc.top	  = points[1].y;
	rc.right  = rc.left + 1;
	rc.bottom = rc.top + 1;

	SelectObject(handle->gdi.hDC, pen);
	MoveToEx(handle->gdi.hDC, points[0].x, points[0].y, NULL);
	LineTo(handle->gdi.hDC, points[1].x, points[1].y);
	FillRect(handle->gdi.hDC, &rc, color->gdi.brush);

	DeleteObject(pen);
}

static MwLLColor MwLLAllocColorImpl(MwLL handle, int r, int g, int b) {
	MwLLColor c = malloc(sizeof(*c));

	c->gdi.brush = NULL;

	MwLLColorUpdate(handle, c, r, g, b);

	return c;
}

static void MwLLColorUpdateImpl(MwLL handle, MwLLColor c, int r, int g, int b) {
	HDC dc = GetDC(handle->gdi.hWnd);

	if(r > 255) r = 255;
	if(g > 255) g = 255;
	if(b > 255) b = 255;
	if(r < 0) r = 0;
	if(g < 0) g = 0;
	if(b < 0) b = 0;

	if(c->gdi.brush != NULL) DeleteObject(c->gdi.brush);
	c->gdi.color	= GetNearestColor(dc, RGB(r, g, b));
	c->gdi.brush	= CreateSolidBrush(c->gdi.color);
	c->common.red	= r;
	c->common.green = g;
	c->common.blue	= b;

	ReleaseDC(handle->gdi.hWnd, dc);
}

static void MwLLFreeColorImpl(MwLLColor color) {
	DeleteObject(color->gdi.brush);

	free(color);
}

static void MwLLGetXYWHImpl(MwLL handle, int* x, int* y, unsigned int* w, unsigned int* h) {
	RECT rc;

	GetClientRect(handle->gdi.hWnd, &rc);
	MapWindowPoints(handle->gdi.hWnd, GetParent(handle->gdi.hWnd), (LPPOINT)&rc, 2);

	*x = rc.left;
	*y = rc.top;
	*w = rc.right - rc.left;
	*h = rc.bottom - rc.top;
}

static void MwLLSetXYImpl(MwLL handle, int x, int y) {
	SetWindowPos(handle->gdi.hWnd, NULL, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
}

static void MwLLSetWHImpl(MwLL handle, int w, int h) {
	SetWindowPos(handle->gdi.hWnd, NULL, 0, 0, w, h, SWP_NOMOVE | SWP_NOZORDER);
	InvalidateRect(handle->gdi.hWnd, NULL, FALSE);
}

static void MwLLSetTitleImpl(MwLL handle, const char* title) {
	SetWindowText(handle->gdi.hWnd, title);
}

static int MwLLPendingImpl(MwLL handle) {
	MSG msg;

	(void)handle;

	if(handle->gdi.get_clipboard || handle->gdi.get_darktheme) return 1;
	return PeekMessage(&msg, handle->gdi.hWnd, 0, 0, PM_NOREMOVE) ? 1 : 0;
}

static void MwLLNextEventImpl(MwLL handle) {
	MSG msg;

	(void)handle;

	if(handle->gdi.get_clipboard) {
		HGLOBAL hg;
		if(OpenClipboard(handle->gdi.hWnd) != 0 && (hg = GetClipboardData(CF_TEXT)) != NULL) {
			char* txt = malloc(GlobalSize(hg));
			char* clp = GlobalLock(hg);

			strcpy(txt, clp);

			GlobalUnlock(hg);
			CloseClipboard();

			MwLLDispatch(handle, clipboard, txt);

			free(txt);
		}

		handle->gdi.get_clipboard = 0;
	}
	if(handle->gdi.get_darktheme) {
		detect_darktheme(handle);

		handle->gdi.get_darktheme = 0;
	}
	while(PeekMessage(&msg, handle->gdi.hWnd, 0, 0, PM_NOREMOVE)) {
		GetMessage(&msg, handle->gdi.hWnd, 0, 0);
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
}

static MwLLPixmap MwLLCreatePixmapImpl(MwLL handle, unsigned char* data, int width, int height) {
	MwLLPixmap	 r  = malloc(sizeof(*r));
	HDC		 dc = GetDC(handle->gdi.hWnd);
	BITMAPINFOHEADER bmih;

	r->common.raw = malloc(width * height * 4);
	memcpy(r->common.raw, data, 4 * width * height);

	r->common.width	 = width;
	r->common.height = height;

	bmih.biSize	     = sizeof(bmih);
	bmih.biWidth	     = width;
	bmih.biHeight	     = -(LONG)height;
	bmih.biPlanes	     = 1;
	bmih.biBitCount	     = 32;
	bmih.biCompression   = BI_RGB;
	bmih.biSizeImage     = 0;
	bmih.biXPelsPerMeter = 0;
	bmih.biYPelsPerMeter = 0;
	bmih.biClrUsed	     = 0;
	bmih.biClrImportant  = 0;

	r->gdi.hBitmap = CreateDIBSection(dc, (BITMAPINFO*)&bmih, DIB_RGB_COLORS, (void**)&r->gdi.quad, NULL, (DWORD)0);

	r->gdi.hMask  = NULL;
	r->gdi.hMask2 = NULL;

	ReleaseDC(handle->gdi.hWnd, dc);

	MwLLPixmapUpdate(r);

	return r;
}

static void MwLLPixmapUpdateImpl(MwLLPixmap r) {
	int   y, x;
	int   w = (r->common.width + (16 - (r->common.width % 16))) / 8;
	WORD* words;
	WORD* words2;

	if(16 * (r->common.width / 16) == r->common.width) w -= 2;

	words  = malloc(w * r->common.height);
	words2 = malloc(w * r->common.height);
	memset(words, 0, w * r->common.height);
	memset(words2, 0, w * r->common.height);
	for(y = 0; y < r->common.height; y++) {
		BYTE* l	 = (BYTE*)&words[y * (w / 2)];
		BYTE* l2 = (BYTE*)&words2[y * (w / 2)];
		for(x = 0; x < r->common.width; x++) {
			RGBQUAD*       q  = &r->gdi.quad[y * r->common.width + x];
			unsigned char* px = &r->common.raw[(y * r->common.width + x) * 4];

			q->rgbRed   = px[0];
			q->rgbGreen = px[1];
			q->rgbBlue  = px[2];

			if(px[3]) {
				l[x / 8] |= 1 << (7 - (x % 8));
			} else {
				l2[x / 8] |= 1 << (7 - (x % 8));
			}
		}
	}

	if(r->gdi.hMask != NULL) DeleteObject(r->gdi.hMask);
	if(r->gdi.hMask2 != NULL) DeleteObject(r->gdi.hMask2);

	r->gdi.hMask  = CreateBitmap(r->common.width, r->common.height, 1, 1, words);
	r->gdi.hMask2 = CreateBitmap(r->common.width, r->common.height, 1, 1, words2);

	free(words);
	free(words2);
}

static void MwLLDestroyPixmapImpl(MwLLPixmap pixmap) {
	free(pixmap->common.raw);
	DeleteObject(pixmap->gdi.hMask);
	DeleteObject(pixmap->gdi.hMask2);
	DeleteObject(pixmap->gdi.hBitmap);

	free(pixmap);
}

static void MwLLDrawPixmapImpl(MwLL handle, MwRect* rect, MwLLPixmap pixmap) {
	HDC   hmdc = CreateCompatibleDC(handle->gdi.hDC);
	POINT p[3];
	SetStretchBltMode(handle->gdi.hDC, HALFTONE);

	p[0].x = rect->x;
	p[0].y = rect->y;

	p[1].x = rect->x + rect->width;
	p[1].y = rect->y;

	p[2].x = rect->x;
	p[2].y = rect->y + rect->height;

	SelectObject(hmdc, pixmap->gdi.hBitmap);

	if(!is_plgblt_reliable || PlgBlt(handle->gdi.hDC, p, hmdc, 0, 0, pixmap->common.width, pixmap->common.height, pixmap->gdi.hMask, 0, 0) == 0) {
		SelectObject(hmdc, pixmap->gdi.hMask2);
		StretchBlt(handle->gdi.hDC, rect->x, rect->y, rect->width, rect->height, hmdc, 0, 0, pixmap->common.width, pixmap->common.height, SRCAND);

		SelectObject(hmdc, pixmap->gdi.hBitmap);
		StretchBlt(handle->gdi.hDC, rect->x, rect->y, rect->width, rect->height, hmdc, 0, 0, pixmap->common.width, pixmap->common.height, SRCPAINT);
	}

	DeleteDC(hmdc);
}

static void MwLLSetIconImpl(MwLL handle, MwLLPixmap pixmap) {
	ICONINFO ii;

	memset(&ii, 0, sizeof(ii));
	ii.fIcon    = TRUE;
	ii.xHotspot = 0;
	ii.yHotspot = 0;
	ii.hbmMask  = pixmap->gdi.hMask2;
	ii.hbmColor = pixmap->gdi.hBitmap;

	if(handle->gdi.icon != NULL) DestroyIcon(handle->gdi.icon);
	handle->gdi.icon = CreateIconIndirect(&ii);
	SendMessage(handle->gdi.hWnd, WM_SETICON, ICON_SMALL, (LPARAM)handle->gdi.icon);
	SendMessage(handle->gdi.hWnd, WM_SETICON, ICON_BIG, (LPARAM)handle->gdi.icon);
}

static void MwLLForceRenderImpl(MwLL handle) {
	if(!handle->gdi.force_render) {
		PostMessage(handle->gdi.hWnd, WM_USER, 0, 0);

		handle->gdi.force_render = 1;
	}
}

HCURSOR MwLLGDICreateCursor(MwCursor* image, MwCursor* mask) {
	HCURSOR cursor;
	BYTE*	dmask  = malloc((MwCursorDataHeight / 8) * MwCursorDataHeight);
	BYTE*	dimage = malloc((MwCursorDataHeight / 8) * MwCursorDataHeight);
	int	y, x, ys, xs;

	xs = -mask->x + image->x;
	ys = mask->height + mask->y;
	ys = image->height + image->y - ys;

	if(ys < 0) ys = -ys;

	memset(dmask, 0xff, (MwCursorDataHeight / 8) * MwCursorDataHeight);
	memset(dimage, 0, (MwCursorDataHeight / 8) * MwCursorDataHeight);

	for(y = 0; y < mask->height; y++) {
		BYTE*	     l = &dmask[y * (MwCursorDataHeight / 8)];
		unsigned int n = mask->data[y];
		for(x = mask->width - 1; x >= 0; x--) {
			l[x / 8] = l[x / 8] >> 1;
			if(!(n & 1)) {
				l[x / 8] |= 1 << 7;
			}

			n = n >> 1;
		}
	}

	for(y = 0; y < image->height; y++) {
		BYTE*	     l = &dimage[(y + ys) * (MwCursorDataHeight / 8)];
		unsigned int n = image->data[y];

		for(x = image->width - 1; x >= 0; x--) {
			if(n & 1) {
				l[(x + xs) / 8] |= (1 << (7 - ((x + xs) % 8)));
			}

			n = n >> 1;
		}
	}

	cursor = CreateCursor(GetModuleHandle(NULL), xs - image->x, ys + (MwCursorDataHeight + image->y), MwCursorDataHeight, MwCursorDataHeight, dmask, dimage);

	free(dimage);
	free(dmask);

	return cursor;
}

static void MwLLSetCursorImpl(MwLL handle, MwCursor* image, MwCursor* mask) {
	HCURSOR cursor = MwLLGDICreateCursor(image, mask);

	if(handle->gdi.cursor != NULL) DestroyCursor(handle->gdi.cursor);
	if(handle->gdi.icon != NULL) DestroyIcon(handle->gdi.icon);
	handle->gdi.cursor = cursor;
}

static void MwLLDetachImpl(MwLL handle, MwPoint* point) {
	RECT   rc, rc2;
	LPARAM lp  = GetWindowLongPtr(handle->gdi.hWnd, GWL_STYLE);
	LPARAM lp2 = GetWindowLongPtr(handle->gdi.hWnd, GWL_EXSTYLE);

	lp &= WS_VISIBLE;

	GetWindowRect(GetParent(handle->gdi.hWnd), &rc);

	GetClientRect(handle->gdi.hWnd, &rc2);

	if(lp2 & WS_EX_TOOLWINDOW) {
		SetWindowLongPtr(handle->gdi.hWnd, GWL_STYLE, (LPARAM)lp);
	} else {
		SetWindowLongPtr(handle->gdi.hWnd, GWL_STYLE, (LPARAM)WS_OVERLAPPEDWINDOW | lp);
	}
	SetParent(handle->gdi.hWnd, NULL);

	rc.left += point->x;
	rc.top += point->y;

	AdjustWindowRect(&rc2, GetWindowLongPtr(handle->gdi.hWnd, GWL_STYLE), FALSE);
	rc2.right -= rc2.left;
	rc2.bottom -= rc2.top;

	SetWindowPos(handle->gdi.hWnd, HWND_TOPMOST, rc.left, rc.top, rc2.right == 0 ? 1 : rc2.right, rc2.bottom == 0 ? 1 : rc2.bottom, SWP_FRAMECHANGED | SWP_NOACTIVATE);

	handle->gdi.get_darktheme = 1;
}

static void MwLLShowImpl(MwLL handle, int show) {
	ShowWindow(handle->gdi.hWnd, show ? SW_NORMAL : SW_HIDE);
	if(show) SetFocus(handle->gdi.hWnd);
}

static void MwLLMakePopupImpl(MwLL handle, MwLL parent) {
	(void)handle;
	(void)parent;
	/* TODO */
}

static void MwLLSetSizeHintsImpl(MwLL handle, int minx, int miny, int maxx, int maxy) {
	userdata_t* u = (userdata_t*)GetWindowLongPtr(handle->gdi.hWnd, GWLP_USERDATA);

	u->min_set = u->max_set = 1;
	u->min.x		= minx;
	u->min.y		= miny;
	u->max.x		= maxx;
	u->max.y		= maxy;
}

static void MwLLMakeBorderlessImpl(MwLL handle, int toggle) {
	LPARAM lp = GetWindowLongPtr(handle->gdi.hWnd, GWL_STYLE);

	if(toggle) {
		lp &= ~WS_CAPTION;
	} else {
		lp |= WS_CAPTION;
	}

	SetWindowLongPtr(handle->gdi.hWnd, GWL_STYLE, lp);

	SetWindowPos(handle->gdi.hWnd, NULL, 0, 0, 0, 0, SWP_FRAMECHANGED | SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER);
}

static void MwLLFocusImpl(MwLL handle) {
	SetFocus(handle->gdi.hWnd);
}

static void MwLLGrabPointerImpl(MwLL handle, int toggle) {
	if(toggle) {
		POINT p;
		RECT  rc;

		GetClientRect(handle->gdi.hWnd, &rc);

		rc.right -= rc.left;
		rc.bottom -= rc.top;

		p.x = (rc.right - rc.left) / 2;
		p.y = (rc.bottom - rc.top) / 2;

		MapWindowPoints(handle->gdi.hWnd, HWND_DESKTOP, &p, 1);

		handle->gdi.grabbed = 1;

		SetFocus(handle->gdi.hWnd);

		GetWindowRect(handle->gdi.hWnd, &rc);
		ClipCursor(&rc);

		SetCursorPos(p.x, p.y);
	} else {
		handle->gdi.grabbed = 0;
		ClipCursor(NULL);
	}
}

static void MwLLSetClipboardImpl(MwLL handle, const char* text, int clipboard_type) {
	HGLOBAL hg;
	(void)clipboard_type;
	if(OpenClipboard(handle->gdi.hWnd) != 0) {
		char* lock;

		EmptyClipboard();
		hg = GlobalAlloc(GHND | GMEM_SHARE, strlen(text) + 1);

		lock = GlobalLock(hg);
		strcpy(lock, text);
		GlobalUnlock(hg);

		SetClipboardData(CF_TEXT, hg);

		CloseClipboard();
	}
}

static void MwLLGetClipboardImpl(MwLL handle, int clipboard_type) {
	(void)clipboard_type;
	handle->gdi.get_clipboard = 1; /* nishi: we do this to make clipboard api work similar to other backends */
}

static void MwLLMakeToolWindowImpl(MwLL handle) {
	LPARAM lp = GetWindowLongPtr(handle->gdi.hWnd, GWL_STYLE) & (WS_VISIBLE | WS_CHILD);
	RECT   rc;
	int    w, h;

	SetWindowLongPtr(handle->gdi.hWnd, GWL_STYLE, (LPARAM)lp);
	SetWindowLongPtr(handle->gdi.hWnd, GWL_EXSTYLE, (LPARAM)WS_EX_TOOLWINDOW);

	GetClientRect(handle->gdi.hWnd, &rc);

	w = rc.right - rc.left;
	h = rc.bottom - rc.top;

	SetWindowPos(handle->gdi.hWnd, NULL, 0, 0, w, h, SWP_FRAMECHANGED | SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOZORDER);
}

static void MwLLGetCursorCoordImpl(MwLL handle, MwPoint* point) {
	POINT p;

	(void)handle;

	GetCursorPos(&p);

	point->x = p.x;
	point->y = p.y;
}

static void MwLLGetScreenSizeImpl(MwLL handle, MwRect* rect) {
	RECT rc;

	(void)handle;

	GetClientRect(GetDesktopWindow(), &rc);

	rect->x = rect->y = 0;
	rect->width	  = rc.right - rc.left;
	rect->height	  = rc.bottom - rc.top;
}

static void MwLLBeginStateChangeImpl(MwLL handle) {
	(void)handle;
}

static void MwLLEndStateChangeImpl(MwLL handle) {
	(void)handle;
}

static void MwLLSetDarkThemeImpl(MwLL handle, int toggle) {
	BOOL v = toggle ? TRUE : FALSE;

	if(DwmSetWindowAttribute)
		DwmSetWindowAttribute(handle->gdi.hWnd, 20, &v, sizeof(v));
}

static MwBool MwLLDoModernImpl(MwLL handle) {
	OSVERSIONINFO osvi;

	(void)handle;

	osvi.dwOSVersionInfoSize = sizeof(osvi);

	GetVersionEx(&osvi);

	if(osvi.dwMajorVersion < 5 || (osvi.dwMajorVersion == 5 && osvi.dwMinorVersion < 1)) {
		return MwFALSE;
	}

	return MwTRUE;
}

static void MwLLRaiseImpl(MwLL handle) {
	SetWindowPos(handle->gdi.hWnd, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
}

static void MwLLClipImpl(MwLL handle, MwRect* rect) {
	if(rect == NULL) {
		SelectClipRgn(handle->gdi.hDC, NULL);

		if(handle->gdi.clip != NULL) {
			DeleteObject(handle->gdi.clip);
			handle->gdi.clip = NULL;
		}
	} else {
		HRGN clip = CreateRectRgn(rect->x, rect->y, rect->x + rect->width, rect->y + rect->height);

		SelectClipRgn(handle->gdi.hDC, clip);

		if(handle->gdi.clip != NULL) DeleteObject(handle->gdi.clip);

		handle->gdi.clip = clip;
	}
}

static int MwLLGDICallInitImpl(void) {
	void* ntdll;

	memset(&wsymtbl, 0, sizeof(wsymtbl));

	wsymtbl.lib_dwmapi = MwDynamicOpen("dwmapi.dll");

	if(wsymtbl.lib_dwmapi != NULL) DwmSetWindowAttribute = MwDynamicSymbol(wsymtbl.lib_dwmapi, "DwmSetWindowAttribute");

	if((ntdll = GetModuleHandle("ntdll.dll")) != NULL && GetProcAddress(ntdll, "wine_get_version") != NULL) {
		is_plgblt_reliable = 0;
	} else {
		is_plgblt_reliable = 1;
	}

	/* TODO: check properly */
	return 0;
}

#include "call.c"
CALL(GDI);
