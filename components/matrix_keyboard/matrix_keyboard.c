#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "matrix_keyboard.h"
#include "driver/gpio.h"

int cursor_position = 0;
int is_setting_time = 1;

const char keyboard_mapping[4][4] = {{'1', '2', '3', 'L'},
                                     {'4', '5', '6', 'R'},
                                     {'7', '8', '9', 'U'},
                                     {'*', '0', '#', 'D'}};

// gpio event queue
static xQueueHandle gpio_evt_queue = NULL;

// keyboard variables
struct matrix_keyboard g_device;
int g_row;
int g_old_col = -1;
int g_old_row = -1;

// interrupt service function
static void IRAM_ATTR gpio_isr_handler(void *arg)
{
    uint32_t col = (uint32_t)arg;
    xQueueSendFromISR(gpio_evt_queue, &col, NULL);
}

static void gpio_interrupt_task(void *arg)
{
    uint32_t col;
    for (;;)
    {
        if (xQueueReceive(gpio_evt_queue, &col, portMAX_DELAY))
        {
            printf("row:%d col:%d key:%c\n", g_row, col, keyboard_mapping[g_row - 1][col - 1]);
            if (keyboard_mapping[g_row - 1][col - 1] == 'L')
            {
                if (cursor_position > 0)
                {
                    if (cursor_position == 3 || cursor_position == 6)
                        cursor_position -= 2;
                    else
                        cursor_position -= 1;
                }
            }
            else if (keyboard_mapping[g_row - 1][col - 1] == 'R')
            {
                if (cursor_position < 7)
                {
                    if (cursor_position == 1 || cursor_position == 4)
                        cursor_position += 2;
                    else
                        cursor_position += 1;
                }
            }
            else if (keyboard_mapping[g_row - 1][col - 1] == 'U')
            {
                g_mode = (g_mode + 1) % 3;
            }
            else if (keyboard_mapping[g_row - 1][col - 1] == 'D')
            {
                g_mode = (g_mode + 2) % 3;
            }
            else if (keyboard_mapping[g_row - 1][col - 1] >= '0' && keyboard_mapping[g_row - 1][col - 1] <= '9')
            {
                int num = keyboard_mapping[g_row - 1][col - 1] - '0';
                switch (cursor_position)
                {
                case 0:
                    clock_hour = num * 10 + clock_hour % 10;
                    break;
                case 1:
                    clock_hour = clock_hour / 10 * 10 + num;
                    break;
                case 3:
                    clock_minute = num * 10 + clock_minute % 10;
                    break;
                case 4:
                    clock_minute = clock_minute / 10 * 10 + num;
                    break;
                case 6:
                    clock_second = num * 10 + clock_second % 10;
                    break;
                case 7:
                    clock_second = clock_second / 10 * 10 + num;
                    break;
                default:
                    break;
                }
            }
            else if (keyboard_mapping[g_row - 1][col - 1] == '#')
            {
                switch (g_mode)
                {
                case NORMAL:
                    is_setting_time = 1;
                    break;
                case TIMER:
                    is_timer_set = true;
                    break;
                case ALARM:
                    is_alarm_set = true;
                    break;
                default:
                    break;
                }
            }
            else if (keyboard_mapping[g_row - 1][col - 1] == '*')
            {
                switch (g_mode)
                {
                case NORMAL:
                    is_setting_time = 0;
                    break;
                case TIMER:
                    is_timer_set = false;
                    break;
                case ALARM:
                    is_alarm_set = false;
                    break;
                default:
                    break;
                }
            }
        }
    }
}

static void scan_task(void *arg)
{
    while (1)
    {
        for (int i = 0; i < 4; i++)
        {
            gpio_set_level(g_device.output_pins[i], 1);
            g_row = i + 1;
            vTaskDelay(50 / portTICK_PERIOD_MS);
            gpio_set_level(g_device.output_pins[i], 0);
        }
    }
}

void matrix_keyboard_init(struct matrix_keyboard *device)
{
    g_device = *device;
    unsigned long long gpio_output_pin_sel = 0;
    unsigned long long gpio_input_pin_sel = 0;
    for (int i = 0; i < 4; i++)
    {
        gpio_output_pin_sel |= (1ULL << device->output_pins[i]);
    }
    for (int i = 0; i < 4; i++)
    {
        gpio_input_pin_sel |= (1ULL << device->input_pins[i]);
    }

    gpio_config_t io_conf;

    /* config output pins */
    //disable interrupt
    io_conf.intr_type = GPIO_INTR_DISABLE;
    //set as output mode
    io_conf.mode = GPIO_MODE_OUTPUT;
    //bit mask of the pins that you want to set,e.g.GPIO18/19
    io_conf.pin_bit_mask = gpio_output_pin_sel;
    //disable pull-down mode
    io_conf.pull_down_en = 0;
    //disable pull-up mode
    io_conf.pull_up_en = 0;
    //configure GPIO with the given settings
    gpio_config(&io_conf);

    /* config input pins with interrupt */
    //interrupt of rising edge
    io_conf.intr_type = GPIO_INTR_POSEDGE;
    //bit mask of the pins, use GPIO4/5 here
    io_conf.pin_bit_mask = gpio_input_pin_sel;
    //set as input mode
    io_conf.mode = GPIO_MODE_INPUT;
    //disable pull-up mode
    io_conf.pull_up_en = 0;
    //enable pull-down mode
    io_conf.pull_down_en = 1;
    gpio_config(&io_conf);

    //create a queue to handle gpio event from isr
    gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));
    //start gpio task
    xTaskCreate(gpio_interrupt_task, "gpio_interrupt_task", 2048, NULL, 9, NULL);
    xTaskCreate(scan_task, "scan_task", 2048, NULL, 10, NULL);

    //install gpio isr service
    gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
    //hook isr handler for specific gpio pin
    for (int i = 0; i < 4; i++)
    {
        gpio_isr_handler_add((device->input_pins)[i], gpio_isr_handler, (void *)(i + 1));
    }

    for (int i = 0; i < 4; i++)
        gpio_set_level(device->output_pins[i], 0);
}
