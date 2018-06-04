/*
 * Programming introduction with the SOD Embedded Image Processing API.
 * Copyright (C) PixLab | Symisc Systems, https://sod.pixlab.io
 */
/*
* Compile this file together with the SOD embedded source code to generate
* the executable. For example:
*
*  gcc sod.c erode_image.c -lm -Ofast -march=native -Wall -std=c99 -o sod_img_proc
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
* Erode an input binary image.
*/
int main(int argc, char *argv[])
{
	/* Input image (pass a path or use the test image shipped with the samples ZIP archive) */
	const char *zInput = argc > 1 ? argv[1] : "./text.jpg";
	/* Processed output image path */
	const char *zOut = argc > 2 ? argv[2] : "./out_dilated.png";
	/* Load the input image in the grayscale colorspace */
	sod_img imgIn = sod_img_load_grayscale(zInput);
	if (imgIn.data == 0) {
		/* Invalid path, unsupported format, memory failure, etc. */
		puts("Cannot load input image..exiting");
		return 0;
	}
	/* 
	 * Binarize the input image before the dilation process.
	 */
	sod_img binImg = sod_binarize_image(imgIn, 0);
	/* Finally, erode the binary image, say  5 times */
	sod_img erodeImg = sod_erode_image(binImg, 5);
	/* Save the eroded image to the specified path */
	sod_img_save_as_png(erodeImg, zOut);
	/* Cleanup */
	sod_free_image(imgIn);
	sod_free_image(binImg);
	sod_free_image(erodeImg);
	return 0;
}