#include <PR/ultratypes.h>

#include "sm64.h"
#include "actors/common1.h"
#include "gfx_dimensions.h"
#include "game_init.h"
#include "level_update.h"
#include "camera.h"
#include "print.h"
#include "ingame_menu.h"
#include "hud.h"
#include "segment2.h"
#include "area.h"
#include "save_file.h"
#include "print.h"
#include "engine/surface_load.h"

/* @file hud.c
 * This file implements HUD rendering and power meter animations.
 * That includes stars, lives, coins, camera status, power meter, timer
 * cannon reticle, and the unused keys.
 **/

f32 gHudLocationOffset = -128.0f;
f32 gHudLocationTarget = -128.0f;
u8 gShownHud = FALSE;
u8 gShownFromCoins = FALSE;
s16 gShownTimer = 0;

#define POWER_METER_X 320 - 64
#define POWER_METER_Y 0
// ------------- FPS COUNTER ---------------
// To use it, call print_fps(x,y); every frame.
#define FRAMETIME_COUNT 30

OSTime frameTimes[FRAMETIME_COUNT];
u8 curFrameTimeIndex = 0;

#include "PR/os_convert.h"

// Call once per frame
f32 calculate_and_update_fps()
{
    OSTime newTime = osGetTime();
    OSTime oldTime = frameTimes[curFrameTimeIndex];
    frameTimes[curFrameTimeIndex] = newTime;

    curFrameTimeIndex++;
    if (curFrameTimeIndex >= FRAMETIME_COUNT)
        curFrameTimeIndex = 0;


    return ((f32)FRAMETIME_COUNT * 1000000.0f) / (s32)OS_CYCLES_TO_USEC(newTime - oldTime);
}

void print_fps(s32 x, s32 y)
{
    f32 fps = calculate_and_update_fps();
    char text[10];

    sprintf(text, "%2.2f", fps);

    print_text(x, y, text);
}

// ------------ END OF FPS COUNER -----------------

struct PowerMeterHUD {
    s8 animation;
    f32 x;
    f32 y;
    f32 unused;
};

struct UnusedHUDStruct {
    u32 unused1;
    u16 unused2;
    u16 unused3;
};

struct CameraHUD {
    s16 status;
};

// Stores health segmented value defined by numHealthWedges
// When the HUD is rendered this value is 8, full health.
static s16 sPowerMeterStoredHealth;

static struct PowerMeterHUD sPowerMeterHUD = {
    POWER_METER_HIDDEN,
    POWER_METER_X,
    POWER_METER_Y,
    1.0,
};

// Power Meter timer that keeps counting when it's visible.
// Gets reset when the health is filled and stops counting
// when the power meter is hidden.
s32 sPowerMeterVisibleTimer = 0;

static struct UnusedHUDStruct sUnusedHUDValues = { 0x00, 0x0A, 0x00 };

static struct CameraHUD sCameraHUD = { CAM_STATUS_NONE };

/**
 * Renders a rgba16 16x16 glyph texture from a table list.
 */
void render_hud_tex_lut(s32 x, s32 y, u8 *texture) {
    gDPPipeSync(gDisplayListHead++);
    gDPSetTextureImage(gDisplayListHead++, G_IM_FMT_RGBA, G_IM_SIZ_16b, 1, texture);
    gSPDisplayList(gDisplayListHead++, &dl_hud_img_load_tex_block);
    gSPTextureRectangle(gDisplayListHead++, x << 2, y << 2, (x + 15) << 2, (y + 15) << 2,
                        G_TX_RENDERTILE, 0, 0, 4 << 10, 1 << 10);
}

/**
 * Renders a rgba16 8x8 glyph texture from a table list.
 */
void render_hud_small_tex_lut(s32 x, s32 y, u8 *texture) {
    gDPSetTile(gDisplayListHead++, G_IM_FMT_RGBA, G_IM_SIZ_16b, 0, 0, G_TX_LOADTILE, 0,
                G_TX_WRAP | G_TX_NOMIRROR, G_TX_NOMASK, G_TX_NOLOD, G_TX_WRAP | G_TX_NOMIRROR, G_TX_NOMASK, G_TX_NOLOD);
    gDPTileSync(gDisplayListHead++);
    gDPSetTile(gDisplayListHead++, G_IM_FMT_RGBA, G_IM_SIZ_16b, 2, 0, G_TX_RENDERTILE, 0,
                G_TX_CLAMP, 3, G_TX_NOLOD, G_TX_CLAMP, 3, G_TX_NOLOD);
    gDPSetTileSize(gDisplayListHead++, G_TX_RENDERTILE, 0, 0, (8 - 1) << G_TEXTURE_IMAGE_FRAC, (8 - 1) << G_TEXTURE_IMAGE_FRAC);
    gDPPipeSync(gDisplayListHead++);
    gDPSetTextureImage(gDisplayListHead++, G_IM_FMT_RGBA, G_IM_SIZ_16b, 1, texture);
    gDPLoadSync(gDisplayListHead++);
    gDPLoadBlock(gDisplayListHead++, G_TX_LOADTILE, 0, 0, 8 * 8 - 1, CALC_DXT(8, G_IM_SIZ_16b_BYTES));
    gSPTextureRectangle(gDisplayListHead++, x << 2, y << 2, (x + 7) << 2, (y + 7) << 2, G_TX_RENDERTILE,
                        0, 0, 4 << 10, 1 << 10);
}

