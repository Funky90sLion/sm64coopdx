// configfile.c - handles loading and saving the configuration options
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>

#include "platform.h"
#include "configfile.h"
#include "cliopts.h"
#include "gfx/gfx_screen_config.h"
#include "gfx/gfx_window_manager_api.h"
#include "controller/controller_api.h"
#include "fs/fs.h"
#include "pc/mods/mods.h"
#include "pc/network/ban_list.h"
#include "pc/crash_handler.h"
#include "pc/network/moderator_list.h"
#include "debuglog.h"

#define ARRAY_LEN(arr) (sizeof(arr) / sizeof(arr[0]))

enum ConfigOptionType {
    CONFIG_TYPE_BOOL,
    CONFIG_TYPE_UINT,
    CONFIG_TYPE_FLOAT,
    CONFIG_TYPE_BIND,
    CONFIG_TYPE_STRING,
    CONFIG_TYPE_U64,
    CONFIG_TYPE_COLOR,
};

struct ConfigOption {
    const char *name;
    enum ConfigOptionType type;
    union {
        bool *boolValue;
        unsigned int *uintValue;
        float* floatValue;
        char* stringValue;
        u64* u64Value;
        u8 (*colorValue)[3];
    };
    int maxStringLength;
};

struct FunctionConfigOption {
    const char *name;
    void (*read)(char**, int);
    void (*write)(FILE*);
};

/*
 *Config options and default values
 */

// Video/audio stuff
ConfigWindow configWindow       = {
    .x = WAPI_WIN_CENTERPOS,
    .y = WAPI_WIN_CENTERPOS,
    .w = DESIRED_SCREEN_WIDTH,
    .h = DESIRED_SCREEN_HEIGHT,
    .vsync = 0,
    .reset = false,
    .fullscreen = false,
    .exiting_fullscreen = false,
    .settings_changed = false,
    .msaa = 0,
};
unsigned int configFiltering    = 2;          // 0=force nearest, 1=linear, 2=three-point
unsigned int configMasterVolume = 80; // 0 - MAX_VOLUME
unsigned int configMusicVolume = MAX_VOLUME;
unsigned int configSfxVolume = MAX_VOLUME;
unsigned int configEnvVolume = MAX_VOLUME;

