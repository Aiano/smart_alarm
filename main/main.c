#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

#include "matrix_keyboard.h"
#include "main.h"

// ssd1306 headers
#include "ssd1306.h"
#include "ssd1306_draw.h"
#include "ssd1306_font.h"
#include "ssd1306_default_if.h"


// buzzer
#define BUZZER_PIN 15
#define BUZZER_SOUND_TIME 5000

// clock mode
smart_clock_mode g_mode = NORMAL;

// ssd1306 parameters
static const int I2CDisplayAddress = 0x3C;
static const int I2CDisplayWidth = 128;
static const int I2CDisplayHeight = 64;
static const int I2CResetPin = -1;
struct SSD1306_Device I2CDisplay;
// matrix keyboard
struct matrix_keyboard mk = {{CONFIG_OUTPUT_PIN_1, CONFIG_OUTPUT_PIN_2, CONFIG_OUTPUT_PIN_3, CONFIG_OUTPUT_PIN_4},
                             {CONFIG_INPUT_PIN_1, CONFIG_INPUT_PIN_2, CONFIG_INPUT_PIN_3, CONFIG_INPUT_PIN_4}};

char clock_content[20] = "00:00:00";
int clock_hour = 0;
int clock_minute = 0;
int clock_second = 0;

// normal mode
int normal_hour = 0;
int normal_minute = 0;
int normal_second = 0;

// timer mode
bool is_timer_set = false;
int timer_hour = 0;
int timer_minute = 0;
int timer_second = 0;

// alarm mode
bool is_alarm_set = false;
int alarm_hour = 0;
int alarm_minute = 0;
int alarm_second = 0;

void buzzer_init(){
    unsigned long long buzzer_pin_sel = 0;
    buzzer_pin_sel |= (1ULL << BUZZER_PIN);

    gpio_config_t io_conf;

    //disable interrupt
    io_conf.intr_type = GPIO_INTR_DISABLE;
    //set as output mode
    io_conf.mode = GPIO_MODE_OUTPUT;
    //bit mask of the pins that you want to set,e.g.GPIO18/19
    io_conf.pin_bit_mask = buzzer_pin_sel;
    //disable pull-down mode
    io_conf.pull_down_en = 1;
    //disable pull-up mode
    io_conf.pull_up_en = 0;
    //configure GPIO with the given settings
    gpio_config(&io_conf);
}

void buzzer_set_state(bool state){
    gpio_set_level(BUZZER_PIN,(int)state);
}

/**
 * @brief Initialize ssd1306 module
 * 
 * @param DisplayHandle 
 * @param Font 
 */
void InitSSD1306(struct SSD1306_Device *DisplayHandle, const struct SSD1306_FontDef *Font)
{

    assert(SSD1306_I2CMasterAttachDisplayDefault(&I2CDisplay, I2CDisplayWidth, I2CDisplayHeight, I2CDisplayAddress, I2CResetPin) == true);

    SSD1306_Clear(DisplayHandle, SSD_COLOR_BLACK);
    SSD1306_SetFont(DisplayHandle, Font);
    SSD1306_SetVFlip(DisplayHandle, true);
    SSD1306_SetHFlip(DisplayHandle, true);
}

void show(const char *title, int show_hour, int show_minute, int show_second, bool use_cursor, int position)
{
    // fill character matrix
    clock_content[0] = '0' + show_hour / 10;
    clock_content[1] = '0' + show_hour % 10;

    clock_content[2] = ' ';

    clock_content[3] = '0' + show_minute / 10;
    clock_content[4] = '0' + show_minute % 10;

    clock_content[5] = ' ';

    clock_content[6] = '0' + show_second / 10;
    clock_content[7] = '0' + show_second % 10;

    // set cursor
    if (use_cursor)
    {
        clock_content[position] = ' ';
    }

    // Clear buffer
    SSD1306_Clear(&I2CDisplay, SSD_COLOR_BLACK);

    // set title
    SSD1306_SetFont(&I2CDisplay, &Font_droid_sans_fallback_11x13);
    SSD1306_FontDrawString(&I2CDisplay, 0, 0, title, SSD_COLOR_WHITE);

    // fill time content
    SSD1306_SetFont(&I2CDisplay, &Font_Tarable7Seg_16x32);
    SSD1306_FontDrawString(&I2CDisplay, 0, 20, clock_content, SSD_COLOR_WHITE);

    // update screen
    SSD1306_Update(&I2CDisplay);
}