/**
 * Renders power meter health segment texture using a table list.
 */
void render_power_meter_health_segment(s16 numHealthWedges) {
    u8 *(*healthLUT)[];

    healthLUT = segmented_to_virtual(&power_meter_health_segments_lut);

    gDPPipeSync(gDisplayListHead++);
    gDPSetTextureImage(gDisplayListHead++, G_IM_FMT_RGBA, G_IM_SIZ_16b, 1,
                       (*healthLUT)[numHealthWedges - 1]);
    gDPLoadSync(gDisplayListHead++);
    gDPLoadBlock(gDisplayListHead++, G_TX_LOADTILE, 0, 0, 32 * 32 - 1, CALC_DXT(32, G_IM_SIZ_16b_BYTES));
    gSP1Triangle(gDisplayListHead++, 0, 1, 2, 0);
    gSP1Triangle(gDisplayListHead++, 0, 2, 3, 0);
}

/**
 * Renders power meter display lists.
 * That includes the "POWER" base and the colored health segment textures.
 */
void render_dl_power_meter(s16 numHealthWedges) {
    Mtx *mtx;

    mtx = alloc_display_list(sizeof(Mtx));

    if (mtx == NULL) {
        return;
    }

    guTranslate(mtx, (f32) sPowerMeterHUD.x, (f32) sPowerMeterHUD.y, 0);

    gSPMatrix(gDisplayListHead++, VIRTUAL_TO_PHYSICAL(mtx++),
              G_MTX_MODELVIEW | G_MTX_MUL | G_MTX_PUSH);
    gSPDisplayList(gDisplayListHead++, &dl_power_meter_base);

    if (numHealthWedges != 0) {
        gSPDisplayList(gDisplayListHead++, &dl_power_meter_health_segments_begin);
        render_power_meter_health_segment(numHealthWedges);
        gSPDisplayList(gDisplayListHead++, &dl_power_meter_health_segments_end);
    }

    gSPPopMatrix(gDisplayListHead++, G_MTX_MODELVIEW);
}

/**
 * Render functions for the power meter.
 */

void render_hud_power_meter_three(s32 x, s32 y, s32 width, s32 height, s32 s, s32 t) {
	s32 xl = MAX(0, x);
	s32 yl = MAX(0, y);
	s32 xh = MAX(0, x + width - 1);
	s32 yh = MAX(0, y + height - 1);
	s = (x < 0) ? s - x : s;
	t = (y < 0) ? t - y : t;
	gDPPipeSync(gDisplayListHead++);
	gDPSetCycleType(gDisplayListHead++, G_CYC_COPY);
	gDPSetTexturePersp(gDisplayListHead++, G_TP_NONE);
	gDPSetAlphaCompare(gDisplayListHead++, G_AC_THRESHOLD);
	gDPSetBlendColor(gDisplayListHead++, 255, 255, 255, 255);
	gDPSetRenderMode(gDisplayListHead++, G_RM_AA_XLU_SURF, G_RM_AA_XLU_SURF2);
	gDPSetTextureLUT(gDisplayListHead++, G_TT_RGBA16);
	gDPSetTextureImage(gDisplayListHead++, G_IM_FMT_RGBA, G_IM_SIZ_16b, 1, segmented_to_virtual(render_hud_power_meter_three_texture_pal_rgba16));
	gDPTileSync(gDisplayListHead++);
	gDPSetTile(gDisplayListHead++, 0, 0, 0, 256, 7, 0, G_TX_WRAP | G_TX_NOMIRROR, 0, 0, G_TX_WRAP | G_TX_NOMIRROR, 0, 0);
	gDPLoadSync(gDisplayListHead++);
	gDPLoadTLUTCmd(gDisplayListHead++, 7, 8);
	gDPPipeSync(gDisplayListHead++);
	gDPTileSync(gDisplayListHead++);
	gDPSetTextureImage(gDisplayListHead++, G_IM_FMT_CI, G_IM_SIZ_16b, 1, segmented_to_virtual(render_hud_power_meter_three_texture));
	gDPSetTile(gDisplayListHead++, G_IM_FMT_CI, G_IM_SIZ_16b, 0, 0, 7, 0, G_TX_WRAP | G_TX_NOMIRROR, 6, 0, G_TX_WRAP | G_TX_NOMIRROR, 6, 0);
	gDPLoadSync(gDisplayListHead++);
	gDPLoadBlock(gDisplayListHead++, 7, 0, 0, 1023, 512);
	gDPPipeSync(gDisplayListHead++);
	gDPSetTile(gDisplayListHead++, G_IM_FMT_CI, G_IM_SIZ_4b, 4, 0, 0, 0, G_TX_WRAP | G_TX_NOMIRROR, 6, 0, G_TX_WRAP | G_TX_NOMIRROR, 6, 0);
	gDPSetTileSize(gDisplayListHead++, 0, 0, 0, 252, 252);
	gSPScisTextureRectangle(gDisplayListHead++, xl << 2, yl << 2, xh << 2, yh << 2, 0, s << 5, t << 5,  4096, 1024);
	gDPSetTextureLUT(gDisplayListHead++, G_TT_NONE);
	gDPPipeSync(gDisplayListHead++);
	gDPSetCycleType(gDisplayListHead++, G_CYC_1CYCLE);
	gSPTexture(gDisplayListHead++, 65535, 65535, 0, G_TX_RENDERTILE, G_OFF);
	gDPSetTexturePersp(gDisplayListHead++, G_TP_PERSP);
	gDPSetAlphaCompare(gDisplayListHead++, G_AC_NONE);
	gDPSetRenderMode(gDisplayListHead++, G_RM_AA_ZB_OPA_SURF, G_RM_AA_ZB_OPA_SURF2);
}





