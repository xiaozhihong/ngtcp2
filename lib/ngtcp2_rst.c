/*
 * ngtcp2
 *
 * Copyright (c) 2019 ngtcp2 contributors
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
#include "ngtcp2_rst.h"
#include "ngtcp2_rtb.h"
#include "ngtcp2_cc.h"
#include "ngtcp2_macro.h"

void ngtcp2_rs_init(ngtcp2_rs *rs) {
  rs->interval = UINT64_MAX;
  rs->delivered = 0;
  rs->prior_delivered = 0;
  rs->prior_ts = 0;
  rs->send_elapsed = 0;
  rs->ack_elapsed = 0;
  rs->is_app_limited = 0;
}

void ngtcp2_rst_init(ngtcp2_rst *rst) {
  ngtcp2_rs_init(&rst->rs);
  rst->delivered = 0;
  rst->delivered_ts = 0;
  rst->first_sent_ts = 0;
  rst->app_limited = 0;
}

void ngtcp2_rst_on_pkt_sent(ngtcp2_rst *rst, ngtcp2_rtb_entry *ent,
                            const ngtcp2_conn_stat *cstat) {
  if (cstat->bytes_in_flight == 0) {
    rst->first_sent_ts = rst->delivered_ts = ent->ts;
  }
  ent->rst.first_sent_ts = rst->first_sent_ts;
  ent->rst.delivered_ts = rst->delivered_ts;
  ent->rst.delivered = rst->delivered;
  ent->rst.is_app_limited = rst->app_limited != 0;
}

int ngtcp2_rst_on_ack_recv(ngtcp2_rst *rst, ngtcp2_conn_stat *cstat) {
  ngtcp2_rs *rs = &rst->rs;

  if (rst->app_limited && rst->delivered > rst->app_limited) {
    rst->app_limited = 0;
  }

  cstat->app_limited = rst->app_limited;

  if (rs->prior_ts == 0) {
    return 0;
  }

  rs->interval = ngtcp2_max(rs->send_elapsed, rs->ack_elapsed);

  rs->delivered = rst->delivered - rs->prior_delivered;

  if (rs->interval < cstat->min_rtt) {
    rs->interval = UINT64_MAX;
    return 0;
  }

  if (rs->interval) {
    cstat->delivered = rs->delivered;
    cstat->delivery_rate_sec = rs->delivered * NGTCP2_SECONDS / rs->interval;
  }

  return 0;
}

void ngtcp2_rst_update_rate_sample(ngtcp2_rst *rst, const ngtcp2_rtb_entry *ent,
                                   ngtcp2_tstamp ts) {
  ngtcp2_rs *rs = &rst->rs;

  rst->delivered += ent->pktlen;
  rst->delivered_ts = ts;

  if (ent->rst.delivered > rs->prior_delivered) {
    rs->prior_delivered = ent->rst.delivered;
    rs->prior_ts = ent->rst.delivered_ts;
    rs->is_app_limited = ent->rst.is_app_limited;
    rs->send_elapsed = ent->ts - ent->rst.first_sent_ts;
    rs->ack_elapsed = rst->delivered_ts - ent->rst.delivered_ts;
    rst->first_sent_ts = ent->ts;
  }
}

void ngtcp2_rst_update_app_limited(ngtcp2_rst *rst, ngtcp2_conn_stat *cstat) {
  (void)rst;
  (void)cstat;
  /* TODO Not implemented */
}
