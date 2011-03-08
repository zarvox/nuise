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

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <libusb.h>

static libusb_device_handle *dev;
uint32_t tag;
unsigned char page[0x4000];

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

#define LOG(...) printf(__VA_ARGS__)
#define le32(x) (x)
// TODO: support architectures that aren't little-endian

void dump_bl_cmd(bootloader_command cmd) {
	int i;
	for(i = 0; i < 24; i++)
		LOG("%02X ", ((unsigned char*)(&cmd))[i]);
	LOG("\n");
}

void dump_cemd_cmd(cemdloader_command cmd) {
	int i;
	for(i = 0; i < 24; i++)
		LOG("%02X ", ((unsigned char*)(&cmd))[i]);
	LOG("(%d more zeros)\n", sizeof(cmd)-24);
}

int get_first_reply() {
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

int get_reply() {
	unsigned char dump[512];
	bootloader_status_code buffer = ((bootloader_status_code*)dump)[0];
	int res;
	int transferred;
	res = libusb_bulk_transfer(dev, 0x81, (unsigned char*)&buffer, 512, &transferred, 0);
	if(res != 0 || transferred != sizeof(bootloader_status_code)) {
		LOG("Error reading reply: %d\ttransferred: %d (expected %d)\n", res, transferred, sizeof(bootloader_status_code));
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

	LOG("About to send: ");
	dump_bl_cmd(bootcmd);
	int res;
	int transferred;

	res = libusb_bulk_transfer(dev, 1, (unsigned char*)&bootcmd, sizeof(bootcmd), &transferred, 0);
	if(res != 0 || transferred != sizeof(bootcmd)) {
		LOG("Error: res: %d\ttransferred: %d (expected %d)\n",res, transferred, sizeof(bootcmd));
		return -1;
	}
	res = get_first_reply(); // This first one doesn't have the usual magic bytes at the beginning, and is 96 bytes long - much longer than the usual 12-byte replies.
	res = get_reply(); // I'm not sure why we do this twice here, but maybe it'll make sense later.
	tag++;

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
			LOG("Error: res: %d\ttransferred: %d (expected %d)\n",res, transferred, sizeof(bootcmd));
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
		res = get_reply();
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
		LOG("Error: res: %d\ttransferred: %d (expected %d)\n", res, transferred, sizeof(bootcmd));
		return -1;
	}
	res = get_reply();
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
		LOG("Error: res: %d\ttransferred: %d (expected %d)\n", res, transferred, sizeof(cemdcmd));
		return -1;
	}
	res = get_reply();
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
			LOG("Error: res: %d\ttransferred: %d (expected %d)\n",res, transferred, sizeof(cemdcmd));
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
		res = get_reply();
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
		LOG("Error: res: %d\ttransferred: %d (expected %d)\n", res, transferred, sizeof(cemdcmd));
		return -1;
	}
	res = get_reply();
	tag++;
	LOG("CEMD data uploaded successfully.\n");
	return 0;
}

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

void prepare_iso_out_data(iso_out_stream* stream, uint8_t* buffer) {
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
		prepare_iso_out_data(xfer->user_data , buf);
		libusb_submit_transfer(xfer);
	} else {
		LOG("Isochronous OUT transfer error: %d\n", xfer->status);
	}
}

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

static void iso_in_callback(struct libusb_transfer* xfer) {
	iso_in_stream* stream = (iso_in_stream*)xfer->user_data;

	if(xfer->status == LIBUSB_TRANSFER_COMPLETED) {
		// Do whatever we need to with the IN data.
		// Check packet size, then tag, then write to the correct file?
		int i;
		for(i = 0; i < stream->pkts; i++) {
			if( xfer->iso_packet_desc[i].actual_length == 524 ) {
				// Cool, this is audio data.
				audio_data_block* block = (audio_data_block*)xfer->buffer;
				if( block->magic != 0x80000080) {
					LOG("\tInvalid magic in iso IN packet: %08X\n", block->magic);
					continue;
				} else {
					// TODO: reorder packets if needed, fill dropped gaps with zero'd data
					if (block->window != stream->window) {
						// update the current window
						LOG("audio window changed: was %04X now %04X\n", stream->window, block->window);
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
				}
			} else if ( xfer->iso_packet_desc[i].actual_length == 60 ) {
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
	stream->timestamp = 0x949C;
	stream->window = 0xFEED;
	stream->seq = 0;
	stream->seq_in_window = 0;
	stream->window_parity = 0;
	stream->pkts = pkts;
	stream->len = len;
	stream->buffer_space = (uint8_t*)malloc(xfers * pkts * len);
	stream->xfers = (struct libusb_transfer**)malloc(sizeof(struct libusb_transfer*) * xfers);

	uint8_t* bufp = stream->buffer_space;
	int i, ret;
	for(i = 0; i < xfers; i++) {
		LOG("Creating iso OUT transfer %d\n", i);
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
	stream->file_handles = (FILE**)malloc(sizeof(FILE*) * 5);
	stream->file_handles[0] = fopen("cancelled.dat", "wb");
	stream->file_handles[1] = fopen("channel1.dat", "wb");
	stream->file_handles[2] = fopen("channel2.dat", "wb");
	stream->file_handles[3] = fopen("channel3.dat", "wb");
	stream->file_handles[4] = fopen("channel4.dat", "wb");
	stream->xfers = (struct libusb_transfer**)malloc(sizeof(struct libusb_transfer*) * xfers);
	stream->buffer_space = (uint8_t*)malloc(xfers * pkts * len);
	stream->num_xfers = xfers;
	stream->pkts = pkts;
	stream->len = len;
	stream->window = 0xFDFB; // Not bothering with these for now
	memset(stream->last_seen_window, 0 , 10*sizeof(uint16_t));

	uint8_t* bufp = stream->buffer_space;
	int i, ret;
	for(i = 0; i < xfers; i++) {
		LOG("Creating iso IN transfer %d\n", i);
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

