#ifndef DEBUG_S4G
#define DEBUG_S4G 0
#endif

#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <errno.h>

#include <windows.h>

#include "s4g.h"

struct s4g_display {
	int dummy;
};

struct s4g_window {
	s4g_display_t *S;
	HWND hwnd;
	HDC raw_hdc;
	HDC compat_hdc;

	HBITMAP bitmap;
	RECT window_rect;
	int width;
	int height;
	uint8_t *pixels;
};

struct cursor_window_enum_state {
	s4g_display_t *S;
	s4g_window_t *W;
	POINT cursor_pos;
};

struct title_window_enum_state {
	s4g_display_t *S;
	s4g_window_t *W;
	const char *reference_name;
	char result_name[1024];
};

void s4g_close(s4g_display_t *S)
{
	if(S == NULL) {
		return;
	}

	free(S);
}

s4g_display_t *s4g_open(const char *display_string)
{
	s4g_display_t *S = malloc(sizeof(s4g_display_t));
	if(S == NULL) {
		goto fail;
	}

	memset(S, 0, sizeof(*S));

	success:
	return S;

	fail:
	s4g_close(S);
	return NULL;
}

void s4g_close_window(s4g_window_t *W)
{
	if(W == NULL) {
		return;
	}

	if(W->bitmap) {
		DeleteObject(W->bitmap);
		W->bitmap = NULL;
	}

	if(W->raw_hdc) {
		ReleaseDC(W->hwnd, W->raw_hdc);
		W->raw_hdc = NULL;
	}

	if(W->compat_hdc) {
		DeleteDC(W->compat_hdc);
		W->bitmap = NULL;
	}

	if(W->hwnd) {
		W->hwnd = NULL;
	}
}

static s4g_window_t *open_window_from_hwnd(s4g_display_t *S, HWND hwnd)
{
	s4g_window_t *W = malloc(sizeof(s4g_window_t));
	if(W == NULL) {
		goto fail;
	}

	memset(W, 0, sizeof(*W));

	W->hwnd = hwnd;
	W->raw_hdc = GetDC(W->hwnd);
	assert(W->raw_hdc);
	W->compat_hdc = CreateCompatibleDC(W->compat_hdc);
	assert(W->compat_hdc);

	if(W->hwnd == NULL) {
		// Desktop
		W->width = GetSystemMetrics(SM_CXVIRTUALSCREEN);
		W->height = GetSystemMetrics(SM_CYVIRTUALSCREEN);

	} else {
		// Actual window
		BOOL got_rect = GetClientRect(W->hwnd, &W->window_rect);
		assert(got_rect != FALSE);
		W->width = W->window_rect.right - W->window_rect.left;
		W->height = W->window_rect.bottom - W->window_rect.top;
	}

	//printf("size: %d x %d\n", W->width, W->height);

	W->bitmap = CreateCompatibleBitmap(W->raw_hdc, W->width, W->height);
	assert(W->bitmap);

	HGDIOBJ replaced_obj = SelectObject(W->compat_hdc, W->bitmap);
	assert(replaced_obj != NULL);

	return W;

	fail:
	s4g_close_window(W);
	return NULL;
}

s4g_window_t *s4g_open_root_window(s4g_display_t *S)
{
	//return open_window_from_hwnd(S, GetDesktopWindow());
	return open_window_from_hwnd(S, NULL);
}

static BOOL CALLBACK find_window_at_cursor(HWND hwnd, LPARAM lparam)
{
	struct cursor_window_enum_state *wes = (struct cursor_window_enum_state *)(void *)lparam;

	RECT rect;
	BOOL did_get_rect = GetWindowRect(hwnd, &rect);
	assert(did_get_rect);
	// At this point, Windows got rect

	//printf("wnd %08X: %d < %d < %d | %d < %d < %d\n", hwnd, rect.left, wes->cursor_pos.x, rect.right, rect.top, wes->cursor_pos.x, rect.bottom);

	if(rect.left <= wes->cursor_pos.x
		&& wes->cursor_pos.x < rect.right
		&& rect.top <= wes->cursor_pos.y
		&& wes->cursor_pos.y < rect.bottom)
	{
		wes->W = open_window_from_hwnd(wes->S, hwnd);

		return FALSE;

	} else {
		BOOL no_windows_found = EnumChildWindows(
			NULL,
			find_window_at_cursor,
			(LPARAM)(void *)wes);

		return no_windows_found;
	}
}

