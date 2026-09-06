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

#ifndef CLOCK_DATE_FACE_H_
#define CLOCK_DATE_FACE_H_

/*
 * CLOCK + DATE FACE
 *
 * Behaves exactly like the standard clock face, but adds a short press of
 * ALARM to toggle the bottom line between two views:
 *
 *   - Time view (default): HH:MM:SS, weekday top-left, day-of-month top-right.
 *   - Date view: DD MM YY (no separators), "SEM"/"SM" top-left instead of the
 *     weekday, and the ISO week number top-right instead of the day-of-month.
 *
 * Long-press ALARM still toggles the hourly chime, exactly like clock_face.
 */

#include "movement.h"

typedef struct {
    struct {
        watch_date_time_t previous;
    } date_time;
    uint8_t last_battery_check;
    uint8_t watch_face_index;
    bool time_signal_enabled;
    bool battery_low;
    bool show_date;
} clock_date_state_t;

void clock_date_face_setup(uint8_t watch_face_index, void ** context_ptr);
void clock_date_face_activate(void *context);
bool clock_date_face_loop(movement_event_t event, void *context);
void clock_date_face_resign(void *context);
movement_watch_face_advisory_t clock_date_face_advise(void *context);

#define clock_date_face ((const watch_face_t) { \
    clock_date_face_setup, \
    clock_date_face_activate, \
    clock_date_face_loop, \
    clock_date_face_resign, \
    clock_date_face_advise, \
})

#endif // CLOCK_DATE_FACE_H_
