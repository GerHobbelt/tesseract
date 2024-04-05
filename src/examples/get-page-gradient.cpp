// example as part of https://github.com/tesseract-ocr/tesseract/pull/4070

#include <leptonica/allheaders.h>
#include <tesseract/baseapi.h>


#if defined(BUILD_MONOLITHIC)
#define main   tesseract_get_page_gradient_main
#endif

extern "C" int main(int argc, const char **argv)
{
  tesseract::TessBaseAPI *api = new tesseract::TessBaseAPI();
  // Initialize tesseract-ocr with English, without specifying tessdata path
  if (api->InitSimple(NULL, "eng")) {
    fprintf(stderr, "Could not initialize tesseract.\n");
    return 1;
  }

  // Open input image with leptonica library
  std::string filepath;
  if (argc > 1 && argv[1] != nullptr) {
    filepath = argv[1];
  }
  if (filepath.empty()) {
    filepath = "rotate_image.png";
  }
  Pix *image = pixRead(filepath.c_str());
  if (!image) {
    fprintf(stderr, "Could not open image file: %s\n", filepath.c_str());
    return 1;
  }

  api->SetImage(image);

  // Find lines, get average gradient
  api->AnalyseLayout();
  float gradient = api->GetGradient();

  printf("Average Gradient: %f\n", gradient);

  // Destroy used object and release memory
  api->End();
  delete api;
  pixDestroy(&image);

  return 0;
}