// Keyboard mappings (VK_ values, by default keyboard/gamepad/mouse)
unsigned int configKeyA[MAX_BINDS]          = { 0x0026,     0x1000,     0x1103     };
unsigned int configKeyB[MAX_BINDS]          = { 0x0033,     0x1001,     0x1101     };
unsigned int configKeyX[MAX_BINDS]          = { 0x0017,     0x1002,     VK_INVALID };
unsigned int configKeyY[MAX_BINDS]          = { 0x0032,     0x1003,     VK_INVALID };
unsigned int configKeyStart[MAX_BINDS]      = { 0x0039,     0x1006,     VK_INVALID };
unsigned int configKeyL[MAX_BINDS]          = { 0x002A,     0x1009,     0x1104     };
unsigned int configKeyR[MAX_BINDS]          = { 0x0036,     0x100A,     0x101B     };
unsigned int configKeyZ[MAX_BINDS]          = { 0x0025,     0x1007,     0x101A     };
unsigned int configKeyCUp[MAX_BINDS]        = { 0x0148,     VK_INVALID, VK_INVALID };
unsigned int configKeyCDown[MAX_BINDS]      = { 0x0150,     VK_INVALID, VK_INVALID };
unsigned int configKeyCLeft[MAX_BINDS]      = { 0x014B,     VK_INVALID, VK_INVALID };
unsigned int configKeyCRight[MAX_BINDS]     = { 0x014D,     VK_INVALID, VK_INVALID };
unsigned int configKeyStickUp[MAX_BINDS]    = { 0x0011,     VK_INVALID, VK_INVALID };
unsigned int configKeyStickDown[MAX_BINDS]  = { 0x001F,     VK_INVALID, VK_INVALID };
unsigned int configKeyStickLeft[MAX_BINDS]  = { 0x001E,     VK_INVALID, VK_INVALID };
unsigned int configKeyStickRight[MAX_BINDS] = { 0x0020,     VK_INVALID, VK_INVALID };
unsigned int configKeyChat[MAX_BINDS]       = { 0x001C,     VK_INVALID, VK_INVALID };
unsigned int configKeyPlayerList[MAX_BINDS] = { 0x000F,     0x1004,     VK_INVALID };
unsigned int configKeyDUp[MAX_BINDS]        = { 0x0147,     0x100b,     VK_INVALID };
unsigned int configKeyDDown[MAX_BINDS]      = { 0x014f,     0x100c,     VK_INVALID };
unsigned int configKeyDLeft[MAX_BINDS]      = { 0x0153,     0x100d,     VK_INVALID };
unsigned int configKeyDRight[MAX_BINDS]     = { 0x0151,     0x100e,     VK_INVALID };
unsigned int configKeyConsole[MAX_BINDS]    = { 0x0029,     0x003B,     VK_INVALID };
unsigned int configKeyPrevPage[MAX_BINDS]   = { 0x0016,     VK_INVALID, VK_INVALID };
unsigned int configKeyNextPage[MAX_BINDS]   = { 0x0018,     VK_INVALID, VK_INVALID };
unsigned int configKeyDisconnect[MAX_BINDS] = { VK_INVALID, VK_INVALID, VK_INVALID };
unsigned int configStickDeadzone = 16; // 16*DEADZONE_STEP=4960 (the original default deadzone)
unsigned int configRumbleStrength = 50;
// better camera settings
unsigned int configCameraXSens   = 50;
unsigned int configCameraYSens   = 50;
unsigned int configCameraAggr    = 0;
unsigned int configCameraPan     = 0;
unsigned int configCameraDegrade = 50; // 0 - 100%
bool         configCameraInvertX = false;
bool         configCameraInvertY = true;
bool         configEnableCamera  = false;
bool         configCameraAnalog  = false;
bool         configCameraCUp     = false;
bool         configCameraMouse   = false;
// coop-specific
bool         configSkipIntro                      = 0;
bool         configBubbleDeath                    = true;
unsigned int configAmountofPlayers                = 16;
char         configJoinIp[MAX_CONFIG_STRING]      = "";
unsigned int configJoinPort                       = DEFAULT_PORT;
unsigned int configHostPort                       = DEFAULT_PORT;
unsigned int configHostSaveSlot                   = 1;
unsigned int configPlayerInteraction              = 1;
unsigned int configPlayerKnockbackStrength        = 25;
unsigned int configStayInLevelAfterStar           = 0;
bool         configNametags                       = true;
unsigned int configBouncyLevelBounds              = 0;
unsigned int configNetworkSystem                  = 0;
char         configPlayerName[MAX_PLAYER_STRING]  = "";
unsigned int configPlayerModel                    = 0;
bool         configMenuStaffRoll                  = true;
unsigned int configMenuLevel                      = 0;
bool         configMenuSound                      = false;
bool         configMenuRandom                     = false;
bool         configMenuDemos                      = false;
struct PlayerPalette configPlayerPalette          = {{{ 0x00, 0x00, 0xff }, { 0xff, 0x00, 0x00 }, { 0xff, 0xff, 0xff }, { 0x72, 0x1c, 0x0e }, { 0x73, 0x06, 0x00 }, { 0xfe, 0xc1, 0x79 }, { 0xff, 0x00, 0x00 }}};
struct PlayerPalette configCustomPalette          = {{{ 0x00, 0x00, 0xff }, { 0xff, 0x00, 0x00 }, { 0xff, 0xff, 0xff }, { 0x72, 0x1c, 0x0e }, { 0x73, 0x06, 0x00 }, { 0xfe, 0xc1, 0x79 }, { 0xff, 0x00, 0x00 }}};
bool         configShowFPS                        = false;
bool         configUncappedFramerate              = false;
unsigned int configFrameLimit                     = 144;
unsigned int configDrawDistance                   = 4;
bool         configDisablePopups                  = false;
bool         configLuaProfiler                    = false;
#ifdef DEVELOPMENT
bool         configCtxProfiler                    = false;
#endif
unsigned int configInterpolationMode              = 1;
unsigned int configGamepadNumber                  = 0;
bool         configBackgroundGamepad              = true;
bool         configDisableGamepads                = false;
bool         configDebugPrint                     = false;
bool         configDebugInfo                      = false;
bool         configDebugError                     = false;
char         configLanguage[MAX_CONFIG_STRING]    = "";
bool         configForce4By3                      = false;
char         configCoopNetIp[MAX_CONFIG_STRING]   = DEFAULT_COOPNET_IP;
unsigned int configCoopNetPort                    = DEFAULT_COOPNET_PORT;
char         configPassword[MAX_CONFIG_STRING]    = "";
char         configDestId[MAX_CONFIG_STRING]      = "0";
bool         configFadeoutDistantSounds           = false;
unsigned int configDjuiTheme                      = DJUI_THEME_DARK;
bool         configDjuiThemeCenter                = true;
unsigned int configDjuiScale                      = 0;
bool         configGlobalPlayerModels             = true;
char         configLastVersion[MAX_CONFIG_STRING] = "";

