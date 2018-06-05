/*
 * Programming introduction with the SOD Embedded RealNets Model Training API.
 * Training must be enabled via the compile-time directive SOD_ENABLE_NET_TRAIN.
 *
 * Copyright (C) PixLab | Symisc Systems, https://sod.pixlab.io
 */
/*
* Compile this file together with the SOD embedded source code to generate
* the executable. For example:
*
*  gcc sod.c realnet_train_model.c -D SOD_ENABLE_NET_TRAIN -lm -Ofast -march=native -Wall -std=c99 -o sod_realnet_train_intro
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
 * Training log consumer callback that should be called
 * by the Realnet trainer to report training progress.
 */
void log_consumer_callback(const char *zText, size_t text_len, void *pUserdata)
{
	/* Simply redirect to stdout */
	puts(zText);
}
int main(int argc, char *argv[])
{
	/* Training instructions (i.e. where positive and negative samples
	 * are located, tree minimal depth, max trees, model copyright notice and so on).
	 * Pass a path or download one from https://pixlab.io/downloads
	 */
	const char *zTrainFile = argc > 1 ? argv[1] : "train.txt";
	/*
	 * Relanet trainer handle 
	 */
	sod_realnet_trainer *pNet;
	int rc;
	/* Allocate a new Realnet Trainer handle */
	rc = sod_realnet_train_init(&pNet);
	if (rc != SOD_OK) return rc;
	/*
	 * Install our training progress log consumer callback.
	 */
	rc = sod_realnet_train_config(pNet, SOD_REALNET_TR_LOG_CALLBACK, log_consumer_callback, 0);
	if (rc != SOD_OK) return rc;
	/*
	 * Where to store the output model.
	 */
	rc = sod_realnet_train_config(pNet, SOD_REALNET_TR_OUTPUT_MODEL, "./pedestrian_detetcor.realnet");
	if (rc != SOD_OK) return rc;
	/*
	 * Start the heavy training process on your CPU driven by
	 * the Realnet instructions found on `zTrainFile`.
	 */
	rc = sod_realnet_train_start(pNet, zTrainFile);
	/* Wait some days...*/
	sod_realnet_train_release(pNet);
	/* check the progress log and you should find
	* a working model on the path you specified earlier.
	*/
	return rc;
}