#ifndef DEBUG_S4G
#define DEBUG_S4G 0
#endif
#ifndef S4G_SUPPORT_XSHM
#define S4G_SUPPORT_XSHM 1
#endif

#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <errno.h>

#include <unistd.h>
#include <sys/time.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#if S4G_SUPPORT_XSHM
#include <sys/ipc.h>
#include <sys/shm.h>
#endif
#include <X11/extensions/XShm.h>

#include "s4g.h"

struct s4g_display {
	Display *dpy;

	// XShm
	bool has_xshm;
};

struct s4g_window {
	s4g_display_t *S;
	Window w;
	XWindowAttributes attrs;
	XImage *ximage;

	// XShm
	XShmSegmentInfo shminfo;
};

void s4g_close(s4g_display_t *S)
{
	if(S == NULL) {
		return;
	}

	if(S->dpy != NULL) {
		XCloseDisplay(S->dpy);
		S->dpy = NULL;
	}

	free(S);
}

s4g_display_t *s4g_open(const char *display_string)
{
	if(display_string == NULL) {
		display_string = getenv("DISPLAY");
	}

	s4g_display_t *S = malloc(sizeof(s4g_display_t));
	if(S == NULL) {
		goto fail;
	}

	memset(S, 0, sizeof(*S));

	S->dpy = XOpenDisplay(display_string);
	if(S->dpy == NULL) {
		fprintf(stderr, "FAILED TO GET DISPLAY \"%s\"\n", display_string);
		goto fail;
	}

#if S4G_SUPPORT_XSHM
	S->has_xshm = (XShmQueryExtension(S->dpy) == True);

	if(S->has_xshm) {
		int xshm_pixmap_fmt = XShmPixmapFormat(S->dpy);
		if(xshm_pixmap_fmt != ZPixmap) {
			fprintf(stderr, "ERROR: Pixmap format is not ZPixmap!\nTIP: Don't use X11 on an Amiga.\n");
			goto fail;
		}
	}
#else
	S->has_xshm = false;
#endif

	success:
	return S;

	fail:
	s4g_close(S);
	return NULL;
}

bool s4g_really_update_window_image(s4g_window_t *W)
{
	s4g_display_t *S = W->S;

#if DEBUG_S4G
	printf("window attrs: %d, %d -> %d x %d - %d bpp\n",
		W->attrs.x,
		W->attrs.y,
		W->attrs.width,
		W->attrs.height,
		W->attrs.depth);
#endif

	if(W->ximage != NULL) {
		XDestroyImage(W->ximage);
		W->ximage = NULL;
	}

#if S4G_SUPPORT_XSHM
	if(S->has_xshm) {
		W->shminfo.shmid = -1;

		W->ximage = XShmCreateImage(
			S->dpy,
			W->attrs.visual,
			W->attrs.depth,
			ZPixmap,
			NULL,
			&W->shminfo,
			W->attrs.width,
			W->attrs.height);

		if(W->ximage == NULL) {
			goto fail;
		}

		size_t bsize = W->ximage->width * W->ximage->bytes_per_line;
		W->shminfo.shmid = shmget(
			IPC_PRIVATE,
			bsize,
			IPC_CREAT|0777);

#if DEBUG_S4G
		printf("shmid = 0x%x / size = 0x%x\n", W->shminfo.shmid, bsize);
#endif

		if(W->shminfo.shmid == -1) {
			perror("shmget");
			goto fail;
		}

		W->shminfo.readOnly = False;
		W->shminfo.shmaddr = W->ximage->data = shmat(
			W->shminfo.shmid,
			NULL,
			0);

#if DEBUG_S4G
		printf("shm addr = %p\n", W->ximage->data);
#endif

		if(W->shminfo.shmaddr == (void *)-1) {
			W->shminfo.shmaddr = NULL;
		}

		if(W->shminfo.shmaddr == NULL) {
			perror("shmat");
			goto fail;
		}

		Bool did_attach = XShmAttach(S->dpy, &W->shminfo);

		if(!did_attach) {
			goto fail;
		}
	}
#endif

	success:
	return true;

	fail:
	return false;
}

