// Display a count variable.

#include <stdint.h>
#include <stdio.h>
#include "ui_library_tutorial.c"

/////////////////////////////////////////
// Global state.
/////////////////////////////////////////

int count;

Label *labelCount;
Panel *row;

/////////////////////////////////////////
// Setting up the UI.
/////////////////////////////////////////

#ifdef PLATFORM_WIN32
int WinMain(HINSTANCE instance, HINSTANCE previousInstance, LPSTR commandLine, int showCommand) {
	(void) instance;
	(void) previousInstance;
	(void) commandLine;
	(void) showCommand;
#else
int main() {
#endif
	count = 5;

	Initialise();
	Window *window = WindowCreate("Hello, world", 400, 120);
	Panel *panel = PanelCreate(&window->e, PANEL_GRAY);
	panel->border = RectangleMake(10, 10, 10, 10);
	panel->gap = 15;

	Element *container = &PanelCreate(&panel->e, ELEMENT_H_FILL | ELEMENT_V_FILL)->e;

	row = PanelCreate(container, PANEL_HORIZONTAL | ELEMENT_H_FILL);

	char buffer[64];
	snprintf(buffer, sizeof(buffer), "Count: %d", count);
	labelCount = LabelCreate(&row->e, ELEMENT_H_FILL | LABEL_CENTER, buffer, -1);

	return MessageLoop();
}
