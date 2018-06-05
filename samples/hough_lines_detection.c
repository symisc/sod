/*
 * Programming introduction with the SOD Embedded Image Processing API.
 * Copyright (C) PixLab | Symisc Systems, https://sod.pixlab.io
 */
/*
* Compile this file together with the SOD embedded source code to generate
* the executable. For example:
*
*  gcc sod.c hough_lines_detection.c -lm -Ofast -march=native -Wall -std=c99 -o sod_img_proc
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
 * Perform hough line detection (Generally named Hough Transform) on an input image.
 */
int main(int argc, char *argv[])
{
	/* Input image (pass a path or use the test image shipped with the samples ZIP archive) */
	const char *zInput = argc > 1 ? argv[1] : "./test.png";
	/* Processed output image path */
	const char *zOut = argc > 2 ? argv[2] : "./out_lines.png";
	/* Load the input image in the grayscale colorspace */
	sod_img imgIn = sod_img_load_grayscale(zInput);
	if (imgIn.data == 0) {
		/* Invalid path, unsupported format, memory failure, etc. */
		puts("Cannot load input image..exiting");
		return 0;
	}
	/* A full color copy of the input image so we can draw later rose lines on it.
	 */
	sod_img imgCopy = sod_img_load_color(zInput);
	/* 
	 * Perform Canny edge detection first which is a mandatory step */
	sod_img cannyImg = sod_canny_edge_image(imgIn, 0);
	/*
	 * Each detected line is represented by an instance of the `sod_pts`
	 * structure returned as an array by the Hough interface where
	 * each entry of this array (i and i + 1) hold the starting and 
	 * ending position (x_start, y_start, x_end, y_end) for each line.
	 */
	sod_pts * aLines;
	int i, nPts, nLines;
	/* Perform hough line detection on the canny edged image
	 * Depending on the analyzed image/frame, you should experiment
	 * with different thresholds for best results.
	 */
	aLines = sod_hough_lines_detect(cannyImg, 0 /* default threshold which may not good for all images */, &nPts);
	/* Report */
	nLines = nPts / 2;
	printf("%d line(s) were detected\n", nLines);
	/* Draw a rose line for each entry on the full color image copy */
	for (i = 0; i < nLines; i += 2) {
		sod_image_draw_line(imgCopy, aLines[i], aLines[i + 1], 255, 0, 255);
	}
	/* Finally save the output image to the specified path */
	sod_img_save_as_png(imgCopy, zOut);
	/* Cleanup */
	sod_hough_lines_release(aLines);
	sod_free_image(imgIn);
	sod_free_image(cannyImg);
	sod_free_image(imgCopy);
	return 0;
}