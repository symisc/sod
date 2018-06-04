/*
 * Programming introduction with the SOD Embedded Image Processing API.
 * Copyright (C) PixLab | Symisc Systems, https://sod.pixlab.io
 */
/*
* Compile this file together with the SOD embedded source code to generate
* the executable. For example:
*
*  gcc sod.c batch_img_loading.c -lm -Ofast -march=native -Wall -std=c99 -o sod_img_proc
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
* Load all supported image format present in a given directory.
*/
int main(int argc, char *argv[])
{
	/* Input directory (pass a path or load from the current working directory) */
	const char *zDir = argc > 1 ? argv[1] : "./";
	/* Our image array */
	sod_img *aEntries;
	int nEntries;
	int i, rc;
	/* Bulk loading */
	rc = sod_img_set_load_from_directory(zDir, &aEntries, &nEntries, 100 /* Maximum images to load, pass 0 to load every image */);
	if (rc != SOD_OK) {
		printf("IO error while loading images from '%s'\n", zDir);
		return -1;
	}
	/* Report */
	printf("%d image(s) were loaded from '%s'..\n", nEntries, zDir);
	for (i = 0; i < nEntries; ++i) {
		printf("Width: %d, Height: %d, Color channels: %d\n", aEntries[i].w, aEntries[i].h, aEntries[i].c);
		/* Uncomment to apply some transformation if desired */
		//sod_grayscale_image_3c(aEntries[i]);
	}
	/* Cleanup */
	sod_img_set_release(aEntries, nEntries);
	return 0;
}
