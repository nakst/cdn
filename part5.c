// Add layout infrastructure.

#include <stdint.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>

#ifdef PLATFORM_WIN32
#define Rectangle W32Rectangle
#include <windows.h>
#undef Rectangle
#endif

#ifdef PLATFORM_LINUX
#define Window X11Window
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/cursorfont.h>
#undef Window
#endif

/////////////////////////////////////////
// Definitions.
/////////////////////////////////////////

typedef enum Message {
	MSG_LAYOUT,
	MSG_USER,
} Message;

typedef struct Rectangle {
	int l, r, t, b;
} Rectangle;

struct Element;
typedef int (*MessageHandler)(struct Element *element, Message message, int di, void *dp);

typedef struct Element {
	uint32_t flags; // First 16 bits are element specific.
	uint32_t childCount;
	Rectangle bounds, clip;
	struct Element *parent;
	struct Element **children;
	struct Window *window;
	void *cp; // Context pointer (for user).
	MessageHandler messageClass, messageUser;
} Element;

typedef struct Window {
	Element e;
	uint32_t *bits;
	int width, height;

#ifdef PLATFORM_WIN32
	HWND hwnd;
	bool trackingLeave;
#endif

#ifdef PLATFORM_LINUX
	X11Window window;
	XImage *image;
#endif
} Window;

typedef struct GlobalState {
	Window **windows;
	size_t windowCount;

#ifdef PLATFORM_LINUX
	Display *display;
	Visual *visual;
	Atom windowClosedID;
#endif
} GlobalState;

void Initialise();
int MessageLoop();

Element *ElementCreate(size_t bytes, Element *parent, uint32_t flags, MessageHandler messageClass);
void ElementMove(Element *element, Rectangle bounds, bool alwaysLayout);
int ElementMessage(Element *element, Message message, int di, void *dp);

Window *WindowCreate(const char *cTitle, int width, int height);

Rectangle RectangleMake(int l, int r, int t, int b);
Rectangle RectangleIntersection(Rectangle a, Rectangle b);
Rectangle RectangleBounding(Rectangle a, Rectangle b);
bool RectangleValid(Rectangle a);
bool RectangleEquals(Rectangle a, Rectangle b);
bool RectangleContains(Rectangle a, int x, int y);
void StringCopy(char **destination, size_t *destinationBytes, const char *source, ptrdiff_t sourceBytes);

/////////////////////////////////////////
// Helper functions.
/////////////////////////////////////////

Rectangle RectangleMake(int l, int r, int t, int b) {
	Rectangle x;
	x.l = l, x.r = r, x.t = t, x.b = b;
	return x;
}

Rectangle RectangleIntersection(Rectangle a, Rectangle b) {
	if (a.l < b.l) a.l = b.l;
	if (a.t < b.t) a.t = b.t;
	if (a.r > b.r) a.r = b.r;
	if (a.b > b.b) a.b = b.b;
	return a;
}

Rectangle RectangleBounding(Rectangle a, Rectangle b) {
	if (a.l > b.l) a.l = b.l;
	if (a.t > b.t) a.t = b.t;
	if (a.r < b.r) a.r = b.r;
	if (a.b < b.b) a.b = b.b;
	return a;
}

bool RectangleValid(Rectangle a) {
	return a.r > a.l && a.b > a.t;
}

bool RectangleEquals(Rectangle a, Rectangle b) {
	return a.l == b.l && a.r == b.r && a.t == b.t && a.b == b.b;
}

bool RectangleContains(Rectangle a, int x, int y) {
	return a.l <= x && a.r > x && a.t <= y && a.b > y;
}

void StringCopy(char **destination, size_t *destinationBytes, const char *source, ptrdiff_t sourceBytes) {
	if (sourceBytes == -1) sourceBytes = strlen(source);
	*destination = (char *) realloc(*destination, sourceBytes);
	*destinationBytes = sourceBytes;
	memcpy(*destination, source, sourceBytes);
}

/////////////////////////////////////////
// Core user interface logic.
/////////////////////////////////////////

GlobalState global;

void ElementMove(Element *element, Rectangle bounds, bool alwaysLayout) {
	Rectangle oldClip = element->clip;
	element->clip = RectangleIntersection(element->parent->clip, bounds);

	if (!RectangleEquals(element->bounds, bounds) || !RectangleEquals(element->clip, oldClip) || alwaysLayout) {
		element->bounds = bounds;
		ElementMessage(element, MSG_LAYOUT, 0, 0);
	}
}