bool s4g_update_window_image(s4g_window_t *W)
{
	s4g_display_t *S = W->S;
	XWindowAttributes old_attrs = W->attrs;

	// Update attributes
	Bool got_attrs = XGetWindowAttributes(S->dpy, W->w, &W->attrs);
	if(got_attrs != True) {
		goto fail;
	}

#if 1
	if(W->attrs.width != old_attrs.width
		|| W->attrs.height != old_attrs.height
	) {
		if(!s4g_really_update_window_image(W)) {
			goto fail;
		}
	}
#endif

	// Take a screenshot!
#if S4G_SUPPORT_XSHM
	if(S->has_xshm) {
		Bool got_image = XShmGetImage(
			S->dpy,
			W->w,
			W->ximage,
			0, 0,
			0xFFFFFFFF);

		if(got_image != True) {
			goto fail;
		}

		goto success;
	}
#endif
	{
		if(W->ximage != NULL) {
			XDestroyImage(W->ximage);
			W->ximage = NULL;
		}

		W->ximage = XGetImage(
			S->dpy,
			W->w,
			0, 0,
			W->attrs.width,
			W->attrs.height,
			0xFFFFFFFF,
			ZPixmap);
		
		if(W->ximage == NULL) {
			goto fail;
		}
	}

	success:
	return true;

	fail:
	return false;
}

void s4g_close_window(s4g_window_t *W)
{
	if(W == NULL) {
		return;
	}

#if S4G_SUPPORT_XSHM
	if(W->shminfo.shmaddr != NULL) {
		XShmDetach(W->S->dpy, &W->shminfo);
		shmdt(W->shminfo.shmaddr);
		W->shminfo.shmaddr = NULL;
		if(W->ximage != NULL) {
			W->ximage->data = NULL;
		}
	}
#endif

	if(W->ximage != NULL) {
#if S4G_SUPPORT_XSHM
		if(W->shminfo.shmaddr != NULL) {
			XShmDetach(W->S->dpy, &W->shminfo);
			shmdt(W->shminfo.shmaddr);
			W->shminfo.shmaddr = NULL;
			W->ximage->data = NULL;
		}
#endif

		XDestroyImage(W->ximage);
		W->ximage = NULL;
	}

#if S4G_SUPPORT_XSHM
	if(W->shminfo.shmid != -1 && W->shminfo.shmid != 0) {
		shmctl(W->shminfo.shmid, IPC_RMID, 0);
		W->shminfo.shmid = -1;
	}
#endif

	if(W->w != 0) {
		W->w = 0;
	}

	if(W->S != NULL) {
		W->S = NULL;
	}

	free(W);
}

static char *s4g_get_x11_window_string_property(s4g_display_t *S, Window w, const char *name, const char *type)
{
	Atom string_atom = XInternAtom(S->dpy, type, False);
	Atom wm_prop_property_atom = XInternAtom(S->dpy, name, False);
	Atom wm_prop_type = 0;
	int wm_prop_format = 0;
	long wm_prop_nitems = 0;
	long wm_prop_bytes = 0;
	unsigned char *wm_prop = NULL;
	int got_wm_prop = XGetWindowProperty(
		S->dpy,
		w,
		wm_prop_property_atom,
		0, 1024, False,
		string_atom,
		&wm_prop_type,
		&wm_prop_format,
		&wm_prop_nitems,
		&wm_prop_bytes,
		&wm_prop);

	if(wm_prop_format == 0) {
		XFree(wm_prop);
		return NULL;
	} else {
		return wm_prop;
	}
}

static s4g_window_t *s4g_open_window_from_x11(s4g_display_t *S, Window w, bool punch_until_class_exists)
{
	if(punch_until_class_exists)
	{
		// Get window class

		char *wm_class = s4g_get_x11_window_string_property(S, w, "WM_CLASS", "STRING");

		if(wm_class == NULL) {
			Window root, child;
			int root_x, root_y, win_x, win_y;
			unsigned int mask;
			Bool got_pointer = XQueryPointer(
				S->dpy,
				w,
				&root,
				&child,
				&root_x, &root_y,
				&win_x, &win_y,
				&mask);

			if(got_pointer != True) {
				printf("FAILED TO GET POINTER\n");
				goto fail;
			}

			return s4g_open_window_from_x11(S, child, punch_until_class_exists);

		} else {
			XFree(wm_class);
		}
	}

	s4g_window_t *W = malloc(sizeof(s4g_window_t));
	if(W == NULL) {
		goto fail;
	}

	memset(W, 0, sizeof(*W));
	W->S = S;
	W->w = w;

	// Get attributes
	Bool got_attrs = XGetWindowAttributes(S->dpy, W->w, &W->attrs);
	if(got_attrs != True) {
		goto fail;
	}

	if(!s4g_really_update_window_image(W)) {
		goto fail;
	}

	success:
	return W;

	fail:
	s4g_close_window(W);

}

