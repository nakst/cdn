// Remove interactivity.

#include <stdint.h>
#include <stdio.h>
#include "ui_library_tutorial.c"

/////////////////////////////////////////
// Definitions.
/////////////////////////////////////////

void Populate();

/////////////////////////////////////////
// Global state.
/////////////////////////////////////////

Element *container;

int count;

/////////////////////////////////////////
// Setting up the UI.
/////////////////////////////////////////

int ButtonDisabledMessage(Element *element, Message message, int di, void *dp) {
	(void) element;
	(void) di;

	if (message == MSG_BUTTON_GET_COLOR) {
		*(uint32_t *) dp = 0xCCCCCC; 
	}

	return 0;
}

void Populate() {
	for (uintptr_t i = 0; i < container->childCount; i++) {
		ElementDestroy(container->children[i]);
	}

	Panel *row = PanelCreate(container, PANEL_HORIZONTAL | ELEMENT_H_FILL);

	Button *buttonDecrement = ButtonCreate(&row->e, 0, "-", -1);
	if (count == 0) buttonDecrement->e.messageUser = ButtonDisabledMessage;

	char buffer[64];
	snprintf(buffer, sizeof(buffer), "Count: %d", count);
	LabelCreate(&row->e, ELEMENT_H_FILL | LABEL_CENTER, buffer, -1);

	Button *buttonIncrement = ButtonCreate(&row->e, 0, "+", -1);
	if (count == 10) buttonIncrement->e.messageUser = ButtonDisabledMessage;

	ElementMessage(container, MSG_LAYOUT, 0, 0);
	ElementRepaint(container, NULL);
}

#ifdef PLATFORM_WIN32
int WinMain(HINSTANCE instance, HINSTANCE previousInstance, LPSTR commandLine, int showCommand) {
	(void) instance;
	(void) previousInstance;
	(void) commandLine;
	(void) showCommand;
#else
int main() {
#endif
	count = 10;

	Initialise();
	Window *window = WindowCreate("Hello, world", 400, 120);
	Panel *panel = PanelCreate(&window->e, PANEL_GRAY);
	panel->border = RectangleMake(10, 10, 10, 10);
	panel->gap = 15;

	container = &PanelCreate(&panel->e, ELEMENT_H_FILL | ELEMENT_V_FILL)->e;
	Populate();
	return MessageLoop();
}