int ElementMessage(Element *element, Message message, int di, void *dp) {
	if (element->messageUser) {
		int result = element->messageUser(element, message, di, dp);

		if (result) {
			return result;
		}
	}

	if (element->messageClass) {
		return element->messageClass(element, message, di, dp);
	} else {
		return 0;
	}
}

Element *ElementCreate(size_t bytes, Element *parent, uint32_t flags, MessageHandler messageClass)  {
	Element *element = (Element *) calloc(1, bytes);
	element->flags = flags;
	element->messageClass = messageClass;

	if (parent) {
		element->window = parent->window;
		element->parent = parent;
		parent->childCount++;
		parent->children = realloc(parent->children, sizeof(Element *) * parent->childCount);
		parent->children[parent->childCount - 1] = element;
	}

	return element;
}

/////////////////////////////////////////
// Platform specific code.
/////////////////////////////////////////

#ifdef PLATFORM_WIN32

LRESULT CALLBACK _WindowProcedure(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
	Window *window = (Window *) GetWindowLongPtr(hwnd, GWLP_USERDATA);

	if (!window) {
		return DefWindowProc(hwnd, message, wParam, lParam);
	}

	if (message == WM_CLOSE) {
		PostQuitMessage(0);
	} else if (message == WM_SIZE) {
		RECT client;
		GetClientRect(hwnd, &client);
		window->width = client.right;
		window->height = client.bottom;
		window->e.bounds = RectangleMake(0, window->width, 0, window->height);
		window->e.clip = RectangleMake(0, window->width, 0, window->height);
		ElementMessage(&window->e, MSG_LAYOUT, 0, 0);
	} else {
		return DefWindowProc(hwnd, message, wParam, lParam);
	}

	return 0;
}

int _WindowMessage(Element *element, Message message, int di, void *dp) {
	(void) di;
	(void) dp;

	if (message == MSG_LAYOUT && element->childCount) {
		ElementMove(element->children[0], element->bounds, false);
	}

	return 0;
}

Window *WindowCreate(const char *cTitle, int width, int height) {
	Window *window = (Window *) ElementCreate(sizeof(Window), NULL, 0, _WindowMessage);
	window->e.window = window;
	global.windowCount++;
	global.windows = realloc(global.windows, sizeof(Window *) * global.windowCount);
	global.windows[global.windowCount - 1] = window;

	window->hwnd = CreateWindow("UILibraryTutorial", cTitle, WS_OVERLAPPEDWINDOW, 
			CW_USEDEFAULT, CW_USEDEFAULT, width, height, NULL, NULL, NULL, NULL);
	SetWindowLongPtr(window->hwnd, GWLP_USERDATA, (LONG_PTR) window);
	ShowWindow(window->hwnd, SW_SHOW);
	PostMessage(window->hwnd, WM_SIZE, 0, 0);
	return window;
}

int MessageLoop() {
	MSG message = { 0 };

	while (GetMessage(&message, NULL, 0, 0)) {
		TranslateMessage(&message);
		DispatchMessage(&message);
	}

	return message.wParam;
}

void Initialise() {
	WNDCLASS windowClass = { 0 };
	windowClass.lpfnWndProc = _WindowProcedure;
	windowClass.hCursor = LoadCursor(NULL, IDC_ARROW);
	windowClass.lpszClassName = "UILibraryTutorial";
	RegisterClass(&windowClass);
}

#endif

#ifdef PLATFORM_LINUX

Window *_FindWindow(X11Window window) {
	for (uintptr_t i = 0; i < global.windowCount; i++) {
		if (global.windows[i]->window == window) {
			return global.windows[i];
		}
	}

	return NULL;
}

int _WindowMessage(Element *element, Message message, int di, void *dp) {
	(void) di;
	(void) dp;

	if (message == MSG_LAYOUT && element->childCount) {
		ElementMove(element->children[0], element->bounds, false);
	}

	return 0;
}

Window *WindowCreate(const char *cTitle, int width, int height) {
	Window *window = (Window *) ElementCreate(sizeof(Window), NULL, 0, _WindowMessage);
	window->e.window = window;
	global.windowCount++;
	global.windows = realloc(global.windows, sizeof(Window *) * global.windowCount);
	global.windows[global.windowCount - 1] = window;

	XSetWindowAttributes attributes = {};
	window->window = XCreateWindow(global.display, DefaultRootWindow(global.display), 0, 0, width, height, 0, 0, 
		InputOutput, CopyFromParent, CWOverrideRedirect, &attributes);
	XStoreName(global.display, window->window, cTitle);
	XSelectInput(global.display, window->window, SubstructureNotifyMask | ExposureMask | PointerMotionMask 
		| ButtonPressMask | ButtonReleaseMask | KeyPressMask | KeyReleaseMask | StructureNotifyMask
		| EnterWindowMask | LeaveWindowMask | ButtonMotionMask | KeymapStateMask | FocusChangeMask | PropertyChangeMask);
	XMapRaised(global.display, window->window);
	XSetWMProtocols(global.display, window->window, &global.windowClosedID, 1);
	window->image = XCreateImage(global.display, global.visual, 24, ZPixmap, 0, NULL, 10, 10, 32, 0);
	return window;
}

