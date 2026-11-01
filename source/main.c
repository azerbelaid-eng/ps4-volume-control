#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

#include <Common.h>
#include "volume_control.h"

// Plugin metadata required by GoldHEN
attr_public const char *g_pluginName = PLUGIN_NAME;
attr_public const char *g_pluginDesc = PLUGIN_DESC;
attr_public const char *g_pluginAuth = PLUGIN_AUTH;
attr_public uint32_t g_pluginVersion = PLUGIN_VER;

// Current volume state (0 to 100)
static int32_t g_volume = 100;
static bool g_is_muted = false;
static int32_t g_unmute_volume = 100;
static uint64_t g_last_btn_time = 0;

// Port registry for active AudioOut handles
static AudioPortInfo g_ports[MAX_AUDIO_PORTS];

// Function pointers & Detour hooks
HOOK_INIT(scePadRead);
HOOK_INIT(scePadReadState);
HOOK_INIT(sceAudioOutOpen);
HOOK_INIT(sceAudioOutClose);
HOOK_INIT(sceAudioOutOutput);
HOOK_INIT(sceAudioOutOutputs);

// Track hook installation status
static bool g_pad_hooked = false;
static bool g_audio_hooked = false;

// ----------------------------------------------------------------------------
// Configuration Persistence (/data/GoldHEN/volume.ini)
// ----------------------------------------------------------------------------
static void load_config(void) {
    FILE *fp = fopen(CONFIG_PATH, "r");
    if (!fp) return;

    char line[128];
    while (fgets(line, sizeof(line), fp)) {
        int vol = 0;
        if (sscanf(line, "volume=%d", &vol) == 1) {
            if (vol >= 0 && vol <= 100) {
                g_volume = vol;
                g_unmute_volume = (vol > 0) ? vol : 50;
            }
        }
        int muted = 0;
        if (sscanf(line, "muted=%d", &muted) == 1) {
            g_is_muted = (muted != 0);
        }
    }
    fclose(fp);
}

static void save_config(void) {
    FILE *fp = fopen(CONFIG_PATH, "w");
    if (!fp) return;
    fprintf(fp, "[volume]\nvolume=%d\nmuted=%d\n", g_volume, g_is_muted ? 1 : 0);
    fclose(fp);
}

// ----------------------------------------------------------------------------
// On-Screen System Notification
// ----------------------------------------------------------------------------
static void show_volume_notification(void) {
    char bar[11] = "..........";
    int filled = (g_volume + 5) / 10;
    if (filled > 10) filled = 10;
    if (filled < 0) filled = 0;
    for (int i = 0; i < filled; i++) {
        bar[i] = '|';
    }

    OrbisNotificationRequest req;
    memset(&req, 0, sizeof(req));
    req.type = 0;
    req.targetId = -1;
    req.useIconImageUri = 1;
    strcpy(req.iconUri, "cxml://psnotification/tex_icon_system");

    if (g_is_muted) {
        snprintf(req.message, sizeof(req.message), "Volume: [ MUTED ]");
    } else {
        snprintf(req.message, sizeof(req.message), "Volume: [%s] %d%%", bar, g_volume);
    }

    sceKernelSendNotificationRequest(0, &req, sizeof(req), 0);
}

