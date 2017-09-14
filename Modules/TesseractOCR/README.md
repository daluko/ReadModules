# Tesseract OCR
This plugin adds Optical Character Recognition (OCR) support by utilizing the tesseract OCR engine [4].

## Build on Windows
Prerequisites:

 1. Download git, cmake and add them to PATH 
 2. Download the latest CPPAN (https://cppan.org/) client from https://cppan.org/client/  
 3. Add cppan to PATH too.
 4. Compile READ Modules [3]

Build Module:

1. Open command line interface and start cppan in TesseractOCR plugin folder
``` console
cd ReadModules_DIR\Modules\TesseractOCR
cppan
```
2.  Open ReadModules Folder with CMake GUI 
3. Set the "ENABLE_TESSERACT_OCR" varaible to true
4. Hit Configure then Generate
5. Open the ReadFramework.sln
6. Right-click the TesseractOCR project and choose Set as StartUp Project
7. Compile the Solution

## Running the module

 - Run the solution/nomcas and open the Read Config plugin.
 - Go to Tesseract OCR -> Tesseract Plugin
 - Set "TessdataDir" to the path containing the tessdata folder (ReadModules\Modules\TesseractOCR).
 - The TextLevel variable can be set to 0, 1, 2, 3 producing block, paragraph, line or word level results.
 - Apply OCR by choosing Plugin -> Optical Character Recognition -> OCR of current image
 - OCR results for the current image are saved as "imageName.xml" file (overwrites existing xml) in the same directory as the image
 - The option "OCR using given segmentation" is not working atm.


### Author
David Körner

### related links:
[1] https://github.com/nomacs/nomacs

[2] https://github.com/TUWien/ReadFramework

[3] https://github.com/TUWien/ReadModules

[4] https://github.com/tesseract-ocr/