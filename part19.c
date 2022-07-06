// Move out react element creation functions.

#define STB_DS_IMPLEMENTATION
#include "stb_ds.h"
#include <stdint.h>
#include <stdio.h>
#include "ui_library_tutorial.c"

/////////////////////////////////////////
// Definitions.
/////////////////////////////////////////

#define PROPERTY_U32  (1)
// ...

typedef struct Property {
	uint8_t type;
#define PROPERTY_KEY_MAX_SIZE (14)
	char key[PROPERTY_KEY_MAX_SIZE + 1]; // ... could be an ID.

	union {
		uint32_t u32;
		// ...
	};
} Property;

#define OBJECT_COUNTER (1)
// ...

typedef struct Object {
	int type;
	Property *properties;
} Object;

void Populate();

/////////////////////////////////////////
// Global state.
/////////////////////////////////////////

struct { uint64_t key; Object value; } *objects;
uint64_t objectIDAllocator;

uint64_t selectedObjectID; // ... or an array of IDs
Element *container;

/////////////////////////////////////////
// Freeing state.
/////////////////////////////////////////

void PropertyFree(Property property) {
	(void) property;
	// ...
}

void ObjectFree(Object object) {
	for (int i = 0; i < arrlen(object.properties); i++) {
		PropertyFree(object.properties[i]);
	}

	arrfree(object.properties);
}

void DocumentFree() {
	for (int i = 0; i < hmlen(objects); i++) {
		ObjectFree(objects[i].value);
	}

	hmfree(objects);
}

/////////////////////////////////////////
// Reading properties.
/////////////////////////////////////////

Property ObjectReadAny(Object *object, const char *key) {
	for (int i = 0; i < arrlen(object->properties); i++) {
		if (0 == strcmp(object->properties[i].key, key)) {
			return object->properties[i];
		}
	}

	Property empty = { 0 };
	return empty;
}

uint32_t ObjectReadU32(Object *object, const char *key, uint32_t defaultValue) {
	Property property = ObjectReadAny(object, key);
	return property.type == PROPERTY_U32 ? property.u32 : defaultValue;
}

Object *SelectedObject() {
	return &hmgetp(objects, selectedObjectID)->value;
}

/////////////////////////////////////////
// Reactive elements.
/////////////////////////////////////////

int ReactU32DisplayMessage(Element *element, Message message, int di, void *dp) {
	(void) element;
	(void) message;
	(void) di;
	(void) dp;
	return 0;
}

Label *ReactU32DisplayCreate(Element *parent, int flags, const char *key, const char *prefix) {
	char buffer[64];
	snprintf(buffer, sizeof(buffer), "%s%d", prefix, ObjectReadU32(SelectedObject(), key, 0));
	Label *label = LabelCreate(parent, flags, buffer, -1);
	label->e.messageUser = ReactU32DisplayMessage;
	return label;
}

int ReactU32ButtonEnabledMessage(Element *element, Message message, int di, void *dp) {
	(void) element;
	(void) message;
	(void) di;
	(void) dp;
	return 0;
}

int ReactU32ButtonDisabledMessage(Element *element, Message message, int di, void *dp) {
	(void) element;
	(void) di;

	if (message == MSG_BUTTON_GET_COLOR) {
		*(uint32_t *) dp = 0xCCCCCC; 
	}

	return 0;
}

Button *ReactU32ButtonCreate(Element *parent, int flags, const char *key, const char *label, 
		uint32_t minimum, uint32_t maximum, int32_t clickDelta) {
	Button *button = ButtonCreate(parent, flags, label, -1);
	int64_t result = clickDelta + ObjectReadU32(SelectedObject(), key, 0);
	bool disabled = result < minimum || result > maximum;
	button->e.messageUser = disabled ? ReactU32ButtonDisabledMessage : ReactU32ButtonEnabledMessage;
	return button;
}

/////////////////////////////////////////
// Setting up the UI.
/////////////////////////////////////////

void Populate() {
	for (uintptr_t i = 0; i < container->childCount; i++) {
		ElementDestroy(container->children[i]);
	}

	Object *object = SelectedObject();

	if (object->type == OBJECT_COUNTER) {
		Panel *row = PanelCreate(container, PANEL_HORIZONTAL | ELEMENT_H_FILL);
		ReactU32ButtonCreate(&row->e, 0, "count", "-", 0, 10, -1);
		ReactU32DisplayCreate(&row->e, ELEMENT_H_FILL | LABEL_CENTER, "count", "Count: ");
		ReactU32ButtonCreate(&row->e, 0, "count", "+", 0, 10, 1);
	} else {
		// ...
	}

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
	// Create the initial document.
	// You might want to put this into a separate function,
	// so if loading a document from a file fails, you can fall back to this.
	Object objectCounter = { 0 };
	objectCounter.type = OBJECT_COUNTER;
	Property propertyCount = { 0 };
	propertyCount.type = PROPERTY_U32;
	strcpy(propertyCount.key, "count");
	propertyCount.u32 = 10;
	arrput(objectCounter.properties, propertyCount);
	objectIDAllocator++;
	hmput(objects, objectIDAllocator, objectCounter);
	selectedObjectID = objectIDAllocator;

	Initialise();
	Window *window = WindowCreate("Hello, world", 400, 120);
	Panel *panel = PanelCreate(&window->e, PANEL_GRAY);
	panel->border = RectangleMake(10, 10, 10, 10);
	panel->gap = 15;

	container = &PanelCreate(&panel->e, ELEMENT_H_FILL | ELEMENT_V_FILL)->e;
	Populate();
	int result = MessageLoop();
	DocumentFree();
	return result;
}