// ----------------------------------------------------------------------------
// Gamepad Button Processing
// ----------------------------------------------------------------------------
static void process_pad_input(ScePadDataSimple *data) {
    if (!data) return;

    uint32_t btns = data->buttons;

    // Check if L3 modifier is held down
    if (btns & SCE_PAD_BUTTON_L3) {
        uint64_t now = sceKernelGetProcessTime();
        // Debounce: 140ms delay between volume steps
        if (now - g_last_btn_time >= 140000) {
            bool changed = false;

            // L3 + D-Pad Up: Volume UP
            if (btns & SCE_PAD_BUTTON_UP) {
                if (g_is_muted) {
                    g_is_muted = false;
                }
                if (g_volume < 100) {
                    g_volume += 5;
                    if (g_volume > 100) g_volume = 100;
                }
                g_unmute_volume = g_volume;
                changed = true;
            }
            // L3 + D-Pad Down: Volume DOWN
            else if (btns & SCE_PAD_BUTTON_DOWN) {
                if (g_is_muted) {
                    g_is_muted = false;
                }
                if (g_volume > 0) {
                    g_volume -= 5;
                    if (g_volume < 0) g_volume = 0;
                }
                g_unmute_volume = g_volume;
                changed = true;
            }
            // L3 + D-Pad Left: Toggle Mute
            else if (btns & SCE_PAD_BUTTON_LEFT) {
                g_is_muted = !g_is_muted;
                if (!g_is_muted && g_volume == 0) {
                    g_volume = (g_unmute_volume > 0) ? g_unmute_volume : 50;
                }
                changed = true;
            }
            // L3 + D-Pad Right: Reset to 100%
            else if (btns & SCE_PAD_BUTTON_RIGHT) {
                g_volume = 100;
                g_is_muted = false;
                g_unmute_volume = 100;
                changed = true;
            }

            if (changed) {
                g_last_btn_time = now;
                show_volume_notification();
                save_config();
            }
        }

        // Mask out D-Pad inputs so in-game menus/actions don't trigger while adjusting volume
        data->buttons &= ~(SCE_PAD_BUTTON_UP | SCE_PAD_BUTTON_DOWN | SCE_PAD_BUTTON_LEFT | SCE_PAD_BUTTON_RIGHT);
    }
}

// ----------------------------------------------------------------------------
// Gamepad Hooks (scePadRead / scePadReadState)
// ----------------------------------------------------------------------------
static int32_t scePadRead_hook(int32_t handle, ScePadDataSimple *data, int32_t count) {
    int32_t ret = HOOK_CONTINUE(scePadRead, int32_t (*)(int32_t, ScePadDataSimple *, int32_t), handle, data, count);
    if (ret > 0 && data) {
        for (int32_t i = 0; i < ret; i++) {
            process_pad_input(&data[i]);
        }
    }
    return ret;
}

static int32_t scePadReadState_hook(int32_t handle, ScePadDataSimple *data) {
    int32_t ret = HOOK_CONTINUE(scePadReadState, int32_t (*)(int32_t, ScePadDataSimple *), handle, data);
    if (ret == 0 && data) {
        process_pad_input(data);
    }
    return ret;
}

// ----------------------------------------------------------------------------
// Audio Port Registry Helpers
// ----------------------------------------------------------------------------
static AudioPortInfo* get_port_info(int32_t handle) {
    for (int i = 0; i < MAX_AUDIO_PORTS; i++) {
        if (g_ports[i].active && g_ports[i].handle == handle) {
            return &g_ports[i];
        }
    }
    return NULL;
}

static void register_port(int32_t handle, uint32_t frames, uint32_t param) {
    uint32_t fmt = param & 0xff;
    uint32_t channels = 2;
    bool is_float = false;

    switch (fmt) {
        case FMT_S16_MONO:
            channels = 1; is_float = false; break;
        case FMT_S16_STEREO:
            channels = 2; is_float = false; break;
        case FMT_S16_8CH:
        case FMT_S16_8CH_STD:
            channels = 8; is_float = false; break;
        case FMT_FLOAT_MONO:
            channels = 1; is_float = true; break;
        case FMT_FLOAT_STEREO:
            channels = 2; is_float = true; break;
        case FMT_FLOAT_8CH:
        case FMT_FLOAT_8CH_STD:
            channels = 8; is_float = true; break;
        default:
            channels = 2; is_float = false; break;
    }

    // Reuse existing or find free slot
    for (int i = 0; i < MAX_AUDIO_PORTS; i++) {
        if (!g_ports[i].active || g_ports[i].handle == handle) {
            g_ports[i].handle = handle;
            g_ports[i].frames = (frames > 0) ? frames : 256;
            g_ports[i].fmt = fmt;
            g_ports[i].channels = channels;
            g_ports[i].is_float = is_float;
            g_ports[i].active = true;
            return;
        }
    }
}

