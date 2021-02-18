#ifndef MEM_ERROR_SCREEN_H
#define MEM_ERROR_SCREEN_H

#include <types.h>

extern u8 gCoinAnimRemovalDelay;
extern s32 gCoinAnimGoal;
extern u8 gCoinAnimRunOnce;

void thread5_mem_error_message_loop(UNUSED void *arg);
u8 does_pool_end_lie_out_of_bounds(void *end);
s32 lvl_respawn_from_coins(UNUSED s32 arg);

#endif