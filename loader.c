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

#include "loader.h"
#include <libusb.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#define LOG(...) printf(__VA_ARGS__)
// TODO: support architectures that aren't little-endian
#define le32(x) (x)

unsigned char page[0x4000];
extern uint32_t tag;

static void dump_bl_cmd(bootloader_command cmd) {
	int i;
	for(i = 0; i < 24; i++)
		LOG("%02X ", ((unsigned char*)(&cmd))[i]);
	LOG("\n");
}

static void dump_cemd_cmd(cemdloader_command cmd) {
	int i;
	for(i = 0; i < 24; i++)
		LOG("%02X ", ((unsigned char*)(&cmd))[i]);
	LOG("(%d more zeros)\n", (int)(sizeof(cmd)-24));
}

static int get_first_reply(libusb_device_handle* dev) {
	unsigned char buffer[512];
	int res;
	int transferred;
	res = libusb_bulk_transfer(dev, 0x81, buffer, 512, &transferred, 0);
	if(res != 0 ) {
		LOG("Error reading first reply: %d\ttransferred: %d (expected %d)\n", res, transferred, 0x60);
		return res;
	}
	LOG("Reading first reply: ");
	int i;
	for(i = 0; i < transferred; ++i) {
		LOG("%02X ", buffer[i]);
	}
	LOG("\n");
	return res;
}

static int get_reply(libusb_device_handle* dev) {
	unsigned char dump[512];
	bootloader_status_code buffer = ((bootloader_status_code*)dump)[0];
	int res;
	int transferred;
	res = libusb_bulk_transfer(dev, 0x81, (unsigned char*)&buffer, 512, &transferred, 0);
	if(res != 0 || transferred != sizeof(bootloader_status_code)) {
		LOG("Error reading reply: %d\ttransferred: %d (expected %d)\n", res, transferred, (int)(sizeof(bootloader_status_code)));
		return res;
	}
	if(le32(buffer.magic) != 0x0a6fe000) {
		LOG("Error reading reply: invalid magic %08X\n",buffer.magic);
		return -1;
	}
	if(le32(buffer.tag) != tag) {
		LOG("Error reading reply: non-matching taguence number %08X (expected %08X)\n", buffer.tag, tag);
		return -1;
	}
	if(le32(buffer.status) != 0) {
		LOG("Notice reading reply: last uint32_t was nonzero: %d\n", buffer.status);
	}
	LOG("Reading reply: ");
	int i;
	for(i = 0; i < transferred; ++i) {
		LOG("%02X ", ((unsigned char*)(&buffer))[i]);
	}
	LOG("\n");
	return res;
}

int upload_firmware(libusb_device_handle* dev) {
	bootloader_command bootcmd;
	memset(&bootcmd, 0, sizeof(bootcmd));
	bootcmd.magic = le32(0x06022009);
	bootcmd.tag   = le32(tag);
	bootcmd.bytes = le32(0x60);
	bootcmd.cmd   = le32(0);
	bootcmd.addr  = le32(0x15);

	int res;
	int transferred;

/*
    // First transfer happens to be a version query, and not necessary.
	LOG("About to send: ");
	dump_bl_cmd(bootcmd);

	res = libusb_bulk_transfer(dev, 1, (unsigned char*)&bootcmd, sizeof(bootcmd), &transferred, 0);
	if(res != 0 || transferred != sizeof(bootcmd)) {
		LOG("Error: res: %d\ttransferred: %d (expected %d)\n",res, transferred, sizeof(bootcmd));
		return -1;
	}
	res = get_first_reply(dev); // This first one doesn't have the usual magic bytes at the beginning, and is 96 bytes long - much longer than the usual 12-byte replies.
	res = get_reply(dev); // I'm not sure why we do this twice here, but maybe it'll make sense later.
	tag++;
*/
	const char* fw_filename = "audios.bin";
	FILE* fw = fopen(fw_filename, "r");
	if(fw == NULL) {
		fprintf(stderr, "Failed to open %s: error %d", fw_filename, errno);
		return -errno;
	}
	uint32_t addr = 0x00080000;
	int read;
	do {
		read = fread(page, 1, 0x4000, fw);
		if(read <= 0) {
			break;
		}
		bootcmd.tag = le32(tag);
		bootcmd.bytes = le32(read);
		bootcmd.cmd = le32(0x03);
		bootcmd.addr = le32(addr);
		LOG("About to send: ");
		dump_bl_cmd(bootcmd);
		// Send it off!
		res = libusb_bulk_transfer(dev, 1, (unsigned char*)&bootcmd, sizeof(bootcmd), &transferred, 0);
		if(res != 0 || transferred != sizeof(bootcmd)) {
			LOG("Error: res: %d\ttransferred: %d (expected %d)\n",res, transferred, (int)(sizeof(bootcmd)));
			return -1;
		}
		int bytes_sent = 0;
		while(bytes_sent < read) {
			int to_send = (read - bytes_sent > 512 ? 512 : read - bytes_sent);
			res = libusb_bulk_transfer(dev, 1, &page[bytes_sent], to_send, &transferred, 0);
			if(res != 0 || transferred != to_send) {
				LOG("Error: res: %d\ttransferred: %d (expected %d)\n",res, transferred, to_send);
				return -1;
			}
			bytes_sent += to_send;
		}
		res = get_reply(dev);
		addr += (uint32_t)read;
		tag++;
	} while (read > 0);
	fclose(fw);
	fw = NULL;

	bootcmd.tag   = le32(tag);
	bootcmd.bytes = le32(0);
	bootcmd.cmd   = le32(0x04);
	bootcmd.addr  = le32(0x00080030);
	dump_bl_cmd(bootcmd);
	res = libusb_bulk_transfer(dev, 1, (unsigned char*)&bootcmd, sizeof(bootcmd), &transferred, 0);
	if(res != 0 || transferred != sizeof(bootcmd)) {
		LOG("Error: res: %d\ttransferred: %d (expected %d)\n", res, transferred, (int)sizeof(bootcmd));
		return -1;
	}
	res = get_reply(dev);
	tag++;
	LOG("Firmware successfully uploaded and launched.  Device will disconnect and reenumerate.\n");
	return 0;
}