static const struct ConfigOption options[] = {
    {.name = "fullscreen",                     .type = CONFIG_TYPE_BOOL, .boolValue = &configWindow.fullscreen},
    {.name = "window_x",                       .type = CONFIG_TYPE_UINT, .uintValue = &configWindow.x},
    {.name = "window_y",                       .type = CONFIG_TYPE_UINT, .uintValue = &configWindow.y},
    {.name = "window_w",                       .type = CONFIG_TYPE_UINT, .uintValue = &configWindow.w},
    {.name = "window_h",                       .type = CONFIG_TYPE_UINT, .uintValue = &configWindow.h},
    {.name = "vsync",                          .type = CONFIG_TYPE_BOOL, .boolValue = &configWindow.vsync},
    {.name = "texture_filtering",              .type = CONFIG_TYPE_UINT, .uintValue = &configFiltering},
    {.name = "msaa",                           .type = CONFIG_TYPE_UINT, .uintValue = &configWindow.msaa},
    {.name = "master_volume",                  .type = CONFIG_TYPE_UINT, .uintValue = &configMasterVolume},
    {.name = "music_volume",                   .type = CONFIG_TYPE_UINT, .uintValue = &configMusicVolume},
    {.name = "sfx_volume",                     .type = CONFIG_TYPE_UINT, .uintValue = &configSfxVolume},
    {.name = "env_volume",                     .type = CONFIG_TYPE_UINT, .uintValue = &configEnvVolume},
    {.name = "key_a",                          .type = CONFIG_TYPE_BIND, .uintValue = configKeyA},
    {.name = "key_b",                          .type = CONFIG_TYPE_BIND, .uintValue = configKeyB},
    {.name = "key_x",                          .type = CONFIG_TYPE_BIND, .uintValue = configKeyX},
    {.name = "key_y",                          .type = CONFIG_TYPE_BIND, .uintValue = configKeyY},
    {.name = "key_start",                      .type = CONFIG_TYPE_BIND, .uintValue = configKeyStart},
    {.name = "key_l",                          .type = CONFIG_TYPE_BIND, .uintValue = configKeyL},
    {.name = "key_r",                          .type = CONFIG_TYPE_BIND, .uintValue = configKeyR},
    {.name = "key_z",                          .type = CONFIG_TYPE_BIND, .uintValue = configKeyZ},
    {.name = "key_cup",                        .type = CONFIG_TYPE_BIND, .uintValue = configKeyCUp},
    {.name = "key_cdown",                      .type = CONFIG_TYPE_BIND, .uintValue = configKeyCDown},
    {.name = "key_cleft",                      .type = CONFIG_TYPE_BIND, .uintValue = configKeyCLeft},
    {.name = "key_cright",                     .type = CONFIG_TYPE_BIND, .uintValue = configKeyCRight},
    {.name = "key_stickup",                    .type = CONFIG_TYPE_BIND, .uintValue = configKeyStickUp},
    {.name = "key_stickdown",                  .type = CONFIG_TYPE_BIND, .uintValue = configKeyStickDown},
    {.name = "key_stickleft",                  .type = CONFIG_TYPE_BIND, .uintValue = configKeyStickLeft},
    {.name = "key_stickright",                 .type = CONFIG_TYPE_BIND, .uintValue = configKeyStickRight},
    {.name = "key_chat",                       .type = CONFIG_TYPE_BIND, .uintValue = configKeyChat},
    {.name = "key_playerlist",                 .type = CONFIG_TYPE_BIND, .uintValue = configKeyPlayerList},
    {.name = "key_dup",                        .type = CONFIG_TYPE_BIND, .uintValue = configKeyDUp},
    {.name = "key_ddown",                      .type = CONFIG_TYPE_BIND, .uintValue = configKeyDDown},
    {.name = "key_dleft",                      .type = CONFIG_TYPE_BIND, .uintValue = configKeyDLeft},
    {.name = "key_dright",                     .type = CONFIG_TYPE_BIND, .uintValue = configKeyDRight},
    {.name = "key_prev",                       .type = CONFIG_TYPE_BIND, .uintValue = configKeyPrevPage},
    {.name = "key_next",                       .type = CONFIG_TYPE_BIND, .uintValue = configKeyNextPage},
    {.name = "key_disconnect",                 .type = CONFIG_TYPE_BIND, .uintValue = configKeyDisconnect},
    {.name = "key_console",                    .type = CONFIG_TYPE_BIND, .uintValue = configKeyConsole},
    {.name = "stick_deadzone",                 .type = CONFIG_TYPE_UINT, .uintValue = &configStickDeadzone},
    {.name = "rumble_strength",                .type = CONFIG_TYPE_UINT, .uintValue = &configRumbleStrength},
    {.name = "bettercam_enable",               .type = CONFIG_TYPE_BOOL, .boolValue = &configEnableCamera},
    {.name = "bettercam_analog",               .type = CONFIG_TYPE_BOOL, .boolValue = &configCameraAnalog},
    {.name = "bettercam_cup",                  .type = CONFIG_TYPE_BOOL, .boolValue = &configCameraCUp},
    {.name = "bettercam_mouse_look",           .type = CONFIG_TYPE_BOOL, .boolValue = &configCameraMouse},
    {.name = "bettercam_invertx",              .type = CONFIG_TYPE_BOOL, .boolValue = &configCameraInvertX},
    {.name = "bettercam_inverty",              .type = CONFIG_TYPE_BOOL, .boolValue = &configCameraInvertY},
    {.name = "bettercam_xsens",                .type = CONFIG_TYPE_UINT, .uintValue = &configCameraXSens},
    {.name = "bettercam_ysens",                .type = CONFIG_TYPE_UINT, .uintValue = &configCameraYSens},
    {.name = "bettercam_aggression",           .type = CONFIG_TYPE_UINT, .uintValue = &configCameraAggr},
    {.name = "bettercam_pan_level",            .type = CONFIG_TYPE_UINT, .uintValue = &configCameraPan},
    {.name = "bettercam_degrade",              .type = CONFIG_TYPE_UINT, .uintValue = &configCameraDegrade},
    {.name = "skip_intro",                     .type = CONFIG_TYPE_BOOL, .boolValue = &configSkipIntro},
    // debug
    {.name = "debug_offset",                   .type = CONFIG_TYPE_U64   , .u64Value    = &gPcDebug.bhvOffset},
    {.name = "debug_tags",                     .type = CONFIG_TYPE_U64   , .u64Value    = gPcDebug.tags},
    // coop-specific
    {.name = "show_fps",                       .type = CONFIG_TYPE_BOOL  , .boolValue   = &configShowFPS},
    {.name = "uncapped_framerate",             .type = CONFIG_TYPE_BOOL  , .boolValue   = &configUncappedFramerate},
    {.name = "frame_limit"       ,             .type = CONFIG_TYPE_UINT  , .uintValue   = &configFrameLimit},
    {.name = "amount_of_players",              .type = CONFIG_TYPE_UINT  , .uintValue   = &configAmountofPlayers},
    {.name = "bubble_death",                   .type = CONFIG_TYPE_BOOL  , .boolValue   = &configBubbleDeath},
    {.name = "coop_draw_distance",             .type = CONFIG_TYPE_UINT  , .uintValue   = &configDrawDistance},
    {.name = "coop_host_port",                 .type = CONFIG_TYPE_UINT  , .uintValue   = &configHostPort},
    {.name = "coop_host_save_slot",            .type = CONFIG_TYPE_UINT  , .uintValue   = &configHostSaveSlot},
    {.name = "coop_join_ip",                   .type = CONFIG_TYPE_STRING, .stringValue = (char*)&configJoinIp, .maxStringLength = MAX_CONFIG_STRING},
    {.name = "coop_join_port",                 .type = CONFIG_TYPE_UINT  , .uintValue   = &configJoinPort},
    {.name = "coop_network_system",            .type = CONFIG_TYPE_UINT  , .uintValue   = &configNetworkSystem},
    {.name = "coop_player_interaction",        .type = CONFIG_TYPE_UINT  , .uintValue   = &configPlayerInteraction},
    {.name = "coop_player_knockback_strength", .type = CONFIG_TYPE_UINT  , .uintValue   = &configPlayerKnockbackStrength},
    {.name = "coopdx_nametags",                .type = CONFIG_TYPE_BOOL  , .boolValue   = &configNametags},
    {.name = "coopdx_bouncy_bounds",           .type = CONFIG_TYPE_UINT  , .uintValue   = &configBouncyLevelBounds},
    {.name = "coop_player_model",              .type = CONFIG_TYPE_UINT  , .uintValue   = &configPlayerModel},
    {.name = "coop_player_name",               .type = CONFIG_TYPE_STRING, .stringValue = (char*)&configPlayerName, .maxStringLength = MAX_PLAYER_STRING},
    {.name = "coopdx_menu_staff_roll",         .type = CONFIG_TYPE_BOOL  , .boolValue   = &configMenuStaffRoll},
    {.name = "coop_menu_level",                .type = CONFIG_TYPE_UINT  , .uintValue   = &configMenuLevel},
    {.name = "coop_menu_sound",                .type = CONFIG_TYPE_BOOL  , .boolValue   = &configMenuSound},
    {.name = "coop_menu_random",               .type = CONFIG_TYPE_BOOL  , .boolValue   = &configMenuRandom},
    {.name = "coop_menu_demos",                .type = CONFIG_TYPE_BOOL  , .boolValue   = &configMenuDemos},
    {.name = "coop_player_palette_pants",      .type = CONFIG_TYPE_COLOR , .colorValue  = &configPlayerPalette.parts[PANTS]},
    {.name = "coop_player_palette_shirt",      .type = CONFIG_TYPE_COLOR , .colorValue  = &configPlayerPalette.parts[SHIRT]},
    {.name = "coop_player_palette_gloves",     .type = CONFIG_TYPE_COLOR , .colorValue  = &configPlayerPalette.parts[GLOVES]},
    {.name = "coop_player_palette_shoes",      .type = CONFIG_TYPE_COLOR , .colorValue  = &configPlayerPalette.parts[SHOES]},
    {.name = "coop_player_palette_hair",       .type = CONFIG_TYPE_COLOR , .colorValue  = &configPlayerPalette.parts[HAIR]},
    {.name = "coop_player_palette_skin",       .type = CONFIG_TYPE_COLOR , .colorValue  = &configPlayerPalette.parts[SKIN]},
    {.name = "coop_player_palette_cap",        .type = CONFIG_TYPE_COLOR , .colorValue  = &configPlayerPalette.parts[CAP]},
    {.name = "coop_custom_palette_pants",      .type = CONFIG_TYPE_COLOR , .colorValue  = &configCustomPalette.parts[PANTS]},
    {.name = "coop_custom_palette_shirt",      .type = CONFIG_TYPE_COLOR , .colorValue  = &configCustomPalette.parts[SHIRT]},
    {.name = "coop_custom_palette_gloves",     .type = CONFIG_TYPE_COLOR , .colorValue  = &configCustomPalette.parts[GLOVES]},
    {.name = "coop_custom_palette_shoes",      .type = CONFIG_TYPE_COLOR , .colorValue  = &configCustomPalette.parts[SHOES]},
    {.name = "coop_custom_palette_hair",       .type = CONFIG_TYPE_COLOR , .colorValue  = &configCustomPalette.parts[HAIR]},
    {.name = "coop_custom_palette_skin",       .type = CONFIG_TYPE_COLOR , .colorValue  = &configCustomPalette.parts[SKIN]},
    {.name = "coop_custom_palette_cap",        .type = CONFIG_TYPE_COLOR , .colorValue  = &configCustomPalette.parts[CAP]},
    {.name = "coop_stay_in_level_after_star",  .type = CONFIG_TYPE_UINT  , .uintValue   = &configStayInLevelAfterStar},
    {.name = "coopdx_global_player_models",    .type = CONFIG_TYPE_BOOL  , .boolValue   = &configGlobalPlayerModels},
    {.name = "disable_popups",                 .type = CONFIG_TYPE_BOOL  , .boolValue   = &configDisablePopups},
    {.name = "lua_profiler",                   .type = CONFIG_TYPE_BOOL  , .boolValue   = &configLuaProfiler},
#ifdef DEVELOPMENT
    {.name = "ctx_profiler",                   .type = CONFIG_TYPE_BOOL  , .boolValue   = &configCtxProfiler},
#endif
    {.name = "interpolation_mode",             .type = CONFIG_TYPE_UINT  , .uintValue   = &configInterpolationMode},
    {.name = "gamepad_number",                 .type = CONFIG_TYPE_UINT  , .uintValue   = &configGamepadNumber},
    {.name = "background_gamepad",             .type = CONFIG_TYPE_UINT  , .boolValue   = &configBackgroundGamepad},
    {.name = "disable_gamepads",               .type = CONFIG_TYPE_BOOL  , .boolValue   = &configDisableGamepads},
    {.name = "debug_print",                    .type = CONFIG_TYPE_BOOL  , .boolValue   = &configDebugPrint},
    {.name = "debug_info",                     .type = CONFIG_TYPE_BOOL  , .boolValue   = &configDebugInfo},
    {.name = "debug_error",                    .type = CONFIG_TYPE_BOOL  , .boolValue   = &configDebugError},
    {.name = "language",                       .type = CONFIG_TYPE_STRING, .stringValue = (char*)&configLanguage, .maxStringLength = MAX_CONFIG_STRING},
    {.name = "force_4by3",                     .type = CONFIG_TYPE_BOOL  , .boolValue   = &configForce4By3},
    {.name = "coopnet_ip",                     .type = CONFIG_TYPE_STRING, .stringValue = (char*)&configCoopNetIp, .maxStringLength = MAX_CONFIG_STRING},
    {.name = "coopnet_port",                   .type = CONFIG_TYPE_UINT  , .uintValue   = &configCoopNetPort},
    {.name = "coopnet_password",               .type = CONFIG_TYPE_STRING, .stringValue = (char*)&configPassword, .maxStringLength = MAX_CONFIG_STRING},
    {.name = "coopnet_dest",                   .type = CONFIG_TYPE_STRING, .stringValue = (char*)&configDestId, .maxStringLength = MAX_CONFIG_STRING},
    {.name = "fade_distant_sounds",            .type = CONFIG_TYPE_BOOL  , .boolValue   = &configFadeoutDistantSounds},
    {.name = "djui_theme",                     .type = CONFIG_TYPE_UINT  , .uintValue   = &configDjuiTheme},
    {.name = "djui_theme_center",              .type = CONFIG_TYPE_BOOL  , .boolValue   = &configDjuiThemeCenter},
    {.name = "djui_scale",                     .type = CONFIG_TYPE_UINT  , .uintValue   = &configDjuiScale},
    {.name = "last_version",                   .type = CONFIG_TYPE_STRING, .stringValue = (char*)&configLastVersion, .maxStringLength = MAX_CONFIG_STRING}
};

