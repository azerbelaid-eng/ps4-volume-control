#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define PLUGIN_NAME    "ps4_volume_control"
#define PLUGIN_DESC    "System HDMI Volume Control with DualShock 4"
#define PLUGIN_AUTH    "Antigravity"
#define PLUGIN_VER     0x00010000 // 1.0.0

#define GOLDHEN_PATH   "/data/GoldHEN"
#define CONFIG_PATH    GOLDHEN_PATH "/volume.ini"

// Button bitmasks
#define SCE_PAD_BUTTON_L3          0x00000002
#define SCE_PAD_BUTTON_R3          0x00000004
#define SCE_PAD_BUTTON_OPTIONS     0x00000008
#define SCE_PAD_BUTTON_UP          0x00000010
#define SCE_PAD_BUTTON_RIGHT       0x00000020
#define SCE_PAD_BUTTON_DOWN        0x00000040
#define SCE_PAD_BUTTON_LEFT        0x00000080
#define SCE_PAD_BUTTON_L2          0x00000100
#define SCE_PAD_BUTTON_R2          0x00000200
#define SCE_PAD_BUTTON_L1          0x00000400
#define SCE_PAD_BUTTON_R1          0x00000800
#define SCE_PAD_BUTTON_TRIANGLE    0x00001000
#define SCE_PAD_BUTTON_CIRCLE      0x00002000
#define SCE_PAD_BUTTON_CROSS       0x00004000
#define SCE_PAD_BUTTON_SQUARE      0x00008000
#define SCE_PAD_BUTTON_TOUCH_PAD   0x00100000

// Audio format definitions (param & 0xff from sceAudioOutOpen)
#define FMT_S16_MONO               0
#define FMT_S16_STEREO             1
#define FMT_S16_8CH                2
#define FMT_FLOAT_MONO             3
#define FMT_FLOAT_STEREO           4
#define FMT_FLOAT_8CH              5
#define FMT_S16_8CH_STD            6
#define FMT_FLOAT_8CH_STD          7

// Maximum audio ports to track
#define MAX_AUDIO_PORTS            16

typedef struct {
    int32_t handle;
    uint32_t frames;     // Grains per channel (e.g. 256, 512, 1024)
    uint32_t fmt;        // Format id (0..7)
    uint32_t channels;   // 1, 2, or 8
    bool is_float;       // true if float, false if 16-bit integer
    bool active;
} AudioPortInfo;

typedef struct {
    uint32_t buttons;
    uint8_t leftStickX;
    uint8_t leftStickY;
    uint8_t rightStickX;
    uint8_t rightStickY;
    uint8_t padding[124];
} ScePadDataSimple;

typedef struct {
    int32_t handle;
    const void *ptr;
} OrbisAudioOutOutputParam;

typedef struct {
    int32_t type;
    int32_t reqId;
    int32_t priority;
    int32_t msgId;
    int32_t targetId;
    int32_t userId;
    int32_t unk1;
    int32_t unk2;
    int32_t appId;
    int32_t errorNum;
    int32_t unk3;
    uint8_t useIconImageUri;
    char message[1024];
    char iconUri[1024];
    char packageDigest[32];
    int32_t reserved[4];
} OrbisNotificationRequest;

#ifdef __cplusplus
extern "C" {
#endif

int sceKernelSendNotificationRequest(int device, const void *req, size_t size, int blocking);
uint64_t sceKernelGetProcessTime(void);

#ifdef __cplusplus
}
#endif
