/* NUIse - a proof-of-concept driver for the NUI Audio device in the Kinect
 * Copyright (C) 2011 Drew Fisher <drew.m.fisher@gmail.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 */
#ifndef __LOADER_H__
#define __LOADER_H__

#include <stdint.h>
#include <libusb.h>

typedef struct {
	uint32_t magic;
	uint32_t tag;
	uint32_t bytes;
	uint32_t cmd;
	uint32_t addr;
	uint32_t unk;
} bootloader_command;

typedef struct {
	uint32_t magic;
	uint32_t tag;
	uint32_t arg1; // initial command: 0.  Firmware blocks: byte count.
	uint32_t cmd;
	uint32_t arg2; // initial command: byte count.  Firmware blocks: target address.
	uint32_t zeros[8];
} cemdloader_command;

typedef struct {
	uint32_t magic;
	uint32_t tag;
	uint32_t status;
} bootloader_status_code;

int upload_firmware(libusb_device_handle* dev);
int upload_cemd_data(libusb_device_handle* dev);

#endif //__LOADER_H__