// FunctionConfigOption functions

struct QueuedMods {
    char* path;
    struct QueuedMods *next;
};

static struct QueuedMods *sQueuedEnableModsHead = NULL;

void enable_queued_mods(void) {
    while (sQueuedEnableModsHead) {
        struct QueuedMods *next = sQueuedEnableModsHead->next;
        mods_enable(sQueuedEnableModsHead->path);
        free(sQueuedEnableModsHead->path);
        free(sQueuedEnableModsHead);
        sQueuedEnableModsHead = next;
    }
}

static void enable_mod_read(char** tokens, UNUSED int numTokens) {
    char combined[256] = { 0 };
    for (int i = 1; i < numTokens; i++) {
        if (i != 1) { strncat(combined, " ", 255); }
        strncat(combined, tokens[i], 255);
    }

    struct QueuedMods* queued = malloc(sizeof(struct QueuedMods));
    queued->path = strdup(combined);
    queued->next = NULL;
    if (!sQueuedEnableModsHead) {
        sQueuedEnableModsHead = queued;
    } else {
        struct QueuedMods* tail = sQueuedEnableModsHead;
        while (tail->next) { tail = tail->next; }
        tail->next = queued;
    }
}

static void enable_mod_write(FILE* file) {
    for (unsigned int i = 0; i < gLocalMods.entryCount; i++) {
        struct Mod* mod = gLocalMods.entries[i];
        if (mod == NULL) { continue; }
        if (!mod->enabled) { continue; }
        fprintf(file, "%s %s\n", "enable-mod:", mod->relativePath);
    }
}

