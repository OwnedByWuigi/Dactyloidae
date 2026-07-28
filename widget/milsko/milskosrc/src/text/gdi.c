#ifdef USE_GDI_TEXT
#include <Mw/Milsko.h>

static HANDLE(WINAPI* _AddFontMemResourceEx)(PVOID pFileView, DWORD cjSize, PVOID pvResrved, DWORD* pNumFonts) = NULL;
static BOOL(WINAPI* _RemoveFontMemResourceEx)(HANDLE h)							       = NULL;
static HANDLE WINAPI _AddFontMemResourceExPolyFill(PVOID pFileView, DWORD cjSize, PVOID pvResrved, DWORD* pNumFonts);
static BOOL WINAPI   _RemoveFontMemResourceExPolyFill(HANDLE h);

struct _MwFLFont {
	HANDLE handle;
	HFONT  font;
	HDC    dc;
};

static int GDI_MwDrawText(MwWidget handle, MwFLFont ttf, MwPoint* point, const char* text, MwLLColor color) {
	BITMAPINFOHEADER bmih;
	int		 tw;
	int		 th;
	HBITMAP		 hbm;
	RGBQUAD*	 bits = NULL;
	HDC		 hmdc;
	char*		 t = MwUTF8ToACP(text);
	unsigned char*	 px;
	int		 y, x;
	MwRect		 r;
	MwLLPixmap	 p;

	tw = MwTextWidth(handle, ttf, text);
	th = MwTextHeight(handle, ttf, text);

	if(handle->lowlevel->common.type == MwLLBackendGDI && MwGetInteger(handle, MwNmodernLook) == MwFALSE) {
		HFONT	 old_font   = SelectObject(handle->lowlevel->gdi.hDC, ttf->font);
		int	 old_bkmode = SetBkMode(handle->lowlevel->gdi.hDC, TRANSPARENT);
		COLORREF old_color  = SetTextColor(handle->lowlevel->gdi.hDC, color->gdi.color);

		TextOut(handle->lowlevel->gdi.hDC, point->x, point->y - th / 2, t, strlen(t));

		SetTextColor(handle->lowlevel->gdi.hDC, old_color);
		SetBkMode(handle->lowlevel->gdi.hDC, old_bkmode);
		SelectObject(handle->lowlevel->gdi.hDC, old_font);

		free(t);

		return 0;
	}

	px = malloc(tw * th * 4);

	bmih.biSize	     = sizeof(bmih);
	bmih.biWidth	     = tw;
	bmih.biHeight	     = -(LONG)th;
	bmih.biPlanes	     = 1;
	bmih.biBitCount	     = 32;
	bmih.biCompression   = BI_RGB;
	bmih.biSizeImage     = 0;
	bmih.biXPelsPerMeter = 0;
	bmih.biYPelsPerMeter = 0;
	bmih.biClrUsed	     = 0;
	bmih.biClrImportant  = 0;

	hbm = CreateDIBSection(ttf->dc, (BITMAPINFO*)&bmih, DIB_RGB_COLORS, (void**)&bits, NULL, 0);

	hmdc = CreateCompatibleDC(ttf->dc);
	SelectObject(hmdc, hbm);
	SelectObject(hmdc, ttf->font);

	TextOut(hmdc, 0, 0, t, strlen(t));

	DeleteDC(hmdc);

	for(y = 0; y < th; y++) {
		for(x = 0; x < tw; x++) {
			unsigned char* opx = &px[(y * tw + x) * 4];

			opx[0] = color->common.red;
			opx[1] = color->common.green;
			opx[2] = color->common.blue;
			opx[3] = 255 - bits[y * tw + x].rgbRed;
		}
	}

	p	 = MwLoadRaw(handle, px, tw, th);
	r.x	 = point->x;
	r.y	 = point->y - th / 2;
	r.width	 = tw;
	r.height = th;

	MwLLDrawPixmap(handle->lowlevel, &r, p);
	MwLLDestroyPixmap(p);
	free(px);

	DeleteObject(hbm);
	free(t);

	return 0;
}

static int GDI_MwTextWidth(MwFLFont ttf, const char* text) {
	char* t = MwUTF8ToACP(text);
	SIZE  sz;

	GetTextExtentPoint32(ttf->dc, t, strlen(t), &sz);

	free(t);

	return sz.cx;
}

static int GDI_MwTextHeight(MwFLFont ttf, int count) {
	TEXTMETRIC tm;

	GetTextMetrics(ttf->dc, &tm);

	return tm.tmHeight * count;
}

