/* clang-format off */
/*
 * mem_error_screen.inc.c
 *
 * This enhancement should be used for ROM hacks that require the expansion pak.
 *
 */
/* clang-format on */

#include <types.h>
#include "segments.h"
#include "text_strings.h"
#include "game_init.h"
#include "main.h"
#include "print.h"
#include "ingame_menu.h"
#include "segment2.h"
#include "level_update.h"
#include "../engine/level_script.h"
#include "seq_ids.h"
#include "audio/external.h"
#include "include/sounds.h"

gCoinAnimRemovalDelay = 0;
gCoinAnimGoal = 0;
gCoinAnimRunOnce = FALSE;


s32 lvl_respawn_from_coins(UNUSED s32 arg) {
    level_trigger_warp(gMarioState, WARP_OP_RESPAWN);
    return 0;
}

Gfx *geo18_display_coin_anim(u32 run, UNUSED struct GraphNode *sp44, UNUSED u32 sp48) {
    if (run) {
        if (!gCoinAnimRunOnce) {
            gCoinAnimRunOnce = TRUE;
            gCoinAnimGoal = gMarioState->numCoins - 10;
            gHudDisplay.coins = gMarioState->numCoins;
        }
   
        if (gCoinAnimRemovalDelay > 20 && gCoinAnimRemovalDelay % 2 == 0) {
            if (gMarioState->numCoins > gCoinAnimGoal) {
                gMarioState->numCoins--;
                gHudDisplay.coins = gMarioState->numCoins;
                play_sound(SOUND_GENERAL_COIN, gGlobalSoundSource);
            }
        }

        if (gCoinAnimRemovalDelay > 60) {
            level_trigger_warp(gMarioState, WARP_OP_RESPAWN);
            print_text_centered(160, 170, "FUCK");
        }

        char buf[10];
        sprintf(buf, "+ %d", gHudDisplay.coins);

        print_text_centered(160, 120, buf);

        gCoinAnimRemovalDelay++;
    }
    return 0;
}
