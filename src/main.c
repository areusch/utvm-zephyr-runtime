/*
 * Copyright (c) 2012-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>
#include <kernel.h>
#include <console/console.h>
#include <sys/printk.h>
#include <tvm/runtime/micro/micro_rpc_server.h>
#include <tvm/runtime/crt/logging.h>

#include "crt_config.h"

ssize_t write_serial(void* unused_context, const uint8_t* data, size_t size) {
  return console_write(NULL, data, size);
}

void TVMPlatformAbort(int error) {
  for (;;) ;
}

uint32_t g_utvm_start_time;

#define MILLIS_TIL_EXPIRY 200
#define TIME_TIL_EXPIRY (K_MSEC(MILLIS_TIL_EXPIRY))
K_TIMER_DEFINE(g_utvm_timer, /* expiry func */ NULL, /* stop func */ NULL);

int g_utvm_timer_running = 0;

int TVMPlatformTimerStart() {
  if (g_utvm_timer_running) {
    TVMLogf("timer already running");
    return -1;
  }
  k_timer_start(&g_utvm_timer, TIME_TIL_EXPIRY, TIME_TIL_EXPIRY);
  g_utvm_start_time = k_cycle_get_32();
  g_utvm_timer_running = 1;
  return 0;
}

int TVMPlatformTimerStop(double* res_us) {
  if (!g_utvm_timer_running) {
    TVMLogf("timer not running");
    return -1;
  }

  uint32_t stop_time = k_cycle_get_32();

  // compute how long the work took
  uint32_t cycles_spent = stop_time - g_utvm_start_time;
  if (stop_time < g_utvm_start_time) {
      // we rolled over *at least* once, so correct the rollover it was *only*
      // once, because we might still use this result
      cycles_spent = ~((uint32_t) 0) - (g_utvm_start_time - stop_time);
  }

  uint32_t ns_spent = (uint32_t) k_cyc_to_ns_floor64(cycles_spent);
  double hw_clock_res_us = ns_spent / 1000.0;

  // need to grab time remaining *before* stopping. when stopped, this function
  // always returns 0.
  int32_t time_remaining_ms = k_timer_remaining_get(&g_utvm_timer);
  k_timer_stop(&g_utvm_timer);
  // check *after* stopping to prevent extra expiries on the happy path
  if (time_remaining_ms < 0) {
    TVMLogf("negative time remaining");
    return -1;
  }
  uint32_t num_expiries = k_timer_status_get(&g_utvm_timer);
  uint32_t timer_res_ms = ((num_expiries * MILLIS_TIL_EXPIRY) + time_remaining_ms);
  double approx_num_cycles = (double) k_ticks_to_cyc_floor32(1) * (double) k_ms_to_ticks_ceil32(timer_res_ms);
  // if we approach the limits of the HW clock datatype (uint32_t), use the
  // coarse-grained timer result instead
  if (approx_num_cycles > (0.5 * (~((uint32_t) 0)))) {
    *res_us = timer_res_ms * 1000.0;
  } else {
    *res_us = hw_clock_res_us;
  }

  g_utvm_timer_running = 0;
  return 0;
}

#define WORKSPACE_SIZE_BYTES (120 * 1024)
#define WORKSPACE_PAGE_SIZE_BYTES_LOG2 8

uint8_t workspace[WORKSPACE_SIZE_BYTES];

void main(void)
{
    console_init();
    utvm_rpc_server_t server = utvm_rpc_server_init(
      workspace, WORKSPACE_SIZE_BYTES, WORKSPACE_PAGE_SIZE_BYTES_LOG2, write_serial, NULL);
    TVMLogf("uTVM On-Device Runtime");

    while (true)
    {
        uint8_t c = console_getchar();
        utvm_rpc_server_receive_byte(server, c);
        utvm_rpc_server_loop(server);
    }
}
