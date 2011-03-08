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
#ifndef __NUISE_H__
#define __NUISE_H__

#include <stdio.h>
#include <stdint.h>
#include <libusb.h>

typedef struct {
	struct libusb_transfer **xfers;
	uint8_t* buffer_space; // aggregate buffer space for all num_xfers libusb_transfer's.
	int num_xfers; // Number of xfers to queue at a time.
	int pkts;  // 
	int len;
	uint16_t window;
	uint16_t timestamp;
	uint8_t seq;
	uint8_t seq_in_window;
	uint8_t window_parity; // 0, 1, or 2.
} iso_out_stream;

typedef struct {
	struct libusb_transfer **xfers;
	FILE** file_handles; // Log file handles.
	uint8_t* buffer_space; // aggregate buffer space for all num_xfers libusb_transfers
	int num_xfers; // Number of xfers to queue at a time
	int pkts; // how many packets in each USB transfer
	int len;
	uint16_t window; // last seen window value
	uint16_t last_seen_window[10]; // Per-channel log so we can fill in where we lost data with zeros (TODO)
} iso_in_stream;

typedef struct {
	uint32_t magic;
	uint16_t channel;
	uint16_t len;
	uint16_t window;
	uint16_t unknown;
	int32_t samples[128];
} audio_data_block;

int get_first_reply(libusb_device_handle* dev);
int get_reply(libusb_device_handle* dev);
int start_iso_out(libusb_device_handle* dev, iso_out_stream* stream, int endpoint, int xfers, int pkts, int len);
int start_iso_in(libusb_device_handle* dev, iso_in_stream* stream, int endpoint, int xfers, int pkts, int len);

#endif //__NUISE_H__
