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

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <libusb.h>

#include "loader.h"
#include "nuise.h"

#define LOG(...) printf(__VA_ARGS__)
// TODO: support architectures that aren't little-endian
#define le32(x) (x)

extern uint32_t tag;

static void prepare_iso_out_data(iso_out_stream* stream, uint8_t* buffer) {
	memset(buffer, 0, 76);
	((uint16_t*)buffer)[0] = stream->window;
	buffer[2] = stream->seq;
	if(stream->window_parity == 0) {
		if (stream->seq_in_window < 4) {
			// madness type 1 - high nibble of buffer[3] should be the seq_in_window-th nibble of timestamp
			buffer[3] = (((stream->timestamp >> (stream->seq_in_window*4)) & 0x000f) << 4) | 0x05; // I have no idea why we do this.
		} else if (stream->seq_in_window < 8) {
			// madness type 2 - same thing mod 4, but we add 23 to timestamp for no clear reason.
			buffer[3] = (((stream->timestamp+23 >> ((stream->seq_in_window-4)*4)) & 0x000f) << 4) | 0x05; // I have even less idea why we do this.
		} else {
			buffer[3] = 0x01; // Oh, and then this.
		}
	} else {
		if( stream->seq_in_window < 4) {
			// madness type 1
			buffer[3] = (((stream->timestamp >> (stream->seq_in_window*4)) & 0x000f) << 4) | 0x05; // I have no idea why we do this.
		} else {
			buffer[3] = 0x01;
		}
	}
	// Now, update the values for their next usage:
	stream->seq++;
	stream->seq_in_window++;
	stream->timestamp += (stream->window_parity == 1) ? 6 : 5;
	switch(stream->seq) {
		case 0x80:
			stream->seq = 0;
		case 0x2b:
		case 0x56:
			stream->seq_in_window = 0;
			stream->window++;
			stream->window_parity++;
		default:
			break;
	}
	if(stream->window_parity == 3) stream->window_parity = 0;
	return;
}

static void iso_out_callback(struct libusb_transfer* xfer) {
	if(xfer->status == LIBUSB_TRANSFER_COMPLETED) {
		//LOG("Sent ISO packet\n");
		// queue the next packet to be sent
		uint8_t* buf = xfer->buffer;
		prepare_iso_out_data((iso_out_stream*)xfer->user_data , buf);
		libusb_submit_transfer(xfer);
	} else {
		LOG("Isochronous OUT transfer error: %d\n", xfer->status);
	}
}

static void iso_in_callback(struct libusb_transfer* xfer) {
	iso_in_stream* stream = (iso_in_stream*)xfer->user_data;

	if(xfer->status == LIBUSB_TRANSFER_COMPLETED) {
		// Do whatever we need to with the IN data.
		// Check packet size, then tag, then write to the correct file?
		int i;
		for(i = 0; i < stream->pkts; i++) {
			if( xfer->iso_packet_desc[i].actual_length == 524 ) {
				// Cool, this is audio data.
				audio_in_block* block = (audio_in_block*)xfer->buffer;
				if( block->magic != 0x80000080) {
					LOG("\tInvalid magic in iso IN packet: %08X\n", block->magic);
					continue;
				} else {
					// TODO: reorder packets if needed, fill dropped gaps with zero'd data
					if (block->window != stream->window) {
						// update the current window
						LOG("audio window changed: was %04X now %04X\n", stream->window, block->window);
						int t;
						for(t = 0 ; t < 10; t++) {
							if(stream->last_seen_window[t] != stream->window) {
								LOG("\tDid not receive data for channel 0x%02x\n",t+1);
							}
						}
						if(block->window - stream->window > 3)
							LOG("Packet loss: dropped %d windows\n", (block->window - stream->window - 3) / 3);
						stream->window = block->window;
						// The difference should be no greater than 3.
					}
					switch(block->channel) {
						case 1:
								fwrite(block->samples, 1, 512, stream->file_handles[0]);
								break;
						case 2:
						case 3:
								fwrite(block->samples, 1, 512, stream->file_handles[1]);
								break;
						case 4:
						case 5:
								fwrite(block->samples, 1, 512, stream->file_handles[2]);
								break;
						case 6:
						case 7:
								fwrite(block->samples, 1, 512, stream->file_handles[3]);
								break;
						case 8:
						case 9:
								fwrite(block->samples, 1, 512, stream->file_handles[4]);
								break;
						default:
							LOG("Invalid channel in iso IN packet: %d\n", block->channel);
					}
					stream->last_seen_window[block->channel-1] = block->window;
				}
			} else if ( xfer->iso_packet_desc[i].actual_length == 60 ) {
				fwrite(xfer->buffer, 1, 60, stream->file_handles[5]);
				// Cool, this is the uninterpreted signalling information.  Let's ignore it for now.
			} else if ( xfer->iso_packet_desc[i].actual_length != 0 ) {
				LOG("Received an iso IN packet of strange length: %d\n", xfer->iso_packet_desc[i].actual_length);
			}
		}
		libusb_submit_transfer(xfer);
	} else {
		LOG("Isochronous IN transfer error: %d\n", xfer->status);
	}
}

