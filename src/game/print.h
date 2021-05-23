#ifndef PRINT_H
#define PRINT_H

#include <PR/ultratypes.h>

#define TEXRECT_MIN_X 10
#define TEXRECT_MAX_X 300
#define TEXRECT_MIN_Y 5
#define TEXRECT_MAX_Y 220

#define GLYPH_SPACE           -1
#define GLYPH_U               30
#define GLYPH_EXCLAMATION_PNT 36
#define GLYPH_TWO_EXCLAMATION 37
#define GLYPH_QUESTION_MARK   38
#define GLYPH_AMPERSAND       39
#define GLYPH_PERCENT         40
#define GLYPH_MULTIPLY        50
#define GLYPH_COIN            51
#define GLYPH_MARIO_HEAD      52
#define GLYPH_STAR            53
#define GLYPH_PERIOD          54
#define GLYPH_BETA_KEY        55
#define GLYPH_APOSTROPHE      56
#define GLYPH_DOUBLE_QUOTE    57

#ifdef VERSION_EU
#define GLYPH_UMLAUT          58
#define NUM_ZERO              59
#define NUM_ONE               60
#define NUM_TWO               61
#define NUM_THREE             62
#define NUM_FOUR              63
#define NUM_FIVE              64
#define NUM_SIX               65
#define NUM_SEVEN             66
#define NUM_EIGHT             67
#define NUM_NINE              68

#else

#define NUM_ZERO              58
#define NUM_ONE               59
#define NUM_TWO               60
#define NUM_THREE             61
#define NUM_FOUR              62
#define NUM_FIVE              63
#define NUM_SIX               64
#define NUM_SEVEN             65
#define NUM_EIGHT             66
#define NUM_NINE              67
#endif

void print_text_fmt_int(s32 x, s32 y, const char *str, s32 n);
void print_text(s32 x, s32 y, const char *str);
void print_hud_number(s32 x, s32 y, s32 number);
void print_text_centered(s32 x, s32 y, const char *str);
void render_text_labels(void);

#endif // PRINT_H
