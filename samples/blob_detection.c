/*
 * Programming introduction with the SOD Embedded Image Processing API.
 * Copyright (C) PixLab | Symisc Systems, https://sod.pixlab.io
 */
/*
* Compile this file together with the SOD embedded source code to generate
* the executable. For example:
*
*  gcc sod.c blob_detection.c -lm -Ofast -march=native -Wall -std=c99 -o sod_img_proc
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
static int filter_cb(int width, int height)
{
	/* A filter callback invoked by the blob routine each time
	 * a potential blob region is identified.
	 * We use the `width` and `height` parameters supplied
	 * to discard regions of non interest (i.e. too big or too small).
	 */
	if ((width > 300 && height > 300) || width < 35 || height < 35) {
		/* Ignore small or big boxes */
		return 0;
	}
	return 1; /* Region accepted */
}
/*
* Perform connected component labeling on an input binary image
* which is useful for finding blobs (i.e. bloc of texts).
*/
int main(int argc, char *argv[])
{
	/* Input image (pass a path or use the test image shipped with the samples ZIP archive) */
	const char *zInput = argc > 1 ? argv[1] : "./text.jpg";
	/* Processed output image path */
	const char *zOut = argc > 2 ? argv[2] : "./out_blobs.png";
	/* Load the input image in the grayscale colorspace */
	sod_img imgIn = sod_img_load_grayscale(zInput);
	if (imgIn.data == 0) {
		/* Invalid path, unsupported format, memory failure, etc. */
		puts("Cannot load input image..exiting");
		return 0;
	}
	/* A full color copy of the input image so we can draw rose rectangles on blobs. 
	 * Note that drawing on grayscale images is also possible via
	 * sod_image_draw_box_grayscale()
	 */
	sod_img imgCopy = sod_img_load_color(zInput);
	/* 
	 * Binarize the input image before the dilation process.
	 */
	sod_img binImg = sod_binarize_image(imgIn, 0);
	/* Dilate the binary image, say  12 times */
	sod_img dilImg = sod_dilate_image(binImg, 12);
	/* Perform the blob detection process on our dilated image */
	sod_box *box = 0;
	int i, nbox;
	sod_image_find_blobs(dilImg, &box, &nbox, filter_cb /* Our filter callback to discard small and big regions*/);
	/*
	 * Draw a rectangle on each extracted & validated blob region.
	 */
	for (i = 0; i < nbox; i++) {
		sod_image_draw_bbox_width(imgCopy, box[i], 5, 255., 0, 225.); /* rose box */
	}
	/* Finally save the output image to the specified path */
	sod_img_save_as_png(imgCopy, zOut);
	/* Cleanup */
	sod_image_blob_boxes_release(box);
	sod_free_image(imgIn);
	sod_free_image(binImg);
	sod_free_image(dilImg);
	sod_free_image(imgCopy);
	return 0;
}