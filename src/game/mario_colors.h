#ifndef MARIOCOL_H
#define MARIOCOL_H

#include "macros.h"
#include "types.h"

enum mario_colors {
    MCOL_NORMAL,
    MCOL_FIRE,
    MCOL_LUIGI,
    MCOL_RAINBOW,
    MCOL_SMG4,
};

extern u8 mario_shirt[][3];
extern u8 mario_overalls[][3];

void update_mario_colors();

#endif // MARIOCOL_H