int MessageLoop() {
	while (true) {
		XEvent event;
		XNextEvent(global.display, &event);

		if (event.type == ClientMessage && (Atom) event.xclient.data.l[0] == global.windowClosedID) {
			return 0;
		} else if (event.type == ConfigureNotify) {
			Window *window = _FindWindow(event.xconfigure.window);
			if (!window) continue;

			if (window->width != event.xconfigure.width || window->height != event.xconfigure.height) {
				window->width = event.xconfigure.width;
				window->height = event.xconfigure.height;
				window->image->width = window->width;
				window->image->height = window->height;
				window->image->bytes_per_line = window->width * 4;
				window->image->data = (char *) window->bits;
				window->e.bounds = RectangleMake(0, window->width, 0, window->height);
				window->e.clip = RectangleMake(0, window->width, 0, window->height);
				ElementMessage(&window->e, MSG_LAYOUT, 0, 0);
			}
		}
	}
}

void Initialise() {
	global.display = XOpenDisplay(NULL);
	global.visual = XDefaultVisual(global.display, 0);
	global.windowClosedID = XInternAtom(global.display, "WM_DELETE_WINDOW", 0);
}

#endif

/////////////////////////////////////////
// Test usage code.
/////////////////////////////////////////

#include <stdio.h>

Element *elementA, *elementB, *elementC, *elementD;

int ElementAMessage(Element *element, Message message, int di, void *dp) {
	(void) di;
	(void) dp;

	Rectangle bounds = element->bounds;

	if (message == MSG_LAYOUT) {
		fprintf(stderr, "layout A with bounds (%d->%d;%d->%d)\n", bounds.l, bounds.r, bounds.t, bounds.b);
		ElementMove(elementB, RectangleMake(bounds.l + 20, bounds.r - 20, bounds.t + 20, bounds.b - 20), false);
	}

	return 0;
}

int ElementBMessage(Element *element, Message message, int di, void *dp) {
	(void) di;
	(void) dp;

	Rectangle bounds = element->bounds;

	if (message == MSG_LAYOUT) {
		fprintf(stderr, "layout B with bounds (%d->%d;%d->%d)\n", bounds.l, bounds.r, bounds.t, bounds.b);
		ElementMove(elementC, RectangleMake(bounds.l - 40, bounds.l + 40, bounds.t + 40, bounds.b - 40), false);
		ElementMove(elementD, RectangleMake(bounds.r - 40, bounds.r + 40, bounds.t + 40, bounds.b - 40), false);
	}

	return 0;
}

int ElementCMessage(Element *element, Message message, int di, void *dp) {
	(void) di;
	(void) dp;

	Rectangle bounds = element->bounds;
	Rectangle clip = element->clip;

	if (message == MSG_LAYOUT) {
		fprintf(stderr, "layout C with bounds (%d->%d;%d->%d)\n", bounds.l, bounds.r, bounds.t, bounds.b);
		fprintf(stderr, "\tclipped to (%d->%d;%d->%d)\n", clip.l, clip.r, clip.t, clip.b);
	}

	return 0;
}

int ElementDMessage(Element *element, Message message, int di, void *dp) {
	(void) di;
	(void) dp;

	Rectangle bounds = element->bounds;
	Rectangle clip = element->clip;

	if (message == MSG_LAYOUT) {
		fprintf(stderr, "layout D with bounds (%d->%d;%d->%d)\n", bounds.l, bounds.r, bounds.t, bounds.b);
		fprintf(stderr, "\tclipped to (%d->%d;%d->%d)\n", clip.l, clip.r, clip.t, clip.b);
	}

	return 0;
}

int main() {
	Initialise();
	Window *window = WindowCreate("Hello, world", 300, 200);
	elementA = ElementCreate(sizeof(Element), &window->e, 0, ElementAMessage);
	elementB = ElementCreate(sizeof(Element), elementA, 0, ElementBMessage);
	elementC = ElementCreate(sizeof(Element), elementB, 0, ElementCMessage);
	elementD = ElementCreate(sizeof(Element), elementB, 0, ElementDMessage);
	return MessageLoop();
}
