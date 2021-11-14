#pragma once


typedef enum{
    NORMAL = 0,
    ALARM,
    TIMER
}smart_clock_mode;

extern int clock_hour;
extern int clock_minute;
extern int clock_second;

extern smart_clock_mode g_mode;

extern bool is_timer_set;
extern bool is_alarm_set;