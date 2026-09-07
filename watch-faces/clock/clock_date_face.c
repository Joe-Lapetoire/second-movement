/* SPDX-License-Identifier: MIT */

/*
 * MIT License
 *
 * Copyright © 2021-2023 Joey Castillo <joeycastillo@utexas.edu> <jose.castillo@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <stdlib.h>
#include "clock_date_face.h"
#include "watch.h"
#include "watch_utility.h"
#include "watch_common_display.h"

// 2.4 volts seems to offer adequate warning of a low battery condition?
#ifndef CLOCK_FACE_LOW_BATTERY_VOLTAGE_THRESHOLD
#define CLOCK_FACE_LOW_BATTERY_VOLTAGE_THRESHOLD 2400
#endif

static void clock_indicate(watch_indicator_t indicator, bool on) {
    if (on) {
        watch_set_indicator(indicator);
    } else {
        watch_clear_indicator(indicator);
    }
}

static void clock_indicate_alarm(void) {
    clock_indicate(WATCH_INDICATOR_SIGNAL, movement_alarm_enabled());
}

static void clock_indicate_time_signal(clock_date_state_t *state) {
    clock_indicate(WATCH_INDICATOR_BELL, state->time_signal_enabled);
}

static bool clock_is_pm(watch_date_time_t date_time) {
    return date_time.unit.hour >= 12;
}

static void clock_indicate_pm(watch_date_time_t date_time) {
    if (movement_clock_mode_24h()) { return; }
    clock_indicate(WATCH_INDICATOR_PM, clock_is_pm(date_time));
}

static void clock_indicate_low_available_power(clock_date_state_t *state) {
    if (watch_get_lcd_type() == WATCH_LCD_TYPE_CUSTOM) {
        clock_indicate(WATCH_INDICATOR_ARROWS, state->battery_low);
    } else {
        clock_indicate(WATCH_INDICATOR_LAP, state->battery_low);
    }
}

static watch_date_time_t clock_24h_to_12h(watch_date_time_t date_time) {
    date_time.unit.hour %= 12;
    if (date_time.unit.hour == 0) {
        date_time.unit.hour = 12;
    }
    return date_time;
}

static void clock_check_battery_periodically(clock_date_state_t *state, watch_date_time_t date_time) {
    if (date_time.unit.day == state->last_battery_check) { return; }

    state->last_battery_check = date_time.unit.day;

    uint16_t voltage = watch_get_vcc_voltage();
    state->battery_low = voltage < CLOCK_FACE_LOW_BATTERY_VOLTAGE_THRESHOLD;

    clock_indicate_low_available_power(state);
}

static void clock_toggle_time_signal(clock_date_state_t *state) {
    state->time_signal_enabled = !state->time_signal_enabled;
    clock_indicate_time_signal(state);
}

// ---- Mode-dependent indicators (colon + 24h) -------------------------------
// The colon only makes sense in time view (there's nothing to separate in
// date view), and per user preference the small "24h" indicator should be
// hidden entirely while showing the date/week view, even if the movement's
// clock mode is set to 24h.

static void clock_date_update_mode_indicators(clock_date_state_t *state) {
    if (state->show_date) {
        watch_clear_colon();
        watch_clear_indicator(WATCH_INDICATOR_24H);
    } else {
        watch_set_colon();
        clock_indicate(WATCH_INDICATOR_24H, !!movement_clock_mode_24h());
    }
}

// ---- Self-contained, verified ISO 8601 week number -------------------------
// watch_utility_get_weeknumber() is NOT used here: with the default
// use_iso_8601_weeknumber = 0 it happens to match ISO 8601 for most of the
// year, but both that setting and use_iso_8601_weeknumber = 1 return the
// wrong week number for the last few days of December in some years (they
// report week 1 instead of week 52/53). This local implementation has been
// checked against several reference dates, including year-end edge cases,
// and matches ISO 8601 in all of them.

static uint8_t clock_date_iso_weeks_in_year(uint16_t year) {
    uint8_t jan1_weekday = watch_utility_get_iso8601_weekday_number(year, 1, 1); // 1=Mon..7=Sun
    bool leap = (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
    // A year has 53 ISO weeks if Jan 1st is a Thursday, or if it's a leap
    // year and Jan 1st is a Wednesday.
    if (jan1_weekday == 4 || (leap && jan1_weekday == 3)) return 53;
    return 52;
}

static uint8_t clock_date_get_iso_weeknumber(watch_date_time_t date_time) {
    uint16_t year = date_time.unit.year + WATCH_RTC_REFERENCE_YEAR;
    uint8_t weekday = watch_utility_get_iso8601_weekday_number(year, date_time.unit.month, date_time.unit.day); // 1=Mon..7=Sun
    uint16_t ordinal = watch_utility_days_since_new_year(year, date_time.unit.month, date_time.unit.day);
    int16_t week = (ordinal - weekday + 10) / 7;

    if (week < 1) {
        return clock_date_iso_weeks_in_year(year - 1);
    } else if (week > clock_date_iso_weeks_in_year(year)) {
        return 1;
    }
    return (uint8_t) week;
}

// ---- Time view (HH:MM:SS, weekday, day of month) -------------------------

static void clock_display_time_all(watch_date_time_t date_time) {
    char buf[8 + 1];
    char day_buf[2 + 1];

    // day of month is always zero-padded, regardless of the movement's clock mode.
    snprintf(day_buf, sizeof(day_buf), "%02d", date_time.unit.day);

    snprintf(
        buf,
        sizeof(buf),
        movement_clock_mode_24h() == MOVEMENT_CLOCK_MODE_024H ? "%s%02d%02d%02d" : "%s%2d%02d%02d",
        day_buf,
        date_time.unit.hour,
        date_time.unit.minute,
        date_time.unit.second
    );

    watch_display_text_with_fallback(WATCH_POSITION_TOP_LEFT, watch_utility_get_long_weekday(date_time), watch_utility_get_weekday(date_time));
    watch_display_text(WATCH_POSITION_TOP_RIGHT, buf);
    watch_display_text(WATCH_POSITION_BOTTOM, buf + 2);
}

static bool clock_display_time_some(watch_date_time_t current, watch_date_time_t previous) {
    if ((current.reg >> 6) == (previous.reg >> 6)) {
        watch_display_character_lp_seconds('0' + current.unit.second / 10, 8);
        watch_display_character_lp_seconds('0' + current.unit.second % 10, 9);
        return true;
    } else if ((current.reg >> 12) == (previous.reg >> 12)) {
        char buf[4 + 1];
        snprintf(buf, sizeof(buf), "%02d%02d", current.unit.minute, current.unit.second);
        watch_display_text(WATCH_POSITION_MINUTES, buf);
        watch_display_text(WATCH_POSITION_SECONDS, buf + 2);
        return true;
    } else {
        return false;
    }
}

static void clock_display_time_low_energy(watch_date_time_t date_time) {
    if (movement_clock_mode_24h() == MOVEMENT_CLOCK_MODE_12H) {
        clock_indicate_pm(date_time);
        date_time = clock_24h_to_12h(date_time);
    }
    char buf[8 + 1];
    char day_buf[2 + 1];

    snprintf(day_buf, sizeof(day_buf), "%02d", date_time.unit.day);

    snprintf(
        buf,
        sizeof(buf),
        movement_clock_mode_24h() == MOVEMENT_CLOCK_MODE_024H ? "%s%02d%02d  " : "%s%2d%02d  ",
        day_buf,
        date_time.unit.hour,
        date_time.unit.minute
    );

    watch_display_text_with_fallback(WATCH_POSITION_TOP_LEFT, watch_utility_get_long_weekday(date_time), watch_utility_get_weekday(date_time));
    watch_display_text(WATCH_POSITION_TOP_RIGHT, buf);
    watch_display_text(WATCH_POSITION_BOTTOM, buf + 2);
}

// ---- Date view (DD MM YY, "SEM"/"SM", ISO week number) --------------------

static void clock_display_date(watch_date_time_t date_time) {
    char buf[6 + 1];
    char week_buf[2 + 1];
    uint16_t year = date_time.unit.year + WATCH_RTC_REFERENCE_YEAR;
    uint8_t week = clock_date_get_iso_weeknumber(date_time);

    snprintf(buf, sizeof(buf), "%02d%02d%02d", date_time.unit.day, date_time.unit.month, year % 100);
    snprintf(week_buf, sizeof(week_buf), "%02d", week);

    watch_display_text_with_fallback(WATCH_POSITION_TOP_LEFT, "SEM", "SM");
    watch_display_text(WATCH_POSITION_TOP_RIGHT, week_buf);
    watch_display_text(WATCH_POSITION_BOTTOM, buf);
}

// ---- Dispatchers -----------------------------------------------------------

static void clock_display_clock(clock_date_state_t *state, watch_date_time_t current) {
    if (state->show_date) {
        // date only changes once a day: skip the redraw unless day/month/year moved.
        if ((current.reg >> 17) != (state->date_time.previous.reg >> 17)) {
            clock_display_date(current);
        }
        return;
    }

    if (!clock_display_time_some(current, state->date_time.previous)) {
        if (movement_clock_mode_24h() == MOVEMENT_CLOCK_MODE_12H) {
            clock_indicate_pm(current);
            current = clock_24h_to_12h(current);
        }
        clock_display_time_all(current);
    }
}

static void clock_display_low_energy_dispatch(clock_date_state_t *state, watch_date_time_t date_time) {
    if (state->show_date) {
        clock_display_date(date_time);
    } else {
        clock_display_time_low_energy(date_time);
    }
}

static void clock_start_tick_tock_animation(bool with_colon) {
    if (!watch_sleep_animation_is_running()) {
        watch_start_sleep_animation(500);
        if (with_colon) {
            watch_start_indicator_blink_if_possible(WATCH_INDICATOR_COLON, 500);
        }
    }
}

static void clock_stop_tick_tock_animation(void) {
    if (watch_sleep_animation_is_running()) {
        watch_stop_sleep_animation();
        watch_stop_blink();
    }
}

// ---- Watch face lifecycle ---------------------------------------------------

void clock_date_face_setup(uint8_t watch_face_index, void ** context_ptr) {
    (void) watch_face_index;

    if (*context_ptr == NULL) {
        *context_ptr = malloc(sizeof(clock_date_state_t));
        clock_date_state_t *state = (clock_date_state_t *) *context_ptr;
        state->time_signal_enabled = false;
        state->watch_face_index = watch_face_index;
        state->show_date = false;
    }
}

void clock_date_face_activate(void *context) {
    clock_date_state_t *state = (clock_date_state_t *) context;

    clock_stop_tick_tock_animation();

    clock_indicate_time_signal(state);
    clock_indicate_alarm();
    clock_date_update_mode_indicators(state);

    // this ensures that none of the timestamp fields will match, so we can re-render them all.
    state->date_time.previous.reg = 0xFFFFFFFF;
}

bool clock_date_face_loop(movement_event_t event, void *context) {
    clock_date_state_t *state = (clock_date_state_t *) context;
    watch_date_time_t current;

    switch (event.event_type) {
        case EVENT_LOW_ENERGY_UPDATE:
            clock_start_tick_tock_animation(!state->show_date);
            clock_display_low_energy_dispatch(state, movement_get_local_date_time());
            break;
        case EVENT_TICK:
        case EVENT_ACTIVATE:
            current = movement_get_local_date_time();

            clock_display_clock(state, current);

            clock_check_battery_periodically(state, current);

            state->date_time.previous = current;

            break;
        case EVENT_ALARM_BUTTON_UP:
            // short press: toggle between time view and date view.
            state->show_date = !state->show_date;

            clock_date_update_mode_indicators(state);

            // force a full redraw of whichever view we just switched to.
            state->date_time.previous.reg = 0xFFFFFFFF;
            clock_display_clock(state, movement_get_local_date_time());
            break;
        case EVENT_ALARM_LONG_PRESS:
            clock_toggle_time_signal(state);
            break;
        case EVENT_BACKGROUND_TASK:
            movement_play_signal();
            break;
        default:
            return movement_default_loop_handler(event);
    }

    return true;
}

void clock_date_face_resign(void *context) {
    (void) context;
}

movement_watch_face_advisory_t clock_date_face_advise(void *context) {
    movement_watch_face_advisory_t retval = { 0 };
    clock_date_state_t *state = (clock_date_state_t *) context;

    if (state->time_signal_enabled) {
        watch_date_time_t date_time = movement_get_local_date_time();
        retval.wants_background_task = date_time.unit.minute == 0;
    }

    return retval;
}
