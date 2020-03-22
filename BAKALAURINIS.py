import os
import time
from difflib import SequenceMatcher

'''
import cv2
import numpy as np
from scipy.ndimage import interpolation as inter

def ocr_preprocess_image(image, output_file, delta=1, limit=5):
    #https://stackoverflow.com/questions/57964634/python-opencv-skew-correction-for-ocr
    image = cv2.imread(image)

    def determine_score(arr, angle):
        data = inter.rotate(arr, angle, reshape=False, order=0)
        histogram = np.sum(data, axis=1)
        score = np.sum((histogram[1:] - histogram[:-1]) ** 2)
        return histogram, score

    gray = cv2.cvtColor(image, cv2.COLOR_BGR2GRAY)
    thresh = cv2.threshold(gray, 0, 255, cv2.THRESH_BINARY_INV + cv2.THRESH_OTSU)[1] 

    scores = []
    angles = np.arange(-limit, limit + delta, delta)
    for angle in angles:
        histogram, score = determine_score(thresh, angle)
        scores.append(score)

    best_angle = angles[scores.index(max(scores))]

    (h, w) = image.shape[:2]
    center = (w // 2, h // 2)
    M = cv2.getRotationMatrix2D(center, best_angle, 1.0)
    rotated = cv2.warpAffine(image, M, (w, h), flags=cv2.INTER_CUBIC, \
              borderMode=cv2.BORDER_REPLICATE)

    ##################################################
    
    corr = cv2.cvtColor(rotated, cv2.COLOR_BGR2GRAY)
    
    for x in range(len(corr)):
        for y in range(len(corr[x])):
            if corr[x][y] < 150:
                corr[x][y] = 0
            else:
                corr[x][y] = 255
    

    cv2.imwrite(output_file,corr)
'''

def ocr_folder(folder, file_prefix, amount):
    for i in range(1, amount):
        start = time.time()
        
        id = ""

        if i <= 9 and i > 0:
            id = "0" + str(i)
        else:
            id = str(i)
            
        #ocr_preprocess_image(folder+'/'+file_prefix+'-'+id+'.jpg',folder+'/'+file_prefix+'-'+id+'-corrected.jpg')

        #os.system('tesseract ' + folder + '/' + file_prefix + '-' + id + '-corrected.jpg ' + folder + '/text' + id + ' -l lit > /dev/null 2>&1')
	os.system('tesseract ' + folder + '/' + file_prefix + '-' + id + '.jpg ' + folder + '/text' + id + ' -l lit > /dev/null 2>&1')
        f = open(folder + "/text" + id + ".txt", "r")
        text = f.read()
        text = text[:-1] # removes last symbol
        f.close()
        
        f = open(folder + "/text" + id + ".txt", "w")
        f.write(text)
        f.close()
        
        end = time.time()
        print(str(end-start))

def ocr_compare(folder, amount):
    for i in range(1, amount):
        id = ""

        if i <= 9 and i > 0:
            id = "0" + str(i)
        else:
            id = str(i)

        f = open(folder + "/text" + id + ".txt", "r")
        text = f.read()
        f.close()
        
        f = open(with_folder + "/text" + id + ".txt", "r")
        with_text = f.read()
        f.close()
        
        ratio = SequenceMatcher(None, text, with_text).ratio()
        
        print(ratio)


#ocr_folder("BAKALAURINIS/main_images", "metai", 26)
#ocr_folder("BAKALAURINIS/scanned_images", "scanned", 26)
#ocr_folder("BAKALAURINIS/edited_images", "metai", 26)
ocr_folder("BAKALAURINIS/plates", "plate", 8)
print("---------------------------")
#ocr_compare("BAKALAURINIS/main_images", "BAKALAURINIS/scanned_images", 26)
#ocr_compare("BAKALAURINIS/main_images", "BAKALAURINIS/edited_images", 26)

#ocr_preprocess_image("scanned_images/scanned-04.jpg")

'''
f = open("scanned.txt", "w")
f.write(original_text)
f.close()
'''

'''
f = open("main.txt", "r")
main = f.read()
f.close()

f = open("scanned.txt", "r")
scanned = f.read()
f.close()
'''

#print(SequenceMatcher(None, main, scanned).ratio())
