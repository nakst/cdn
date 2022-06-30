// Add layout panels.

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

#define UPDATE_HOVERED (1)
#define UPDATE_PRESSED (2)

typedef enum Message {
	MSG_PAINT, // dp = pointer to Painter
	MSG_LAYOUT,
	MSG_UPDATE, // di = UPDATE_... constant
	MSG_GET_WIDTH, // di = height (if known); return width
	MSG_GET_HEIGHT, // di = width (if known); return height

	MSG_LEFT_DOWN,
	MSG_LEFT_UP,
	MSG_MIDDLE_DOWN,
	MSG_MIDDLE_UP,
	MSG_RIGHT_DOWN,
	MSG_RIGHT_UP,
	MSG_MOUSE_MOVE,
	MSG_MOUSE_DRAG,
	MSG_CLICKED,

	MSG_BUTTON_GET_COLOR,

	MSG_USER,
} Message;

typedef struct Rectangle {
	int l, r, t, b;
} Rectangle;

typedef struct Painter {
	Rectangle clip;
	uint32_t *bits;
	int width, height;
} Painter;

struct Element;
typedef int (*MessageHandler)(struct Element *element, Message message, int di, void *dp);

typedef struct Element {
#define ELEMENT_V_FILL (1 << 16)
#define ELEMENT_H_FILL (1 << 17)
	uint32_t flags; // First 16 bits are element specific.
	uint32_t childCount;
	Rectangle bounds, clip;
	struct Element *parent;
	struct Element **children;
	struct Window *window;
	void *cp; // Context pointer (for user).
	MessageHandler messageClass, messageUser;
} Element;

typedef struct Button {
	Element e;
	char *text;
	size_t textBytes;
} Button;

typedef struct Label {
#define LABEL_CENTER (1 << 0)
	Element e;
	char *text;
	size_t textBytes;
} Label;

typedef struct Panel {
#define PANEL_HORIZONTAL (1 << 0)
#define PANEL_GRAY (1 << 1)
#define PANEL_WHITE (1 << 2)
	Element e;
	Rectangle border;
	int gap;
} Panel;

