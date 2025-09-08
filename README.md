# tesseract-ocr with various patches and enhanced diagnostics output

This is a modified version of Tesseract. The official upstream project is here: https://github.com/tesseract-ocr/tesseract

Dependencies:
* leptonica: (tested with leptonica 1.83+: master branch)

## Modifications / enhancements

**TODO**

The modifications in the visible_pdf_image branch enable the user to input both a "cleaned" image to be used for OCR and a "visible" image that is used in the output PDF. Cleaning an image helps OCR engines by removing background colors and patterns, sharpening text, increasing contrast, etc. The process usually makes the image look terrible to humans, so the idea with this fork is to give us the best of both worlds. This is very useful for digitizing documents.

To clean an image for OCR, try using textcleaner from Fred's ImageMagick Scripts: http://www.fmwconcepts.com/imagemagick/textcleaner/

For the visible image, you can use a compressed version to save space. The only requirement is that the dimensions of the "cleaned" and "visible" images are the same.

Once you've built the visible_pdf_image branch along with the other Tesseract dependencies, just add `--visible-pdf-image <image>` to the arguments. For example:

	tesseract -l eng --visible-pdf-image compressed.webp cleaned.pnm out pdf

Here's the original feature request upstream: https://github.com/tesseract-ocr/tesseract/issues/210

-------------------------------------------------------------------------------

# Tesseract OCR 

<div align="center">