static void unregister_port(int32_t handle) {
    for (int i = 0; i < MAX_AUDIO_PORTS; i++) {
        if (g_ports[i].active && g_ports[i].handle == handle) {
            g_ports[i].active = false;
            g_ports[i].handle = -1;
            return;
        }
    }
}

// ----------------------------------------------------------------------------
// Audio Buffer Scaling (In-Memory PCM Attenuation)
// ----------------------------------------------------------------------------
static void scale_audio_buffer(int32_t handle, const void *ptr) {
    if (!ptr) return;

    // If 100% volume and unmuted, no modifications needed
    if (!g_is_muted && g_volume >= 100) return;

    AudioPortInfo *port = get_port_info(handle);
    uint32_t frames = port ? port->frames : 256;
    uint32_t channels = port ? port->channels : 2;
    bool is_float = port ? port->is_float : false;
    size_t sample_count = (size_t)frames * channels;

    // Muted or volume is 0: zero out buffer for absolute silence
    if (g_is_muted || g_volume <= 0) {
        size_t bytes = sample_count * (is_float ? sizeof(float) : sizeof(int16_t));
        memset((void *)ptr, 0, bytes);
        return;
    }

    // Attenuate samples proportionally
    float factor = (float)g_volume / 100.0f;
    if (is_float) {
        float *samples = (float *)ptr;
        for (size_t i = 0; i < sample_count; i++) {
            samples[i] *= factor;
        }
    } else {
        int16_t *samples = (int16_t *)ptr;
        for (size_t i = 0; i < sample_count; i++) {
            samples[i] = (int16_t)(samples[i] * factor);
        }
    }
}

// ----------------------------------------------------------------------------
// Audio Output Hooks
// ----------------------------------------------------------------------------
static int32_t sceAudioOutOpen_hook(int32_t userId, int32_t type, int32_t index, uint32_t len, uint32_t freq, uint32_t param) {
    int32_t handle = HOOK_CONTINUE(sceAudioOutOpen,
                                   int32_t (*)(int32_t, int32_t, int32_t, uint32_t, uint32_t, uint32_t),
                                   userId, type, index, len, freq, param);
    if (handle >= 0) {
        register_port(handle, len, param);
    }
    return handle;
}

static int32_t sceAudioOutClose_hook(int32_t handle) {
    unregister_port(handle);
    return HOOK_CONTINUE(sceAudioOutClose, int32_t (*)(int32_t), handle);
}

static int32_t sceAudioOutOutput_hook(int32_t handle, const void *ptr) {
    scale_audio_buffer(handle, ptr);
    return HOOK_CONTINUE(sceAudioOutOutput, int32_t (*)(int32_t, const void *), handle, ptr);
}

static int32_t sceAudioOutOutputs_hook(OrbisAudioOutOutputParam *params, uint32_t num) {
    if (params && num > 0) {
        for (uint32_t i = 0; i < num; i++) {
            scale_audio_buffer(params[i].handle, params[i].ptr);
        }
    }
    return HOOK_CONTINUE(sceAudioOutOutputs, int32_t (*)(OrbisAudioOutOutputParam *, uint32_t), params, num);
}

