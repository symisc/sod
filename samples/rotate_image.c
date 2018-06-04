/*
 * Programming introduction with the SOD Embedded Image Processing API.
 * Copyright (C) PixLab | Symisc Systems, https://sod.pixlab.io
 */
/*
* Compile this file together with the SOD embedded source code to generate
* the executable. For example:
*
*  gcc sod.c rotate_image.c -lm -Ofast -march=native -Wall -std=c99 -o sod_img_proc
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
* Rotate an image 180 degree.
*/
int main(int argc, char *argv[])
{
	/* Input image (pass a path or use the test image shipped with the samples ZIP archive) */
	const char *zInput = argc > 1 ? argv[1] : "./test.png";
	/* Processed output image path */
	const char *zOut = argc > 2 ? argv[2] : "./out_rotate.png";
	/* Load the input image in full color */
	sod_img imgIn = sod_img_load_from_file(zInput, SOD_IMG_COLOR /* full color channels */);
	if (imgIn.data == 0) {
		/* Invalid path, unsupported format, memory failure, etc. */
		puts("Cannot load input image..exiting");
		return 0;
	}
	/* 
	 * Perform the rotation process.
	 */
	sod_img rot = sod_rotate_image(imgIn, 180.0);
	/* Save the rotated image to the specified path */
	sod_img_save_as_png(rot, zOut);
	/* Cleanup */
	sod_free_image(imgIn);
	sod_free_image(rot);
	return 0;
}