void render_hud_power_meter_two(s32 x, s32 y, s32 width, s32 height, s32 s, s32 t) {
	s32 xl = MAX(0, x);
	s32 yl = MAX(0, y);
	s32 xh = MAX(0, x + width - 1);
	s32 yh = MAX(0, y + height - 1);
	s = (x < 0) ? s - x : s;
	t = (y < 0) ? t - y : t;
	gDPPipeSync(gDisplayListHead++);
	gDPSetCycleType(gDisplayListHead++, G_CYC_COPY);
	gDPSetTexturePersp(gDisplayListHead++, G_TP_NONE);
	gDPSetAlphaCompare(gDisplayListHead++, G_AC_THRESHOLD);
	gDPSetBlendColor(gDisplayListHead++, 255, 255, 255, 255);
	gDPSetRenderMode(gDisplayListHead++, G_RM_AA_XLU_SURF, G_RM_AA_XLU_SURF2);
	gDPSetTextureLUT(gDisplayListHead++, G_TT_RGBA16);
	gDPSetTextureImage(gDisplayListHead++, G_IM_FMT_RGBA, G_IM_SIZ_16b, 1, segmented_to_virtual(render_hud_power_meter_two_texture_pal_rgba16));
	gDPTileSync(gDisplayListHead++);
	gDPSetTile(gDisplayListHead++, 0, 0, 0, 256, 7, 0, G_TX_WRAP | G_TX_NOMIRROR, 0, 0, G_TX_WRAP | G_TX_NOMIRROR, 0, 0);
	gDPLoadSync(gDisplayListHead++);
	gDPLoadTLUTCmd(gDisplayListHead++, 7, 8);
	gDPPipeSync(gDisplayListHead++);
	gDPTileSync(gDisplayListHead++);
	gDPSetTextureImage(gDisplayListHead++, G_IM_FMT_CI, G_IM_SIZ_16b, 1, segmented_to_virtual(render_hud_power_meter_two_texture));
	gDPSetTile(gDisplayListHead++, G_IM_FMT_CI, G_IM_SIZ_16b, 0, 0, 7, 0, G_TX_WRAP | G_TX_NOMIRROR, 6, 0, G_TX_WRAP | G_TX_NOMIRROR, 6, 0);
	gDPLoadSync(gDisplayListHead++);
	gDPLoadBlock(gDisplayListHead++, 7, 0, 0, 1023, 512);
	gDPPipeSync(gDisplayListHead++);
	gDPSetTile(gDisplayListHead++, G_IM_FMT_CI, G_IM_SIZ_4b, 4, 0, 0, 0, G_TX_WRAP | G_TX_NOMIRROR, 6, 0, G_TX_WRAP | G_TX_NOMIRROR, 6, 0);
	gDPSetTileSize(gDisplayListHead++, 0, 0, 0, 252, 252);
	gSPScisTextureRectangle(gDisplayListHead++, xl << 2, yl << 2, xh << 2, yh << 2, 0, s << 5, t << 5,  4096, 1024);
	gDPSetTextureLUT(gDisplayListHead++, G_TT_NONE);
	gDPPipeSync(gDisplayListHead++);
	gDPSetCycleType(gDisplayListHead++, G_CYC_1CYCLE);
	gSPTexture(gDisplayListHead++, 65535, 65535, 0, G_TX_RENDERTILE, G_OFF);
	gDPSetTexturePersp(gDisplayListHead++, G_TP_PERSP);
	gDPSetAlphaCompare(gDisplayListHead++, G_AC_NONE);
	gDPSetRenderMode(gDisplayListHead++, G_RM_AA_ZB_OPA_SURF, G_RM_AA_ZB_OPA_SURF2);
}



