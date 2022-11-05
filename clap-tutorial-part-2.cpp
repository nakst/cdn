#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <math.h>
#include "clap/clap.h"

template <class T>
struct Array {
	T *array;
	size_t length, allocated;

	void Insert(T newItem, uintptr_t index) {
		if (length + 1 > allocated) {
			allocated *= 2;
			if (length + 1 > allocated) allocated = length + 1;
			array = (T *) realloc(array, allocated * sizeof(T));
		}

		length++;
		memmove(array + index + 1, array + index, (length - index - 1) * sizeof(T));
		array[index] = newItem;
	}

	void Delete(uintptr_t index) { 
		memmove(array + index, array + index + 1, (length - index - 1) * sizeof(T)); 
		length--;
	}

	void Add(T item) { Insert(item, length); }
	void Free() { free(array); array = nullptr; length = allocated = 0; }
	int Length() { return length; }
	T &operator[](uintptr_t index) { assert(index < length); return array[index]; }
};

#ifdef _WIN32
#include <windows.h>
typedef HANDLE Mutex;
#define MutexAcquire(mutex) WaitForSingleObject(mutex, INFINITE)
#define MutexRelease(mutex) ReleaseMutex(mutex)
#define MutexInitialise(mutex) (mutex = CreateMutex(nullptr, FALSE, nullptr))
#define MutexDestroy(mutex) CloseHandle(mutex)
#else
#include <pthread.h>
typedef pthread_mutex_t Mutex;
#define MutexAcquire(mutex) pthread_mutex_lock(&(mutex))
#define MutexRelease(mutex) pthread_mutex_unlock(&(mutex))
#define MutexInitialise(mutex) pthread_mutex_init(&(mutex), nullptr)
#define MutexDestroy(mutex) pthread_mutex_destroy(&(mutex))
#endif

// Parameters.
#define P_VOLUME (0)
#define P_COUNT (1)

struct Voice {
	bool held;
	int32_t noteID;
	int16_t channel, key;

	float phase;
	float parameterOffsets[P_COUNT];
};

struct MyPlugin {
	clap_plugin_t plugin;
	const clap_host_t *host;
	float sampleRate;
	Array<Voice> voices;
	float parameters[P_COUNT], mainParameters[P_COUNT];
	bool changed[P_COUNT], mainChanged[P_COUNT];
	Mutex syncParameters;
};

static float FloatClamp01(float x) {
	return x >= 1.0f ? 1.0f : x <= 0.0f ? 0.0f : x;
}

static void PluginProcessEvent(MyPlugin *plugin, const clap_event_header_t *event) {
	if (event->space_id == CLAP_CORE_EVENT_SPACE_ID) {
		if (event->type == CLAP_EVENT_NOTE_ON || event->type == CLAP_EVENT_NOTE_OFF || event->type == CLAP_EVENT_NOTE_CHOKE) {
			const clap_event_note_t *noteEvent = (const clap_event_note_t *) event;

			for (int i = 0; i < plugin->voices.Length(); i++) {
				Voice *voice = &plugin->voices[i];

				if ((noteEvent->key == -1 || voice->key == noteEvent->key)
						&& (noteEvent->note_id == -1 || voice->noteID == noteEvent->note_id)
						&& (noteEvent->channel == -1 || voice->channel == noteEvent->channel)) {
					if (event->type == CLAP_EVENT_NOTE_CHOKE) {
						plugin->voices.Delete(i--);
					} else {
						voice->held = false;
					}
				}
			}

			if (event->type == CLAP_EVENT_NOTE_ON) {
				Voice voice = { 
					.held = true, 
					.noteID = noteEvent->note_id, 
					.channel = noteEvent->channel, 
					.key = noteEvent->key,
					.phase = 0.0f,
					.parameterOffsets = {},
				};

				plugin->voices.Add(voice);
			}
		} else if (event->type == CLAP_EVENT_PARAM_VALUE) {
			const clap_event_param_value_t *valueEvent = (const clap_event_param_value_t *) event;
			uint32_t i = (uint32_t) valueEvent->param_id;
			MutexAcquire(plugin->syncParameters);
			plugin->parameters[i] = valueEvent->value;
			plugin->changed[i] = true;
			MutexRelease(plugin->syncParameters);
		} else if (event->type == CLAP_EVENT_PARAM_MOD) {
			const clap_event_param_mod_t *modEvent = (const clap_event_param_mod_t *) event;

			for (int i = 0; i < plugin->voices.Length(); i++) {
				Voice *voice = &plugin->voices[i];

				if ((modEvent->key == -1 || voice->key == modEvent->key)
						&& (modEvent->note_id == -1 || voice->noteID == modEvent->note_id)
						&& (modEvent->channel == -1 || voice->channel == modEvent->channel)) {
					voice->parameterOffsets[modEvent->param_id] = modEvent->amount;
					break;
				}
			}
		}
	}
}