static void ban_read(char** tokens, UNUSED int numTokens) {
    ban_list_add(tokens[1], true);
}

static void ban_write(FILE* file) {
    for (unsigned int i = 0; i < gBanCount; i++) {
        if (gBanAddresses == NULL) { break; }
        if (gBanAddresses[i] == NULL) { continue; }
        if (!gBanPerm[i]) { continue; }
        fprintf(file, "%s %s\n", "ban:", gBanAddresses[i]);
    }
}

static void moderator_read(char** tokens, UNUSED int numTokens) {
    moderator_list_add(tokens[1], true);
}

static void moderator_write(FILE* file) {
    for (unsigned int i = 0; i < gModeratorCount; i++) {
        if (gModeratorAddresses == NULL) { break; }
        if (gModeratorAddresses[i] == NULL) { continue; }
        if (!gModerator[i]) { continue; }
        fprintf(file, "%s %s\n", "moderator:", gModeratorAddresses[i]);
    }
}

static void dynos_pack_read(char** tokens, int numTokens) {
    if (numTokens < 3) { return; }
    char fullPackName[256] = { 0 };
    for (int i = 1; i < numTokens-1; i++) {
        if (i != 1) { strncat(fullPackName, " ", 255); }
        strncat(fullPackName, tokens[i], 255);
    }

    bool enabled = !(strcmp(tokens[numTokens-1], "true"));
    int packCount = dynos_pack_get_count();

    for (int i = 0; i < packCount; i++) {
        const char* pack = dynos_pack_get_name(i);
        if (!strcmp(fullPackName, pack)) {
            dynos_pack_set_enabled(i, enabled);
            break;
        }
    }
}