void render_hud_power_meter_one(s32 x, s32 y, s32 width, s32 height, s32 s, s32 t) {
	s32 xl = MAX(0, x);
	s32 yl = MAX(0, y);
	s32 xh = MAX(0, x + width - 1);
	s32 yh = MAX(0, y + height - 1);
	s = (x < 0) ? s - x : s;
	t = (y < 0) ? t - y : t;
	gDPPipeSync(gDisplayListHead++);
	gDPSetCycleType(gDisplayListHead++, G_CYC_COPY);
	gDPSetTexturePersp(gDisplayListHead++, G_TP_NONE);
	gDPSetAlphaCompare(gDisplayListHead++, G_AC_THRESHOLD);
	gDPSetBlendColor(gDisplayListHead++, 255, 255, 255, 255);
	gDPSetRenderMode(gDisplayListHead++, G_RM_AA_XLU_SURF, G_RM_AA_XLU_SURF2);
	gDPSetTextureLUT(gDisplayListHead++, G_TT_RGBA16);
	gDPSetTextureImage(gDisplayListHead++, G_IM_FMT_RGBA, G_IM_SIZ_16b, 1, segmented_to_virtual(render_hud_power_meter_one_texture_pal_rgba16));
	gDPTileSync(gDisplayListHead++);
	gDPSetTile(gDisplayListHead++, 0, 0, 0, 256, 7, 0, G_TX_WRAP | G_TX_NOMIRROR, 0, 0, G_TX_WRAP | G_TX_NOMIRROR, 0, 0);
	gDPLoadSync(gDisplayListHead++);
	gDPLoadTLUTCmd(gDisplayListHead++, 7, 8);
	gDPPipeSync(gDisplayListHead++);
	gDPTileSync(gDisplayListHead++);
	gDPSetTextureImage(gDisplayListHead++, G_IM_FMT_CI, G_IM_SIZ_16b, 1, segmented_to_virtual(render_hud_power_meter_one_texture));
	gDPSetTile(gDisplayListHead++, G_IM_FMT_CI, G_IM_SIZ_16b, 0, 0, 7, 0, G_TX_WRAP | G_TX_NOMIRROR, 6, 0, G_TX_WRAP | G_TX_NOMIRROR, 6, 0);
	gDPLoadSync(gDisplayListHead++);
	gDPLoadBlock(gDisplayListHead++, 7, 0, 0, 1023, 512);
	gDPPipeSync(gDisplayListHead++);
	gDPSetTile(gDisplayListHead++, G_IM_FMT_CI, G_IM_SIZ_4b, 4, 0, 0, 0, G_TX_WRAP | G_TX_NOMIRROR, 6, 0, G_TX_WRAP | G_TX_NOMIRROR, 6, 0);
	gDPSetTileSize(gDisplayListHead++, 0, 0, 0, 252, 252);
	gSPScisTextureRectangle(gDisplayListHead++, xl << 2, yl << 2, xh << 2, yh << 2, 0, s << 5, t << 5,  4096, 1024);
	gDPSetTextureLUT(gDisplayListHead++, G_TT_NONE);
	gDPPipeSync(gDisplayListHead++);
	gDPSetCycleType(gDisplayListHead++, G_CYC_1CYCLE);
	gSPTexture(gDisplayListHead++, 65535, 65535, 0, G_TX_RENDERTILE, G_OFF);
	gDPSetTexturePersp(gDisplayListHead++, G_TP_PERSP);
	gDPSetAlphaCompare(gDisplayListHead++, G_AC_NONE);
	gDPSetRenderMode(gDisplayListHead++, G_RM_AA_ZB_OPA_SURF, G_RM_AA_ZB_OPA_SURF2);
}


void render_hud_power_meter_zero(s32 x, s32 y, s32 width, s32 height, s32 s, s32 t) {
	s32 xl = MAX(0, x);
	s32 yl = MAX(0, y);
	s32 xh = MAX(0, x + width - 1);
	s32 yh = MAX(0, y + height - 1);
	s = (x < 0) ? s - x : s;
	t = (y < 0) ? t - y : t;
	gDPPipeSync(gDisplayListHead++);
	gDPSetCycleType(gDisplayListHead++, G_CYC_COPY);
	gDPSetTexturePersp(gDisplayListHead++, G_TP_NONE);
	gDPSetAlphaCompare(gDisplayListHead++, G_AC_THRESHOLD);
	gDPSetBlendColor(gDisplayListHead++, 255, 255, 255, 255);
	gDPSetRenderMode(gDisplayListHead++, G_RM_AA_XLU_SURF, G_RM_AA_XLU_SURF2);
	gDPSetTextureLUT(gDisplayListHead++, G_TT_RGBA16);
	gDPSetTextureImage(gDisplayListHead++, G_IM_FMT_RGBA, G_IM_SIZ_16b, 1, segmented_to_virtual(render_hud_power_meter_zero_texture_pal_rgba16));
	gDPTileSync(gDisplayListHead++);
	gDPSetTile(gDisplayListHead++, 0, 0, 0, 256, 7, 0, G_TX_WRAP | G_TX_NOMIRROR, 0, 0, G_TX_WRAP | G_TX_NOMIRROR, 0, 0);
	gDPLoadSync(gDisplayListHead++);
	gDPLoadTLUTCmd(gDisplayListHead++, 7, 3);
	gDPPipeSync(gDisplayListHead++);
	gDPTileSync(gDisplayListHead++);
	gDPSetTextureImage(gDisplayListHead++, G_IM_FMT_CI, G_IM_SIZ_16b, 1, segmented_to_virtual(render_hud_power_meter_zero_texture));
	gDPSetTile(gDisplayListHead++, G_IM_FMT_CI, G_IM_SIZ_16b, 0, 0, 7, 0, G_TX_WRAP | G_TX_NOMIRROR, 6, 0, G_TX_WRAP | G_TX_NOMIRROR, 6, 0);
	gDPLoadSync(gDisplayListHead++);
	gDPLoadBlock(gDisplayListHead++, 7, 0, 0, 1023, 512);
	gDPPipeSync(gDisplayListHead++);
	gDPSetTile(gDisplayListHead++, G_IM_FMT_CI, G_IM_SIZ_4b, 4, 0, 0, 0, G_TX_WRAP | G_TX_NOMIRROR, 6, 0, G_TX_WRAP | G_TX_NOMIRROR, 6, 0);
	gDPSetTileSize(gDisplayListHead++, 0, 0, 0, 252, 252);
	gSPScisTextureRectangle(gDisplayListHead++, xl << 2, yl << 2, xh << 2, yh << 2, 0, s << 5, t << 5,  4096, 1024);
	gDPSetTextureLUT(gDisplayListHead++, G_TT_NONE);
	gDPPipeSync(gDisplayListHead++);
	gDPSetCycleType(gDisplayListHead++, G_CYC_1CYCLE);
	gSPTexture(gDisplayListHead++, 65535, 65535, 0, G_TX_RENDERTILE, G_OFF);
	gDPSetTexturePersp(gDisplayListHead++, G_TP_PERSP);
	gDPSetAlphaCompare(gDisplayListHead++, G_AC_NONE);
	gDPSetRenderMode(gDisplayListHead++, G_RM_AA_ZB_OPA_SURF, G_RM_AA_ZB_OPA_SURF2);
}





