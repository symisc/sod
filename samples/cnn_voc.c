/*
 * Programming introduction with the SOD Embedded Convolutional/Recurrent Neural Networks (CNN/RNN) API.
 * Copyright (C) PixLab | Symisc Systems, https://sod.pixlab.io
 */
/*
* Compile this file together with the SOD embedded source code to generate
* the executable. For example:
*
*  gcc sod.c cnn_voc.c -lm -Ofast -march=native -Wall -std=c99 -o sod_cnn_intro
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
int main(int argc, char *argv[])
{
	/* Input image (pass a path or use the test image shipped with the samples ZIP archive) */
	const char *zInput = argc > 1 ? argv[1] : "./test.png";
	/* Draw detection boxes (i.e. rectangles) on this output image which
	 * is a copy of the input plus the boxes.
	 */
	const char *zOut = argc > 2 ? argv[2] : "./out.png";
	/*
	 * The CNN handle that should perform the detection process */
	sod_cnn *pNet;
	/* Load the input image */
	sod_img imgIn = sod_img_load_from_file(zInput,SOD_IMG_COLOR/* Full colors*/);
	if (imgIn.data == 0) {
		/* Invalid path, unsupported format, memory failure, etc. */
		puts("Cannot load input image..exiting");
		return 0;
	}
	/* Make a copy so we can draw anything we want. */
	sod_img imgOut = sod_copy_image(imgIn);
	int rc;
	const char *zErr; /* Error log if any */
	/*
	 * Create our CNN handle using the built-in fast
	 * architecture trained on the Pascal VOC dataset
	 * and is able to detect 20 classes of objects at
	 * real-time on a modern CPU.
	 */
	rc = sod_cnn_create(&pNet, ":voc", "./tiny20.sod", &zErr);
	/*
	 * ":voc" is the magic word for the built-in Pascal VOC (20 classes) 
	 * fast architecture. The list of built-in Magic words (pre-ready to use 
	 * configurations and their associated models) are documented here:
	 * https://sod.pixlab.io/c_api/sod_cnn_create.html.
	 *
	 * "tiny20.sod" is the pre-trained model associated with the ":fast" architecture 
	 *  and is available to download from https://pixlab.io/downloads
	 */
	if (rc != SOD_OK) {
		/* Display the error message and exit */
		puts(zErr);
		return 0;
	}
	/*
	 * A sod_box instance always store the coordinates for each detected object
	 * returned by the CNN via sod_cnn_predict() as we'll see later.
	 */
	sod_box *box;
	int i, nbox;
	/* Prepare our input image for the detection process which 
	 * is resized to the network dimension (This op is always very fast)
	 */
	float * blob = sod_cnn_prepare_image(pNet, imgIn);
	if (!blob) {
		/* Very unlikely this happen: Invalid architecture, out-of-memory */
		puts("Something went wrong while preparing image..");
		return 0;
	}
	puts("Starting CNN object detection");
	/* Detect.. */
	sod_cnn_predict(pNet, blob, &box, &nbox);
	/* Report the detection result. */
	printf("%d object(s) were detected..\n",nbox);
	for (i = 0; i < nbox; i++) {
		/* Report the coordinates, name and score of the current detected object */
		printf("(%s) X:%d Y:%d Width:%d Height:%d score:%f%%\n", box[i].zName, box[i].x, box[i].y, box[i].w, box[i].h, box[i].score * 100);
		if( box[i].score < 0.3) continue;   /* Discard low score detection, remove if you want to report all objects */
		/*
		 * Draw a rose (RGB: 255,0,255) rectangle of width 3 on the object coordinates. */
		sod_image_draw_bbox_width(imgOut, box[i], 3, 255., 0, 225.);
		/* Of course, one could draw a circle via sod_image_draw_circle() or 
		 * crop the entire region via sod_crop_image() instead of drawing a rectangle. */
	}
	/* Finally save our output image with the boxes drawn on it */
	sod_img_save_as_png(imgOut, zOut);
	/* Cleanup */
	sod_free_image(imgIn);
	sod_free_image(imgOut);
	/* Release all resources allocated to the CNN handle */
	sod_cnn_destroy(pNet);
	return 0;
}