// Add buttons to change the current count.

#include <stdint.h>
#include <stdio.h>
#include "ui_library_tutorial.c"

/////////////////////////////////////////
// Global state.
/////////////////////////////////////////

int count;

Button *buttonIncrement;
Button *buttonDecrement;
Label *labelCount;
Panel *row;

/////////////////////////////////////////
// Setting up the UI.
/////////////////////////////////////////

int ButtonIncrementMessage(Element *element, Message message, int di, void *dp) {
	(void) element;
	(void) di;

	if (message == MSG_CLICKED) {
		if (count < 10) {
			count++;

			char buffer[64];
			snprintf(buffer, sizeof(buffer), "Count: %d", count);
			LabelSetContent(labelCount, buffer, -1);

			ElementRepaint(&buttonIncrement->e, NULL);
			ElementRepaint(&buttonDecrement->e, NULL);
			ElementRepaint(&labelCount->e, NULL);
		}
	} else if (message == MSG_BUTTON_GET_COLOR) {
		if (count == 10) {
			*(uint32_t *) dp = 0xCCCCCC; 
		}
	}

	return 0;
}

int ButtonDecrementMessage(Element *element, Message message, int di, void *dp) {
	(void) element;
	(void) di;

	if (message == MSG_CLICKED) {
		if (count > 0) {
			count--;

			char buffer[64];
			snprintf(buffer, sizeof(buffer), "Count: %d", count);
			LabelSetContent(labelCount, buffer, -1);

			ElementRepaint(&buttonIncrement->e, NULL);
			ElementRepaint(&buttonDecrement->e, NULL);
			ElementRepaint(&labelCount->e, NULL);
		}
	} else if (message == MSG_BUTTON_GET_COLOR) {
		if (count == 0) {
			*(uint32_t *) dp = 0xCCCCCC; 
		}
	}

	return 0;
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
	count = 5;

	Initialise();
	Window *window = WindowCreate("Hello, world", 400, 120);
	Panel *panel = PanelCreate(&window->e, PANEL_GRAY);
	panel->border = RectangleMake(10, 10, 10, 10);
	panel->gap = 15;

	Element *container = &PanelCreate(&panel->e, ELEMENT_H_FILL | ELEMENT_V_FILL)->e;

	row = PanelCreate(container, PANEL_HORIZONTAL | ELEMENT_H_FILL);

	buttonDecrement = ButtonCreate(&row->e, 0, "-", -1);
	buttonDecrement->e.messageUser = ButtonDecrementMessage;

	char buffer[64];
	snprintf(buffer, sizeof(buffer), "Count: %d", count);
	labelCount = LabelCreate(&row->e, ELEMENT_H_FILL | LABEL_CENTER, buffer, -1);

	buttonIncrement = ButtonCreate(&row->e, 0, "+", -1);
	buttonIncrement->e.messageUser = ButtonIncrementMessage;

	return MessageLoop();
}