/**
 * Power meter animation called when there's less than 8 health segments
 * Checks its timer to later change into deemphasizing mode.
 */
void animate_power_meter_emphasized(void) {
    s16 hudDisplayFlags;
    hudDisplayFlags = gHudDisplay.flags;

    if (!(hudDisplayFlags & HUD_DISPLAY_FLAG_EMPHASIZE_POWER)) {
        if (sPowerMeterVisibleTimer == 45.0) {
            sPowerMeterHUD.animation = POWER_METER_DEEMPHASIZING;
        }
    } else {
        sPowerMeterVisibleTimer = 0;
    }
}

/**
 * Power meter animation called after emphasized mode.
 * Moves power meter y pos speed until it's at 200 to be visible.
 */
static void animate_power_meter_deemphasizing(void) {
    f32 xSpeed = (POWER_METER_X - sPowerMeterHUD.x) / 8;
    f32 ySpeed = (POWER_METER_Y  - sPowerMeterHUD.y) / 8;

    sPowerMeterHUD.x += xSpeed;
    sPowerMeterHUD.y += ySpeed;

    if (((s16) sPowerMeterHUD.y == POWER_METER_Y) && ((s16) sPowerMeterHUD.x == POWER_METER_X)) {
        sPowerMeterHUD.x = POWER_METER_X;
        sPowerMeterHUD.y = POWER_METER_Y;
        sPowerMeterHUD.animation = POWER_METER_VISIBLE;
    }
}

/**
 * Power meter animation called when there's 8 health segments.
 * Moves power meter y pos quickly until it's at 301 to be hidden.
 */
static void animate_power_meter_hiding(void) {
    f32 xSpeed = (340 - sPowerMeterHUD.x) / 4;
    sPowerMeterHUD.x += xSpeed;
    if (sPowerMeterHUD.x > 320) {
        sPowerMeterHUD.animation = POWER_METER_HIDDEN;
        sPowerMeterVisibleTimer = 0;
    }
}

/**
 * Handles power meter actions depending of the health segments values.
 */
void handle_power_meter_actions(s16 numHealthWedges) {
    // Show power meter if health is not full, less than 3
    if (numHealthWedges < 3 && sPowerMeterStoredHealth == 3 && sPowerMeterHUD.animation == POWER_METER_HIDDEN) {
        sPowerMeterHUD.animation = POWER_METER_EMPHASIZED;
        sPowerMeterHUD.x = gMarioScreenX;
        sPowerMeterHUD.y = gMarioScreenY - 20;
    }

    // Show power meter if health is full, has 3
    if (numHealthWedges == 3 && sPowerMeterStoredHealth == 2) {
        sPowerMeterVisibleTimer = 0;
    }

    // After health is full, hide power meter
    if (numHealthWedges == 3 && sPowerMeterVisibleTimer > 45.0) {
        if (!gShownHud) sPowerMeterHUD.animation = POWER_METER_HIDING;
    }

    // Update to match health value
    sPowerMeterStoredHealth = numHealthWedges;

    // If Mario is swimming, keep power meter visible
    if (gPlayerCameraState->action & ACT_FLAG_SWIMMING) {
        if (sPowerMeterHUD.animation == POWER_METER_HIDDEN
            || sPowerMeterHUD.animation == POWER_METER_EMPHASIZED) {
            sPowerMeterHUD.animation = POWER_METER_DEEMPHASIZING;
            sPowerMeterHUD.x = 320;
            sPowerMeterHUD.y = POWER_METER_Y;
        }
        sPowerMeterVisibleTimer = 0;
    }
}

/**
 * Renders the power meter that shows when Mario is in underwater
 * or has taken damage and has less than 3 health segments.
 * And calls a power meter animation function depending of the value defined.
 */
void render_hud_power_meter(void) {
    s16 shownHealthWedges = gHudDisplay.wedges;

    if (sPowerMeterHUD.animation != POWER_METER_HIDING) {
        handle_power_meter_actions(shownHealthWedges);
    }

    if (sPowerMeterHUD.animation == POWER_METER_HIDDEN) {
        return;
    }

    switch (sPowerMeterHUD.animation) {
        case POWER_METER_EMPHASIZED:
            animate_power_meter_emphasized();
            break;
        case POWER_METER_DEEMPHASIZING:
            animate_power_meter_deemphasizing();
            break;
        case POWER_METER_HIDING:
            animate_power_meter_hiding();
            break;
        default:
            break;
    }

    switch (shownHealthWedges) {
        case 2:
            render_hud_power_meter_two((s32) sPowerMeterHUD.x, (s32) sPowerMeterHUD.y, 64, 64, 0, 0);
            break;
        case 1:
            render_hud_power_meter_one((s32) sPowerMeterHUD.x, (s32) sPowerMeterHUD.y, 64, 64, 0, 0);
            break;
        case 0:
            render_hud_power_meter_zero((s32) sPowerMeterHUD.x, (s32) sPowerMeterHUD.y, 64, 64, 0, 0);
            break;
        case 3:
        default:
            render_hud_power_meter_three((s32) sPowerMeterHUD.x, (s32) sPowerMeterHUD.y, 64, 64, 0, 0);
            break;
    }
    //render_dl_power_meter(shownHealthWedges);

    sPowerMeterVisibleTimer += 1;
}

