#include "mario_colors.h"
#include "engine/math_util.h"

u8 mario_shirt[][3] = {
    {0xff, 0x0f, 0x0f}, // Mario's shirt/cap
    {0xff, 0xff, 0xff}, // Mario's fire flower shirt/cap
    {0x0f, 0xff, 0x0f}, // Luigi's shirt/cap
    {0xff, 0x0f, 0xff}, // Rainbow
};
u8 mario_overalls[][3] = {
    {0x14, 0x14, 0x96}, // Mario's overalls
    {0xff, 0x0f, 0x0f}, // Mario's fire flower overalls
    {0x32, 0x14, 0x96}, // Luigi's overalls
    {0x14, 0x14, 0x96}, // Rainbow
};

u8 r = 255;
u8 g = 0;
u8 b = 0;

void update_mario_colors() {
    if (r == 255 && g < 255 && b == 0) g+=15;
    if (g == 255 && r > 0 && b == 0) r-=15;
    if (g == 255 && b < 255 && r == 0) b+=15;
    if (b == 255 && g > 0 && r == 0) g-=15;
    if (b == 255 && r < 255 && g == 0) r+=15;
    if (r == 255 && b > 0 && g == 0) b-=15;
    mario_shirt[MCOL_RAINBOW][0] = r;
    mario_shirt[MCOL_RAINBOW][1] = g;
    mario_shirt[MCOL_RAINBOW][2] = b;
}
