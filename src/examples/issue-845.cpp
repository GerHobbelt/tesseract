// https://github.com/tesseract-ocr/tesseract/issues/845
/*
SetRectangle maybe has an odd thing #845

Today，I want to set a rectangle area to recognition，but I find the parameters may be not explain as baseapi.h .

		void setrectangle（int left,int top,int width,int height）;

when I setrectangle(126,40,1152,28), it will recognizing (0,0,1152,28), I don't know why.

Look forward to your reply，thank you！

@hanoilu

---

Hello,

SetRectangle appears to be broken in v4, cf: sirfz/tesserocr#26

In the meantime, you are probably better off creating a sub-image yourself and performing OCR on it.

---

A bit more details, with a minor change to the base API to use the SetRectangle API call just after loading an image:

diff --git a/api/baseapi.cpp b/api/baseapi.cpp
index 8b2ef07..a63d9f6 100644
--- a/api/baseapi.cpp
+++ b/api/baseapi.cpp
@@ -1204,6 +1204,7 @@ bool TessBaseAPI::ProcessPage(Pix* pix, int page_index, const char* filename,
   PERF_COUNT_START("ProcessPage")
   SetInputName(filename);
   SetImage(pix);
+  SetRectangle(36, 92, 544, 30);
   bool failed = false;
 
   if (tesseract_->tessedit_pageseg_mode == PSM_AUTO_ONLY) {
Then run tesseract on testing/phototest.tif.

With branch 3.04:

$ tesseract -psm 6 tesseract/testing/phototest.tif stdout
TIFFFetchNormalTag: Warning, ASCII value for tag "Photoshop" does not end in null byte. Forcing it to be null.
Page 1
This is a lot of 12 point text to test the

TIFFFetchNormalTag: Warning, ASCII value for tag "Photoshop" does not end in null byte. Forcing it to be null.
With master branch:

$tesseract -psm 6 tesseract/testing/phototest.tif stdout
Page 1
s

leptonica-1.74.1 is used in both cases, both on clean debian/jessie64 VMs with identical configurations. ./configure was ran with no option. Without the SetRectangle call, both tesseract versions generate perfect output.

@hanoilu
Author

---

yes, you can use version 3.x instead of version 4.0 if you really need to use the SetRectangle call. Alternatively, you can create an image corresponding to the rectangle you want to recognise, and recognise that instead; it won't be exactly equivalent as the bounding boxes would be shifted compared to the ones in the original image, but it is easy to correct the bounding boxes.

---

@nguyenq nguyenq mentioned this issue on Jun 20, 2017
4.0 instance.doOCR(bi, rect) can not work! nguyenq/tess4j#57

---

it won't be exactly equivalent as the bounding boxes would be shifted compared to the ones in the original image,


---

@abieler Because the bounding boxes in each elements (paragraph, word, etc.) would be relative to the "new" sub-image you have created rather than the original image - while setRectangle would normally return bounding boxes relative to the original image. So if you need to know where the recognised text comes from precisely in the original image, you would need to do an additional step to have their exact position: you would need to shift the returned bounding boxes in the original coordinate space... which is admittedly not very hard to do: you just need to add the coordinates of the top left corner of your extracted sub-image to all bounding boxes.

---

Thanks @bpotard ! I just started using the API and setRectangle and found that the ocr quality is very sensitive to the size of the bounding box, where 1 px more or less on the y axis makes a huge difference, even though there seems no reason by looking at those regions by eye. Is there a "best practice" on how many pixels there should be between the last pixel row of the characters and the bounding box? say, text height is 30 px, then the boundingbox should be 40 px, adding 5 extra pixels on each side (which seems to work ok in my case..) Sorry I should not abuse github issues for this kind of questions...

---

If you are using the master branch, SetRectangle is probably still broken so will not work - the bug has not been fixed as far as I know. If you really need the functionality, either use the 3.x branch of tesseract, or create you own sub-images and process them as whole images using the normal API. Do not use SetRectangle in tesseract 4.x.

Alternatively, you can try to figure out where the bug in SetRectangle comes from and fix it :-)

---

zdenop commented on Sep 29, 2018
Can you please provide test case that can demonstrate your problem? I can not reproduce with:

		Pix *image = pixRead("/usr/src/testing/phototest.tif");
		api->SetImage(image);
		api->SetRectangle(36, 126, 582, 31);
		outText = api->GetUTF8Text();
		printf("Region text:\n%s\n", outText);

and results is:

Region test:
ocr code and see if it works on all types
Which is exactly what e.g. gimp shows for this area. Or do I miss something?

@zdenop
Contributor

---

Tesseract 4.1 - Setting region of interest doesn't work charlesw/tesseract#489

---

Top property in rectangles naptha/tesseract.js#485

---

https://groups.google.com/g/tesseract-ocr/c/PMHq6YSpRRE

@zdenop,

In the linked thread you confirmed that SetRectangle() is not working as expected.

---

zdenop commented 2 weeks ago
Notes:

Tesseract uses left&bottom coordinate system (0,0) for box files (text2image, tesseract image outputname makebox)
SetRectangle was created 14y ago for 3.x and it was always left, top, width, height based. It uses left&top coordinate system (see example below)
As far as I tested: it never worked with correctly with LSTM engine, but it works ok with legacy engine.
Here is my test code:

*/

