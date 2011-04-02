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

#include "nuise.h"
#include "loader.h"
#include <libusb.h>
#include <stdlib.h>

#define LOG(...) printf(__VA_ARGS__)

uint32_t tag;
static libusb_device_handle *dev;
libusb_device** devlist;

int main(int argc, char** argv) {
	libusb_init(NULL);
	libusb_set_debug(0,3);

	int need_fw_upload = 0;
	int n_devs = libusb_get_device_list(NULL, &devlist);
	if(n_devs < 0) {
		LOG("error: LIBUSB_ERROR_NO_MEM in libusb_get_device_list()\n");
		exit(1);
	}
	ssize_t item;
	for(item = 0; item < n_devs - 1; item++) { // devlist is NULL terminated
		struct libusb_device_descriptor descriptor;
		int ret;
		ret = libusb_get_device_descriptor(devlist[item], &descriptor);
		if(ret != 0) {
			LOG("Error in libusb_get_device_descriptor: %d\n", ret);
			exit(1);
		} else {
			if(descriptor.idVendor == 0x045e && descriptor.idProduct == 0x02ad) {
				// Check if this is the bootloader or the audio firmware
				struct libusb_config_descriptor* config;
				libusb_get_active_config_descriptor(devlist[item], &config);
				if(config->bNumInterfaces == 1) // The audio firmware has 2 interfaces, the bootloader 1
					need_fw_upload = 1;
				libusb_free_config_descriptor(config);
				// Now, get a device handle
				ret = libusb_open(devlist[item], &dev);
				if(ret != 0) {
					LOG("Error in libusb_open: %d\n",ret);
					exit(1);
				}
				break;
			}
		}
	}

	libusb_free_device_list(devlist, 1);
	devlist = NULL;

	if(dev == NULL) {
		printf("Couldn't open device.\n");
		return 1;
	}

	LOG("Successfully found device.\n");

	libusb_set_configuration(dev, 1);
	libusb_claim_interface(dev, 0);

	tag = 0x08040201;

	/*
	// The bootloader has one configuration; audios.bin has two.
	// We only try to upload firmware if we haven't already sent it this boot
	LOG("bNumConfigurations: %d\n", descriptor.bNumConfigurations);
	*/
	if(need_fw_upload) {
		if(upload_firmware(dev) != 0) {
			LOG("Something went wrong in upload_firmware(), aborting\n");
			goto cleanup;
		}

		libusb_release_interface(dev, 0);
		libusb_close(dev);

		// Now the device reenumerates.  Let it reappear:
		dev = NULL;
		do {
			usleep(1000000);
			dev = libusb_open_device_with_vid_pid(NULL, 0x045e, 0x02ad);
			printf("Trying to reopen device...\n");
		} while (dev == NULL);
		libusb_set_configuration(dev, 1);
		libusb_claim_interface(dev, 0);

		/*
		 * We skip uploading CEMD data because:
		 * 1) it's not required
		 * 2) I don't know how to produce it anyway
		 * 3) I'm not sure what it means, and I don't want to ship it
		 *
		 * The code exists here to show how it would work, though.
		 */
		/*
		if(upload_cemd_data(dev) != 0) {
			LOG("Something went wrong in upload_cemd_data(), aborting\n");
			goto cleanup;
		}
		*/
	} else {
		LOG("Firmware already loaded this boot.\n");
	}

	// Start up the isochronous transfers, then handle events forever.
	
	iso_in_stream* in_stream = (iso_in_stream*)malloc(sizeof(iso_in_stream));
	iso_out_stream* out_stream = (iso_out_stream*)malloc(sizeof(iso_out_stream));
	start_iso_in(dev, in_stream, 0x82, 256, 1, 524);
	start_iso_out(dev, out_stream, 0x02, 256, 1, 76);
	
	int counter = 0;
	while(1) {
		libusb_handle_events(NULL);
		counter++;
		if(counter == 0x200) {
			fflush(in_stream->file_handles[0]);
			fflush(in_stream->file_handles[1]);
			fflush(in_stream->file_handles[2]);
			fflush(in_stream->file_handles[3]);
			fflush(in_stream->file_handles[4]);
			counter = 0;
		}
	}

cleanup:
	libusb_close(dev);
	libusb_exit(NULL);
	return 0;
}