[![Build status](https://ci.appveyor.com/api/projects/status/miah0ikfsf0j3819/branch/master?svg=true)](https://ci.appveyor.com/project/zdenop/tesseract/)
[![Build status](https://github.com/tesseract-ocr/tesseract/actions/workflows/sw.yml/badge.svg)](https://github.com/tesseract-ocr/tesseract/actions/workflows/sw.yml)
[![Coverity Scan Build Status](https://scan.coverity.com/projects/tesseract-ocr/badge.svg)](https://scan.coverity.com/projects/tesseract-ocr)
[![CodeQL](https://github.com/tesseract-ocr/tesseract/workflows/CodeQL/badge.svg)](https://github.com/tesseract-ocr/tesseract/security/code-scanning)
[![OSS-Fuzz](https://img.shields.io/badge/oss--fuzz-fuzzing-brightgreen)](https://issues.oss-fuzz.com/issues?q=is:open%20title:tesseract-ocr)
[![GitHub license](https://img.shields.io/badge/license-Apache--2.0-blue.svg)](https://raw.githubusercontent.com/tesseract-ocr/tesseract/main/LICENSE)
[![Downloads](https://img.shields.io/badge/download-all%20releases-brightgreen.svg)](https://github.com/tesseract-ocr/tesseract/releases/)

</div>


# tesseract-ocr

This is a modified version of Tesseract. The official upstream project is here: https://github.com/tesseract-ocr/tesseract

Dependencies:
* leptonica: latest release / master branch.

The modifications in the visible_pdf_image branch enable the user to input both a "cleaned" image to be used for OCR and a "visible" image that the user will see in the output PDF. Cleaning an image helps OCR engines by removing background colors and patterns, sharpening text, increasing contrast, etc. The process usually makes the image look terrible to humans, so the idea with these patches is to give us the best of both worlds.

To clean an image for OCR, I suggest using textcleaner from Fred's ImageMagick Scripts: http://www.fmwconcepts.com/imagemagick/textcleaner/

For the visible image you can use the original copy, but I suggest using a compressed version instead to save space. The only requirement is that the dimensions of the "cleaned" and "visible" images are the same.

Once you've built the visible_pdf_image branch along with the other Tesseract dependencies, just add `--visible-pdf-image <image>` to the arguments. For example:

	tesseract -l eng --visible-pdf-image compressed.webp cleaned.pnm out pdf

I've found this to be very helpful when creating searchable PDFs from scanned documents, receipts, etc. YMMV. Here's the original feature request upstream: https://github.com/tesseract-ocr/tesseract/issues/210



## Table of Contents 📚 

* [Tesseract OCR](#tesseract-ocr)
  * [📖About](#about)
  * [⏳Brief history](#brief-history)
  * [💻Installing Tesseract](#installing-tesseract)
  * [🚀 Running Tesseract](#running-tesseract)
  * [👩‍💻For developers](#for-developers)
  * [🤝Support](#support)
  * [📜License](#license)
  * [🔗Dependencies](#dependencies)
  * [📅Latest Version of README](#latest-version-of-readme)

## 📖About
    
This package contains an Optical Character Recognition (OCR)👁️‍🗨️ engine—**libtesseract**—and a command-line program called **tesseract**.

### Tesseract 4 Features

![Tesseract 4 Features](https://www.stormthecastle.com/indeximages/the-completed-tesseract.jpg)

Tesseract 4 adds a new neural network 🧠 (LSTM) based OCR engine that focuses on line recognition, while still supporting the legacy Tesseract OCR engine from Tesseract 3, which recognizes character patterns. Compatibility with Tesseract 3 can be enabled by using the Legacy OCR Engine mode (`--oem 0`). This mode requires trained data files that support the legacy engine, available from the [tessdata repository](https://tesseract-ocr.github.io/tessdoc/Data-Files.html).

### Development Team 👥

The current lead developer is **Stefan Weil**. **Ray Smith** served as the lead developer until 2018. The project maintainer is **Zdenko Podobny**. For a list of contributors, please refer to the `AUTHORS` file and the contributors’ log on GitHub.

### Language and Format Support 📄 

Tesseract supports Unicode (UTF-8) and can recognize over 100 languages "out of the box." It supports various image formats including PNG, JPEG, and TIFF.

Tesseract also supports various output formats:

- Plain text
- hOCR (HTML)
- PDF
- Invisible-text-only PDF
- TSV
- ALTO
- PAGE

### Image Quality: 🖼️

To achieve better OCR results, it is important to enhance the quality of the images provided to Tesseract.

### GUI Applications 🖥️ 

This project does not include a graphical user interface (GUI). If you need one, please refer to the third-party documentation.

### Training Tesseract📚

Tesseract can be trained to recognize additional languages. For more information, please see the [Tesseract Training documentation](https://tesseract-ocr.github.io/tessdoc/Data-Files.html) and the [tessdata repository](https://github.com/tesseract-ocr/tessdata).

### Additional Information ℹ️ 

For more about Optical Character Recognition, visit [Wikipedia on OCR](https://en.wikipedia.org/wiki/Optical_character_recognition).


## ⏳Brief history

Tesseract was originally developed at Hewlett-Packard Laboratories in Bristol, UK, and at Hewlett-Packard Co. in Greeley, Colorado, USA, between 1985 and 1994. Subsequent changes were made in 1996 to port it to Windows, with further enhancements in C++ occurring in 1998. In 2005, Tesseract was open-sourced by HP, and it was actively developed by Google from 2006 until November 2018.

The current stable version, **Major Version 5**, began with the release of 5.0.0 on November 30, 2021. Newer minor and bug fix versions are continuously made available on [GitHub](https://github.com/tesseract-ocr/tesseract).

The latest source code can be found in the main branch on [GitHub](https://github.com/tesseract-ocr/tesseract). You can also view open issues in the [issue tracker](https://github.com/tesseract-ocr/tesseract/issues) and consult the planning documentation.

For more details on the releases, see the [Release Notes](https://github.com/tesseract-ocr/tesseract/releases) and the [Change Log](https://github.com/tesseract-ocr/tesseract/blob/master/CHANGELOG.md).

## 💻Installing Tesseract

You can either [Install Tesseract via pre-built binary package](https://tesseract-ocr.github.io/tessdoc/Installation.html) or [build it from source](https://tesseract-ocr.github.io/tessdoc/Compiling.html).

Before building Tesseract from source, please check that your system has a compiler which is one of the [supported compilers](https://tesseract-ocr.github.io/tessdoc/supported-compilers.html).

### Windows Installation clarification

For Windows users, once you download the Tesseract OCR installer app, follow the pop-up instructions, 
choose which components of the software you would like to install (if storage is limited).

Make sure to remember where you saved the file, you will need it to add your environment variables.

You will notice that if you try to use tesseract right after the installation it will not be 
available on the command line (at least not yet). To remedy this, we need to add it the path where 
the application was saved by taking the following steps:

* Open system environment properties from your settings menu and select environment variable.
* Open file explorer and go the Tesseract folder (should be in C drive by default)
* Once you find the executable (tesserac.exe) fill, copy the path to it.
* Get back to the environment variable window, click path then edit then new path variable and past the path, and save.
* After this, you are good to open a new command line, to make sure it is there, type tesseract, this should open the help page with all the command you can use.


## 🚀Running Tesseract

Basic **[command line usage](https://tesseract-ocr.github.io/tessdoc/Command-Line-Usage.html)**:

For more information about the various command line options use `tesseract --help` or `man tesseract`.

Examples can be found in the [documentation](https://tesseract-ocr.github.io/tessdoc/Command-Line-Usage.html#simplest-invocation-to-ocr-an-image).

## Tips and Tricks

1. For non-roman alphabet languages (languages with alphabet other abc…) it is best to have the 
  image with the word oriented in a way that makes the words horizontal. This helps prevent errors
  sometimes. 
2. If you are running an image with just one line and it is not recognized, try running again with different psm mode setting, it should
  output the right words.
3. Do not run an image with no words on it as it will sometimes stall the system.


## Server (TODO / PLANNED)

The time to initialise tesseract can be significant if you have a lot of
files to process, so tesseractCLI supports a "server" mode where a background
process provides a way to reuse a single tesseract instance to convert
multiple documents.

This works via a Unix domain socket.  Bear in mind that if another user on
the same machine can write to the domain socket you use then they can probably
read your documents by using tesseract, so it's a good idea to create the socket
in its own directory which only you can read (`chmod 700 DIRECTORY`).

If you specify `-s SOCKETPATH` and a document path and a server isn't running
for SOCKETPATH then one will be started in the background.  You can also start
a server explicitly by using `tesseract -l -s SOCKETPATH` first without
specifying a document.

Currently you can't use `-u` and `-s SOCKETPATH` together, which means when
using a server you can convert files from paths specified at server startup, but not files from arbitrary
URLs (i.e. "uploads").


## 👩‍💻For developers

Developers can use `libtesseract` [C](https://github.com/tesseract-ocr/tesseract/blob/main/include/tesseract/capi.h) or [C++](https://github.com/tesseract-ocr/tesseract/blob/main/include/tesseract/baseapi.h) API to build their own application. If you need bindings to `libtesseract` for other programming languages, please see the [wrapper](https://tesseract-ocr.github.io/tessdoc/AddOns.html#tesseract-wrappers) section in the AddOns documentation.

Documentation of Tesseract generated from source code by doxygen can be found on [tesseract-ocr.github.io](https://tesseract-ocr.github.io/).

## 🤝Support

Before you submit an issue, please review **[the guidelines for this repository](https://github.com/tesseract-ocr/tesseract/blob/main/CONTRIBUTING.md)**.

For support, first read the [documentation](https://tesseract-ocr.github.io/tessdoc/), particularly the [FAQ](https://tesseract-ocr.github.io/tessdoc/FAQ.html) to see if your problem is addressed there. If not, search the [Tesseract user forum](https://groups.google.com/g/tesseract-ocr), the [Tesseract developer forum](https://groups.google.com/g/tesseract-dev), and [past issues](https://github.com/tesseract-ocr/tesseract/issues). If you still can't find what you need, ask for support in the mailing-lists.


### Mailing-lists:

* [tesseract-ocr](https://groups.google.com/g/tesseract-ocr) - For tesseract users.
* [tesseract-dev](https://groups.google.com/g/tesseract-dev) - For tesseract developers.

Please report an issue only for a **bug**, not for asking questions.

## 📜License

The code in this repository is licensed under the Apache License, Version 2.0 (the "License"); you may not use this file except in compliance with the License. You may obtain a copy of the License at

[Apache License](http://www.apache.org/licenses/LICENSE-2.0)

Unless required by applicable law or agreed to in writing, software distributed under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the License for the specific language governing permissions and limitations under the License.

**NOTE**: This software depends on other packages that may be licensed under different open source licenses.

Tesseract uses [Leptonica library](http://leptonica.com/) which essentially uses a [BSD 2-clause license](http://leptonica.com/about-the-license.html).

## 🔗Dependencies

Tesseract uses [Leptonica library](https://github.com/DanBloomberg/leptonica) for opening input images (hence *not* documents like PDF). 
It is suggested to use Leptonica with built-in support for [zlib](https://zlib.net), 
[png](https://sourceforge.net/projects/libpng), and 
[tiff](http://www.simplesystems.org/libtiff) (for multipage TIFF).

## 📅Latest Version of README

For the latest online version of the README.md see:

<https://github.com/tesseract-ocr/tesseract/blob/main/README.md>