void normal_mode()
{
    normal_second++;
    if (normal_second >= 60)
    {
        normal_second = 0;
        normal_minute++;
    }
    if (normal_minute >= 60)
    {
        normal_minute = 0;
        normal_hour++;
    }
    if (normal_hour >= 100)
    {
        normal_hour = 0;
    }

    if (!is_setting_time)
    {
        clock_hour = normal_hour;
        clock_minute = normal_minute;
        clock_second = normal_second;
    }
    else
    {
        normal_hour = clock_hour;
        normal_minute = clock_minute;
        normal_second = clock_second;
    }

    if (!is_setting_time && normal_hour == alarm_hour && normal_minute == alarm_minute && normal_second == alarm_second)
    {
        // sound the alarm
        buzzer_set_state(true);
        printf("Sound the alarm!\n");
        vTaskDelay(BUZZER_SOUND_TIME / portTICK_PERIOD_MS);
        buzzer_set_state(false);
    }

    char title[30];
    static bool use_cursor = false;
    if (is_setting_time)
    {
        strcpy(title, "Normal Mode:Setting Time:");
        use_cursor = !use_cursor;
    }
    else
    {
        use_cursor = false;
        strcpy(title, "Normal Mode:");
    }
    show(title, clock_hour, clock_minute, clock_second, use_cursor, cursor_position);

    // delay a second
    vTaskDelay(1000 / portTICK_PERIOD_MS);
}

void alarm_mode()
{
    char title[30];
    strcpy(title, "Alarm Mode:");

    alarm_hour = clock_hour;
    alarm_minute = clock_minute;
    alarm_second = clock_second;

    if (is_alarm_set)
    {
        g_mode = NORMAL;
        return;
    }
    is_alarm_set = false;

    static bool alarm_use_cursor = false;
    alarm_use_cursor = !alarm_use_cursor;
    show(title, clock_hour, clock_minute, clock_second, alarm_use_cursor, cursor_position);
}

void timer_mode()
{
    char title[30];
    strcpy(title, "Timer Mode:");

    if (is_timer_set)
    {
        while (clock_hour || clock_minute || clock_second)
        {
            clock_second--;
            if (clock_second < 0)
            {
                clock_second = 59;
                clock_minute--;
            }
            if (clock_minute < 0)
            {
                clock_minute = 59;
                clock_hour--;
            }
            if (clock_hour < 0)
            {
                printf("%d\n", clock_hour);
                clock_hour = 0;
                clock_minute = 0;
                clock_second = 0;
                break;
            }
            show(title, clock_hour, clock_minute, clock_second, false, cursor_position);
            vTaskDelay(1000 / portTICK_PERIOD_MS);
        }
        // sound the alarm
        buzzer_set_state(true);
        printf("Sound the alarm!\n");
        vTaskDelay(BUZZER_SOUND_TIME / portTICK_PERIOD_MS);
        buzzer_set_state(false);

        is_timer_set = false;
    }
    else
    {
        timer_hour = clock_hour;
        timer_minute = clock_minute;
        timer_second = clock_second;

        static bool timer_use_cursor = false;
        timer_use_cursor = !timer_use_cursor;
        show(title, clock_hour, clock_minute, clock_second, timer_use_cursor, cursor_position);
    }
}


void app_main(void)
{
    printf("Startup...\n");

    assert(SSD1306_I2CMasterInitDefault() == true);
    InitSSD1306(&I2CDisplay, &Font_droid_sans_fallback_24x28);
    matrix_keyboard_init(&mk);
    buzzer_init();

    printf("Done!\n");

    while (1)
    {

        switch (g_mode)
        {
        case NORMAL:
            normal_mode();
            break;
        case ALARM:
            alarm_mode();
            break;
        case TIMER:
            timer_mode();
            break;
        default:
            break;
        }
    }
}
