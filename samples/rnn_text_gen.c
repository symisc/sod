/*
 * Programming introduction with the SOD Embedded Recurrent Neural Networks (RNN) API.
 * Copyright (C) PixLab | Symisc Systems, https://sod.pixlab.io
 */
/*
* Compile this file together with the SOD embedded source code to generate
* the executable. For example:
*
*  gcc sod.c rnn_text_gen.c -lm -Ofast -march=native -Wall -std=c99 -o sod_rnn
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
/* RNN text generation (i.e. Kant, Shakespeare, Tolstoy, Python code, 4 Chan, etc.) depending
 * on the pre-trained model.
 */
static void text_consumer_callback(
	const char *zText, /* Text ready to be consumed (always nil terminated) */
	size_t text_len,   /* zText[] length */
	void *pUserdata    /* Arbitrary user pointer passed verbatim by the RNN layer */
)
{
	/*
	 * This is the callback that is called by the RNN
	 * each time a generated text is ready
	 * to be consumed. See below on how to install
	 * this callback.
	 */
	puts(zText); /* Simply redirect the generated text to stdout */
}
int main(int argc, char *argv[])
{
	/*
	 * Path to the pre-trained RNN model where the generated text is based on.
	 * You can download pre-trained RNN models on https://pixlab.io/downloads.
	 */
	const char *zRnnModel = argc > 1 ? argv[1] : "./tolstoy-rnn.sod";
	/*
	 * The RNN (although named sod_cnn) handle that should perform text generation for us */
	sod_cnn *pNet;
	int rc;
	const char *zErr; /* Error log if any */
	/*
	 * Create our RNN handle using the built-in `rnn`
	 * architecture and associate the desired pre-trained
	 * model with it.
	 */
	rc = sod_cnn_create(&pNet, ":rnn", zRnnModel, &zErr);
	/*
	 * ":rnn" is the magic word for the built-in RNN
	 * architecture. The list of built-in Magic words (pre-ready to use 
	 * configurations and their associated models) are documented here:
	 * https://sod.pixlab.io/c_api/sod_cnn_create.html.
	 */
	if (rc != SOD_OK) {
		/* Display the error message and exit */
		puts(zErr);
		return 0;
	}
	/* Register the text consumer callback */
	sod_cnn_config(pNet, SOD_RNN_CALLBACK, text_consumer_callback, 0 /* user data (NULL) */);
	/* Generate a text of length 500 characters */
	sod_cnn_config(pNet, SOD_RNN_TEXT_LENGTH, 500);
	/* Seed for the text */
	sod_cnn_config(pNet, SOD_RNN_SEED, "@>");
	/* 
	 * Start the text generation process. The consumer callback
	 * should redirect the generated text to STDOUT.
	 */
	sod_cnn_predict(pNet, 0, 0, 0);
	/*
	 * At this stage, text_consumer_callback() should have been called
	 * and the generated text already consumed.
	 */
	/* Clean-up */
	sod_cnn_destroy(pNet);
	return 0;
}
