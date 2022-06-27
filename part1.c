// Add helper functions.

#include <stdint.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>

/////////////////////////////////////////
// Definitions.
/////////////////////////////////////////

typedef struct Rectangle {
	int l, r, t, b;
} Rectangle;

typedef struct GlobalState {
} GlobalState;

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

/////////////////////////////////////////
// Platform specific code.
/////////////////////////////////////////

/////////////////////////////////////////
// Test usage code.
/////////////////////////////////////////

#include <stdio.h>

void PrintRectangle(const char *prefix, Rectangle x) {
	fprintf(stderr, "%s: %d->%d; %d->%d\n", prefix, x.l, x.r, x.t, x.b);
}

int main() {
	PrintRectangle("make", RectangleMake(10, 20, 30, 40));
	PrintRectangle("intersection", RectangleIntersection(RectangleMake(10, 20, 30, 40), RectangleMake(15, 25, 35, 45)));
	PrintRectangle("bounding", RectangleBounding(RectangleMake(10, 20, 30, 40), RectangleMake(15, 25, 35, 45)));
	fprintf(stderr, "valid: %d\n", RectangleValid(RectangleMake(10, 20, 30, 40)));
	fprintf(stderr, "valid: %d\n", RectangleValid(RectangleMake(20, 10, 30, 40)));
	fprintf(stderr, "equals: %d\n", RectangleEquals(RectangleMake(10, 20, 30, 40), RectangleMake(10, 20, 30, 40)));
	fprintf(stderr, "equals: %d\n", RectangleEquals(RectangleMake(10, 20, 30, 40), RectangleMake(15, 25, 35, 45)));
	fprintf(stderr, "contains: %d\n", RectangleContains(RectangleMake(10, 20, 30, 40), 15, 35));
	fprintf(stderr, "contains: %d\n", RectangleContains(RectangleMake(10, 20, 30, 40), 25, 35));

	char *destination = NULL;
	size_t destinationBytes = 0;
	StringCopy(&destination, &destinationBytes, "Hello!", 6);
	fprintf(stderr, "'%.*s'\n", (int) destinationBytes, destination);
	StringCopy(&destination, &destinationBytes, "World!", 6);
	fprintf(stderr, "'%.*s'\n", (int) destinationBytes, destination);
	free(destination);
}
