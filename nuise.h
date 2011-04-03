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
	uint32_t magic;    // 0x80000080
	uint16_t channel;  // Values between 0x1 and 0xa indicate audio channel
	uint16_t len;      // packet length
	uint16_t window;   // timestamp
	uint16_t unknown;  // ???
	int32_t samples[]; // Size depends on len
} audio_in_block;

typedef struct {
	int16_t left;
	int16_t right;
	int16_t center;
	int16_t lfe;
	int16_t surround_left;
	int16_t surround_right;
} sample_51;

typedef struct {
	uint16_t window;
	uint8_t seq;
	uint8_t weird;
	sample_51 samples[6];
} audio_out_block;

int start_iso_out(libusb_device_handle* dev, iso_out_stream* stream, int endpoint, int xfers, int pkts, int len);
int start_iso_in(libusb_device_handle* dev, iso_in_stream* stream, int endpoint, int xfers, int pkts, int len);

#endif //__NUISE_H__