int start_iso_out(libusb_device_handle* dev, iso_out_stream* stream, int endpoint, int xfers, int pkts, int len) {
	stream->timestamp = 0;
	stream->window = 0;
	stream->seq = 0;
	stream->seq_in_window = 0;
	stream->window_parity = 0;
	stream->pkts = pkts;
	stream->len = len;
	stream->buffer_space = (uint8_t*)malloc(xfers * pkts * len);
	stream->xfers = (struct libusb_transfer**)malloc(sizeof(struct libusb_transfer*) * xfers);

	uint8_t* bufp = stream->buffer_space;
	int i, ret;
	LOG("Creating iso OUT transfers\n");
	for(i = 0; i < xfers; i++) {
		//LOG("Creating iso OUT transfer %d\n", i);
		stream->xfers[i] = libusb_alloc_transfer(pkts);
		prepare_iso_out_data(stream, bufp);
		libusb_fill_iso_transfer(stream->xfers[i], dev, endpoint, bufp, pkts * len, pkts, iso_out_callback, stream, 0);
		libusb_set_iso_packet_lengths(stream->xfers[i], len);
		ret = libusb_submit_transfer(stream->xfers[i]);
		if( ret < 0)
			LOG("Failed to submit iso OUT transfer %d: %d\n", i, ret);
		bufp += pkts*len;
	}
	return 0;
}

int start_iso_in(libusb_device_handle* dev, iso_in_stream* stream, int endpoint, int xfers, int pkts, int len) {
	stream->file_handles = (FILE**)malloc(sizeof(FILE*) * 6);
	stream->file_handles[0] = fopen("cancelled.dat", "wb");
	stream->file_handles[1] = fopen("channel1.dat", "wb");
	stream->file_handles[2] = fopen("channel2.dat", "wb");
	stream->file_handles[3] = fopen("channel3.dat", "wb");
	stream->file_handles[4] = fopen("channel4.dat", "wb");
	stream->file_handles[5] = fopen("sig.dat", "wb");
	stream->xfers = (struct libusb_transfer**)malloc(sizeof(struct libusb_transfer*) * xfers);
	stream->buffer_space = (uint8_t*)malloc(xfers * pkts * len);
	stream->num_xfers = xfers;
	stream->pkts = pkts;
	stream->len = len;
	stream->window = 0x0; // Not bothering with these for now
	memset(stream->last_seen_window, 0 , 10*sizeof(uint16_t));

	uint8_t* bufp = stream->buffer_space;
	int i, ret;
	LOG("Creating iso IN transfers\n");
	for(i = 0; i < xfers; i++) {
		//LOG("Creating iso IN transfer %d\n", i);
		stream->xfers[i] = libusb_alloc_transfer(pkts);
		libusb_fill_iso_transfer(stream->xfers[i], dev, endpoint, bufp, pkts * len, pkts, iso_in_callback, stream, 0);
		libusb_set_iso_packet_lengths(stream->xfers[i], len);
		ret = libusb_submit_transfer(stream->xfers[i]);
		if( ret < 0)
			LOG("Failed to submit iso IN transfer %d: %d\n", i, ret);
		bufp += pkts*len;
	}
	return 0;
}


