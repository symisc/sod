/*
 * Programming introduction with the SOD Embedded RealNets API (Frontal Facial detection).
 * Copyright (C) PixLab | Symisc Systems, https://sod.pixlab.io
 */
/*
* Compile this file together with the SOD embedded source code to generate
* the executable. For example:
*
*  gcc sod.c realnet_face_detection.c -lm -Ofast -march=native -Wall -std=c99 -o sod_realnet_face_detect
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
	const char *zFile = argc > 1 ? argv[1] : "./realnet_faces.jpg";
	/*
	* By default, RealNets are designed to process video streams thanks
	* to their very fast processing speed. However, for the sake of simplicity
	* we'll stick with images for this programming intro to RealNets.
	*/
	sod_realnet *pNet; /* Realnet handle */
	int i,rc;
	/*
	 * Allocate a new RealNet handle */
	rc = sod_realnet_create(&pNet);
	if (rc != SOD_OK) return rc;
	/* 
	 * Register and load a RealNet model.
	 * You can train your own RealNet model on your CPU using the training interfaces [sod_realnet_train_start()]
	 * or download pre-trained models like this one from https://pixlab.io/downloads
	 */
	rc = sod_realnet_load_model_from_disk(pNet, "./face.realnet.sod", 0);
	if (rc != SOD_OK) return rc;
	/* Load the target image in grayscale colorspace */
	sod_img img = sod_img_load_grayscale(zFile);
	if (img.data == 0) {
		puts("Cannot load image");
		return 0;
	}
	/* Load a full color copy of the target image so we draw rose boxes
	 * Note that drawing on grayscale images is also supported.
	 */
	sod_img color = sod_img_load_color(zFile);
	/*
	 * convert the grayscale image to blob.
	 */
	unsigned char *zBlob = sod_image_to_blob(img);
	/* 
	 * Bounding boxes array
	 */
	sod_box *aBoxes;
	int nbox;
	/* 
	 * Perform Real-Time detection on this blob
	 */
	rc = sod_realnet_detect(pNet, zBlob, img.w, img.h, &aBoxes, &nbox);
	if (rc != SOD_OK) return rc;
	/* Consume result */
	printf("%d potential face(s) were detected..\n", nbox);
	for (i = 0; i < nbox; i++) {
		/* Ignore low score detection */
		if (aBoxes[i].score < 5.0) continue;
		/* Report current object */
		printf("(%s) x:%d y:%d w:%d h:%d prob:%f\n", aBoxes[i].zName, aBoxes[i].x, aBoxes[i].y, aBoxes[i].w, aBoxes[i].h, aBoxes[i].score);
		/* Draw a rose box on the target coordinates */
		sod_image_draw_bbox_width(color, aBoxes[i], 3, 255., 0, 225.);
		//sod_image_draw_circle(color, aBoxes[i].x + (aBoxes[i].w / 2), aBoxes[i].y + (aBoxes[i].h / 2), aBoxes[i].w, 255., 0, 225.);
	}
	/* Save the detection result */
	sod_img_save_as_png(color, argc > 2 ? argv[2] : "./out.png");
	/* cleanup */
	sod_free_image(img);
	sod_free_image(color);
	sod_image_free_blob(zBlob);
	sod_realnet_destroy(pNet);
	return 0;
}