#ifdef VERSION_JP
#define HUD_TOP_Y 210
#else
#define HUD_TOP_Y 209
#endif

/**
 * Renders the amount of lives Mario has.
 */
void render_hud_mario_lives(void) {
    //print_text(GFX_DIMENSIONS_RECT_FROM_LEFT_EDGE(22), HUD_TOP_Y, ","); // 'Mario Head' glyph
    //print_text(GFX_DIMENSIONS_RECT_FROM_LEFT_EDGE(38), HUD_TOP_Y, "*"); // 'X' glyph
    //print_text_fmt_int(GFX_DIMENSIONS_RECT_FROM_LEFT_EDGE(54), HUD_TOP_Y, "%d", gHudDisplay.lives);
}

f32 gCoinVelocity = 0;
f32 gCoinOffset = 0;

void show_hud(void) {
    if (!gShownHud) {
        gHudLocationTarget = 0;
        sPowerMeterHUD.animation = POWER_METER_DEEMPHASIZING;
        sPowerMeterHUD.x = 320;
        sPowerMeterHUD.y = POWER_METER_Y;
        sPowerMeterVisibleTimer = 0;
        gShownHud = TRUE;
        gShownFromCoins = FALSE;
        gShownTimer = 0;
    } else if (gShownFromCoins) {
        sPowerMeterHUD.animation = POWER_METER_DEEMPHASIZING;
        sPowerMeterHUD.x = 320;
        sPowerMeterHUD.y = POWER_METER_Y;
        sPowerMeterVisibleTimer = 0;
        gShownFromCoins = FALSE;
        gShownTimer = 0;
    }
}

void show_left_hud_instant(void) {
    if (!gShownHud) {
        gHudLocationTarget = 0;
        gHudLocationOffset = 0;
        gShownHud = TRUE;
        gShownFromCoins = TRUE;
        gShownTimer = 0;
    }
    gCoinVelocity = 2;
}

void hide_hud(void) {
    if (gShownHud) {
        if (gShownFromCoins && (gShownTimer < 90)) return;
        gHudLocationTarget = -128;
        if (!gShownFromCoins) sPowerMeterHUD.animation = POWER_METER_HIDING;
        gShownHud = FALSE;
    }
}

void render_hud_info(void) {

    gShownTimer++;

    gCoinVelocity -= 0.5;
    gCoinOffset += gCoinVelocity;
    if (gCoinOffset < 0) {
        gCoinOffset = 0;
        gCoinVelocity = 0;
    }

    f32 speed = (gHudLocationTarget - gHudLocationOffset) / 8;

    gHudLocationOffset += speed;

    if ((gHudLocationOffset < gHudLocationTarget) && (speed == 0)) gHudLocationOffset++;
    if ((gHudLocationOffset > gHudLocationTarget) && (speed == 0)) gHudLocationOffset--;

    // Render the blue bar
    render_hud_bar((s32)gHudLocationOffset + 9,25,62,10,0,0);
    // Render the coin and star glyphs
    print_text((s32)gHudLocationOffset + 9  - 1, 240 - 32 + 15 - 8 , "+"); // Coin
    print_text((s32)gHudLocationOffset + 55 - 1, 240 - 32 + 16 - 35, "-"); // Star


    char buf[10];
    sprintf(buf, "%03d", gHudDisplay.coins);
    print_hud_numbers((s32)gHudLocationOffset + 16,240 - 32 + 2 + (s16) gCoinOffset,buf);

}

/*#ifdef VERSION_JP
#define HUD_STARS_X 73
#else
#define HUD_STARS_X 78
#endif*/

/**
 * Renders the amount of stars collected.
 * Disables "X" glyph when Mario has 100 stars or more.
 */
void render_hud_stars(void) {
    if (gHudFlash == 1 && gGlobalTimer & 0x08) {
        return;
    }

    print_text(GFX_DIMENSIONS_RECT_FROM_LEFT_EDGE(22), HUD_TOP_Y - 16, "-"); // 'Star' glyph
    print_text(GFX_DIMENSIONS_RECT_FROM_LEFT_EDGE(38), HUD_TOP_Y - 16, "*"); // 'X' glyph
    print_text_fmt_int(GFX_DIMENSIONS_RECT_FROM_LEFT_EDGE(54),
                       HUD_TOP_Y - 16, "%d", gHudDisplay.stars);
}

/**
 * Unused function that renders the amount of keys collected.
 * Leftover function from the beta version of the game.
 */
void render_hud_keys(void) {
    s16 i;

    for (i = 0; i < gHudDisplay.keys; i++) {
        print_text((i * 16) + 220, 142, "/"); // unused glyph - beta key
    }
}

/**
 * Renders the timer when Mario start sliding in PSS.
 */
