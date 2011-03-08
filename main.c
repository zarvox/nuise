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

#include "nuise.h"
#include "loader.h"
#include <libusb.h>
#include <stdlib.h>

#define LOG(...) printf(__VA_ARGS__)

uint32_t tag;
static libusb_device_handle *dev;

int main(int argc, char** argv) {
	libusb_init(NULL);
	libusb_set_debug(0,3);
	dev = libusb_open_device_with_vid_pid(NULL, 0x045e, 0x02ad);

	if(dev == NULL) {
		printf("Couldn't open device.\n");
		return 1;
	}

	libusb_set_configuration(dev, 1);
	libusb_claim_interface(dev, 0);

	tag = 0x08040201;

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

	if(upload_cemd_data(dev) != 0) {
		LOG("Something went wrong in upload_cemd_data(), aborting\n");
		goto cleanup;
	}
	*/

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

