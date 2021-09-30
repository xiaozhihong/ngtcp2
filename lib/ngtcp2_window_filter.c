/*
 * ngtcp2
 *
 * Copyright (c) 2021 ngtcp2 contributors
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/*
 * Translated to C from the original C++ code
 * https://quiche.googlesource.com/quiche/+/5be974e29f7e71a196e726d6e2272676d33ab77d/quic/core/congestion_control/windowed_filter.h
 * with the following license:
 *
 * // Copyright (c) 2016 The Chromium Authors. All rights reserved.
 * // Use of this source code is governed by a BSD-style license that can be
 * // found in the LICENSE file.
 */
#include "ngtcp2_window_filter.h"

#include <string.h>

void ngtcp2_window_filter_init(ngtcp2_window_filter *m, uint64_t t, uint64_t meas) {
    ngtcp2_window_filter_sample val = { .t = t, .v = meas };
    m->s[2] = m->s[1] = m->s[0] = val;
}

uint64_t ngtcp2_window_filter_reset(ngtcp2_window_filter *m, uint64_t t, uint64_t meas)
{
    ngtcp2_window_filter_sample val = { .t = t, .v = meas };

	m->s[2] = m->s[1] = m->s[0] = val;
	return m->s[0].v;
}


/* As time advances, update the 1st, 2nd, and 3rd choices. */
uint64_t ngtcp2_window_filter_update(ngtcp2_window_filter *m, uint64_t win, const ngtcp2_window_filter_sample* val) {
    uint64_t dt = val->t - m->s[0].t;

    if ((dt > win)) {
        /*
         * Passed entire window without a new val so make 2nd
         * choice the new val & 3rd choice the new 2nd choice.
         * we may have to iterate this since our 2nd choice
         * may also be outside the window (we checked on entry
         * that the third choice was in the window).
         */
        m->s[0] = m->s[1];
        m->s[1] = m->s[2];
        m->s[2] = *val;
        if ((val->t - m->s[0].t > win)) {
            m->s[0] = m->s[1];
            m->s[1] = m->s[2];
            m->s[2] = *val;
        }
    } else if ((m->s[1].t == m->s[0].t) && dt > win/4) {
        /*
         * We've passed a quarter of the window without a new val
         * so take a 2nd choice from the 2nd quarter of the window.
         */
        m->s[2] = m->s[1] = *val;
    } else if ((m->s[2].t == m->s[1].t) && dt > win/2) {
        /*
         * We've passed half the window without finding a new val
         * so take a 3rd choice from the last half of the window
         */
        m->s[2] = *val;
    }
    return m->s[0].v;
}

/* Check if new measurement updates the 1st, 2nd or 3rd choice max. */
uint64_t ngtcp2_window_filter_running_max(struct ngtcp2_window_filter *m, uint64_t win, uint64_t t, uint64_t meas)
{
    ngtcp2_window_filter_sample val = { .t = t, .v = meas };

    if ((val.v >= m->s[0].v) ||      /* found new max? */
        (val.t - m->s[2].t > win))      /* nothing left in window? */
        return ngtcp2_window_filter_reset(m, t, meas);  /* forget earlier samples */

    if ((val.v >= m->s[1].v))
        m->s[2] = m->s[1] = val;
    else if ((val.v >= m->s[2].v))
        m->s[2] = val;

    return ngtcp2_window_filter_update(m, win, &val);
}

uint64_t ngtcp2_window_filter_get_max(struct ngtcp2_window_filter* m) {
    return m->s[0].v;
}
