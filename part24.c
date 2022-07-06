// Add serialization.

#define STB_DS_IMPLEMENTATION
#include "stb_ds.h"
#include <stdint.h>
#include <stdio.h>
#include "ui_library_tutorial.c"

// Our UI library doesn't provide a nice way to do this.
// It wouldn't be that difficult to add one though.
// Have a look at what Luigi does!
#define BUTTON_HANDLE_CLICK_PROLOGUE(name) \
	int name(Element *element, Message message, int di, void *dp) { \
		(void) element; \
		(void) di; \
		(void) dp; \
		if (message == MSG_CLICKED) {
#define BUTTON_HANDLE_CLICK_EPILOGUE() \
	} \
	return 0; \
}

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

#define STEP_APPLY_NORMAL (0)
#define STEP_APPLY_UNDO   (1)
#define STEP_APPLY_REDO   (2)

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
Step *undoStack, *redoStack;

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

	for (int i = 0; i < arrlen(undoStack); i++) {
		StepFree(undoStack[i]);
	}

	for (int i = 0; i < arrlen(redoStack); i++) {
		StepFree(redoStack[i]);
	}

	hmfree(objects);
	arrfree(undoStack);
	arrfree(redoStack);
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

void StepApply(Step step, Element *updateExclude, int mode) {
	bool changedSelectedObject = false;

	if (selectedObjectID != step.objectID) {
		selectedObjectID = step.objectID;
		changedSelectedObject = true;
	}

	Object *object = SelectedObject();
	Step reverse = { 0 };
	const char *updateKey = NULL;

	if (step.type == STEP_SET_PROPERTY) {
		reverse = step;
		reverse.property = ObjectReadAny(object, step.property.key);
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

	if (mode == STEP_APPLY_NORMAL) {
		for (int i = 0; i < arrlen(redoStack); i++) StepFree(redoStack[i]);
		arrfree(redoStack);
		arrput(undoStack, reverse);
	} else if (mode == STEP_APPLY_UNDO) {
		arrput(redoStack, reverse);
	} else if (mode == STEP_APPLY_REDO) {
		arrput(undoStack, reverse);
	}

	if (changedSelectedObject) {
		Populate();
	} else {
		ReactUpdate(updateExclude, updateKey);
	}
}

BUTTON_HANDLE_CLICK_PROLOGUE(ButtonUndoMessage) {
	if (arrlen(undoStack)) {
		StepApply(arrpop(undoStack), NULL, STEP_APPLY_UNDO);
	}
} BUTTON_HANDLE_CLICK_EPILOGUE()

BUTTON_HANDLE_CLICK_PROLOGUE(ButtonRedoMessage) {
	if (arrlen(redoStack)) {
		StepApply(arrpop(redoStack), NULL, STEP_APPLY_REDO);
	}
} BUTTON_HANDLE_CLICK_EPILOGUE()

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
			StepApply(step, NULL, STEP_APPLY_NORMAL);
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
// Serialization.
/////////////////////////////////////////

BUTTON_HANDLE_CLICK_PROLOGUE(ButtonSaveMessage) {
	// TODO Signature, checksum, etc.
	// TODO Output as JSON?
	// TODO Save the undo stack?

	FILE *f = fopen("tutorial.dat", "wb");

	fwrite(&objectIDAllocator, 1, sizeof(uint64_t), f);
	uint32_t objectCount = hmlenu(objects);
	fwrite(&objectCount, 1, sizeof(uint32_t), f);

	for (int i = 0; i < hmlen(objects); i++) {
		fwrite(&objects[i].key, 1, sizeof(uint64_t), f);
		uint32_t type = objects[i].value.type;
		fwrite(&type, 1, sizeof(uint32_t), f);
		uint32_t propertyCount = arrlen(objects[i].value.properties);
		fwrite(&propertyCount, 1, sizeof(uint32_t), f);

		for (uint32_t j = 0; j < propertyCount; j++) {
			fwrite(&objects[i].value.properties[j].type, 1, sizeof(uint8_t), f);
			fwrite(&objects[i].value.properties[j].key, 1, PROPERTY_KEY_MAX_SIZE + 1, f);

			if (objects[i].value.properties[j].type == PROPERTY_U32) {
				fwrite(&objects[i].value.properties[j].u32, 1, sizeof(uint32_t), f);
			} else {
				// ...
			}
		}
	}

	fclose(f);
} BUTTON_HANDLE_CLICK_EPILOGUE()

BUTTON_HANDLE_CLICK_PROLOGUE(ButtonLoadMessage) {
	// TODO Handle IO errors, out of memory errors and such.
		
	FILE *f = fopen("tutorial.dat", "rb");

	if (!f) {
		return 0;
	}

	DocumentFree();

	fread(&objectIDAllocator, 1, sizeof(uint64_t), f);
	uint32_t objectCount = 0;
	fread(&objectCount, 1, sizeof(uint32_t), f);

	for (uint32_t i = 0; i < objectCount; i++) {
		Object object = {};
		uint64_t id = 0;
		fread(&id, 1, sizeof(uint64_t), f);
		uint32_t type = 0;
		fread(&type, 1, sizeof(uint32_t), f);
		uint32_t propertyCount = 0;
		fread(&propertyCount, 1, sizeof(propertyCount), f);
		object.type = type;

		if (objectIDAllocator < id) {
			objectIDAllocator = id;
		}

		for (uint32_t j = 0; j < propertyCount; j++) {
			Property property = {};
			fread(&property.type, 1, sizeof(property.type), f);
			fread(&property.key, 1, PROPERTY_KEY_MAX_SIZE + 1, f);
			property.key[PROPERTY_KEY_MAX_SIZE] = 0;

			if (property.type == PROPERTY_U32) {
				fread(&property.u32, 1, sizeof(uint32_t), f);
			} else {
				// ...
			}

			arrput(object.properties, property);
		}

		hmput(objects, id, object);

		if (type == OBJECT_COUNTER) {
			selectedObjectID = id;
		}
	}

	fclose(f);
	Populate();
} BUTTON_HANDLE_CLICK_EPILOGUE()

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

	// These would typically be keyboard shortcuts and menu items.
	// But we don't support those in our UI library yet.
	Panel *row = PanelCreate(&panel->e, ELEMENT_H_FILL | PANEL_HORIZONTAL);
	LabelCreate(&row->e, 0, "Controls: ", -1);
	ButtonCreate(&row->e, 0, "Undo", -1)->e.messageUser = ButtonUndoMessage;
	ButtonCreate(&row->e, 0, "Redo", -1)->e.messageUser = ButtonRedoMessage;
	ButtonCreate(&row->e, 0, "Save", -1)->e.messageUser = ButtonSaveMessage;
	ButtonCreate(&row->e, 0, "Load", -1)->e.messageUser = ButtonLoadMessage;

	container = &PanelCreate(&panel->e, ELEMENT_H_FILL | ELEMENT_V_FILL)->e;
	Populate();
	int result = MessageLoop();
	DocumentFree();
	return result;
}