typedef struct Window {
	Element e;
	uint32_t *bits;
	int width, height;
	Element *hovered, *pressed;
	int pressedButton;
	int cursorX, cursorY;
	Rectangle updateRegion;

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
void ElementRepaint(Element *element, Rectangle *region);
void ElementMove(Element *element, Rectangle bounds, bool alwaysLayout);
int ElementMessage(Element *element, Message message, int di, void *dp);
Element *ElementFindByPoint(Element *element, int x, int y);

Window *WindowCreate(const char *cTitle, int width, int height);
Button *ButtonCreate(Element *parent, uint32_t flags, const char *label, ptrdiff_t labelBytes);
Panel *PanelCreate(Element *parent, uint32_t flags);
Label *LabelCreate(Element *parent, uint32_t flags, const char *label, ptrdiff_t labelBytes);
void LabelSetContent(Label *code, const char *content, ptrdiff_t byteCount);

Rectangle RectangleMake(int l, int r, int t, int b);
Rectangle RectangleIntersection(Rectangle a, Rectangle b);
Rectangle RectangleBounding(Rectangle a, Rectangle b);
bool RectangleValid(Rectangle a);
bool RectangleEquals(Rectangle a, Rectangle b);
bool RectangleContains(Rectangle a, int x, int y);
void StringCopy(char **destination, size_t *destinationBytes, const char *source, ptrdiff_t sourceBytes);

void DrawString(Painter *painter, Rectangle r, const char *string, size_t bytes, uint32_t color, bool centerAlign);
void DrawRectangle(Painter *painter, Rectangle r, uint32_t fill, uint32_t outline);
void DrawBlock(Painter *painter, Rectangle r, uint32_t fill);

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
// Painting.
/////////////////////////////////////////

// Taken from https://commons.wikimedia.org/wiki/File:Codepage-437.png
// Public domain.

#define GLYPH_WIDTH (9)
#define GLYPH_HEIGHT (16)

const uint64_t _font[] = {
	0x0000000000000000UL, 0x0000000000000000UL, 0xBD8181A5817E0000UL, 0x000000007E818199UL, 0xC3FFFFDBFF7E0000UL, 0x000000007EFFFFE7UL, 0x7F7F7F3600000000UL, 0x00000000081C3E7FUL, 
	0x7F3E1C0800000000UL, 0x0000000000081C3EUL, 0xE7E73C3C18000000UL, 0x000000003C1818E7UL, 0xFFFF7E3C18000000UL, 0x000000003C18187EUL, 0x3C18000000000000UL, 0x000000000000183CUL, 
	0xC3E7FFFFFFFFFFFFUL, 0xFFFFFFFFFFFFE7C3UL, 0x42663C0000000000UL, 0x00000000003C6642UL, 0xBD99C3FFFFFFFFFFUL, 0xFFFFFFFFFFC399BDUL, 0x331E4C5870780000UL, 0x000000001E333333UL, 
	0x3C666666663C0000UL, 0x0000000018187E18UL, 0x0C0C0CFCCCFC0000UL, 0x00000000070F0E0CUL, 0xC6C6C6FEC6FE0000UL, 0x0000000367E7E6C6UL, 0xE73CDB1818000000UL, 0x000000001818DB3CUL, 
	0x1F7F1F0F07030100UL, 0x000000000103070FUL, 0x7C7F7C7870604000UL, 0x0000000040607078UL, 0x1818187E3C180000UL, 0x0000000000183C7EUL, 0x6666666666660000UL, 0x0000000066660066UL, 
	0xD8DEDBDBDBFE0000UL, 0x00000000D8D8D8D8UL, 0x6363361C06633E00UL, 0x0000003E63301C36UL, 0x0000000000000000UL, 0x000000007F7F7F7FUL, 0x1818187E3C180000UL, 0x000000007E183C7EUL, 
	0x1818187E3C180000UL, 0x0000000018181818UL, 0x1818181818180000UL, 0x00000000183C7E18UL, 0x7F30180000000000UL, 0x0000000000001830UL, 0x7F060C0000000000UL, 0x0000000000000C06UL, 
	0x0303000000000000UL, 0x0000000000007F03UL, 0xFF66240000000000UL, 0x0000000000002466UL, 0x3E1C1C0800000000UL, 0x00000000007F7F3EUL, 0x3E3E7F7F00000000UL, 0x0000000000081C1CUL, 
	0x0000000000000000UL, 0x0000000000000000UL, 0x18183C3C3C180000UL, 0x0000000018180018UL, 0x0000002466666600UL, 0x0000000000000000UL, 0x36367F3636000000UL, 0x0000000036367F36UL, 
	0x603E0343633E1818UL, 0x000018183E636160UL, 0x1830634300000000UL, 0x000000006163060CUL, 0x3B6E1C36361C0000UL, 0x000000006E333333UL, 0x000000060C0C0C00UL, 0x0000000000000000UL, 
	0x0C0C0C0C18300000UL, 0x0000000030180C0CUL, 0x30303030180C0000UL, 0x000000000C183030UL, 0xFF3C660000000000UL, 0x000000000000663CUL, 0x7E18180000000000UL, 0x0000000000001818UL, 
	0x0000000000000000UL, 0x0000000C18181800UL, 0x7F00000000000000UL, 0x0000000000000000UL, 0x0000000000000000UL, 0x0000000018180000UL, 0x1830604000000000UL, 0x000000000103060CUL, 
	0xDBDBC3C3663C0000UL, 0x000000003C66C3C3UL, 0x1818181E1C180000UL, 0x000000007E181818UL, 0x0C183060633E0000UL, 0x000000007F630306UL, 0x603C6060633E0000UL, 0x000000003E636060UL, 
	0x7F33363C38300000UL, 0x0000000078303030UL, 0x603F0303037F0000UL, 0x000000003E636060UL, 0x633F0303061C0000UL, 0x000000003E636363UL, 0x18306060637F0000UL, 0x000000000C0C0C0CUL, 
	0x633E6363633E0000UL, 0x000000003E636363UL, 0x607E6363633E0000UL, 0x000000001E306060UL, 0x0000181800000000UL, 0x0000000000181800UL, 0x0000181800000000UL, 0x000000000C181800UL, 
	0x060C183060000000UL, 0x000000006030180CUL, 0x00007E0000000000UL, 0x000000000000007EUL, 0x6030180C06000000UL, 0x00000000060C1830UL, 0x18183063633E0000UL, 0x0000000018180018UL, 
	0x7B7B63633E000000UL, 0x000000003E033B7BUL, 0x7F6363361C080000UL, 0x0000000063636363UL, 0x663E6666663F0000UL, 0x000000003F666666UL, 0x03030343663C0000UL, 0x000000003C664303UL, 
	0x66666666361F0000UL, 0x000000001F366666UL, 0x161E1646667F0000UL, 0x000000007F664606UL, 0x161E1646667F0000UL, 0x000000000F060606UL, 0x7B030343663C0000UL, 0x000000005C666363UL, 
	0x637F636363630000UL, 0x0000000063636363UL, 0x18181818183C0000UL, 0x000000003C181818UL, 0x3030303030780000UL, 0x000000001E333333UL, 0x1E1E366666670000UL, 0x0000000067666636UL, 
	0x06060606060F0000UL, 0x000000007F664606UL, 0xC3DBFFFFE7C30000UL, 0x00000000C3C3C3C3UL, 0x737B7F6F67630000UL, 0x0000000063636363UL, 0x63636363633E0000UL, 0x000000003E636363UL, 
	0x063E6666663F0000UL, 0x000000000F060606UL, 0x63636363633E0000UL, 0x000070303E7B6B63UL, 0x363E6666663F0000UL, 0x0000000067666666UL, 0x301C0663633E0000UL, 0x000000003E636360UL, 
	0x18181899DBFF0000UL, 0x000000003C181818UL, 0x6363636363630000UL, 0x000000003E636363UL, 0xC3C3C3C3C3C30000UL, 0x00000000183C66C3UL, 0xDBC3C3C3C3C30000UL, 0x000000006666FFDBUL, 
	0x18183C66C3C30000UL, 0x00000000C3C3663CUL, 0x183C66C3C3C30000UL, 0x000000003C181818UL, 0x0C183061C3FF0000UL, 0x00000000FFC38306UL, 0x0C0C0C0C0C3C0000UL, 0x000000003C0C0C0CUL, 
	0x1C0E070301000000UL, 0x0000000040607038UL, 0x30303030303C0000UL, 0x000000003C303030UL, 0x0000000063361C08UL, 0x0000000000000000UL, 0x0000000000000000UL, 0x0000FF0000000000UL, 
	0x0000000000180C0CUL, 0x0000000000000000UL, 0x3E301E0000000000UL, 0x000000006E333333UL, 0x66361E0606070000UL, 0x000000003E666666UL, 0x03633E0000000000UL, 0x000000003E630303UL, 
	0x33363C3030380000UL, 0x000000006E333333UL, 0x7F633E0000000000UL, 0x000000003E630303UL, 0x060F0626361C0000UL, 0x000000000F060606UL, 0x33336E0000000000UL, 0x001E33303E333333UL, 
	0x666E360606070000UL, 0x0000000067666666UL, 0x18181C0018180000UL, 0x000000003C181818UL, 0x6060700060600000UL, 0x003C666660606060UL, 0x1E36660606070000UL, 0x000000006766361EUL, 
	0x18181818181C0000UL, 0x000000003C181818UL, 0xDBFF670000000000UL, 0x00000000DBDBDBDBUL, 0x66663B0000000000UL, 0x0000000066666666UL, 0x63633E0000000000UL, 0x000000003E636363UL, 
	0x66663B0000000000UL, 0x000F06063E666666UL, 0x33336E0000000000UL, 0x007830303E333333UL, 0x666E3B0000000000UL, 0x000000000F060606UL, 0x06633E0000000000UL, 0x000000003E63301CUL, 
	0x0C0C3F0C0C080000UL, 0x00000000386C0C0CUL, 0x3333330000000000UL, 0x000000006E333333UL, 0xC3C3C30000000000UL, 0x00000000183C66C3UL, 0xC3C3C30000000000UL, 0x0000000066FFDBDBUL, 
	0x3C66C30000000000UL, 0x00000000C3663C18UL, 0x6363630000000000UL, 0x001F30607E636363UL, 0x18337F0000000000UL, 0x000000007F63060CUL, 0x180E181818700000UL, 0x0000000070181818UL, 
	0x1800181818180000UL, 0x0000000018181818UL, 0x18701818180E0000UL, 0x000000000E181818UL, 0x000000003B6E0000UL, 0x0000000000000000UL, 0x63361C0800000000UL, 0x00000000007F6363UL, 
};

void DrawBlock(Painter *painter, Rectangle rectangle, uint32_t color) {
	rectangle = RectangleIntersection(painter->clip, rectangle);

	for (int y = rectangle.t; y < rectangle.b; y++) {
		for (int x = rectangle.l; x < rectangle.r; x++) {
			painter->bits[y * painter->width + x] = color;
		}
	}
}

void DrawRectangle(Painter *painter, Rectangle r, uint32_t mainColor, uint32_t borderColor) {
	DrawBlock(painter, RectangleMake(r.l, r.r, r.t, r.t + 1), borderColor);
	DrawBlock(painter, RectangleMake(r.l, r.l + 1, r.t + 1, r.b - 1), borderColor);
	DrawBlock(painter, RectangleMake(r.r - 1, r.r, r.t + 1, r.b - 1), borderColor);
	DrawBlock(painter, RectangleMake(r.l, r.r, r.b - 1, r.b), borderColor);
	DrawBlock(painter, RectangleMake(r.l + 1, r.r - 1, r.t + 1, r.b - 1), mainColor);
}

void DrawString(Painter *painter, Rectangle bounds, const char *string, size_t bytes, uint32_t color, bool centerAlign) {
	Rectangle oldClip = painter->clip;
	painter->clip = RectangleIntersection(bounds, oldClip);
	int x = bounds.l;
	int y = (bounds.t + bounds.b - GLYPH_HEIGHT) / 2;

	if (centerAlign) {
		x += (bounds.r - bounds.l - bytes * GLYPH_WIDTH) / 2;
	}

	for (uintptr_t i = 0; i < bytes; i++) {
		uint8_t c = string[i];
		if (c > 127) c = '?';

		Rectangle rectangle = RectangleIntersection(painter->clip, RectangleMake(x, x + 8, y, y + 16));
		const uint8_t *data = (const uint8_t *) _font + c * 16;

		for (int i = rectangle.t; i < rectangle.b; i++) {
			uint32_t *bits = painter->bits + i * painter->width + rectangle.l;
			uint8_t byte = data[i - y];

			for (int j = rectangle.l; j < rectangle.r; j++) {
				if (byte & (1 << (j - x))) {
					*bits = color;
				}

				bits++;
			}
		}

		x += GLYPH_WIDTH;
	}

	painter->clip = oldClip;
}

/////////////////////////////////////////
// Core user interface logic.
/////////////////////////////////////////

void _WindowEndPaint(Window *window, Painter *painter);

GlobalState global;

void _ElementPaint(Element *element, Painter *painter) {
	Rectangle clip = RectangleIntersection(element->clip, painter->clip);

	if (!RectangleValid(clip)) {
		return;
	}

	painter->clip = clip;
	ElementMessage(element, MSG_PAINT, 0, painter);

	for (uintptr_t i = 0; i < element->childCount; i++) {
		painter->clip = clip;
		_ElementPaint(element->children[i], painter);
	}
}

Element *ElementFindByPoint(Element *element, int x, int y) {
	for (uintptr_t i = 0; i < element->childCount; i++) {
		if (RectangleContains(element->children[i]->clip, x, y)) {
			return ElementFindByPoint(element->children[i], x, y);
		}
	}

	return element;
}

void _WindowSetPressed(Window *window, Element *element, int button) {
	Element *previous = window->pressed;
	window->pressed = element;
	window->pressedButton = button;
	if (previous) ElementMessage(previous, MSG_UPDATE, UPDATE_PRESSED, 0);
	if (element) ElementMessage(element, MSG_UPDATE, UPDATE_PRESSED, 0);
}

void _Update() {
	for (uintptr_t i = 0; i < global.windowCount; i++) {
		Window *window = global.windows[i];

		if (RectangleValid(window->updateRegion)) {
			Painter painter;
			painter.bits = window->bits;
			painter.width = window->width;
			painter.height = window->height;
			painter.clip = RectangleIntersection(RectangleMake(0, window->width, 0, window->height), window->updateRegion);
			_ElementPaint(&window->e, &painter);
			_WindowEndPaint(window, &painter);
			window->updateRegion = RectangleMake(0, 0, 0, 0);
		}
	}
}

void _WindowInputEvent(Window *window, Message message, int di, void *dp) {
	if (window->pressed) {
		if (message == MSG_MOUSE_MOVE) {
			ElementMessage(window->pressed, MSG_MOUSE_DRAG, di, dp);
		} else if (message == MSG_LEFT_UP && window->pressedButton == 1) {
			if (window->hovered == window->pressed) {
				ElementMessage(window->pressed, MSG_CLICKED, di, dp);
			}

			if (window->pressed) {
				ElementMessage(window->pressed, MSG_LEFT_UP, di, dp);
				_WindowSetPressed(window, NULL, 1);
			}
		} else if (message == MSG_MIDDLE_UP && window->pressedButton == 2) {
			ElementMessage(window->pressed, MSG_MIDDLE_UP, di, dp);
			_WindowSetPressed(window, NULL, 2);
		} else if (message == MSG_RIGHT_UP && window->pressedButton == 3) {
			ElementMessage(window->pressed, MSG_RIGHT_UP, di, dp);
			_WindowSetPressed(window, NULL, 3);
		}
	}

	if (window->pressed) {
		bool inside = RectangleContains(window->pressed->clip, window->cursorX, window->cursorY);

		if (inside && window->hovered == &window->e) {
			window->hovered = window->pressed;
			ElementMessage(window->pressed, MSG_UPDATE, UPDATE_HOVERED, 0);
		} else if (!inside && window->hovered == window->pressed) {
			window->hovered = &window->e;
			ElementMessage(window->pressed, MSG_UPDATE, UPDATE_HOVERED, 0);
		}
	}

	if (!window->pressed) {
		Element *hovered = ElementFindByPoint(&window->e, window->cursorX, window->cursorY);

		if (message == MSG_MOUSE_MOVE) {
			ElementMessage(hovered, MSG_MOUSE_MOVE, di, dp);
		} else if (message == MSG_LEFT_DOWN) {
			_WindowSetPressed(window, hovered, 1);
			ElementMessage(hovered, message, di, dp);
		} else if (message == MSG_MIDDLE_DOWN) {
			_WindowSetPressed(window, hovered, 2);
			ElementMessage(hovered, message, di, dp);
		} else if (message == MSG_RIGHT_DOWN) {
			_WindowSetPressed(window, hovered, 3);
			ElementMessage(hovered, message, di, dp);
		}

		if (hovered != window->hovered) {
			Element *previous = window->hovered;
			window->hovered = hovered;
			ElementMessage(previous, MSG_UPDATE, UPDATE_HOVERED, 0);
			ElementMessage(window->hovered, MSG_UPDATE, UPDATE_HOVERED, 0);
		}
	}

	_Update();
}

void ElementMove(Element *element, Rectangle bounds, bool alwaysLayout) {
	Rectangle oldClip = element->clip;
	element->clip = RectangleIntersection(element->parent->clip, bounds);

	if (!RectangleEquals(element->bounds, bounds) || !RectangleEquals(element->clip, oldClip) || alwaysLayout) {
		element->bounds = bounds;
		ElementMessage(element, MSG_LAYOUT, 0, 0);
	}
}

void ElementRepaint(Element *element, Rectangle *region) {
	if (!region) {
		region = &element->bounds;
	}

	Rectangle r = RectangleIntersection(*region, element->clip);

	if (RectangleValid(r)) {
		if (RectangleValid(element->window->updateRegion)) {
			element->window->updateRegion = RectangleBounding(element->window->updateRegion, r);
		} else {
			element->window->updateRegion = r;
		}
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
// Layout panels.
/////////////////////////////////////////

int _PanelMeasure(Panel *panel) {
	bool horizontal = panel->e.flags & PANEL_HORIZONTAL;
	int size = 0;

	for (uintptr_t i = 0; i < panel->e.childCount; i++) {
		int childSize = ElementMessage(panel->e.children[i], horizontal ? MSG_GET_HEIGHT : MSG_GET_WIDTH, 0, 0);
		if (childSize > size) size = childSize;
	}

	int border = horizontal ? panel->border.t + panel->border.b : panel->border.l + panel->border.r;
	return size + border;
}

int _PanelLayout(Panel *panel, Rectangle bounds, bool measure) {
	bool horizontal = panel->e.flags & PANEL_HORIZONTAL;
	int position = horizontal ? panel->border.l : panel->border.t;
	int hSpace = bounds.r - bounds.l - panel->border.r - panel->border.l;
	int vSpace = bounds.b - bounds.t - panel->border.b - panel->border.t;

	int available = horizontal ? hSpace : vSpace;
	int fill = 0, perFill = 0, count = 0;

	for (uintptr_t i = 0; i < panel->e.childCount; i++) {
		count++;

		if (horizontal) {
			if (panel->e.children[i]->flags & ELEMENT_H_FILL) {
				fill++;
			} else if (available > 0) {
				available -= ElementMessage(panel->e.children[i], MSG_GET_WIDTH, vSpace, 0);
			}
		} else {
			if (panel->e.children[i]->flags & ELEMENT_V_FILL) {
				fill++;
			} else if (available > 0) {
				available -= ElementMessage(panel->e.children[i], MSG_GET_HEIGHT, hSpace, 0);
			}
		}
	}

	if (count) {
		available -= (count - 1) * panel->gap;
	}

	if (available > 0 && fill) {
		perFill = available / fill;
	}

	int border2 = horizontal ? panel->border.t : panel->border.l;

	for (uintptr_t i = 0; i < panel->e.childCount; i++) {
		Element *child = panel->e.children[i];

		if (horizontal) {
			int height = (child->flags & ELEMENT_V_FILL) ? vSpace : ElementMessage(child, MSG_GET_HEIGHT, (child->flags & ELEMENT_H_FILL) ? perFill : 0, 0);
			int width = (child->flags & ELEMENT_H_FILL) ? perFill : ElementMessage(child, MSG_GET_WIDTH, height, 0);
			Rectangle r = RectangleMake(position + bounds.l, position + width + bounds.l, 
					border2 + (vSpace - height) / 2 + bounds.t, border2 + (vSpace + height) / 2 + bounds.t);
			if (!measure) ElementMove(child, r, false);
			position += width + panel->gap;
		} else {
			int width = (child->flags & ELEMENT_H_FILL) ? hSpace : ElementMessage(child, MSG_GET_WIDTH, (child->flags & ELEMENT_V_FILL) ? perFill : 0, 0);
			int height = (child->flags & ELEMENT_V_FILL) ? perFill : ElementMessage(child, MSG_GET_HEIGHT, width, 0);
			Rectangle r = RectangleMake(border2 + (hSpace - width) / 2 + bounds.l, border2 + (hSpace + width) / 2 + bounds.l, 
					position + bounds.t, position + height + bounds.t);
			if (!measure) ElementMove(child, r, false);
			position += height + panel->gap;
		}
	}

	return position - (count ? panel->gap : 0) + (horizontal ? panel->border.r : panel->border.b);
}

int _PanelMessage(Element *element, Message message, int di, void *dp) {
	Panel *panel = (Panel *) element;
	bool horizontal = element->flags & PANEL_HORIZONTAL;

	if (message == MSG_LAYOUT) {
		_PanelLayout(panel, element->bounds, false);
		ElementRepaint(element, NULL);
	} else if (message == MSG_GET_WIDTH) {
		return horizontal ? _PanelLayout(panel, RectangleMake(0, 0, 0, di), true) : _PanelMeasure(panel);
	} else if (message == MSG_GET_HEIGHT) {
		return horizontal ? _PanelMeasure(panel) : _PanelLayout(panel, RectangleMake(0, di, 0, 0), true);
	} else if (message == MSG_PAINT) {
		if (element->flags & PANEL_GRAY) {
			DrawBlock((Painter *) dp, element->bounds, 0xCCCCCC);
		} else if (element->flags & PANEL_WHITE) {
			DrawBlock((Painter *) dp, element->bounds, 0xFFFFFF);
		} 
	}

	return 0;
}

Panel *PanelCreate(Element *parent, uint32_t flags) {
	return (Panel *) ElementCreate(sizeof(Panel), parent, flags, _PanelMessage);
}

/////////////////////////////////////////
// Buttons and labels.
/////////////////////////////////////////

int _LabelMessage(Element *element, Message message, int di, void *dp) {
	Label *label = (Label *) element;
	(void) di;
	
	if (message == MSG_GET_HEIGHT) {
		return GLYPH_HEIGHT;
	} else if (message == MSG_GET_WIDTH) {
		return GLYPH_WIDTH * label->textBytes;
	} else if (message == MSG_PAINT) {
		DrawString((Painter *) dp, element->bounds, label->text, label->textBytes, 0x000000, element->flags & LABEL_CENTER);
	}

	return 0;
}

void LabelSetContent(Label *label, const char *text, ptrdiff_t textBytes) {
	StringCopy(&label->text, &label->textBytes, text, textBytes);
}

Label *LabelCreate(Element *parent, uint32_t flags, const char *text, ptrdiff_t textBytes) {
	Label *label = (Label *) ElementCreate(sizeof(Label), parent, flags, _LabelMessage);
	LabelSetContent(label, text, textBytes);
	return label;
}

int _ButtonMessage(Element *element, Message message, int di, void *dp) {
	Button *button = (Button *) element;
	(void) di;
	
	if (message == MSG_GET_HEIGHT) {
		return 25;
	} else if (message == MSG_GET_WIDTH) {
		return 30 + GLYPH_WIDTH * button->textBytes;
	} else if (message == MSG_PAINT) {
		Painter *painter = (Painter *) dp;
		bool pressed = element->window->pressed == element && element->window->hovered == element;
		uint32_t c = 0xFFFFFF;
		ElementMessage(element, MSG_BUTTON_GET_COLOR, 0, &c);
		uint32_t c1 = pressed ? 0xFFFFFF : 0x000000, c2 = pressed ? 0x000000 : c;
		DrawRectangle(painter, element->bounds, c2, c1);
		DrawString(painter, element->bounds, button->text, button->textBytes, c1, true);
	} else if (message == MSG_UPDATE) {
		ElementRepaint(element, NULL);
	}

	return 0;
}

Button *ButtonCreate(Element *parent, uint32_t flags, const char *text, ptrdiff_t textBytes) {
	Button *button = (Button *) ElementCreate(sizeof(Button), parent, flags, _ButtonMessage);
	StringCopy(&button->text, &button->textBytes, text, textBytes);
	return button;
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
		window->bits = (uint32_t *) realloc(window->bits, window->width * window->height * 4);
		window->e.bounds = RectangleMake(0, window->width, 0, window->height);
		window->e.clip = RectangleMake(0, window->width, 0, window->height);
		ElementMessage(&window->e, MSG_LAYOUT, 0, 0);
		_Update();
	} else if (message == WM_MOUSEMOVE) {
		if (!window->trackingLeave) {
			window->trackingLeave = true;
			TRACKMOUSEEVENT leave = { 0 };
			leave.cbSize = sizeof(TRACKMOUSEEVENT);
			leave.dwFlags = TME_LEAVE;
			leave.hwndTrack = hwnd;
			TrackMouseEvent(&leave);
		}

		POINT cursor;
		GetCursorPos(&cursor);
		ScreenToClient(hwnd, &cursor);
		window->cursorX = cursor.x;
		window->cursorY = cursor.y;
		_WindowInputEvent(window, MSG_MOUSE_MOVE, 0, 0);
	} else if (message == WM_MOUSELEAVE) {
		window->trackingLeave = false;

		if (!window->pressed) {
			window->cursorX = -1;
			window->cursorY = -1;
		}

		_WindowInputEvent(window, MSG_MOUSE_MOVE, 0, 0);
	} else if (message == WM_LBUTTONDOWN) {
		SetCapture(hwnd);
		_WindowInputEvent(window, MSG_LEFT_DOWN, 0, 0);
	} else if (message == WM_LBUTTONUP) {
		if (window->pressedButton == 1) ReleaseCapture();
		_WindowInputEvent(window, MSG_LEFT_UP, 0, 0);
	} else if (message == WM_MBUTTONDOWN) {
		SetCapture(hwnd);
		_WindowInputEvent(window, MSG_MIDDLE_DOWN, 0, 0);
	} else if (message == WM_MBUTTONUP) {
		if (window->pressedButton == 2) ReleaseCapture();
		_WindowInputEvent(window, MSG_MIDDLE_UP, 0, 0);
	} else if (message == WM_RBUTTONDOWN) {
		SetCapture(hwnd);
		_WindowInputEvent(window, MSG_RIGHT_DOWN, 0, 0);
	} else if (message == WM_RBUTTONUP) {
		if (window->pressedButton == 3) ReleaseCapture();
		_WindowInputEvent(window, MSG_RIGHT_UP, 0, 0);
	} else if (message == WM_PAINT) {
		PAINTSTRUCT paint;
		HDC dc = BeginPaint(hwnd, &paint);
		BITMAPINFOHEADER info = { 0 };
		info.biSize = sizeof(info);
		info.biWidth = window->width, info.biHeight = -window->height;
		info.biPlanes = 1, info.biBitCount = 32;
		StretchDIBits(dc, 0, 0, window->e.bounds.r - window->e.bounds.l, window->e.bounds.b - window->e.bounds.t, 
				0, 0, window->e.bounds.r - window->e.bounds.l, window->e.bounds.b - window->e.bounds.t,
				window->bits, (BITMAPINFO *) &info, DIB_RGB_COLORS, SRCCOPY);
		EndPaint(hwnd, &paint);
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
		ElementRepaint(element, NULL);
	}

	return 0;
}

void _WindowEndPaint(Window *window, Painter *painter) {
	(void) painter;
	HDC dc = GetDC(window->hwnd);
	BITMAPINFOHEADER info = { 0 };
	info.biSize = sizeof(info);
	info.biWidth = window->width, info.biHeight = window->height;
	info.biPlanes = 1, info.biBitCount = 32;
	StretchDIBits(dc, 
		window->updateRegion.l, window->updateRegion.t, 
		window->updateRegion.r - window->updateRegion.l, window->updateRegion.b - window->updateRegion.t,
		window->updateRegion.l, window->updateRegion.b + 1, 
		window->updateRegion.r - window->updateRegion.l, window->updateRegion.t - window->updateRegion.b,
		window->bits, (BITMAPINFO *) &info, DIB_RGB_COLORS, SRCCOPY);
	ReleaseDC(window->hwnd, dc);
}

Window *WindowCreate(const char *cTitle, int width, int height) {
	Window *window = (Window *) ElementCreate(sizeof(Window), NULL, 0, _WindowMessage);
	window->hovered = &window->e;
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

void _WindowEndPaint(Window *window, Painter *painter) {
	(void) painter;

	XPutImage(global.display, window->window, DefaultGC(global.display, 0), window->image, 
		window->updateRegion.l, window->updateRegion.t, window->updateRegion.l, window->updateRegion.t,
		window->updateRegion.r - window->updateRegion.l, window->updateRegion.b - window->updateRegion.t);
}

int _WindowMessage(Element *element, Message message, int di, void *dp) {
	(void) di;
	(void) dp;

	if (message == MSG_LAYOUT && element->childCount) {
		ElementMove(element->children[0], element->bounds, false);
		ElementRepaint(element, NULL);
	}

	return 0;
}

Window *WindowCreate(const char *cTitle, int width, int height) {
	Window *window = (Window *) ElementCreate(sizeof(Window), NULL, 0, _WindowMessage);
	window->hovered = &window->e;
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
	_Update();

	while (true) {
		XEvent event;
		XNextEvent(global.display, &event);

		if (event.type == ClientMessage && (Atom) event.xclient.data.l[0] == global.windowClosedID) {
			return 0;
		} else if (event.type == Expose) {
			Window *window = _FindWindow(event.xexpose.window);
			if (!window) continue;
			XPutImage(global.display, window->window, DefaultGC(global.display, 0), 
					window->image, 0, 0, 0, 0, window->width, window->height);
		} else if (event.type == ConfigureNotify) {
			Window *window = _FindWindow(event.xconfigure.window);
			if (!window) continue;

			if (window->width != event.xconfigure.width || window->height != event.xconfigure.height) {
				window->width = event.xconfigure.width;
				window->height = event.xconfigure.height;
				window->bits = (uint32_t *) realloc(window->bits, window->width * window->height * 4);
				window->image->width = window->width;
				window->image->height = window->height;
				window->image->bytes_per_line = window->width * 4;
				window->image->data = (char *) window->bits;
				window->e.bounds = RectangleMake(0, window->width, 0, window->height);
				window->e.clip = RectangleMake(0, window->width, 0, window->height);
				ElementMessage(&window->e, MSG_LAYOUT, 0, 0);
				_Update();
			}
		} else if (event.type == MotionNotify) {
			Window *window = _FindWindow(event.xmotion.window);
			if (!window) continue;
			window->cursorX = event.xmotion.x;
			window->cursorY = event.xmotion.y;
			_WindowInputEvent(window, MSG_MOUSE_MOVE, 0, 0);
		} else if (event.type == LeaveNotify) {
			Window *window = _FindWindow(event.xcrossing.window);
			if (!window) continue;

			if (!window->pressed) {
				window->cursorX = -1;
				window->cursorY = -1;
			}

			_WindowInputEvent(window, MSG_MOUSE_MOVE, 0, 0);
		} else if (event.type == ButtonPress || event.type == ButtonRelease) {
			Window *window = _FindWindow(event.xbutton.window);
			if (!window) continue;
			window->cursorX = event.xbutton.x;
			window->cursorY = event.xbutton.y;

			if (event.xbutton.button >= 1 && event.xbutton.button <= 3) {
				_WindowInputEvent(window, (Message) ((event.type == ButtonPress ? MSG_LEFT_DOWN : MSG_LEFT_UP) 
					+ event.xbutton.button * 2 - 2), 0, 0);
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

int FixedReportedSizeElement(Element *element, Message message, int di, void *dp) {
	(void) di;

	if (message == MSG_GET_WIDTH) {
		return 25;
	} else if (message == MSG_GET_HEIGHT) {
		return 50;
	} else if (message == MSG_PAINT) {
		DrawBlock((Painter *) dp, element->bounds, (uint32_t) (uintptr_t) element->cp);
	}

	return 0;
}

int AspectRatioElement(Element *element, Message message, int di, void *dp) {
	if (message == MSG_GET_WIDTH) {
		return di * 2;
	} else if (message == MSG_GET_HEIGHT) {
		return di / 2;
	} else if (message == MSG_PAINT) {
		DrawBlock((Painter *) dp, element->bounds, (uint32_t) (uintptr_t) element->cp);
	}

	return 0;
}

int FixedAreaElement(Element *element, Message message, int di, void *dp) {
	if (message == MSG_GET_WIDTH) {
		return di ? 50000 / di : 0;
	} else if (message == MSG_GET_HEIGHT) {
		return di ? 50000 / di : 0;
	} else if (message == MSG_PAINT) {
		DrawBlock((Painter *) dp, element->bounds, (uint32_t) (uintptr_t) element->cp);
	}

	return 0;
}

int main() {
	Initialise();

	Window *window1 = WindowCreate("Window 1", 1000, 800);

	Panel *column1 = PanelCreate(&window1->e, PANEL_GRAY);
	column1->gap = 10;
	column1->border = RectangleMake(10, 10, 10, 10);

	LabelCreate(&column1->e, 0, "Label 1", -1);
	LabelCreate(&column1->e, 0, "Longer label 2", -1);

	Panel *column2 = PanelCreate(&column1->e, PANEL_WHITE);
	column2->gap = 10;
	column2->border = RectangleMake(10, 10, 10, 10);

	LabelCreate(&column2->e, ELEMENT_H_FILL, "Label 3", -1);
	LabelCreate(&column2->e, ELEMENT_H_FILL, "Much Longer label 4", -1);

	Panel *column3 = PanelCreate(&column1->e, PANEL_WHITE | ELEMENT_H_FILL);
	column3->gap = 10;
	column3->border = RectangleMake(10, 10, 10, 10);

	LabelCreate(&column3->e, 0, "Label 4", -1);
	LabelCreate(&column3->e, 0, "Longer label 5", -1);

	Panel *column4 = PanelCreate(&column1->e, PANEL_WHITE | ELEMENT_H_FILL);
	column4->gap = 10;
	column4->border = RectangleMake(10, 10, 10, 10);

	LabelCreate(&column4->e, ELEMENT_H_FILL, "Label 6", -1);
	LabelCreate(&column4->e, ELEMENT_H_FILL, "Longer label 7", -1);

	ButtonCreate(&column1->e, ELEMENT_V_FILL, "Vertical fill button 1", -1);
	ButtonCreate(&column1->e, ELEMENT_V_FILL, "Vertical fill button 2", -1);
	ButtonCreate(&column1->e, ELEMENT_V_FILL | ELEMENT_H_FILL, "Vertical and horizontal fill button 3", -1);
	ButtonCreate(&column1->e, ELEMENT_V_FILL, "Vertical fill button 4", -1);

	Panel *row1 = PanelCreate(&column1->e, PANEL_WHITE | PANEL_HORIZONTAL);
	row1->gap = 10;
	row1->border = RectangleMake(10, 10, 10, 10);

	ButtonCreate(&row1->e, 0, "Button 1 in row", -1);
	ButtonCreate(&row1->e, ELEMENT_H_FILL, "Button 2 in row", -1);
	ButtonCreate(&row1->e, 0, "Button 3 in row", -1);

	Panel *row2 = PanelCreate(&column1->e, PANEL_WHITE | PANEL_HORIZONTAL | ELEMENT_H_FILL);
	row2->gap = 10;
	row2->border = RectangleMake(10, 10, 10, 10);

	ButtonCreate(&row2->e, 0, "Button 4 in row", -1);
	ButtonCreate(&row2->e, ELEMENT_H_FILL, "Button 5 in row", -1);
	ButtonCreate(&row2->e, 0, "Button 6 in row", -1);

	Window *window2 = WindowCreate("Window 2", 500, 500);

	Panel *column5 = PanelCreate(&window2->e, PANEL_GRAY);
	column5->gap = 10;
	column5->border = RectangleMake(10, 10, 10, 10);

	ElementCreate(sizeof(Element), &column5->e, 0, FixedReportedSizeElement)->cp = (void *) (uintptr_t) 0x111111;
	ElementCreate(sizeof(Element), &column5->e, ELEMENT_H_FILL, FixedReportedSizeElement)->cp = (void *) (uintptr_t) 0xFF1111;
	ElementCreate(sizeof(Element), &column5->e, ELEMENT_V_FILL, FixedReportedSizeElement)->cp = (void *) (uintptr_t) 0x11FF11;
	ElementCreate(sizeof(Element), &column5->e, ELEMENT_H_FILL | ELEMENT_V_FILL, FixedReportedSizeElement)->cp = (void *) (uintptr_t) 0x1111FF;

	Window *window3 = WindowCreate("Window 3", 500, 500);
	Panel *column6 = PanelCreate(&window3->e, PANEL_GRAY);
	column6->gap = 10;
	column6->border = RectangleMake(10, 10, 10, 10);

	ElementCreate(sizeof(Element), &column6->e, ELEMENT_H_FILL, AspectRatioElement)->cp = (void *) (uintptr_t) 0x111111;
	ElementCreate(sizeof(Element), &column6->e, ELEMENT_V_FILL, AspectRatioElement)->cp = (void *) (uintptr_t) 0xFF1111;
	ElementCreate(sizeof(Element), &column6->e, ELEMENT_H_FILL, FixedAreaElement)->cp = (void *) (uintptr_t) 0x11FF11;
	ElementCreate(sizeof(Element), &column6->e, ELEMENT_V_FILL, FixedAreaElement)->cp = (void *) (uintptr_t) 0x1111FF;

	return MessageLoop();
}