static void PluginRenderAudio(MyPlugin *plugin, uint32_t start, uint32_t end, float *outputL, float *outputR) {
	for (uint32_t index = start; index < end; index++) {
		float sum = 0.0f;

		for (int i = 0; i < plugin->voices.Length(); i++) {
			Voice *voice = &plugin->voices[i];
			if (!voice->held) continue;
			float volume = FloatClamp01(plugin->parameters[P_VOLUME] + voice->parameterOffsets[P_VOLUME]);
			sum += sinf(voice->phase * 2.0f * 3.14159f) * 0.2f * volume;
			voice->phase += 440.0f * exp2f((voice->key - 57.0f) / 12.0f) / plugin->sampleRate;
			voice->phase -= floorf(voice->phase);
		}

		outputL[index] = sum;
		outputR[index] = sum;
	}
}

static void PluginSyncMainToAudio(MyPlugin *plugin, const clap_output_events_t *out) {
	MutexAcquire(plugin->syncParameters);

	for (uint32_t i = 0; i < P_COUNT; i++) {
		if (plugin->mainChanged[i]) {
			plugin->parameters[i] = plugin->mainParameters[i];
			plugin->mainChanged[i] = false;

			clap_event_param_value_t event = {};
			event.header.size = sizeof(event);
			event.header.time = 0;
			event.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
			event.header.type = CLAP_EVENT_PARAM_VALUE;
			event.header.flags = 0;
			event.param_id = i;
			event.cookie = NULL;
			event.note_id = -1;
			event.port_index = -1;
			event.channel = -1;
			event.key = -1;
			event.value = plugin->parameters[i];
			out->try_push(out, &event.header);
		}
	}

	MutexRelease(plugin->syncParameters);
}

static bool PluginSyncAudioToMain(MyPlugin *plugin) {
	bool anyChanged = false;
	MutexAcquire(plugin->syncParameters);

	for (uint32_t i = 0; i < P_COUNT; i++) {
		if (plugin->changed[i]) {
			plugin->mainParameters[i] = plugin->parameters[i];
			plugin->changed[i] = false;
			anyChanged = true;
		}
	}

	MutexRelease(plugin->syncParameters);
	return anyChanged;
}

static const clap_plugin_descriptor_t pluginDescriptor = {
	.clap_version = CLAP_VERSION_INIT,
	.id = "nakst.HelloCLAP",
	.name = "HelloCLAP",
	.vendor = "nakst",
	.url = "https://nakst.gitlab.io",
	.manual_url = "https://nakst.gitlab.io",
	.support_url = "https://nakst.gitlab.io",
	.version = "1.0.0",
	.description = "The best audio plugin ever.",

	.features = (const char *[]) {
		CLAP_PLUGIN_FEATURE_INSTRUMENT,
		CLAP_PLUGIN_FEATURE_SYNTHESIZER,
		CLAP_PLUGIN_FEATURE_STEREO,
		NULL,
	},
};

static const clap_plugin_note_ports_t extensionNotePorts = {
	.count = [] (const clap_plugin_t *plugin, bool isInput) -> uint32_t {
		return isInput ? 1 : 0;
	},

	.get = [] (const clap_plugin_t *plugin, uint32_t index, bool isInput, clap_note_port_info_t *info) -> bool {
		if (!isInput || index) return false;
		info->id = 0;
		info->supported_dialects = CLAP_NOTE_DIALECT_CLAP;
		info->preferred_dialect = CLAP_NOTE_DIALECT_CLAP;
		snprintf(info->name, sizeof(info->name), "%s", "Note Port");
		return true;
	},
};

static const clap_plugin_audio_ports_t extensionAudioPorts = {
	.count = [] (const clap_plugin_t *plugin, bool isInput) -> uint32_t { 
		return isInput ? 0 : 1; 
	},

	.get = [] (const clap_plugin_t *plugin, uint32_t index, bool isInput, clap_audio_port_info_t *info) -> bool {
		if (isInput || index) return false;
		info->id = 0;
		info->channel_count = 2;
		info->flags = CLAP_AUDIO_PORT_IS_MAIN;
		info->port_type = CLAP_PORT_STEREO;
		info->in_place_pair = CLAP_INVALID_ID;
		snprintf(info->name, sizeof(info->name), "%s", "Audio Output");
		return true;
	},
};