#include <leptonica/allheaders.h>
#include <tesseract/baseapi.h>

#if defined(BUILD_MONOLITHIC)
#  define main tesseract_test_issue_845_main
#endif

extern "C" int main(int argc, const char **argv)
{
  // Show version info
  const char *versionStrP;
  printf("tesseract %s\n", tesseract::TessBaseAPI::Version());
  versionStrP = getLeptonicaVersion();
  printf(" %s\n", versionStrP);
  stringDestroy(&versionStrP);
  versionStrP = getImagelibVersions();
  printf("  %s\n", versionStrP);
  stringDestroy(&versionStrP);

  tesseract::TessBaseAPI *api = new tesseract::TessBaseAPI();  
#if 1
  tesseract::OcrEngineMode enginemode = tesseract::OEM_DEFAULT;
#else
  tesseract::OcrEngineMode enginemode = tesseract::OEM_TESSERACT_ONLY;
#endif
  api->InitOem(NULL, "eng", enginemode);

  const char *imgpath = argv[1];
  if (!imgpath) {
    imgpath = "SetRectangle_test.png";
  }
  const char *imgpath2 = (argc > 2 ? argv[2] : nullptr);
  if (!imgpath2) {
    static char fpathbuf[1024 + 30];
    strncpy(fpathbuf, imgpath, sizeof(fpathbuf));
    fpathbuf[1023] = 0;
    char *dirp = strrchr(fpathbuf, '/');
    if (!dirp)
        dirp = strrchr(fpathbuf, '\\');
    if (dirp)
        strcpy(dirp + 1, "ocred_pix.png");
    else
        strcpy(fpathbuf, "ocred_pix.png");
    imgpath2 = fpathbuf;
  }

  Pix *image = pixRead(imgpath);
  if (!image)
    return 1;
  api->SetImage(image);
  int w = pixGetWidth(image);
  int h =  pixGetHeight(image);
  int h_adj = h * .3;
  api->SetRectangle(0, 0, w, h_adj);
  char *outTextSR = api->GetUTF8Text();
  printf("********\tOCR output after SetRectangle:\n%s", outTextSR);
  Pix *rect_pix = api->GetThresholdedImage();
  pixWrite(imgpath2, rect_pix, IFF_PNG);

  api->SetImage(rect_pix);
  char *outTextSI = api->GetUTF8Text();
  printf("\n********\tOCR output SetImage:\n%s", outTextSI);

  api->End();
  pixDestroy(&image);
  pixDestroy(&rect_pix);
  delete[] outTextSR;
  delete[] outTextSI;
  delete api;
  return 0;
}