void render_hud_timer(void) {
    u8 *(*hudLUT)[58];
    u16 timerValFrames;
    u16 timerMins;
    u16 timerSecs;
    u16 timerFracSecs;

    hudLUT = segmented_to_virtual(&main_hud_lut);
    timerValFrames = gHudDisplay.timer;
#ifdef VERSION_EU
    switch (eu_get_language()) {
        case LANGUAGE_ENGLISH:
            print_text(GFX_DIMENSIONS_RECT_FROM_RIGHT_EDGE(150), 185, "TIME");
            break;
        case LANGUAGE_FRENCH:
            print_text(GFX_DIMENSIONS_RECT_FROM_RIGHT_EDGE(155), 185, "TEMPS");
            break;
        case LANGUAGE_GERMAN:
            print_text(GFX_DIMENSIONS_RECT_FROM_RIGHT_EDGE(150), 185, "ZEIT");
            break;
    }
#endif
    timerMins = timerValFrames / (30 * 60);
    timerSecs = (timerValFrames - (timerMins * 1800)) / 30;

    timerFracSecs = ((timerValFrames - (timerMins * 1800) - (timerSecs * 30)) & 0xFFFF) / 3;
#ifndef VERSION_EU
    print_text(GFX_DIMENSIONS_RECT_FROM_RIGHT_EDGE(150), 185, "TIME");
#endif
    print_text_fmt_int(GFX_DIMENSIONS_RECT_FROM_RIGHT_EDGE(91), 185, "%0d", timerMins);
    print_text_fmt_int(GFX_DIMENSIONS_RECT_FROM_RIGHT_EDGE(71), 185, "%02d", timerSecs);
    print_text_fmt_int(GFX_DIMENSIONS_RECT_FROM_RIGHT_EDGE(37), 185, "%d", timerFracSecs);
    gSPDisplayList(gDisplayListHead++, dl_hud_img_begin);
    render_hud_tex_lut(GFX_DIMENSIONS_RECT_FROM_RIGHT_EDGE(81), 32, (*hudLUT)[GLYPH_APOSTROPHE]);
    render_hud_tex_lut(GFX_DIMENSIONS_RECT_FROM_RIGHT_EDGE(46), 32, (*hudLUT)[GLYPH_DOUBLE_QUOTE]);
    gSPDisplayList(gDisplayListHead++, dl_hud_img_end);
}

/**
 * Sets HUD status camera value depending of the actions
 * defined in update_camera_status.
 */
void set_hud_camera_status(s16 status) {
    sCameraHUD.status = status;
}

/**
 * Renders camera HUD glyphs using a table list, depending of
 * the camera status called, a defined glyph is rendered.
 */
void render_hud_camera_status(void) {
    u8 *(*cameraLUT)[6];
    s32 x;
    s32 y;

    cameraLUT = segmented_to_virtual(&main_hud_camera_lut);
    x = GFX_DIMENSIONS_RECT_FROM_RIGHT_EDGE(54);
    y = 205;

    if (sCameraHUD.status == CAM_STATUS_NONE) {
        return;
    }

    gSPDisplayList(gDisplayListHead++, dl_hud_img_begin);
    render_hud_tex_lut(x, y, (*cameraLUT)[GLYPH_CAM_CAMERA]);

    switch (sCameraHUD.status & CAM_STATUS_MODE_GROUP) {
        case CAM_STATUS_MARIO:
            render_hud_tex_lut(x + 16, y, (*cameraLUT)[GLYPH_CAM_MARIO_HEAD]);
            break;
        case CAM_STATUS_LAKITU:
            render_hud_tex_lut(x + 16, y, (*cameraLUT)[GLYPH_CAM_LAKITU_HEAD]);
            break;
        case CAM_STATUS_FIXED:
            render_hud_tex_lut(x + 16, y, (*cameraLUT)[GLYPH_CAM_FIXED]);
            break;
    }

    switch (sCameraHUD.status & CAM_STATUS_C_MODE_GROUP) {
        case CAM_STATUS_C_DOWN:
            render_hud_small_tex_lut(x + 4, y + 16, (*cameraLUT)[GLYPH_CAM_ARROW_DOWN]);
            break;
        case CAM_STATUS_C_UP:
            render_hud_small_tex_lut(x + 4, y - 8, (*cameraLUT)[GLYPH_CAM_ARROW_UP]);
            break;
    }

    gSPDisplayList(gDisplayListHead++, dl_hud_img_end);
}

/**
 * Render HUD strings using hudDisplayFlags with it's render functions,
 * excluding the cannon reticle which detects a camera preset for it.
 */

