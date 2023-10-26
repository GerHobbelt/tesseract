/*
Gilad Pellaeon <ld.pellaeon@gmail.com>
4:49 PM 14-09-2023
to tesseract-ocr

Hi,

I am new to Tesseract. I searched for an OCR library, found Tesseract and now I want to use it for a specific measure protocol.

I built Tesseract 5.3.2 from source and the dependencies leptonica-1.83, libpng and OpenJPEG for Windows with the Latex Visual C++ compiler for Windows, x64.

Then I did some first tests based on the examples from the documentation ( Basic_example and SetRectangle_example). As data set I use eng.traineddata from the testdata_best repo.

Now, I have a behaviour which I can't classify. I tried to recognize a float value in a given rectangle (with SetRectangle ). Tesseract didn't converted it (empty return). Then I manually copied the rectangle and saved it in a new file (see attached Single_Number.png). Then I tried this file without the SetRectangle call. Now it works.

The attached Protocol_table.png is the original image, but I removed all other stuff in the picture. So it's empty except the number at the original position. Now I have the following behaviour: in DEBUG mode the conversion works, in RELEASE mode not.

I also tried to slighty enlarge the rectangle area (see last SetRectangle call in the code below). But now I got a runtime exception. The resolution of the picture is 2625x1682. So there should be no buffer overflow?!

Am I doing something wrong here? Or what's the problem for this behaviour?

This is my basic code:
*/

//std includes
#include <iostream>

//tesseract includes
#include "tesseract/baseapi.h"

//Leptonica includes
#include <leptonica/allheaders.h>


#if defined(BUILD_MONOLITHIC)
#  define main tesseract_test_issue_ML_1bba6c_main
#endif

extern "C" int main(int argc, const char **argv)
{
    tesseract::TessBaseAPI api;
    // Initialize tesseract-ocr with English, without specifying tessdata path
    if (api.InitSimple(nullptr, "eng"))
    {
        std::cout << "Could not initialize tesseract." << std::endl;
        return 1;
    }

    //
    const char *imgpath = argv[1];
    if (!imgpath) 
    {
#if 1
        imgpath = "Protocol_Table.png";
#else
        imgpath = "Single_Number.png";
#endif
    }
    Pix* image = pixRead(imgpath);
    api.SetImage(image);
    // Restrict recognition to a sub-rectangle of the image
    // SetRectangle(left, top, width, height)
#if 1
    api.SetRectangle(807, 1393, 93, 49);
#else
    api.SetRectangle(707, 1293, 193, 149);
#endif
    // Get OCR result
    char* outText = api.GetUTF8Text();
    if (outText)
        printf("OCR output:\n%s", outText);

    // Destroy used object and release memory
    api.End();    
	pixDestroy(&image);

	return 0;
}
