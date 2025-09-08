#include <stdio.h>
#include <tesseract/baseapi.h>
#include <tesseract/publictypes.h>
#include <leptonica/allheaders.h>
#include <iostream>

using namespace std;

int main(int argc, const char **argv)
{
	char *outText;
	tesseract::TessBaseAPI *api = new tesseract::TessBaseAPI();
	// Initialize tesseract-ocr with English, without specifying tessdata path
	if (api->Init(NULL, "eng")) {
		fprintf(stderr, "Could not initialize tesseract.\n");
		return 1;
	}
	// Open input image with leptonica library
	const char *image_filepath = "C:\\Users\\yiran\\Documents\\Visual Studio 2015\\Projects\\TestOCR\\TestOCR\\input\\1.png";
	if (argc > 1) {
		image_filepath = argv[1];
	}
	Pix *image = pixRead(image_filepath);
	if (!image) {
		fprintf(stderr, "Could not open/read image file: %s\n", image_filepath);
		return 1;
	}
	api->SetImage(image);
	// Get OCR result
	outText = api->GetUTF8Text();
	printf("OCR output:\n%s", outText);

	//report each and every score
	api->SetVariable("save_raw_choices", "T");
	api->Recognize(0);
	tesseract::ResultIterator* ri = api->GetIterator();
	tesseract::PageIteratorLevel level = tesseract::RIL_SYMBOL;
	if (ri != 0) {
		do {
			const char* symbol = ri->GetUTF8Text(level);
			float conf = ri->Confidence(level);
			if (symbol != 0) {
				printf("symbol %s, conf: %f\n", symbol, conf);
				bool indent = true;
				tesseract::ChoiceIterator ci(*ri);
				do {
					if (indent) printf("\t\t ");
					printf("\t- ");
					const char* choice = ci.GetUTF8Text();
					printf("%s conf: %f\n", choice, ci.Confidence());
					indent = true;
				} while (ci.Next());
			}
			printf("---------------------------------------------\n");
			delete[] symbol;
		} while ((ri->Next(level)));
	}

	// Destroy used object and release memory
	api->End();
	delete[] outText;
	pixDestroy(&image);

	return 0;
}

/*

Here is the full recognization result.

OCR output:

CBUMHLBOGUOCCJJWOSM 

symbol C, conf: 79.417328
                        - C conf: 79.417328
                        - 6 conf: 77.730942
                        - G conf: 75.736710
                        - c conf: 70.709084
                        - U conf: 70.475098
                        - 鈧?conf: 70.305206
                        - O conf: 68.585541
                        - S conf: 65.618805
---------------------------------------------
symbol B, conf: 85.324097
                        - B conf: 85.324097
                        - 8 conf: 82.942413
                        - S conf: 74.590630
                        - E conf: 71.548462
                        - 铿?conf: 71.469116
                        - H conf: 71.305191
                        - s conf: 70.927216
---------------------------------------------
symbol U, conf: 80.014610
                        - 0 conf: 84.599174
                        - 铿?conf: 80.744186
                        - U conf: 80.014610
                        - O conf: 78.292427
                        - G conf: 76.453476
                        - H conf: 76.280235
                        - 铿?conf: 75.126778
                        - B conf: 75.008118
                        - a conf: 74.655197
                        - o conf: 70.864799
---------------------------------------------
symbol M, conf: 94.828575
                        - M conf: 94.828575
---------------------------------------------
symbol H, conf: 79.754875
                        - H conf: 79.754875
                        - 铿?conf: 79.602669
                        - R conf: 77.818581
                        - 铿?conf: 77.675552
                        - E conf: 69.655228
                        - N conf: 67.378166
---------------------------------------------
symbol L, conf: 90.597031
                        - L conf: 90.597031
---------------------------------------------
symbol B, conf: 87.735931
                        - B conf: 87.735931
                        - 8 conf: 79.849876
                        - D conf: 77.146729
                        - E conf: 75.215645
                        - H conf: 73.950241
---------------------------------------------
symbol O, conf: 81.632339
                        - 0 conf: 86.087906
                        - O conf: 81.632339
                        - 铿?conf: 76.225067
                        - G conf: 72.772652
                        - o conf: 71.837944
---------------------------------------------
symbol G, conf: 84.402718
                        - G conf: 84.402718
                        - 6 conf: 79.189980
                        - 铿?conf: 73.021805
                        - & conf: 71.743034
                        - 铿?conf: 70.770432
---------------------------------------------
symbol U, conf: 83.307770
                        - 0 conf: 84.609100
                        - U conf: 83.307770
                        - O conf: 80.880013
                        - G conf: 75.340317
                        - 铿?conf: 75.141930
                        - H conf: 74.981506
                        - 铿?conf: 70.093109
                        - o conf: 69.714661
---------------------------------------------
symbol O, conf: 81.405373
                        - 0 conf: 84.595085
                        - O conf: 81.405373
                        - U conf: 79.746880
                        - 铿?conf: 79.459610
                        - G conf: 74.761658
                        - B conf: 72.198845
                        - H conf: 71.787392
                        - o conf: 70.715866
                        - 铿?conf: 70.153809
---------------------------------------------
symbol C, conf: 82.656372
                        - C conf: 82.656372
                        - G conf: 80.284164
                        - 0 conf: 78.578232
                        - O conf: 74.955704
                        - U conf: 71.912804
                        - E conf: 71.027885
                        - 鈧?conf: 70.455338
                        - c conf: 69.879150
                        - 铿?conf: 69.594452
                        - 铿?conf: 67.990509
---------------------------------------------
symbol C, conf: 77.588669
                        - C conf: 77.588669
                        - G conf: 72.312744
                        - 6 conf: 70.384613
                        - 铿?conf: 68.118385
                        - 铿?conf: 68.019379
                        - 拢 conf: 67.908691
                        - 鈧?conf: 65.610016
---------------------------------------------
symbol J, conf: 81.834717
                        - J conf: 81.834717
---------------------------------------------
symbol J, conf: 77.445480
                        - 3 conf: 81.967667
                        - J conf: 77.445480
---------------------------------------------
symbol W, conf: 94.912125
                        - W conf: 94.912125
                        - w conf: 85.750565
---------------------------------------------
symbol O, conf: 70.091782
                        - 0 conf: 81.022514
                        - 铿?conf: 74.809395
                        - a conf: 71.240189
                        - O conf: 70.091782
                        - G conf: 69.503113
                        - 铿?conf: 68.606339
                        - o conf: 68.581573
---------------------------------------------
symbol S, conf: 76.613525
                        - 5 conf: 88.083267
                        - S conf: 76.613525
---------------------------------------------
symbol M, conf: 71.469131
                        - M conf: 71.469131
                        - W conf: 69.292854
                        - N conf: 65.227638
                        - w conf: 64.697189
                        - m conf: 64.298431
---------------------------------------------

*/