// ----------------------------------------------------------------------------
// Plugin Load & Unload Lifecycle
// ----------------------------------------------------------------------------
attr_public int32_t plugin_load(int32_t argc, const char *argv[]) {
    klog("[VolumeControl] Starting PS4 Volume Control Plugin...\n");

    // Load saved volume preferences
    load_config();

    // Reset port registry
    memset(g_ports, 0, sizeof(g_ports));

    // 1. Hook libScePad.sprx
    int32_t pad_h = 0;
    if (sys_dynlib_load_prx("libScePad.sprx", &pad_h) == 0 || pad_h > 0) {
        void *p_read = NULL;
        void *p_state = NULL;
        sys_dynlib_dlsym(pad_h, "scePadRead", &p_read);
        sys_dynlib_dlsym(pad_h, "scePadReadState", &p_state);

        if (p_read) {
            Detour_Construct(&Detour_scePadRead, DetourMode_x32);
            Detour_DetourFunction(&Detour_scePadRead, (uint64_t)p_read, (void *)scePadRead_hook);
        }
        if (p_state) {
            Detour_Construct(&Detour_scePadReadState, DetourMode_x32);
            Detour_DetourFunction(&Detour_scePadReadState, (uint64_t)p_state, (void *)scePadReadState_hook);
        }
        g_pad_hooked = true;
        klog("[VolumeControl] ScePad hooks installed successfully.\n");
    }

    // 2. Hook libSceAudioOut.sprx
    int32_t audio_h = 0;
    if (sys_dynlib_load_prx("libSceAudioOut.sprx", &audio_h) == 0 || audio_h > 0) {
        void *p_open = NULL;
        void *p_close = NULL;
        void *p_output = NULL;
        void *p_outputs = NULL;

        sys_dynlib_dlsym(audio_h, "sceAudioOutOpen", &p_open);
        sys_dynlib_dlsym(audio_h, "sceAudioOutClose", &p_close);
        sys_dynlib_dlsym(audio_h, "sceAudioOutOutput", &p_output);
        sys_dynlib_dlsym(audio_h, "sceAudioOutOutputs", &p_outputs);

        if (p_open) {
            Detour_Construct(&Detour_sceAudioOutOpen, DetourMode_x32);
            Detour_DetourFunction(&Detour_sceAudioOutOpen, (uint64_t)p_open, (void *)sceAudioOutOpen_hook);
        }
        if (p_close) {
            Detour_Construct(&Detour_sceAudioOutClose, DetourMode_x32);
            Detour_DetourFunction(&Detour_sceAudioOutClose, (uint64_t)p_close, (void *)sceAudioOutClose_hook);
        }
        if (p_output) {
            Detour_Construct(&Detour_sceAudioOutOutput, DetourMode_x32);
            Detour_DetourFunction(&Detour_sceAudioOutOutput, (uint64_t)p_output, (void *)sceAudioOutOutput_hook);
        }
        if (p_outputs) {
            Detour_Construct(&Detour_sceAudioOutOutputs, DetourMode_x32);
            Detour_DetourFunction(&Detour_sceAudioOutOutputs, (uint64_t)p_outputs, (void *)sceAudioOutOutputs_hook);
        }
        g_audio_hooked = true;
        klog("[VolumeControl] SceAudioOut hooks installed successfully.\n");
    }

    klog("[VolumeControl] Plugin loaded (Initial Volume: %d%%, Muted: %d)\n", g_volume, g_is_muted ? 1 : 0);
    return 0;
}

attr_public int32_t plugin_unload(int32_t argc, const char *argv[]) {
    klog("[VolumeControl] Unloading Volume Control Plugin...\n");

    if (g_pad_hooked) {
        UNHOOK(scePadRead);
        UNHOOK(scePadReadState);
        g_pad_hooked = false;
    }

    if (g_audio_hooked) {
        UNHOOK(sceAudioOutOpen);
        UNHOOK(sceAudioOutClose);
        UNHOOK(sceAudioOutOutput);
        UNHOOK(sceAudioOutOutputs);
        g_audio_hooked = false;
    }

    save_config();
    return 0;
}

attr_module_hidden int32_t module_start(int64_t argc, const void *args) {
    return 0;
}

attr_module_hidden int32_t module_stop(int64_t argc, const void *args) {
    return 0;
}

