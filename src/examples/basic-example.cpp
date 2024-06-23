#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <tesseract/baseapi.h>
#include <leptonica/allheaders.h>

#if defined(BUILD_MONOLITHIC)
#define main   tesseract_basic_example_main
#endif

extern "C" int main(int argc, const char **argv)
{
    // Initialize Tesseract API
    tesseract::TessBaseAPI api;
    if (api.InitSimple(nullptr, "eng")) {
        std::cerr << "Could not initialize Tesseract." << std::endl;
        return 1;
    }

    // Directory containing images
    std::string imageDir = "image_directory/";

    // List of image file names
    std::vector<std::string> imageFiles = {"image1.png", "image2.png", "image3.png"};

    for (const std::string& imageFile : imageFiles) {
        // Construct full path to the image
        std::string imagePath = imageDir + imageFile;

        // Load and process the image
        Pix* image = pixRead(imagePath.c_str());
        api.SetImage(image);

        // Perform OCR
        char* outText = api.GetUTF8Text();

        // Create a text file for the OCR result
        std::string textFileName = imageFile + ".txt";
        std::ofstream outFile(textFileName);
        if (outFile.is_open()) {
            outFile << outText;
            outFile.close();
            std::cout << "OCR result for " << imageFile << " saved to " << textFileName << std::endl;
        } else {
            std::cerr << "Failed to create the output text file for " << imageFile << std::endl;
        }

        // Clean up
        delete[] outText;
        pixDestroy(&image);
    }

    return 0;
}