static void dynos_pack_write(FILE* file) {
    int packCount = dynos_pack_get_count();
    for (int i = 0; i < packCount; i++) {
        bool enabled = dynos_pack_get_enabled(i);
        const char* pack = dynos_pack_get_name(i);
        fprintf(file, "%s %s %s\n", "dynos-pack:", pack, enabled ? "true" : "false");
    }
}

static const struct FunctionConfigOption functionOptions[] = {
    { .name = "enable-mod:", .read = enable_mod_read, .write = enable_mod_write },
    { .name = "ban:",        .read = ban_read,        .write = ban_write        },
    { .name = "moderator:",  .read = moderator_read,  .write = moderator_write  },
    { .name = "dynos-pack:", .read = dynos_pack_read, .write = dynos_pack_write },
};

// Reads an entire line from a file (excluding the newline character) and returns an allocated string
// Returns NULL if no lines could be read from the file
static char *read_file_line(fs_file_t *file, bool* error) {
    char *buffer;
    size_t bufferSize = 8;
    size_t offset = 0; // offset in buffer to write
    *error = false;

    buffer = malloc(bufferSize);
    buffer[0] = '\0';
    while (1) {
        // Read a line from the file
        if (fs_readline(file, buffer + offset, bufferSize - offset) == NULL) {
            free(buffer);
            return NULL; // Nothing could be read.
        }
        offset = strlen(buffer);
        if (offset <= 0) {
            LOG_ERROR("Configfile offset <= 0");
            *error = true;
            return NULL;
        }


        // If a newline was found, remove the trailing newline and exit
        if (buffer[offset - 1] == '\n') {
            buffer[offset - 1] = '\0';
            break;
        }

        if (fs_eof(file)) // EOF was reached
            break;

        // If no newline or EOF was reached, then the whole line wasn't read.
        bufferSize *= 2; // Increase buffer size
        buffer = realloc(buffer, bufferSize);
        assert(buffer != NULL);
    }

    return buffer;
}

