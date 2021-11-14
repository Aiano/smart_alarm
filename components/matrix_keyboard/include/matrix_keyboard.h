#include "sdkconfig.h"
#include "main.h"

#define ESP_INTR_FLAG_DEFAULT 0

extern int cursor_position;

extern int is_setting_time;

struct matrix_keyboard
{
    /* output pins */
    int output_pins[4];
    /* input pins */
    int input_pins[4];
};


void matrix_keyboard_init(struct matrix_keyboard *device);