s4g_window_t *s4g_open_root_window(s4g_display_t *S)
{
	return s4g_open_window_from_x11(S, XDefaultRootWindow(S->dpy), false);
}

s4g_window_t *s4g_open_window_at_cursor(s4g_display_t *S)
{
	Window root;
	Window child;
	int root_x, root_y;
	int win_x, win_y;
	unsigned int mask;

	Bool got_pointer = XQueryPointer(
		S->dpy,
		XDefaultRootWindow(S->dpy),
		&root,
		&child,
		&root_x, &root_y,
		&win_x, &win_y,
		&mask);

	if(got_pointer != True) {
		printf("FAILED TO GET POINTER\n");
		goto fail;
	}

#if DEBUG_S4G
	printf("root = %x (%d, %d) / child = %x (%d, %d) / mask = %08X\n",
		root, root_x, root_y,
		child, win_x, win_y,
		mask);
#endif

	success:
	return s4g_open_window_from_x11(S, child, true);

	fail:
	return NULL;
}

static s4g_window_t *s4g_find_first_window_with_given_property(s4g_display_t *S, Window w, const char *prop, const char *type, const char *name)
{
	Window root, parent;
	Window *children = NULL;
	unsigned int nchildren = 0;

	Status did_tree = XQueryTree(S->dpy, w, &root, &parent, &children, &nchildren);
#if DEBUG_S4G
	printf("tree %d %p %d\n", did_tree, children, nchildren);
#endif

	if(did_tree == 0) {
		goto fail;
	}
	for(int i = 0; i < nchildren; i++) {
		Window child = children[i];
		char *w_name = s4g_get_x11_window_string_property(S, child, prop, type);
		if(w_name != NULL) {
#if DEBUG_S4G
			printf("%s: 0x%x %d - \"%s\"\n", type, child, i, w_name);
#endif

			if(!strcmp(w_name, name)) {
				// Matched
				XFree(w_name);
				XFree(children);
				return s4g_open_window_from_x11(S, child, false);
			} else {
				// Didn't match
				XFree(w_name);
			}
		}

		s4g_window_t *W = s4g_find_first_window_with_given_property(S, child, prop, type, name);
		if(W != NULL) {
			// Matched
			XFree(children);
			return W;
		}
	}

	fail:
	if(children != NULL) {
		XFree(children);
	}
	return NULL;
}

s4g_window_t *s4g_open_window_with_class_name(s4g_display_t *S, const char *name)
{
	return s4g_find_first_window_with_given_property(S, XDefaultRootWindow(S->dpy), "WM_CLASS", "STRING", name);
}

s4g_window_t *s4g_open_window_with_title(s4g_display_t *S, const char *name)
{
	return s4g_find_first_window_with_given_property(S, XDefaultRootWindow(S->dpy), "WM_NAME", "STRING", name);
}

void *s4g_get_pointer_to_raw_display(s4g_display_t *S)
{
	return &S->dpy;
}

void *s4g_get_pointer_to_raw_window(s4g_window_t *W)
{
	return &W->w;
}

bool s4g_snap_from_window(s4g_window_t *W, void **data_return, int *width_return, int *height_return, int *bytes_per_line_return)
{
	s4g_display_t *S = W->S;
	XSync(S->dpy, False);
	if(!s4g_update_window_image(W)) {
		goto fail;
	}

	success:
	*data_return = (void *)W->ximage->data;
	*width_return = W->ximage->width;
	*height_return = W->ximage->height;
	*bytes_per_line_return = W->ximage->bytes_per_line;
	return true;

	fail:
	return false;
}
