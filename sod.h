#ifndef _SOD_H_
#define _SOD_H_
/*
* SOD - An Embedded Computer Vision & Machine Learning Library.
* Copyright (C) 2018 - 2019 PixLab| Symisc Systems. https://sod.pixlab.io
* Version 1.1.8
*
* Symisc Systems employs a dual licensing model that offers customers
* a choice of either our open source license (GPLv3) or a commercial 
* license.
*
* For information on licensing, redistribution of the SOD library, and for a DISCLAIMER OF ALL WARRANTIES
* please visit:
*     https://pixlab.io/sod
* or contact:
*     licensing@symisc.net
*     support@pixlab.io
*/
/*
 * This file is part of Symisc SOD - Open Source Release (GPLv3)
 *
 * SOD is free software : you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * SOD is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with SOD. If not, see <http://www.gnu.org/licenses/>.
 */
/* Make sure we can call this stuff from C++ */
#ifdef __cplusplus
 extern "C" {
#endif 
/*
 * Marker for exported interfaces.
 */
#if defined (_MSC_VER) || defined (__MINGW32__) ||  defined (__GNUC__) && defined (__declspec)
#define SOD_APIIMPORT	__declspec(dllimport)
#define SOD_APIEXPORT	__declspec(dllexport)
#else
#define	SOD_APIIMPORT
#define	SOD_APIEXPORT
#endif
/*
 * The SOD_VERSION C preprocessor macro evaluates to a string literal
 * that is the SOD version in the format "X.Y.Z" where X is the major
 * version number and Y is the minor version number and Z is the release
 * number.
 */
#define SOD_VERSION "1.1.8"
 /*
 * The SOD_VERSION_NUMBER C preprocessor macro resolves to an integer
 * with the value (X*1000000 + Y*1000 + Z) where X, Y, and Z are the same
 * numbers used in [SOD_VERSION].
 */
#define SOD_VERSION_NUMBER 1001007
/*
 * Forward declarations. 
 */
/* 
 * RealNets handle documented at https://sod.pixlab.io/api.html#sod_realnet. */
typedef struct sod_realnet sod_realnet;
/*
 * Convolutional/Recurrent Neural Networks (CNN/RNN) handle documented at https://sod.pixlab.io/api.html#sod_cnn. */
typedef struct sod_cnn sod_cnn;
/*
 * bounding box structure documented at https://sod.pixlab.io/api.html#sod_box. */
typedef struct sod_box sod_box;
/*
 * Image/Frame container documented at https://sod.pixlab.io/api.html#sod_img. */
typedef struct sod_img sod_img;
/* 
 * Point instance documented at https://sod.pixlab.io/api.html#sod_pts. */
typedef struct sod_pts sod_pts;
/* 
 * RealNets model handle documented at https://sod.pixlab.io/api.html#sod_realnet_model_handle. */
typedef unsigned int sod_realnet_model_handle;
#ifdef SOD_ENABLE_NET_TRAIN
/* 
 * RealNets trainer handle documented at https://sod.pixlab.io/api.html#sod_realnet_trainer. */
typedef struct sod_realnet_trainer sod_realnet_trainer;
#endif
/*
 * A bounding box or bbox for short is represented by an instance of the `sod_box` structure.
 * A sod_box instance always store the coordinates of a rectangle obtained from a prior successful
 * call to one of the object detection routines of a sod_cnn or sod_realnet handle such as
 * `sod_cnn_predict()` or from the connected component labeling interface `sod_image_find_blobs()`.
 *
 * Besides the rectangle coordinates. The `zName` and `score` fields member of this structure hold
 * useful information about the object it surround.
 *
 * This structure and related interfaces are documented at https://sod.pixlab.io/api.html#sod_box.
 */
struct sod_box {
	int x;  /* The x-coordinate of the upper-left corner of the rectangle */
	int y;  /* The y-coordinate of the upper-left corner of the rectangle */
	int w;  /* Rectangle width */
	int h;  /* Rectangle height */
	float score;       /* Confidence threshold. */
	const char *zName; /* Detected object name. I.e. person, face, dog, car, plane, cat, bicycle, etc. */
	void *pUserData;   /* External pointer used by some modules such as the face landmarks, NSFW classifier, pose estimator, etc. */
};
/*
 * Internally, each in-memory representation of an input image or video frame
 * is kept in an instance of the `sod_img` structure. Basically, a `sod_img` is just a record
 * of the width, height and number of color channels in an image, and also the pixel values
 * for every pixel. Images pixels are arranged in CHW format. This means in a 3 channel 
 * image with width 400 and height 300, the first 400 values are the 1st row of the 1st channel
 * of the image. The second 400 pixels are the 2nd row. after 120,000 values we get to pixels
 * in the 2nd channel, and so forth.
 *
 * This structure and related interfaces are documented at https://sod.pixlab.io/api.html#sod_img.
 */
struct sod_img {
	int h;   /* Image/frame height */
	int w;   /* Image/frame width */
	int c;   /* Image depth/Total number of color channels e.g. 1 for grayscale images, 3 RGB, etc. */
	float *data; /* Blob */
};
/*
 * An instance of the `sod_pts` structure describe a 2D point in space with integer coordinates
 * (usually zero-based). This structure is rarely manipulated by SOD and is used mostly by 
 * the Hough line detection interface `sod_hough_lines_detect()` and line drawing routine `sod_image_draw_line()`.
 *
 * This structure and related interfaces are documented at https://sod.pixlab.io/api.html#sod_pts.
 */
struct sod_pts {
	int x; /* The x-coordinate, in logical units of the point offset. */
	int y; /* The y-coordinate, in logical units of the point offset. */
};
/*
 * An integer configuration option that determines what property of a `sod_cnn` handle is to be configured. 
 * Subsequent arguments vary depending on the configuration verb.
 *
 * The documentation (including expected arguments for each configuration verb) is available to consult
 * at https://sod.pixlab.io/c_api/sod_cnn_config.html.
 */
typedef enum {
	SOD_CNN_NETWORK_OUTPUT = 1,
	SOD_CNN_DETECTION_THRESHOLD,
	SOD_CNN_NMS,
	SOD_CNN_DETECTION_CLASSES,
	SOD_CNN_RAND_SEED,
	SOD_CNN_HIER_THRESHOLD,
	SOD_CNN_TEMPERATURE,
	SOD_CNN_LOG_CALLBACK,
	SOD_RNN_CALLBACK,
	SOD_RNN_TEXT_LENGTH,
	SOD_RNN_DATA_LENGTH,
	SOD_RNN_SEED
}SOD_CNN_CONFIG;
/* 
 * RNN Consumer callback to be used in conjunction with the `SOD_RNN_CALLBACK` configuration verb.
 * 
 * The documentation is available to consult at https://sod.pixlab.io/c_api/sod_cnn_config.html.
 */
typedef void (*ProcRnnCallback)(const char *, size_t, void *);
/*
* Log Consumer callback to be used in conjunction with the `SOD_CNN_LOG_CALLBACK` or
* the `SOD_REALNET_TR_LOG_CALLBACK` configuration verb.
*
* The documentation is available to consult at https://sod.pixlab.io/c_api/sod_cnn_config.html.
*/
typedef void(*ProcLogCallback)(const char *, size_t, void *);
/* 
 * Macros to be used in conjunction with the `sod_img_load_from_file()` or `sod_img_load_from_mem()` interfaces.
 */
#define SOD_IMG_COLOR     0 /* Load full color channels. */
#define SOD_IMG_GRAYSCALE 1 /* Load an image in the grayscale colorpsace only (single channel). */
/* 
 * Macros around a stack allocated `sod_img` instance.
 */
#define SOD_IMG_2_INPUT(IMG)  (IMG.data)  /* Pointer to raw binary contents (blobs) of an image or frame. */
#define SOD_IS_EMPTY_IMG(IMG) (!IMG.data) /* NIL pointer test (marker for an empty or broken image format). */
/*
 * Possible return value from each exported SOD interface defined below.
 */
#define SOD_OK           0 /* Everything went well */
#define SOD_UNSUPPORTED -1 /* Unsupported Pixel format */
#define SOD_OUTOFMEM    -4 /* Out-of-Memory */
#define SOD_ABORT	    -5 /* User callback request an operation abort */
#define SOD_IOERR       -6 /* IO error */
#define SOD_LIMIT       -7 /* Limit reached */
/*
 * An integer configuration option that determines what property of a `sod_realnet_trainer` handle is to be configured.
 * Subsequent arguments vary depending on the configuration verb.
 *
 * The documentation (including expected arguments for each configuration verb) is available to consult
 * at https://sod.pixlab.io/c_api/sod_realnet_train_config.html.
 */
#ifdef SOD_ENABLE_NET_TRAIN
typedef enum {
	SOD_REALNET_TR_LOG_CALLBACK = 1,
	SOD_REALNET_TR_OUTPUT_MODEL
}SOD_REALNET_TRAINER_CONFIG;
#endif /* SOD_ENABLE_NET_TRAIN */
/*
* An integer configuration option that determines what property of a `sod_realnet` handle is to be configured.
* Subsequent arguments vary depending on the configuration verb.
*
* The documentation (including expected arguments for each configuration verb) is available to consult
* at https://sod.pixlab.io/c_api/sod_realnet_model_config.html.
*/
typedef enum {
	SOD_REALNET_MODEL_MINSIZE = 1,
	SOD_REALNET_MODEL_MAXSIZE,
	SOD_REALNET_MODEL_SCALEFACTOR,
	SOD_REALNET_MODEL_STRIDEFACTOR,
	SOD_RELANET_MODEL_DETECTION_THRESHOLD,
	SOD_REALNET_MODEL_NMS,
	SOD_REALNET_MODEL_DISCARD_NULL_BOXES,
	SOD_REALNET_MODEL_NAME,
	SOD_REALNET_MODEL_ABOUT_INFO
}SOD_REALNET_MODEL_CONFIG;
/*
 * SOD Embedded C/C++ API. 
 *
 * The API documentation is available to consult at https://sod.pixlab.io/api.html.
 * The introduction course is available to consult at https://sod.pixlab.io/intro.html.
 */
#ifndef SOD_DISABLE_CNN
/*
 * Convolutional/Recurrent Neural Networks (CNN/RNN) API.
 *
 * The interfaces are documented at https://sod.pixlab.io/api.html#cnn.
 */
SOD_APIEXPORT int  sod_cnn_create(sod_cnn **ppOut, const char *zArch, const char *zModelPath, const char **pzErr);
SOD_APIEXPORT int  sod_cnn_config(sod_cnn *pNet, SOD_CNN_CONFIG conf, ...);
SOD_APIEXPORT int  sod_cnn_predict(sod_cnn *pNet, float *pInput, sod_box **paBox, int *pnBox);
SOD_APIEXPORT void sod_cnn_destroy(sod_cnn *pNet);
SOD_APIEXPORT float *  sod_cnn_prepare_image(sod_cnn *pNet, sod_img in);
SOD_APIEXPORT int sod_cnn_get_network_size(sod_cnn *pNet, int *pWidth, int *pHeight, int *pChannels);
#endif /* SOD_DISABLE_CNN */
/*
 * RealNets API.
 *
 * The interfaces are documented at https://sod.pixlab.io/api.html#realnet.
 */
SOD_APIEXPORT int sod_realnet_create(sod_realnet **ppOut);
SOD_APIEXPORT int sod_realnet_load_model_from_mem(sod_realnet *pNet, const void * pModel, unsigned int nBytes, sod_realnet_model_handle *pOutHandle);
SOD_APIEXPORT int sod_realnet_load_model_from_disk(sod_realnet *pNet, const char * zPath, sod_realnet_model_handle *pOutHandle);
SOD_APIEXPORT int sod_realnet_model_config(sod_realnet *pNet, sod_realnet_model_handle handle, SOD_REALNET_MODEL_CONFIG conf, ...);
SOD_APIEXPORT int sod_realnet_detect(sod_realnet *pNet, const unsigned char *zGrayImg, int width, int height, sod_box **apBox, int *pnBox);
SOD_APIEXPORT void sod_realnet_destroy(sod_realnet *pNet);
#ifdef SOD_ENABLE_NET_TRAIN
/*
 * RealNets Training API.
 *
 * The interfaces are documented at https://sod.pixlab.io/api.html#realnet_train.
 */
SOD_APIEXPORT int  sod_realnet_train_init(sod_realnet_trainer **ppOut);
SOD_APIEXPORT int  sod_realnet_train_config(sod_realnet_trainer *pTrainer, SOD_REALNET_TRAINER_CONFIG op, ...);
SOD_APIEXPORT int  sod_realnet_train_start(sod_realnet_trainer *pTrainer, const char *zConf);
SOD_APIEXPORT void sod_realnet_train_release(sod_realnet_trainer *pTrainer);
#endif /* SOD_ENABLE_NET_TRAIN */
/* 
 * Image Processing API.
 *
 * The interfaces are documented at https://sod.pixlab.io/api.html#imgproc.
 */
SOD_APIEXPORT sod_img sod_make_empty_image(int w, int h, int c);
SOD_APIEXPORT sod_img sod_make_image(int w, int h, int c);
SOD_APIEXPORT int sod_grow_image(sod_img *pImg,int w, int h, int c);
SOD_APIEXPORT sod_img sod_make_random_image(int w, int h, int c);
SOD_APIEXPORT sod_img sod_copy_image(sod_img m);
SOD_APIEXPORT void sod_free_image(sod_img m);

#ifndef SOD_DISABLE_IMG_READER
SOD_APIEXPORT sod_img sod_img_load_from_file(const char *zFile, int nChannels);
SOD_APIEXPORT sod_img sod_img_load_from_mem(const unsigned char *zBuf, int buf_len, int nChannels);
SOD_APIEXPORT int  sod_img_set_load_from_directory(const char *zPath, sod_img ** apLoaded, int * pnLoaded, int max_entries);
SOD_APIEXPORT void sod_img_set_release(sod_img *aLoaded, int nEntries);
#ifndef SOD_DISABLE_IMG_WRITER
SOD_APIEXPORT int sod_img_save_as_png(sod_img input, const char *zPath);
SOD_APIEXPORT int sod_img_save_as_jpeg(sod_img input, const char *zPath, int Quality);
SOD_APIEXPORT int sod_img_blob_save_as_png(const char * zPath, const unsigned char *zBlob, int width, int height, int nChannels);
SOD_APIEXPORT int sod_img_blob_save_as_jpeg(const char * zPath, const unsigned char *zBlob, int width, int height, int nChannels, int Quality);
SOD_APIEXPORT int sod_img_blob_save_as_bmp(const char * zPath, const unsigned char *zBlob, int width, int height, int nChannels);
#endif /* SOD_DISABLE_IMG_WRITER */
#define sod_img_load_color(zPath) sod_img_load_from_file(zPath, SOD_IMG_COLOR)
#define sod_img_load_grayscale(zPath) sod_img_load_from_file(zPath, SOD_IMG_GRAYSCALE)
#endif /* SOD_DISABLE_IMG_READER */

SOD_APIEXPORT float sod_img_get_pixel(sod_img m, int x, int y, int c);
SOD_APIEXPORT void sod_img_set_pixel(sod_img m, int x, int y, int c, float val);
SOD_APIEXPORT void sod_img_add_pixel(sod_img m, int x, int y, int c, float val);
SOD_APIEXPORT sod_img sod_img_get_layer(sod_img m, int l);

SOD_APIEXPORT void sod_img_rgb_to_hsv(sod_img im);
SOD_APIEXPORT void sod_img_hsv_to_rgb(sod_img im);
SOD_APIEXPORT void sod_img_rgb_to_bgr(sod_img im);
SOD_APIEXPORT void sod_img_bgr_to_rgb(sod_img im);
SOD_APIEXPORT void sod_img_yuv_to_rgb(sod_img im);
SOD_APIEXPORT void sod_img_rgb_to_yuv(sod_img im);

SOD_APIEXPORT sod_img sod_minutiae(sod_img bin, int *pTotal, int *pEp, int *pBp);
SOD_APIEXPORT sod_img sod_gaussian_noise_reduce(sod_img grayscale);
SOD_APIEXPORT sod_img sod_equalize_histogram(sod_img im);

SOD_APIEXPORT sod_img sod_grayscale_image(sod_img im);
SOD_APIEXPORT void sod_grayscale_image_3c(sod_img im);

SOD_APIEXPORT sod_img sod_threshold_image(sod_img im, float thresh);
SOD_APIEXPORT sod_img sod_otsu_binarize_image(sod_img im);
SOD_APIEXPORT sod_img sod_binarize_image(sod_img im, int reverse);
SOD_APIEXPORT sod_img sod_dilate_image(sod_img im, int times);
SOD_APIEXPORT sod_img sod_erode_image(sod_img im, int times);

SOD_APIEXPORT sod_img sod_sharpen_filtering_image(sod_img im);
SOD_APIEXPORT sod_img sod_hilditch_thin_image(sod_img im);

SOD_APIEXPORT sod_img sod_sobel_image(sod_img im);
SOD_APIEXPORT sod_img sod_canny_edge_image(sod_img im, int reduce_noise);

SOD_APIEXPORT sod_pts * sod_hough_lines_detect(sod_img im, int threshold, int *nPts);
SOD_APIEXPORT void sod_hough_lines_release(sod_pts *pLines);

SOD_APIEXPORT int sod_image_find_blobs(sod_img im, sod_box **paBox, int *pnBox, int(*xFilter)(int width, int height));
SOD_APIEXPORT void sod_image_blob_boxes_release(sod_box *pBox);

SOD_APIEXPORT void sod_composite_image(sod_img source, sod_img dest, int dx, int dy);
SOD_APIEXPORT void sod_flip_image(sod_img input);
SOD_APIEXPORT sod_img sod_image_distance(sod_img a, sod_img b);

SOD_APIEXPORT void sod_embed_image(sod_img source, sod_img dest, int dx, int dy);
SOD_APIEXPORT sod_img sod_blend_image(sod_img fore, sod_img back, float alpha);
SOD_APIEXPORT void sod_scale_image_channel(sod_img im, int c, float v);
SOD_APIEXPORT void sod_translate_image_channel(sod_img im, int c, float v);

SOD_APIEXPORT sod_img sod_resize_image(sod_img im, int w, int h);
SOD_APIEXPORT sod_img sod_resize_max(sod_img im, int max);
SOD_APIEXPORT sod_img sod_resize_min(sod_img im, int min);
SOD_APIEXPORT sod_img sod_rotate_crop_image(sod_img im, float rad, float s, int w, int h, float dx, float dy, float aspect);
SOD_APIEXPORT sod_img sod_rotate_image(sod_img im, float rad);

SOD_APIEXPORT void sod_translate_image(sod_img m, float s);
SOD_APIEXPORT void sod_scale_image(sod_img m, float s);
SOD_APIEXPORT void sod_normalize_image(sod_img p);
SOD_APIEXPORT void sod_transpose_image(sod_img im);

SOD_APIEXPORT sod_img sod_crop_image(sod_img im, int dx, int dy, int w, int h);
SOD_APIEXPORT sod_img sod_random_crop_image(sod_img im, int w, int h);
SOD_APIEXPORT sod_img sod_random_augment_image(sod_img im, float angle, float aspect, int low, int high, int size);

SOD_APIEXPORT void sod_image_draw_box(sod_img im, int x1, int y1, int x2, int y2, float r, float g, float b);
SOD_APIEXPORT void sod_image_draw_box_grayscale(sod_img im, int x1, int y1, int x2, int y2, float g);
SOD_APIEXPORT void sod_image_draw_circle(sod_img im, int x0, int y0, int radius, float r, float g, float b);
SOD_APIEXPORT void sod_image_draw_circle_thickness(sod_img im, int x0, int y0, int radius, int width, float r, float g, float b);
SOD_APIEXPORT void sod_image_draw_bbox(sod_img im, sod_box bbox, float r, float g, float b);
SOD_APIEXPORT void sod_image_draw_bbox_width(sod_img im, sod_box bbox, int width, float r, float g, float b);
SOD_APIEXPORT void sod_image_draw_line(sod_img im, sod_pts start, sod_pts end, float r, float g, float b);

SOD_APIEXPORT unsigned char * sod_image_to_blob(sod_img im);
SOD_APIEXPORT void sod_image_free_blob(unsigned char *zBlob);
/*
 * OpenCV Integration API. The library must be compiled against OpenCV
 * with the compile-time directive SOD_ENABLE_OPENCV defined.
 *
 * The documentation is available to consult at https://sod.pixlab.io/api.html#cmpl_opencv.
 * The interfaces are documented at https://sod.pixlab.io/api.html#cvinter.
 */
#ifdef SOD_ENABLE_OPENCV
/*
 * Change the include paths to the directory where OpenCV reside
 * if those headers are not found by your compiler.
 */
#include <opencv2/highgui/highgui_c.h>
#include <opencv2/imgproc/imgproc_c.h>

SOD_APIEXPORT sod_img sod_img_load_cv_ipl(IplImage* src);
SOD_APIEXPORT sod_img sod_img_load_from_cv(const char *filename, int channels);
SOD_APIEXPORT sod_img sod_img_load_from_cv_stream(CvCapture *cap);
SOD_APIEXPORT int  sod_img_fill_from_cv_stream(CvCapture *cap, sod_img *pImg);
SOD_APIEXPORT void sod_img_save_to_cv_jpg(sod_img im, const char *zPath);
#endif /* SOD_ENABLE_OPENCV */
/*
 * SOD Embedded Release Information & Copyright Notice.
 */
SOD_APIEXPORT const char * sod_lib_copyright(void);
#define SOD_LIB_INFO "SOD Embedded - Release 1.1.8 under GPLv3. Copyright (C) 2018 PixLab| Symisc Systems, https://sod.pixlab.io"
#ifdef __cplusplus
 }
#endif 
#endif /* _SOD_H_ */
