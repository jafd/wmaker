/* load_webp.c - load WEBP image from file
 *
 * Raster graphics library
 *
 * Copyright (c) 2014 Window Maker Team
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public
 *  License along with this library; if not, write to the Free
 *  Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston,
 *  MA 02110-1301, USA.
 */

#include <config.h>

#include <X11/Xlib.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <webp/decode.h>

#include "wraster.h"
#include "imgformat.h"

RImage *
RLoadWEBP(const char *file_name)
{
	FILE *file;
	RImage *image = NULL;
	char buffer[20];
	int raw_data_size;
	int start;
	int r;
	uint8_t *raw_data;
	WebPBitstreamFeatures features;
	uint8_t *ret = NULL;

	file = fopen(file_name, "rb");
	if (!file) {
		RErrorCode = RERR_OPEN;
		return NULL;
	}
	start = ftell(file);

	if (!fread(buffer, sizeof(buffer), 1, file)) {
		RErrorCode = RERR_BADIMAGEFILE;
		fclose(file);
		return NULL;
	}


	if (!(buffer[0] == 'R' &&
	      buffer[1] == 'I' &&
	      buffer[2] == 'F' &&
	      buffer[3] == 'F' &&
	      buffer[8] == 'W' &&
	      buffer[9] == 'E' &&
	      buffer[10] == 'B' &&
	      buffer[11] == 'P' &&
	      buffer[12] == 'V' && buffer[13] == 'P' && buffer[14] == '8' &&
#if WEBP_DECODER_ABI_VERSION < 0x0003	/* old versions don't support WEBPVP8X and WEBPVP8L */
	      buffer[15] == ' ')) {
#else
	      (buffer[15] == ' ' || buffer[15] == 'X' || buffer[15] == 'L'))) {
#endif
		RErrorCode = RERR_BADFORMAT;
		fclose(file);
		return NULL;
	}


	fseek(file, 0, SEEK_END);
	raw_data_size = ftell(file);

	if (raw_data_size <= 0) {
		fprintf(stderr, "Failed to find the WEBP image size\n");
		return NULL;
	}

	fseek(file, start, SEEK_SET);

	raw_data = (uint8_t *) malloc(raw_data_size);

	if (!raw_data) {
		fprintf(stderr, "Failed to allocate enought buffer for WEBP\n");
		return NULL;
	}

	r = fread(raw_data, 1, raw_data_size, file);

	if (r != raw_data_size) {
		fprintf(stderr, "Failed to read WEBP\n");
		return NULL;
	}

	if (WebPGetFeatures(raw_data, raw_data_size, &features) !=
	    VP8_STATUS_OK) {
		fprintf(stderr, "WebPGetFeatures has failed\n");
		return NULL;
	}

	if (features.has_alpha) {
		image = RCreateImage(features.width, features.height, True);
		if (!image)
			return NULL;
		ret =
		    WebPDecodeRGBAInto(raw_data, raw_data_size, image->data,
				       features.width * features.height * 4,
				       features.width * 4);
	} else {
		image = RCreateImage(features.width, features.height, False);
		if (!image)
			return NULL;
		ret =
		    WebPDecodeRGBInto(raw_data, raw_data_size, image->data,
				      features.width * features.height * 3,
				      features.width * 3);
	}

	if (!ret) {
		fprintf(stderr, "Failed to decode WEBP\n");
		return NULL;
	}

	fclose(file);
	return image;
}
