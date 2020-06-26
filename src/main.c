/*
 * Copyright (c) 2012-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>
#include <console/console.h>
#include <sys/printk.h>
#include <tvm/runtime/micro/micro_rpc_server.h>
#include <tvm/runtime/crt/logging.h>

ssize_t write_serial(void* unused_context, const uint8_t* data, size_t size) {
  return console_write(NULL, data, size);
}

void TVMPlatformAbort(int error) {
  for (;;) ;
}

#define WORKSPACE_SIZE_BYTES (60 * 1024)
#define WORKSPACE_PAGE_SIZE_BYTES_LOG2 8

uint8_t workspace[WORKSPACE_SIZE_BYTES];

void main(void)
{
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