static const clap_plugin_params_t extensionParams = {
	.count = [] (const clap_plugin_t *plugin) -> uint32_t {
		return P_COUNT;
	},

	.get_info = [] (const clap_plugin_t *_plugin, uint32_t index, clap_param_info_t *information) -> bool {
		if (index == P_VOLUME) {
			memset(information, 0, sizeof(clap_param_info_t));
			information->id = index;
			information->flags = CLAP_PARAM_IS_AUTOMATABLE | CLAP_PARAM_IS_MODULATABLE | CLAP_PARAM_IS_MODULATABLE_PER_NOTE_ID;
			information->min_value = 0.0f;
			information->max_value = 1.0f;
			information->default_value = 0.5f;
			strcpy(information->name, "Volume");
			return true;
		} else {
			return false;
		}
	},

	.get_value = [] (const clap_plugin_t *_plugin, clap_id id, double *value) -> bool {
		MyPlugin *plugin = (MyPlugin *) _plugin->plugin_data;
		uint32_t i = (uint32_t) id;
		if (i >= P_COUNT) return false;
		MutexAcquire(plugin->syncParameters);
		*value = plugin->mainChanged[i] ? plugin->mainParameters[i] : plugin->parameters[i];
		MutexRelease(plugin->syncParameters);
		return true;
	},

	.value_to_text = [] (const clap_plugin_t *_plugin, clap_id id, double value, char *display, uint32_t size) {
		uint32_t i = (uint32_t) id;
		if (i >= P_COUNT) return false;
		snprintf(display, size, "%f", value);
		return true;
	},

	.text_to_value = [] (const clap_plugin_t *_plugin, clap_id param_id, const char *display, double *value) {
		// TODO Implement this.
		return false;
	},

	.flush = [] (const clap_plugin_t *_plugin, const clap_input_events_t *in, const clap_output_events_t *out) {
		MyPlugin *plugin = (MyPlugin *) _plugin->plugin_data;
		const uint32_t eventCount = in->size(in);
		PluginSyncMainToAudio(plugin, out);

		for (uint32_t eventIndex = 0; eventIndex < eventCount; eventIndex++) {
			PluginProcessEvent(plugin, in->get(in, eventIndex));
		}
	},
};

static const clap_plugin_state_t extensionState = {
	.save = [] (const clap_plugin_t *_plugin, const clap_ostream_t *stream) -> bool {
		MyPlugin *plugin = (MyPlugin *) _plugin->plugin_data;
		PluginSyncAudioToMain(plugin);
		return sizeof(float) * P_COUNT == stream->write(stream, plugin->mainParameters, sizeof(float) * P_COUNT);
	},

	.load = [] (const clap_plugin_t *_plugin, const clap_istream_t *stream) -> bool {
		MyPlugin *plugin = (MyPlugin *) _plugin->plugin_data;
		MutexAcquire(plugin->syncParameters);
		bool success = sizeof(float) * P_COUNT == stream->read(stream, plugin->mainParameters, sizeof(float) * P_COUNT);
		for (uint32_t i = 0; i < P_COUNT; i++) plugin->mainChanged[i] = true;
		MutexRelease(plugin->syncParameters);
		return success;
	},
};