// Returns the position of the first non-whitespace character
static char *skip_whitespace(char *str) {
    while (isspace(*str))
        str++;
    return str;
}

// NULL-terminates the current whitespace-delimited word, and returns a pointer to the next word
static char *word_split(char *str) {
    // Precondition: str must not point to whitespace
    assert(!isspace(*str));

    // Find either the next whitespace char or end of string
    while (!isspace(*str) && *str != '\0')
        str++;
    if (*str == '\0') // End of string
        return str;

    // Terminate current word
    *(str++) = '\0';

    // Skip whitespace to next word
    return skip_whitespace(str);
}

// Splits a string into words, and stores the words into the 'tokens' array
// 'maxTokens' is the length of the 'tokens' array
// Returns the number of tokens parsed
static unsigned int tokenize_string(char *str, int maxTokens, char **tokens) {
    int count = 0;

    str = skip_whitespace(str);
    while (str[0] != '\0' && count < maxTokens) {
        tokens[count] = str;
        str = word_split(str);
        count++;
    }
    return count;
}

// Gets the config file path and caches it
const char *configfile_name(void) {
    return (gCLIOpts.configFile[0]) ? gCLIOpts.configFile : CONFIGFILE_DEFAULT;
}

const char *configfile_backup_name(void) {
    return CONFIGFILE_BACKUP;
}

