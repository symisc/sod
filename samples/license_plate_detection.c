/*
 * Programming introduction with the SOD Embedded Image Processing API.
 * Copyright (C) PixLab | Symisc Systems, https://sod.pixlab.io
 */
/*
* Compile this file together with the SOD embedded source code to generate
* the executable. For example:
*
*  gcc sod.c license_plate_detection.c -lm -Ofast -march=native -Wall -std=c99 -o sod_img_proc
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
* Frontal License Plate detection without deep-learning. Only image processing code.
*/
static int filter_cb(int width, int height)
{
	/* A filter callback invoked by the blob routine each time
	* a potential blob region is identified.
	* We use the `width` and `height` parameters supplied
	* to discard regions of non interest (i.e. too big or too small).
	*/
	if ((width > 300 && height > 200) || width < 45 || height < 45) {
		/* Ignore small or big boxes (You should take in consideration
		* U.S plate size here and adjust accordingly).
		*/
		return 0; /* Discarded region */
	}
	return 1; /* Accepted region */
}
int main(int argc, char *argv[])
{
	/* Input image (pass a path or use the test image shipped with the samples ZIP archive) */
	const char *zInput = argc > 1 ? argv[1] : "./plate.jpg";
	/* Processed output image path */
	const char *zOut = argc > 2 ? argv[2] : "./out_plate.png";
	/* Load the input image in the grayscale colorspace */
	sod_img imgIn = sod_img_load_grayscale(zInput);
	if (imgIn.data == 0) {
		/* Invalid path, unsupported format, memory failure, etc. */
		puts("Cannot load input image..exiting");
		return 0;
	}
	/* A full color copy of the input image so we can draw rose boxes
	 * marking the plate in question if any.
	 */
	sod_img imgCopy = sod_img_load_color(zInput);
	/* Obtain a binary image first */
	sod_img binImg = sod_threshold_image(imgIn, 0.5);
	/* 
	 * Perform Canny edge detection next which is a mandatory step 
	 */
	sod_img cannyImg = sod_canny_edge_image(binImg, 1/* Reduce noise */);
	/*
	 * Dilate the image say 12 times but you should experiment
	 * with different values for best results which depend
	 * on the quality of the input image/frame. */
	sod_img dilImg = sod_dilate_image(cannyImg, 12);
	/* Perform connected component labeling or blob detection
	 * now on the binary, canny edged, Gaussian noise reduced and
	 * finally dilated image using our filter callback that should
	 * discard small or large rectangle areas.
	 */
	sod_box *box = 0;
	int i, nbox;
	sod_image_find_blobs(dilImg, &box, &nbox, filter_cb);
	/* Draw a box on each potential plate coordinates */
	for (i = 0; i < nbox; i++) {
		sod_image_draw_bbox_width(imgCopy, box[i], 5, 255., 0, 225.); // rose box
	}
	sod_image_blob_boxes_release(box);
	/* Finally save the output image to the specified path */
	sod_img_save_as_png(imgCopy, zOut);
	/* Cleanup */
	sod_free_image(imgIn);
	sod_free_image(cannyImg);
	sod_free_image(binImg);
	sod_free_image(dilImg);
	sod_free_image(imgCopy);
	return 0;
}