static const clap_plugin_t pluginClass = {
	.desc = &pluginDescriptor,
	.plugin_data = nullptr,

	.init = [] (const clap_plugin *_plugin) -> bool {
		MyPlugin *plugin = (MyPlugin *) _plugin->plugin_data;

		MutexInitialise(plugin->syncParameters);

		for (uint32_t i = 0; i < P_COUNT; i++) {
			clap_param_info_t information = {};
			extensionParams.get_info(_plugin, i, &information);
			plugin->mainParameters[i] = plugin->parameters[i] = information.default_value;
		}

		return true;
	},

	.destroy = [] (const clap_plugin *_plugin) {
		MyPlugin *plugin = (MyPlugin *) _plugin->plugin_data;
		plugin->voices.Free();
		MutexDestroy(plugin->syncParameters);
		free(plugin);
	},

	.activate = [] (const clap_plugin *_plugin, double sampleRate, uint32_t minimumFramesCount, uint32_t maximumFramesCount) -> bool {
		MyPlugin *plugin = (MyPlugin *) _plugin->plugin_data;
		plugin->sampleRate = sampleRate;
		return true;
	},

	.deactivate = [] (const clap_plugin *_plugin) {
	},

	.start_processing = [] (const clap_plugin *_plugin) -> bool {
		return true;
	},

	.stop_processing = [] (const clap_plugin *_plugin) {
	},

	.reset = [] (const clap_plugin *_plugin) {
		MyPlugin *plugin = (MyPlugin *) _plugin->plugin_data;
		plugin->voices.Free();
	},

	.process = [] (const clap_plugin *_plugin, const clap_process_t *process) -> clap_process_status {
		MyPlugin *plugin = (MyPlugin *) _plugin->plugin_data;

		assert(process->audio_outputs_count == 1);
		assert(process->audio_inputs_count == 0);

		const uint32_t frameCount = process->frames_count;
		const uint32_t inputEventCount = process->in_events->size(process->in_events);
		uint32_t eventIndex = 0;
		uint32_t nextEventFrame = inputEventCount ? 0 : frameCount;

		PluginSyncMainToAudio(plugin, process->out_events);

		for (uint32_t i = 0; i < frameCount; ) {
			while (eventIndex < inputEventCount && nextEventFrame == i) {
				const clap_event_header_t *event = process->in_events->get(process->in_events, eventIndex);

				if (event->time != i) {
					nextEventFrame = event->time;
					break;
				}

				PluginProcessEvent(plugin, event);
				eventIndex++;

				if (eventIndex == inputEventCount) {
					nextEventFrame = frameCount;
					break;
				}
			}

			PluginRenderAudio(plugin, i, nextEventFrame, process->audio_outputs[0].data32[0], process->audio_outputs[0].data32[1]);
			i = nextEventFrame;
		}

		for (int i = 0; i < plugin->voices.Length(); i++) {
			Voice *voice = &plugin->voices[i];

			if (!voice->held) {
				clap_event_note_t event = {};
				event.header.size = sizeof(event);
				event.header.time = 0;
				event.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
				event.header.type = CLAP_EVENT_NOTE_END;
				event.header.flags = 0;
				event.key = voice->key;
				event.note_id = voice->noteID;
				event.channel = voice->channel;
				event.port_index = 0;
				process->out_events->try_push(process->out_events, &event.header);

				plugin->voices.Delete(i--);
			}
		}

		return CLAP_PROCESS_CONTINUE;
	},

	.get_extension = [] (const clap_plugin *plugin, const char *id) -> const void * {
		if (0 == strcmp(id, CLAP_EXT_NOTE_PORTS )) return &extensionNotePorts;
		if (0 == strcmp(id, CLAP_EXT_AUDIO_PORTS)) return &extensionAudioPorts;
		if (0 == strcmp(id, CLAP_EXT_PARAMS     )) return &extensionParams;
		if (0 == strcmp(id, CLAP_EXT_STATE      )) return &extensionState;
		return nullptr;
	},

	.on_main_thread = [] (const clap_plugin *_plugin) {
	},
};

static const clap_plugin_factory_t pluginFactory = {
	.get_plugin_count = [] (const clap_plugin_factory *factory) -> uint32_t { 
		return 1; 
	},

	.get_plugin_descriptor = [] (const clap_plugin_factory *factory, uint32_t index) -> const clap_plugin_descriptor_t * { 
		return index == 0 ? &pluginDescriptor : nullptr; 
	},

	.create_plugin = [] (const clap_plugin_factory *factory, const clap_host_t *host, const char *pluginID) -> const clap_plugin_t * {
		if (!clap_version_is_compatible(host->clap_version) || strcmp(pluginID, pluginDescriptor.id)) {
			return nullptr;
		}

		MyPlugin *plugin = (MyPlugin *) calloc(1, sizeof(MyPlugin));
		plugin->host = host;
		plugin->plugin = pluginClass;
		plugin->plugin.plugin_data = plugin;
		return &plugin->plugin;
	},
};

extern "C" const clap_plugin_entry_t clap_entry = {
	.clap_version = CLAP_VERSION_INIT,

	.init = [] (const char *path) -> bool { 
		return true; 
	},

	.deinit = [] () {},

	.get_factory = [] (const char *factoryID) -> const void * {
		return strcmp(factoryID, CLAP_PLUGIN_FACTORY_ID) ? nullptr : &pluginFactory;
	},
};