// Loads the config file specified by 'filename'
static void configfile_load_internal(const char *filename, bool* error) {
    fs_file_t *file;
    char *line;
    unsigned int temp;
    *error = false;

#ifdef DEVELOPMENT
    printf("Loading configuration from '%s'\n", filename);
#endif

    file = fs_open(filename);
    if (file == NULL) {
        // Create a new config file and save defaults
        printf("Config file '%s' not found. Creating it.\n", filename);
        configfile_save(filename);
        return;
    }

    // Go through each line in the file
    while ((line = read_file_line(file, error)) != NULL && !*error) {
        char *p = line;
        char *tokens[20];
        int numTokens;

        // skip whitespace
        while (isspace(*p))
            p++;

        // skip comment or empty line
        if (!*p || *p == '#') {
            free(line);
            continue;
        }

        numTokens = tokenize_string(p, sizeof(tokens) / sizeof(tokens[0]), tokens);
        if (numTokens != 0) {
            if (numTokens >= 2) {
                const struct ConfigOption *option = NULL;

                // find functionOption
                for (unsigned int i = 0; i < ARRAY_LEN(functionOptions); i++) {
                    if (strcmp(tokens[0], functionOptions[i].name) == 0) {
                        functionOptions[i].read(tokens, numTokens);
                        goto NEXT_OPTION;
                    }
                }

                // find option
                for (unsigned int i = 0; i < ARRAY_LEN(options); i++) {
                    if (strcmp(tokens[0], options[i].name) == 0) {
                        option = &options[i];
                        break;
                    }
                }

                if (option == NULL)
                    printf("unknown option '%s'\n", tokens[0]);
                else {
                    switch (option->type) {
                        case CONFIG_TYPE_BOOL:
                            if (strcmp(tokens[1], "true") == 0)
                                *option->boolValue = true;
                            else
                                *option->boolValue = false;
                            break;
                        case CONFIG_TYPE_UINT:
                            sscanf(tokens[1], "%u", option->uintValue);
                            break;
                        case CONFIG_TYPE_BIND:
                            for (int i = 0; i < MAX_BINDS && i < numTokens - 1; ++i)
                                sscanf(tokens[i + 1], "%x", option->uintValue + i);
                            break;
                        case CONFIG_TYPE_FLOAT:
                            sscanf(tokens[1], "%f", option->floatValue);
                            break;
                        case CONFIG_TYPE_STRING:
                            memset(option->stringValue, '\0', option->maxStringLength);
                            snprintf(option->stringValue, option->maxStringLength, "%s", tokens[1]);
                            break;
                        case CONFIG_TYPE_U64:
                            sscanf(tokens[1], "%llu", option->u64Value);
                            break;
                        case CONFIG_TYPE_COLOR:
                            for (int i = 0; i < 3 && i < numTokens - 1; ++i) {
                                sscanf(tokens[i + 1], "%x", &temp);
                                (*option->colorValue)[i] = temp;
                            }
                            break;
                        default:
                            LOG_ERROR("Configfile read bad type '%d': %s", (int)option->type, line);
                            goto NEXT_OPTION;
                    }
#ifdef DEVELOPMENT
                    printf("option: '%s', value:", tokens[0]);
                    for (int i = 1; i < numTokens; ++i) printf(" '%s'", tokens[i]);
                    printf("\n");
#endif
                }
            } else {
#ifdef DEVELOPMENT
                printf("error: expected value\n");
#endif
            }
        }
NEXT_OPTION:
        free(line);
        line = NULL;
    }

    if (line) {
        free(line);
    }

    fs_close(file);

    if (configFrameLimit < 30)   { configFrameLimit = 30; }
    if (configFrameLimit > 3000) { configFrameLimit = 3000; }

    if (configPlayerModel >= CT_MAX) { configPlayerModel = 0; }

    if (configDjuiTheme >= DJUI_THEME_MAX) { configDjuiTheme = 0; }

    if (!strcmp(configLastVersion, "")) { strncpy(configLastVersion, get_version(), MAX_CONFIG_STRING); }

#ifndef COOPNET
    configNetworkSystem = NS_SOCKET;
#endif
}

void configfile_load(void) {
    bool configReadError = false;
#ifdef DEVELOPMENT
    configfile_load_internal(configfile_name(), &configReadError);
#else
    configfile_load_internal(configfile_name(), &configReadError);
    if (configReadError) {
        configfile_load_internal(configfile_backup_name(), &configReadError);
    } else {
        configfile_save(configfile_backup_name());
    }
#endif
}

// Writes the config file to 'filename'
void configfile_save(const char *filename) {
    FILE *file;

    file = fopen(fs_get_write_path(filename), "w");
    if (file == NULL) {
        // error
        return;
    }

    printf("Saving configuration to '%s'\n", filename);

    for (unsigned int i = 0; i < ARRAY_LEN(options); i++) {
        const struct ConfigOption *option = &options[i];

        switch (option->type) {
            case CONFIG_TYPE_BOOL:
                fprintf(file, "%s %s\n", option->name, *option->boolValue ? "true" : "false");
                break;
            case CONFIG_TYPE_UINT:
                fprintf(file, "%s %u\n", option->name, *option->uintValue);
                break;
            case CONFIG_TYPE_FLOAT:
                fprintf(file, "%s %f\n", option->name, *option->floatValue);
                break;
            case CONFIG_TYPE_BIND:
                fprintf(file, "%s ", option->name);
                for (int i = 0; i < MAX_BINDS; ++i)
                    fprintf(file, "%04x ", option->uintValue[i]);
                fprintf(file, "\n");
                break;
            case CONFIG_TYPE_STRING:
                fprintf(file, "%s %s\n", option->name, option->stringValue);
                break;
            case CONFIG_TYPE_U64:
                fprintf(file, "%s %llu\n", option->name, *option->u64Value);
                break;
            case CONFIG_TYPE_COLOR:
                fprintf(file, "%s %02x %02x %02x\n", option->name, (*option->colorValue)[0], (*option->colorValue)[1], (*option->colorValue)[2]);
                break;
            default:
                LOG_ERROR("Configfile wrote bad type '%d': %s", (int)option->type, option->name);
                break;
        }
    }

    // save function options
    for (unsigned int i = 0; i < ARRAY_LEN(functionOptions); i++) {
        functionOptions[i].write(file);
    }

    fclose(file);
}
