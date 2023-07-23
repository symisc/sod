/*
 * Programming introduction with the SOD Embedded Image Processing API.
 * Copyright (C) PixLab | Symisc Systems, https://sod.pixlab.io
 */
/*
* Compile this file together with the SOD embedded source code to generate
* the executable. For example:
*
*  gcc sod.c sepia_filter.c -lm -Ofast -march=native -Wall -std=c99 -o sod_img_proc
*  
* Under Microsoft Visual Studio (>= 2015), just drop `sod.c` and its accompanying
* header files on your source tree and you're done. If you have any trouble
* integrating SOD in your project, please submit a support request at:
* https://sod.pixlab.io/support.html
*/
/*
* This simple program is a quick introduction on how to embed and start
* experimenting with SOD without having to do a lot of tedious
* reading and configuration.
*
* Make sure you have the latest release of SOD from:
*  https://pixlab.io/downloads
* The SOD Embedded C/C++ documentation is available at:
*  https://sod.pixlab.io/api.html
*/
#include <stdio.h>
#include "sod.h"
/*
* Apply Sepia Tone (filter) to a given image.
*/
int main(int argc, char *argv[])
{
	/* Input image (pass a path or use the test image shipped with the samples ZIP archive) */
	const char *zInput = argc > 1 ? argv[1] : "./flower.jpg";
	/* Processed output image path */
	const char *zOut = argc > 2 ? argv[2] : "./sepia.png";
	/* Load the input image in the RGB colorspace */
	sod_img imgIn = sod_img_load_from_file(zInput, SOD_IMG_COLOR);
	if (imgIn.data == 0) {
		/* Invalid path, unsupported format, memory failure, etc. */
		puts("Cannot load input image..exiting");
		return 0;
	}
	/* Create the SEPIA tone. */
	sod_image_sepia_filter(imgIn);
	/* Finally save our processed image to the specified path */
	sod_img_save_as_png(imgIn, zOut);
	/* Cleanup */
	sod_free_image(imgIn);
	return 0;
}