void render_hud_bar(s32 x, s32 y, s32 width, s32 height, s32 s, s32 t) {
	s32 xl = MAX(0, x);
	s32 yl = MAX(0, y);
	s32 xh = MAX(0, x + width - 1);
	s32 yh = MAX(0, y + height - 1);
	s = (x < 0) ? s - x : s;
	t = (y < 0) ? t - y : t;
	gDPPipeSync(gDisplayListHead++);
	gDPSetCycleType(gDisplayListHead++, G_CYC_COPY);
	gDPSetTexturePersp(gDisplayListHead++, G_TP_NONE);
	gDPSetAlphaCompare(gDisplayListHead++, G_AC_THRESHOLD);
	gDPSetBlendColor(gDisplayListHead++, 255, 255, 255, 255);
	gDPSetRenderMode(gDisplayListHead++, G_RM_AA_XLU_SURF, G_RM_AA_XLU_SURF2);
	gDPSetTextureLUT(gDisplayListHead++, G_TT_RGBA16);
	gDPSetTextureImage(gDisplayListHead++, G_IM_FMT_RGBA, G_IM_SIZ_16b, 1, segmented_to_virtual(render_hud_bar_texture_pal_rgba16));
	gDPTileSync(gDisplayListHead++);
	gDPSetTile(gDisplayListHead++, 0, 0, 0, 256, 7, 0, G_TX_WRAP | G_TX_NOMIRROR, 0, 0, G_TX_WRAP | G_TX_NOMIRROR, 0, 0);
	gDPLoadSync(gDisplayListHead++);
	gDPLoadTLUTCmd(gDisplayListHead++, 7, 5);
	gDPPipeSync(gDisplayListHead++);
	gDPTileSync(gDisplayListHead++);
	gDPSetTextureImage(gDisplayListHead++, G_IM_FMT_CI, G_IM_SIZ_8b, 31, segmented_to_virtual(render_hud_bar_texture));
	gDPSetTile(gDisplayListHead++, G_IM_FMT_CI, G_IM_SIZ_8b, 4, 0, 7, 0, G_TX_WRAP | G_TX_NOMIRROR, 4, 0, G_TX_WRAP | G_TX_NOMIRROR, 6, 0);
	gDPLoadSync(gDisplayListHead++);
	gDPLoadTile(gDisplayListHead++, 7, 0, 0, 122, 36);
	gDPPipeSync(gDisplayListHead++);
	gDPSetTile(gDisplayListHead++, G_IM_FMT_CI, G_IM_SIZ_4b, 4, 0, 0, 0, G_TX_WRAP | G_TX_NOMIRROR, 4, 0, G_TX_WRAP | G_TX_NOMIRROR, 6, 0);
	gDPSetTileSize(gDisplayListHead++, 0, 0, 0, 244, 36);
	gSPScisTextureRectangle(gDisplayListHead++, xl << 2, yl << 2, xh << 2, yh << 2, 0, s << 5, t << 5,  4096, 1024);
	gDPSetTextureLUT(gDisplayListHead++, G_TT_NONE);
	gDPPipeSync(gDisplayListHead++);
	gDPSetCycleType(gDisplayListHead++, G_CYC_1CYCLE);
	gSPTexture(gDisplayListHead++, 65535, 65535, 0, G_TX_RENDERTILE, G_OFF);
	gDPSetTexturePersp(gDisplayListHead++, G_TP_PERSP);
	gDPSetAlphaCompare(gDisplayListHead++, G_AC_NONE);
	gDPSetRenderMode(gDisplayListHead++, G_RM_AA_ZB_OPA_SURF, G_RM_AA_ZB_OPA_SURF2);
}


void render_hud(void) {
    s16 hudDisplayFlags;
#ifdef VERSION_EU
    Mtx *mtx;
#endif

    hudDisplayFlags = gHudDisplay.flags;

    if (hudDisplayFlags == HUD_DISPLAY_NONE) {
        sPowerMeterHUD.animation = POWER_METER_HIDDEN;
        sPowerMeterStoredHealth = 3;
        sPowerMeterVisibleTimer = 0;
    } else {
#ifdef VERSION_EU
        // basically create_dl_ortho_matrix but guOrtho screen width is different

        mtx = alloc_display_list(sizeof(*mtx));
        if (mtx == NULL) {
            return;
        }
        create_dl_identity_matrix();
        guOrtho(mtx, -16.0f, SCREEN_WIDTH + 16, 0, SCREEN_HEIGHT, -10.0f, 10.0f, 1.0f);
        gSPPerspNormalize(gDisplayListHead++, 0xFFFF);
        gSPMatrix(gDisplayListHead++, VIRTUAL_TO_PHYSICAL(mtx),
                G_MTX_PROJECTION | G_MTX_MUL | G_MTX_NOPUSH);
#else
        create_dl_ortho_matrix();
#endif

        if (gCurrentArea != NULL && gCurrentArea->camera->mode == CAMERA_MODE_INSIDE_CANNON) {
            render_hud_cannon_reticle();
        }

        if (hudDisplayFlags & HUD_DISPLAY_FLAG_LIVES) {
            render_hud_mario_lives();
        }

        render_hud_info();

        // always render coins lmao
        //if (hudDisplayFlags & HUD_DISPLAY_FLAG_COIN_COUNT) {
        //    render_hud_coins();
        //}

        //if (hudDisplayFlags & HUD_DISPLAY_FLAG_STAR_COUNT) {
        //    render_hud_stars();
        //}

        if (hudDisplayFlags & HUD_DISPLAY_FLAG_KEYS) {
            render_hud_keys();
        }

        if (hudDisplayFlags & HUD_DISPLAY_FLAG_CAMERA_AND_POWER) {
            render_hud_power_meter();
            render_hud_camera_status();
        }

        if (hudDisplayFlags & HUD_DISPLAY_FLAG_TIMER) {
            render_hud_timer();
        }
        //render_hud_power_meter_three((s32) sPowerMeterHUD.x, (s32) sPowerMeterHUD.y, 64, 64, 1, 0);

        if (gSurfacePoolError & NOT_ENOUGH_ROOM_FOR_SURFACES)
        {
            print_text(10, 40, "SURFACE POOL FULL");
        }
        if (gSurfacePoolError & NOT_ENOUGH_ROOM_FOR_NODES)
        {
            print_text(10, 60, "SURFACE NODE POOL FULL");
        }
    }
}
