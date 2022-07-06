// Add document modification.

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

#define STEP_SET_PROPERTY (1)
// ...

typedef struct Step {
	int type;
	uint64_t objectID; // ... or an array of IDs

	union {
		Property property;
		// ...
	};
} Step;

#define MSG_PROPERTY_CHANGED ((Message) (MSG_USER + 1))

void Populate();
void ReactUpdate(Element *exclude, const char *key);

/////////////////////////////////////////
// Global state.
/////////////////////////////////////////

struct { uint64_t key; Object value; } *objects;
uint64_t objectIDAllocator;

uint64_t selectedObjectID; // ... or an array of IDs
Element *container;
Element **reactElements;

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

void StepFree(Step step) {
	if (step.type == STEP_SET_PROPERTY) {
		PropertyFree(step.property);
	} else {
		// ...
	}
}

void DocumentFree() {
	for (int i = 0; i < hmlen(objects); i++) {
		ObjectFree(objects[i].value);
	}

	hmfree(objects);
	arrfree(reactElements);
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
// Updating the document.
/////////////////////////////////////////

void StepApply(Step step, Element *updateExclude) {
	bool changedSelectedObject = false;

	if (selectedObjectID != step.objectID) {
		selectedObjectID = step.objectID;
		changedSelectedObject = true;
	}

	Object *object = SelectedObject();
	const char *updateKey = NULL;

	if (step.type == STEP_SET_PROPERTY) {
		updateKey = step.property.key;

		bool found = false;

		for (int i = 0; i < arrlen(object->properties); i++) {
			if (0 == strcmp(object->properties[i].key, step.property.key)) {
				object->properties[i] = step.property;
				found = true;
				break;
			}
		}

		if (!found) {
			arrput(object->properties, step.property);
		}
	} else {
		// ...
	}

	if (changedSelectedObject) {
		Populate();
	} else {
		ReactUpdate(updateExclude, updateKey);
	}
}

/////////////////////////////////////////
// Reactive elements.
/////////////////////////////////////////

typedef struct ReactData {
	const char *key;

	union {
		struct {
			const char *prefix;
		} u32Display;

		struct {
			uint32_t minimum, maximum;
			int32_t clickDelta;
		} u32Button;

		// ...
	};
} ReactData;

int ReactU32DisplayMessage(Element *element, Message message, int di, void *dp) {
	Label *label = (Label *) element;
	ReactData *data = (ReactData *) element->cp;
	(void) di;
	(void) dp;

	if (message == MSG_DESTROY) {
		free(data);
	} else if (message == MSG_PROPERTY_CHANGED) {
		char buffer[64];
		snprintf(buffer, sizeof(buffer), "%s%d", data->u32Display.prefix, ObjectReadU32(SelectedObject(), data->key, 0));
		LabelSetContent(label, buffer, -1);
		ElementRepaint(element, NULL);
	}

	return 0;
}

Label *ReactU32DisplayCreate(Element *parent, int flags, const char *key, const char *prefix) {
	ReactData *data = (ReactData *) calloc(1, sizeof(ReactData));
	data->key = key;
	data->u32Display.prefix = prefix;
	Label *label = LabelCreate(parent, flags, NULL, 0);
	label->e.cp = data;
	label->e.messageUser = ReactU32DisplayMessage;
	arrput(reactElements, &label->e);
	return label;
}

int ReactU32ButtonMessage(Element *element, Message message, int di, void *dp) {
	ReactData *data = (ReactData *) element->cp;
	(void) di;
	(void) dp;

	if (message == MSG_DESTROY) {
		free(data);
	} else if (message == MSG_PROPERTY_CHANGED) {
		ElementRepaint(element, NULL);
	} else if (message == MSG_CLICKED) {
		Step step = { 0 };
		step.type = STEP_SET_PROPERTY;
		step.objectID = selectedObjectID;
		step.property.type = PROPERTY_U32;
		strcpy(step.property.key, data->key);
		step.property.u32 = data->u32Button.clickDelta + ObjectReadU32(SelectedObject(), data->key, 0);

		if (step.property.u32 >= data->u32Button.minimum && step.property.u32 <= data->u32Button.maximum) {
			StepApply(step, NULL);
		}
	} else if (message == MSG_BUTTON_GET_COLOR) {
		int64_t result = data->u32Button.clickDelta + ObjectReadU32(SelectedObject(), data->key, 0);
		
		if (result < data->u32Button.minimum || result > data->u32Button.maximum) {
			// Our UI library doesn't have a proper way to disable buttons.
			// But this will do for now!
			*(uint32_t *) dp = 0xCCCCCC; 
		}
	}

	return 0;
}

Button *ReactU32ButtonCreate(Element *parent, int flags, const char *key, const char *label, 
		uint32_t minimum, uint32_t maximum, int32_t clickDelta) {
	ReactData *data = (ReactData *) calloc(1, sizeof(ReactData));
	data->key = key;
	data->u32Button.minimum = minimum;
	data->u32Button.maximum = maximum;
	data->u32Button.clickDelta = clickDelta;
	Button *button = ButtonCreate(parent, flags, label, -1);
	button->e.cp = data;
	button->e.messageUser = ReactU32ButtonMessage;
	arrput(reactElements, &button->e);
	return button;
}

void ReactUpdate(Element *exclude, const char *key) {
	for (int i = 0; i < arrlen(reactElements); i++) {
		ReactData *data = (ReactData *) reactElements[i]->cp;
		if (reactElements[i] == exclude) continue;
		if (key && strcmp(data->key, key)) continue;
		ElementMessage(reactElements[i], MSG_PROPERTY_CHANGED, 0, NULL);
	}
}

/////////////////////////////////////////
// Setting up the UI.
/////////////////////////////////////////

void Populate() {
	arrfree(reactElements);

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

	ReactUpdate(NULL, NULL);
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
	propertyCount.u32 = 5;
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