int upload_cemd_data(libusb_device_handle* dev) {
	// Now we upload the CEMD data.
	cemdloader_command cemdcmd;
	memset(&cemdcmd, 0, sizeof(cemdcmd));
	cemdcmd.magic = le32(0x06022009);
	cemdcmd.tag   = le32(tag);
	cemdcmd.arg1  = le32(0);
	cemdcmd.cmd   = le32(0x00000133);
	cemdcmd.arg2  = le32(0x00064014); // This is the length of the CEMD data.
	LOG("Starting CEMD data upload:\n");
	int res;
	int transferred;
	res = libusb_bulk_transfer(dev, 1, (unsigned char*)&cemdcmd, sizeof(cemdcmd), &transferred, 0);
	if(res != 0 || transferred != sizeof(cemdcmd)) {
		LOG("Error: res: %d\ttransferred: %d (expected %d)\n", res, transferred, (int)sizeof(cemdcmd));
		return -1;
	}
	res = get_reply(dev);
	tag++;
	
	const char* cemd_filename = "cemd_data.bin";
	FILE* cf = fopen(cemd_filename, "r");
	if(cf == NULL) {
		fprintf(stderr, "Failed to open %s: error %d", cemd_filename, errno);
		return errno;
	}
	uint32_t addr = 0x00000000;
	int read = 0;
	do {
		read = fread(page, 1, 0x4000, cf);
		if(read <= 0) {
			break;
		}
		//LOG("");
		cemdcmd.tag  = le32(tag);
		cemdcmd.arg1 = le32(read);
		cemdcmd.cmd  = le32(0x134);
		cemdcmd.arg2 = le32(addr);
		LOG("About to send: ");
		dump_cemd_cmd(cemdcmd);
		// Send it off!
		res = libusb_bulk_transfer(dev, 1, (unsigned char*)&cemdcmd, sizeof(cemdcmd), &transferred, 0);
		if(res != 0 || transferred != sizeof(cemdcmd)) {
			LOG("Error: res: %d\ttransferred: %d (expected %d)\n",res, transferred, (int)sizeof(cemdcmd));
			return -1;
		}
		int bytes_sent = 0;
		while(bytes_sent < read) {
			int to_send = (read - bytes_sent > 512 ? 512 : read - bytes_sent);
			res = libusb_bulk_transfer(dev, 1, &page[bytes_sent], to_send, &transferred, 0);
			if(res != 0 || transferred != to_send) {
				LOG("Error: res: %d\ttransferred: %d (expected %d)\n",res, transferred, to_send);
				return -1;
			}
			bytes_sent += to_send;
		}
		res = get_reply(dev);
		addr += (uint32_t)read;
		tag++;
	} while (read > 0);
	fclose(cf);
	cf = NULL;

	cemdcmd.tag  = le32(tag);
	cemdcmd.arg1 = le32(0); // bytes = 0
	cemdcmd.cmd  = le32(0x135);
	cemdcmd.arg2 = le32(0x00064000); // mimicing the USB logs.  This is the # of bytes of actual CEMD data after the 20-byte CEMD header.
	LOG("Finishing CEMD data upload...\n");
	res = libusb_bulk_transfer(dev, 1, (unsigned char*)&cemdcmd, sizeof(cemdcmd), &transferred, 0);
	if(res != 0 || transferred != sizeof(cemdcmd)) {
		LOG("Error: res: %d\ttransferred: %d (expected %d)\n", res, transferred, (int)sizeof(cemdcmd));
		return -1;
	}
	res = get_reply(dev);
	tag++;
	LOG("CEMD data uploaded successfully.\n");
	return 0;
}