s4g_window_t *s4g_open_window_at_cursor(s4g_display_t *S)
{
	struct cursor_window_enum_state base_wes;
	struct cursor_window_enum_state *wes = &base_wes;
	memset(wes, 0, sizeof(*wes));
	wes->S = S;
	BOOL did_get_cursor_pos = GetCursorPos(&wes->cursor_pos);
	assert(did_get_cursor_pos != 0);

	BOOL no_windows_found = EnumWindows(find_window_at_cursor, (LPARAM)(void *)wes);
	assert(no_windows_found == FALSE);

	assert(wes->W != NULL);
	return wes->W;
}

s4g_window_t *s4g_open_window_with_class_name(s4g_display_t *S, const char *name)
{
	assert(!"TODO");
	return NULL;
}

static BOOL CALLBACK find_window_by_title(HWND hwnd, LPARAM lparam)
{
	struct title_window_enum_state *wes = (struct title_window_enum_state *)(void *)lparam;

	int did_get_text = GetWindowText(hwnd, wes->result_name, sizeof(wes->result_name)-1);

	//printf("wnd %08X = \"%s\"\n", hwnd, wes->result_name);

	if(!strcmp(wes->result_name, wes->reference_name))
	{
		wes->W = open_window_from_hwnd(wes->S, hwnd);

		return FALSE;

	} else {
		BOOL no_windows_found = EnumChildWindows(
			NULL,
			find_window_by_title,
			(LPARAM)(void *)wes);

		return no_windows_found;
	}
}

s4g_window_t *s4g_open_window_with_title(s4g_display_t *S, const char *name)
{
	struct title_window_enum_state base_wes;
	struct title_window_enum_state *wes = &base_wes;
	memset(wes, 0, sizeof(*wes));
	wes->S = S;
	wes->reference_name = name;

	BOOL no_windows_found = EnumWindows(find_window_by_title, (LPARAM)(void *)wes);
	assert(no_windows_found == FALSE);

	assert(wes->W != NULL);
	return wes->W;
}

void *s4g_get_pointer_to_raw_display(s4g_display_t *S)
{
	assert(!"TODO");
}

void *s4g_get_pointer_to_raw_window(s4g_window_t *W)
{
	return &W->hwnd;
}

bool s4g_snap_from_window(s4g_window_t *W, void **data_return, int *width_return, int *height_return, int *bytes_per_line_return)
{
	BOOL did_blit = BitBlt(W->compat_hdc, 0, 0, W->width, W->height, W->raw_hdc, 0, 0, SRCCOPY|CAPTUREBLT);
	assert(did_blit);

	BITMAP concrete_bitmap;
	int did_get_bitmap = GetObject(W->bitmap, sizeof(BITMAP), &concrete_bitmap);
	assert(did_get_bitmap != 0);

	// And now for some Windows boilerplate garbage
	BITMAPINFOHEADER bi;
	bi.biSize = sizeof(BITMAPINFOHEADER);
	bi.biWidth = concrete_bitmap.bmWidth;
	bi.biHeight = concrete_bitmap.bmHeight;
	bi.biPlanes = 1;
	bi.biBitCount = 32;
	bi.biCompression = BI_RGB;
	bi.biSizeImage = 0;
	bi.biXPelsPerMeter = 0;
	bi.biYPelsPerMeter = 0;
	bi.biClrUsed = 0;
	bi.biClrImportant = 0;

	int bmp_pitch = ((W->width * bi.biBitCount + 31) >> 5) * 4;
	int bmp_size = bmp_pitch * W->height;

	if(W->pixels == NULL) {
		W->pixels = malloc(bmp_size);
	}

	int got_dibits = GetDIBits(
		W->raw_hdc,
		W->bitmap,
		0,
		W->height,
		W->pixels,
		(BITMAPINFO *)&bi,
		DIB_RGB_COLORS);
	assert(got_dibits != 0);

	*data_return = W->pixels;
	*width_return = W->width;
	*height_return = W->height;
	*bytes_per_line_return = bmp_pitch;
	return true;

	fail:
	return false;
}