static void* GDI_MwFontLoad(unsigned char* data, unsigned int size, int px) {
	MwFLFont  ttf = malloc(sizeof(*ttf));
	DWORD	  c;
	MwTTFInfo info;

	if((info = MwTTFGetInfo(data, size)) == NULL) {
		free(ttf);
		return NULL;
	}

	ttf->dc = GetDC(NULL);

	ttf->handle = NULL;

	if(_AddFontMemResourceEx) {
		if((ttf->handle = _AddFontMemResourceEx(data, size, 0, &c)) == NULL) {
			ReleaseDC(NULL, ttf->dc);
			MwTTFFreeInfo(info);
			free(ttf);
			return NULL;
		}
	} /*else {
		if((ttf->handle = _AddFontMemResourceExPolyFill(data, size, 0, &c)) == NULL) {
			ReleaseDC(NULL, ttf->dc);
			MwTTFFreeInfo(info);
			free(ttf);
			return NULL;
		}
		}*/

	if((ttf->font = CreateFont(-px, 0, 0, 0, info->weight, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, ttf->handle ? info->family : "Sans Serif")) == NULL) {
		if(_RemoveFontMemResourceEx) {
			_RemoveFontMemResourceEx(ttf->handle);
		} else {
			_RemoveFontMemResourceExPolyFill(ttf->handle);
		}
		ReleaseDC(NULL, ttf->dc);
		MwTTFFreeInfo(info);
		free(ttf);
		return NULL;
	}

	MwTTFFreeInfo(info);

	SelectObject(ttf->dc, ttf->font);

	return ttf;
}

static void GDI_MwFontFree(void* handle) {
	MwFLFont ttf = handle;

	DeleteObject(ttf->font);
	if(_RemoveFontMemResourceEx) {
		_RemoveFontMemResourceEx(ttf->handle);
	} else {
		_RemoveFontMemResourceExPolyFill(ttf->handle);
	}
	ReleaseDC(NULL, ttf->dc);
	free(ttf);
}

static HANDLE WINAPI _AddFontMemResourceExPolyFill(PVOID pFileView, DWORD cjSize, PVOID pvResrved, DWORD* pNumFonts) {
	CHAR   temp_path_dir[MAX_PATH];
	CHAR   temp_path_name[MAX_PATH];
	HANDLE tempFile = NULL;
	int    hr;

	(void)pvResrved;

	hr = GetTempPathA(MAX_PATH, temp_path_dir);
	if(hr == 0) {
		printf("GetTempPathA error %08lX\n", hr);
		return hr;
	}
	hr = GetTempFileNameA(temp_path_dir, TEXT("MILSKO"), 0, temp_path_name);
	if(hr == 0) {
		printf("GetTempFileNameA error %08lX\n", hr);
		return hr;
	}

	printf("extracting mem font to %s\n", temp_path_name);

	tempFile = CreateFileA(temp_path_name,
			       GENERIC_WRITE,
			       0,
			       NULL,
			       CREATE_ALWAYS,
			       FILE_ATTRIBUTE_NORMAL | FILE_FLAG_DELETE_ON_CLOSE,
			       NULL);

	if(tempFile == INVALID_HANDLE_VALUE) {
		DWORD err = GetLastError();
		if(err == ERROR_ALREADY_EXISTS) {
			tempFile = CreateFileA(temp_path_name,
					       GENERIC_WRITE,
					       0,
					       NULL,
					       OPEN_ALWAYS,
					       FILE_ATTRIBUTE_NORMAL | FILE_FLAG_DELETE_ON_CLOSE,
					       NULL);
			printf("%d\n", tempFile);
			if(tempFile != INVALID_HANDLE_VALUE) {
				goto success;
			} else {
				err = GetLastError();
			}
		}
		printf("CreateFileA error %08lX\n", err);
		return hr;
	}

success:
	hr = WriteFile(tempFile, pFileView, cjSize, NULL, NULL);
	if(hr != 0) {
		printf("WriteFile error %08lX\n", hr);
		return 0;
	}

	*pNumFonts = AddFontResource(temp_path_name);

	return tempFile;
};
static BOOL WINAPI _RemoveFontMemResourceExPolyFill(HANDLE h) {
	if(h)
		CloseHandle(h);
	return TRUE;
};

int MwFL_GDISetup(void) {
	void* gdilib = LoadLibrary("gdi32.dll");
	if(!gdilib) {
		printf("gdi32.dll not found(?????)\n");
	} else {
		_AddFontMemResourceEx = (void*)GetProcAddress(gdilib, "AddFontMemResourceEx");
		if(!_AddFontMemResourceEx) {
			_AddFontMemResourceEx = _AddFontMemResourceExPolyFill;
		}
		_RemoveFontMemResourceEx = (void*)GetProcAddress(gdilib, "RemoveFontMemResourceEx");
		if(!_RemoveFontMemResourceEx) {
			_RemoveFontMemResourceEx = _RemoveFontMemResourceExPolyFill;
		}
	}

	MwFLDrawText   = GDI_MwDrawText;
	MwFLTextWidth  = GDI_MwTextWidth;
	MwFLTextHeight = GDI_MwTextHeight;
	MwFLFontLoad   = GDI_MwFontLoad;
	MwFLFontFree   = GDI_MwFontFree;
	return 0;
}

#endif
