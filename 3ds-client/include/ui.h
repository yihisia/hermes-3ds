#ifndef UI_H
#define UI_H

#include <3ds.h>

void ui_init(void);
void ui_draw(int state, const char* input, const char* response,
             const char* status, int scroll_offset);
void ui_show_error(const char* msg);

#endif // UI_H
