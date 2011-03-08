/* NUIse - a proof-of-concept driver for the NUI Audio device in the Kinect
 * Copyright (C) 2011 Drew Fisher <drew.m.fisher@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
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
