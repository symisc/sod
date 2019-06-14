<h1 align="center">SOD<br/><br/>An Embedded Computer Vision & Machine Learning Library<br/><a href="https://sod.pixlab.io">sod.pixlab.io</a></h1>

[![Build Status](https://travis-ci.org/symisc/sod.svg?branch=master)](https://travis-ci.org/symisc/sod)
[![API documentation](https://img.shields.io/badge/API%20documentation-Ready-green.svg)](https://sod.pixlab.io/api.html)
[![dependency](https://img.shields.io/badge/dependency-none-ff96b4.svg)](https://pixlab.io/downloads)
[![Getting Started](https://img.shields.io/badge/Getting%20Started-Now-f49242.svg)](https://sod.pixlab.io/intro.html)
[![license](https://img.shields.io/badge/License-dual--licensed-blue.svg)](https://pixlab.io/downloads)
[![Mailing list](https://img.shields.io/badge/Mailing%20List-G.Groups-42b3f4.svg)](https://groups.google.com/d/forum/sod-embedded)
[![Gitter](https://img.shields.io/gitter/room/nwjs/nw.js.svg)](https://gitter.im/sodcv/Lobby)

![Output](https://i.imgur.com/YIbb8wr.jpg)

* [Introduction](#sod-embedded).
* [Features](#notable-sod-features).
* [Programming with SOD](#programming-interfaces).
* [Useful Links](#other-useful-links).

## SOD Embedded
[Release 1.1.8](https://pixlab.io/downloads)

SOD is an embedded, modern cross-platform computer vision and machine learning software library that expose a set of APIs for deep-learning, advanced media analysis & processing including real-time, multi-class object detection and model training on embedded systems with limited computational resource and IoT devices.

SOD was built to provide a common infrastructure for computer vision applications and to accelerate the use of machine perception in open source as well commercial products.

Designed for computational efficiency and with a strong focus on real-time applications. SOD includes a comprehensive set of both classic and state-of-the-art deep-neural networks with their <a href="https://pixlab.io/downloads">pre-trained models</a>. Built with SOD:
* <a href="https://sod.pixlab.io/intro.html#cnn">Convolutional Neural Networks (CNN)</a> for multi-class (20 and 80) object detection & classification.
* <a href="https://sod.pixlab.io/api.html#cnn">Recurrent Neural Networks (RNN)</a> for text generation (i.e. Shakespeare, 4chan, Kant, Python code, etc.).
* <a href="https://sod.pixlab.io/samples.html">Decision trees</a> for single class, real-time object detection.
* A brand new architecture written specifically for SOD named <a href="https://sod.pixlab.io/intro.html#realnets">RealNets</a>.

![Multi-class object detection](https://i.imgur.com/Mq98uTv.png) 

Cross platform, dependency free, amalgamated (single C file) and heavily optimized. Real world use cases includes:
* Detect & recognize objects (faces included) at Real-time.
* License plate extraction.
* Intrusion detection.
* Mimic Snapchat filters.
* Classify human actions.
* Object identification.
* Eye & Pupil tracking.
* Facial & Body shape extraction.
* Image/Frame segmentation.

## Notable SOD features

* Built for real world and real-time applications.
* State-of-the-art, CPU optimized deep-neural networks including the brand new, exclusive <a href="https://sod.pixlab.io/intro.html#realnets">RealNets architecture</a>.
* Patent-free, advanced computer vision <a href="https://sod.pixlab.io/samples.html">algorithms</a>.
* Support major <a href="https://sod.pixlab.io/api.html#imgproc">image format</a>.
* Simple, clean and easy to use <a href="https://sod.pixlab.io/api.html">API</a>.
* Brings deep learning on limited computational resource, embedded systems and IoT devices.
* Easy interpolatable with <a href="https://sod.pixlab.io/api.html#cvinter">OpenCV</a> or any other proprietary API.
* <a href="https://pixlab.io/downloads">Pre-trained models</a> available for most architectures.</li>
* CPU capable, <a href="https://sod.pixlab.io/c_api/sod_realnet_train_start.html">RealNets model training</a>.
* Production ready, cross-platform, high quality source code.
* SOD is dependency free, written in C, compile and run unmodified on virtually any platform &amp; architecture with a decent C compiler.
* <a href="https://pixlab.io/downloads">Amalgamated</a> - All SOD source files are combined into a single C file (*sod.c*) for easy deployment.
* Open-source, actively developed & maintained product.
* Developer friendly <a href="https://sod.pixlab.io/support.html">support channels.</a>

## Programming Interfaces

The documentation works both as an API reference and a programming tutorial. It describes the internal structure of the library and guides one in creating applications with a few lines of code. Note that SOD is straightforward to learn, even for new programmer.

 Resources |  Description
------------ | -------------
<a href="https://sod.pixlab.io/intro.html">SOD in 5 minutes or less</a> | A quick introduction to programming with the SOD Embedded C/C++ API with real-world code samples implemented in C.
<a href="https://sod.pixlab.io/api.html">C/C++ API Reference Guide</a> | This document describes each API function in details. This is the reference document you should rely on.
<a href="https://sod.pixlab.io/samples.html">C/C++ Code Samples</a> | Real world code samples on how to embed, load models and start experimenting with SOD.
<a href="https://sod.pixlab.io/articles/license-plate-detection.html">License Plate Detection</a> | Learn how to detect vehicles license plates without heavy Machine Learning techniques, just standard image processing routines already implemented in SOD.
<a href="https://sod.pixlab.io/articles/porting-c-face-detector-webassembly.html">Porting our Face Detector to WebAssembly</a> | Learn how we ported the <a href="https://sod.pixlab.io/c_api/sod_realnet_detect.html">SOD Realnets face detector</a> into WebAssembly to achieve Real-time performance in the browser.

## Other useful links

 Resources |  Description
------------ | -------------
<a href="https://pixlab.io/downloads">Downloads</a> | Get a copy of the last public release of SOD, pre-trained models, extensions and more. Start embedding and enjoy programming with.
<a href="https://pixlab.io/sod">Copyright/Licensing</a> | SOD is an open-source, dual-licensed product. Find out more about the licensing situation there.
<a href="https://sod.pixlab.io/support.html">Online Support Channels</a> | Having some trouble integrating SOD? Take a look at our numerous support channels.

![face detection using RealNets](https://i.imgur.com/ZLno8Lz.jpg)
