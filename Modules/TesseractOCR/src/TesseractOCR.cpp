/*******************************************************************************************************
ReadModules are plugins for nomacs developed at CVL/TU Wien for the EU project READ. 

Copyright (C) 2016 Markus Diem <diem@caa.tuwien.ac.at>
Copyright (C) 2016 Stefan Fiel <fiel@caa.tuwien.ac.at>
Copyright (C) 2016 Florian Kleber <kleber@caa.tuwien.ac.at>

This file is part of ReadModules.

ReadFramework is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

ReadFramework is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.

The READ project  has  received  funding  from  the European  Union’s  Horizon  2020  
research  and innovation programme under grant agreement No 674943

related links:
[1] http://www.caa.tuwien.ac.at/cvl/
[2] https://transkribus.eu/Transkribus/
[3] https://github.com/TUWien/
[4] http://nomacs.org
*******************************************************************************************************/

//plugin
#include "TesseractOCR.h"

// nomacs
#include "DkImageStorage.h"

#include "Settings.h"

// ReadFramework
#include "PageParser.h"
#include "Shapes.h"
#include "Utils.h"

//tesseract
#include <allheaders.h> // leptonica main header for image io

// openCV
#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/highgui.hpp>

#pragma warning(push, 0)	// no warnings from includes - begin
#include <QAction>
#include <QSettings>
#include <QUuid>
#pragma warning(pop)		// no warnings from includes - end

namespace rdm {

/**
*	Constructor
**/
TesseractOCR::TesseractOCR(QObject* parent) : QObject(parent) {

	// create run IDs
	QVector<QString> runIds;
	runIds.resize(id_end);

	// create run IDs
	for (QString& rid : runIds)
		rid = QUuid::createUuid().toString();
	mRunIDs = runIds.toList();

	// create menu actions
	QVector<QString> menuNames;
	menuNames.resize(id_end);

	menuNames[id_perform_ocr] = tr("perform OCR on current image");
	mMenuNames = menuNames.toList();

	// create menu status tips
	QVector<QString> statusTips;
	statusTips.resize(id_end);

	statusTips[id_perform_ocr] = tr("Computes optical character recognition results for an image and saves them as a PAGE xml file.");
	mMenuStatusTips = statusTips.toList();

	// this line adds the settings to the config
	// everything else is done by nomacs - so no worries
	rdf::DefaultSettings s;
	s.beginGroup(name());
	mConfig.saveDefaultSettings(s);
	s.endGroup();
}
/**
*	Destructor
**/
TesseractOCR::~TesseractOCR() {

	qDebug() << "destroying tesseract plugin...";
}

/**
* Returns descriptive iamge for every ID
* @param plugin ID
**/
QImage TesseractOCR::image() const {

	return QImage(":/TesseractOCR/img/read.png");
};

QList<QAction*> TesseractOCR::createActions(QWidget* parent) {

	if (mActions.empty()) {

		for (int idx = 0; idx < id_end; idx++) {
			QAction* ca = new QAction(mMenuNames[idx], parent);
			ca->setObjectName(mMenuNames[idx]);
			ca->setStatusTip(mMenuStatusTips[idx]);
			ca->setData(mRunIDs[idx]);	// runID needed for calling function runPlugin()
			mActions.append(ca);
		}
	}

	return mActions;
}


QList<QAction*> TesseractOCR::pluginActions() const {
	return mActions;
}

/**
* Main function: runs plugin based on its ID
* @param plugin ID
* @param image to be processed
**/
QSharedPointer<nmc::DkImageContainer> TesseractOCR::runPlugin(
	const QString &runID,
	QSharedPointer<nmc::DkImageContainer> imgC,
	const nmc::DkSaveInfo& saveInfo,
	QSharedPointer<nmc::DkBatchInfo>& info) const {

	if (!imgC)
		return imgC;


	if (runID == mRunIDs[id_perform_ocr]) {

		auto img = imgC->image();
		cv::Mat imgCv = nmc::DkImage::qImage2Mat(imgC->image());

		// load existing XML or create new one
		QString loadXmlPath = rdf::PageXmlParser::imagePathToXmlPath(saveInfo.inputFilePath());
		rdf::PageXmlParser parser;
		bool xml_found = parser.read(loadXmlPath);

		// set xml header info
		auto xmlPage = parser.page();
		xmlPage->setCreator(QString("CVL"));
		xmlPage->setImageSize(QSize(img.size()));
		xmlPage->setImageFileName(imgC->fileName());

		// Init Tesseract API
		tesseract::TessBaseAPI* tessAPI = initTesseract();	//fixed path, only english

		if (!tessAPI) {	//init failed
			return imgC;
		}
		
		if (!xml_found) {

			qInfo() << "OCR on current image";

			//compute tesseract OCR results
			performOCR(tessAPI, img);
			saveResultsToXML(tessAPI, xmlPage);
		}
		else {
			
			qInfo() << "OCR on current image - using existing PAGE xml";
			addTextToXML(img, xmlPage, tessAPI);
		}
		
		// save xml output
		QString saveXmlPath = rdf::PageXmlParser::imagePathToXmlPath(saveInfo.outputFilePath());
		parser.write(saveXmlPath, xmlPage);

		// close tesseract api
		tessAPI->End();
	}

	// wrong runID? - do nothing
	return imgC;
}

// OCR related functions------------------------------------------------------------------------------
tesseract::TessBaseAPI* TesseractOCR::initTesseract() const{
	
	tesseract::TessBaseAPI* tessAPI = new tesseract::TessBaseAPI();

	//TO DO detailed warnings/error messages
	//TO DO allow setting different languages

	//qWarning() << mConfig.TessdataDir();
	//if (tessAPI->Init((QDir::currentPath()).toStdString().c_str(), "eng"))
	if (tessAPI->Init(mConfig.TessdataDir().toStdString().c_str(), "eng"))
	{
		qWarning() << "Tesseract OCR: Could not initialize tesseract API!";
		qWarning() << "Set path to directory containing \"tessdata\" folder using config plugin!";
		qWarning() << "If tessdata folder is missing, create it and download eng.traineddata from github https:\\\\github.com\\tesseract-ocr\\tessdata";
		return NULL;
	}
	else {
		qInfo() << "Tesseract OCR: Initialized tesseract API.";
	}

	return tessAPI;
}

void TesseractOCR::performOCR(tesseract::TessBaseAPI* tessAPI, QImage img) const{

	//TO DO allow setting different segmentation modes

	tessAPI->SetPageSegMode(tesseract::PageSegMode::PSM_AUTO);
	tessAPI->SetVariable("save_best_choices", "T");
	tessAPI->SetImage(img.bits(), img.width(), img.height(), img.bytesPerLine() / img.width(), img.bytesPerLine());
	tessAPI->Recognize(0);

}

QSharedPointer<rdf::PageElement> TesseractOCR::saveResultsToXML(
	tesseract::TessBaseAPI* tessAPI, 
	QSharedPointer<rdf::PageElement> xmlPage) const{

	tesseract::ResultIterator* ri = tessAPI->GetIterator();
	int level = mConfig.textLevel();

	if (level < 0 || level>3) {
		qWarning() << "TextLevel has to be an Integer from 0-3: 0(block), 1(paragraph), 2(line), 3(word))";
		qInfo() << "TextLevel set to 1.";
		level = 1;
	}

	int b = 0;
	int p = 0; 
	int w = 0; 
	int l = 0; 
	int s = 0;
	int n = 0;
		
	//iterate through result data and write to xml
	if (ri != 0) {
		while (!ri->Empty(tesseract::RIL_BLOCK)) {

			//processing only text blocks
			if (PTIsTextType(ri->BlockType())) {

				//block level processing
				if (ri->IsAtBeginningOf(tesseract::RIL_BLOCK)) {
					if (level == 0) {
						char* blockText = ri->GetUTF8Text(tesseract::RIL_BLOCK);
						//qDebug("new block found: %s", blockText);

						//create text region element
						QSharedPointer<rdf::TextRegion> textR(new rdf::TextRegion());
						textR->setText(QString::fromUtf8(blockText));
						delete[] blockText;
						int x1, y1, x2, y2;
						ri->BoundingBox(tesseract::RIL_BLOCK, &x1, &y1, &x2, &y2);
						rdf::Rect r(QRect(QPoint(x1, y1), QPoint(x2, y2)));
						textR->setPolygon(rdf::Polygon::fromRect(r));

						//add text region to xml
						xmlPage->rootRegion()->addUniqueChild(textR);
					}
					b++;
				}

				//paragraph level processing
				if (ri->IsAtBeginningOf(tesseract::RIL_PARA)) {
					if (level == 1) {
						char* paraText = ri->GetUTF8Text(tesseract::RIL_PARA);
						//qDebug("new paragraph found: %s", paraText);

						//create text region element
						QSharedPointer<rdf::TextRegion> textR(new rdf::TextRegion());
						textR->setText(QString::fromUtf8(paraText));
						delete[] paraText;
						int x1, y1, x2, y2;
						ri->BoundingBox(tesseract::RIL_PARA, &x1, &y1, &x2, &y2);
						rdf::Rect r(QRect(QPoint(x1, y1), QPoint(x2, y2)));
						textR->setPolygon(rdf::Polygon::fromRect(r));

						//add text region to xml
						xmlPage->rootRegion()->addUniqueChild(textR);
					}
					p++;
				}

				//line level processing
				if (ri->IsAtBeginningOf(tesseract::RIL_TEXTLINE)) {
					if (level==2) {
						char* lineText = ri->GetUTF8Text(tesseract::RIL_TEXTLINE);
						//qDebug("new line found: %s", lineText);

						//create text region element
						QSharedPointer<rdf::TextRegion> textR(new rdf::TextRegion());
						textR->setText(QString::fromUtf8(lineText));
						delete[] lineText;
						int x1, y1, x2, y2;
						ri->BoundingBox(tesseract::RIL_TEXTLINE, &x1, &y1, &x2, &y2);
						rdf::Rect r(QRect(QPoint(x1, y1), QPoint(x2, y2)));
						textR->setPolygon(rdf::Polygon::fromRect(r));
						
						//add text region to xml
						xmlPage->rootRegion()->addUniqueChild(textR);
					}
					l++;
					//char* lineText = ri->GetUTF8Text(tesseract::RIL_TEXTLINE);
					//qDebug("new line found: %s", lineText);
					//int x1, y1, x2, y2;
					//ri->Baseline(tesseract::RIL_TEXTLINE, &x1, &y1, &x2, &y2);
					//tessAPI->pClipBaseline(ppi, x1, y1, x2, y2, &line_x1, &line_y1, &line_x2, &line_y2);
				}

				//word level processing
				if (ri->IsAtBeginningOf(tesseract::RIL_WORD)) {
					if (level ==3) {
						char* wordText = ri->GetUTF8Text(tesseract::RIL_WORD);
						//qDebug("new line found: %s", lineText);

						//create text region element
						QSharedPointer<rdf::TextRegion> textR(new rdf::TextRegion());
						textR->setText(QString::fromUtf8(wordText));
						delete[] wordText;
						int x1, y1, x2, y2;
						ri->BoundingBox(tesseract::RIL_WORD, &x1, &y1, &x2, &y2);
						rdf::Rect r(QRect(QPoint(x1, y1), QPoint(x2, y2)));
						textR->setPolygon(rdf::Polygon::fromRect(r));

						//add text region to xml
						xmlPage->rootRegion()->addUniqueChild(textR);
					}
					w++;
				}

				//symbol/character level processing
				if (!ri->Empty(tesseract::RIL_SYMBOL)) {
					//char* symText = ri->GetUTF8Text(tesseract::RIL_SYMBOL);
					//qDebug("new symbol found: %s", symText);
					//delete[] symText;
					s++;
					ri->Next(tesseract::RIL_SYMBOL);
				}
				else {
					ri->Next(tesseract::RIL_SYMBOL);
					//qDebug() << "no more symbols";
					continue;
				}
			}
			else {
				// non-text block processing
				ri->Next(tesseract::RIL_WORD); //move on to next non-text region, RIL_SYMBOL skips non-text 
				//n++;
				//qDebug() << "non-text block found";
			}
		}
	}

	//text region ~= text block set -> text block -> text line set -> text line (elements)
	//qDebug() << "#blocks: " << b << " #para: " << p << " #lines: " << l << " #words: " << w << " #symbols: " << s << " #non-text: " << n;
	return xmlPage;
}

void TesseractOCR::addTextToXML(QImage& img, QSharedPointer<rdf::PageElement> xmlPage, tesseract::TessBaseAPI* tessAPI) const {
	
	// TODO check if ocr result rect > rect for reccognition is caused by my code
	// TODO best value/method for padding of text boxes for OCR
	// TODO use different OCR modes or variables to tune OCR results

	rdf::Timer dt;
	
	tessAPI->SetPageSegMode(tesseract::PageSegMode::PSM_AUTO);
	//tessAPI->SetPageSegMode(tesseract::PageSegMode::PSM_SINGLE_WORD); //alternative mode
	tessAPI->SetVariable("save_best_choices", "T");
	tessAPI->SetImage(img.bits(), img.width(), img.height(), img.bytesPerLine() / img.width(), img.bytesPerLine());

	// get text regions from existing xml (type_text_region + type_text_line)
	QVector<QSharedPointer<rdf::Region>> tRegions = rdf::Region::filter(xmlPage->rootRegion().data(), rdf::Region::type_text_region);
	tRegions = tRegions + rdf::Region::filter(xmlPage->rootRegion().data(), rdf::Region::type_text_line);
	
	//get boxes representing OCR regions
	QVector<rdf::Rect> tBoxes = getOCRBoxes(img.size(), tRegions);

	// merge overlapping regions - avoid misclassification and double recognition
	QVector<rdf::Rect> oBoxes = tBoxes;
	oBoxes = mergeOverlappingBoxes(oBoxes);

	// compute OCR results and add them to the corresponding text regions
	for (auto b : oBoxes) {

		if (b.isNull()) {
			continue;
		}

		// set up and apply tesseract
		tessAPI->SetRectangle((int)b.topLeft().x(), (int)b.topLeft().y(), (int)b.width(), (int)b.height());
		tessAPI->Recognize(0);

		//debug ocr images
		//cv::Mat src = nmc::DkImage::qImage2Mat(img);
		//cv::Mat trImg = cv::Mat(src, b.toCvRect());
		//cv::String winName = "image of text region ";
		//winName = winName + std::to_string(tBoxes.indexOf(b));
		//cv::namedWindow(winName, cv::WINDOW_AUTOSIZE);// Create a window for display.
		//cv::imshow(winName, trImg);

		//iterate through result data and save to text regions
		processRectResults(tessAPI, b, tRegions, tBoxes);
	}
	
	//for (auto r : tRegions) {
	//	xmlPage->rootRegion()->addUniqueChild(r);
	//}

	//qInfo() << "There are " << tBoxes.size() << " text regions in the XML";
	qInfo() << "ocr results computed in" << dt;

}


// get boxes representing text regions where OCR should be applied
QVector<rdf::Rect> TesseractOCR::getOCRBoxes(QSize& imgSize, QVector<QSharedPointer<rdf::Region>> tRegions) const {
	
	QVector<rdf::Rect> tBoxes = QVector<rdf::Rect>(tRegions.size());
	for (int i = 0; i < tRegions.size(); ++i) {
		auto r = tRegions[i];
		if (r->type() == rdf::Region::type_text_region) {
			auto trc = qSharedPointerCast<rdf::TextRegion>(r);
			if (trc && !trc->polygon().isEmpty() && trc->text().isEmpty()) {
				rdf::Rect tmp = polygonToOCRBox(imgSize, trc->polygon());
				tBoxes[i] = tmp;
			}
		}
		else {
			auto trc = qSharedPointerCast<rdf::TextLine>(r);
			if (trc && !trc->polygon().isEmpty() && trc->text().isEmpty()) {
				rdf::Rect tmp = polygonToOCRBox(imgSize, trc->polygon());
				tBoxes[i] = tmp;
			}
		}
	}

	return tBoxes;
}

rdf::Rect TesseractOCR::polygonToOCRBox(QSize imgSize, rdf::Polygon poly) const {

	rdf::Rect tmp = rdf::Rect::fromPoints(poly.toPoints());
	tmp.expand(10); // expanding image improves OCR results - avoids errors if text is connected to the image border
	tmp = tmp.clipped(rdf::Vector2D(imgSize));

	return tmp;
}

QVector<rdf::Rect> TesseractOCR::mergeOverlappingBoxes(QVector<rdf::Rect> boxes) const {

	for (int i = 0; i < boxes.size(); ++i) {
		for (int j = 0; j < boxes.size(); ++j) {
			if (i == j) { continue; }
			if (boxes[i].intersects(boxes[j])) {
				//qInfo() << "joining : " << boxes[i].toString() << "and" << boxes[j].toString();
				boxes[i] = boxes[i].joined(boxes[j]);
				boxes.remove(j);

				return mergeOverlappingBoxes(boxes);
			}
		}
	}
	return boxes;
}

void TesseractOCR::processRectResults(tesseract::TessBaseAPI* tessAPI, 
	rdf::Rect b, QVector<QSharedPointer<rdf::Region>> tRegions, QVector<rdf::Rect> tBoxes) const{

	tesseract::ResultIterator* ri = tessAPI->GetIterator();

	if (ri != 0) {
		while (!ri->Empty(tesseract::RIL_BLOCK)) {

			//processing only text blocks
			if (PTIsTextType(ri->BlockType())) {

				//line level processing
				if (ri->IsAtBeginningOf(tesseract::RIL_TEXTLINE)) {

					// get line text
					char* lineText = ri->GetUTF8Text(tesseract::RIL_TEXTLINE);

					// get bb of current text line
					int x1, y1, x2, y2;
					ri->BoundingBox(tesseract::RIL_TEXTLINE, &x1, &y1, &x2, &y2);
					rdf::Rect lr(QRect(QPoint((double)x1, (double)y1), QPoint((double)x2, (double)y2)));

					// make sure that the result rect created is smaller than actual ROI
					if (!b.contains(lr)) {
						lr = lr.intersected(b);
					}

					//save OCR results to text region
					writeTextToRegions(lineText, lr, tRegions, tBoxes);

					ri->Next(tesseract::RIL_TEXTLINE);
				}
				else {
					ri->Next(tesseract::RIL_TEXTLINE);
					continue;
				}
			}
			else {
				// non-text block processing
				ri->Next(tesseract::RIL_TEXTLINE); //move on to next non-text region, RIL_SYMBOL skips non-text 
			}
		}
	}

}
void TesseractOCR::writeTextToRegions(char* lineText, rdf::Rect lineRect, 
	QVector<QSharedPointer<rdf::Region>>& tRegions, QVector<rdf::Rect> tBoxes) const {
	int rIdx = -1;
	
	for (int i = 0; i < tRegions.size(); ++i) {
		if (tBoxes[i].contains(lineRect)) {
			if (rIdx == -1) {
				rIdx = i;
			}
			else {
				if (tBoxes[rIdx].area() > tBoxes[i].area())
					rIdx = i;
			}
		}
	}
	
	if(rIdx!=-1){
		auto r = tRegions[rIdx];
		if (r->type() == rdf::Region::type_text_region) {
			auto rc = qSharedPointerCast<rdf::TextRegion>(r);
			rc->setText(rc->text() + QString::fromUtf8(lineText));
			tRegions[rIdx] = rc;
		}
		else {
			auto rc = qSharedPointerCast<rdf::TextLine>(r);
			rc->setText(rc->text() + QString::fromUtf8(lineText));
			tRegions[rIdx] = rc;
		}	
	}
	else {
		qWarning() << "Could not find text region for OCR results";
	}
}


// plugin functions----------------------------------------------------------------------------------
void TesseractOCR::preLoadPlugin() const {

	//qDebug() << "[PRE LOADING] Batch Test";
}

void TesseractOCR::postLoadPlugin(const QVector<QSharedPointer<nmc::DkBatchInfo>>& batchInfo) const {
	int runIdx = mRunIDs.indexOf(batchInfo.first()->id());

	for (auto bi : batchInfo) {
		qDebug() << bi->filePath() << "computed...";

	}
}

QString TesseractOCR::name() const {
	return "Tesseract OCR";
}

QString TesseractOCR::settingsFilePath() const {
	return rdf::Config::instance().settingsFilePath();
}

void TesseractOCR::loadSettings(QSettings & settings) {

	// update settings
	settings.beginGroup(name());
	mConfig.loadSettings(settings);
	settings.endGroup();
}

void TesseractOCR::saveSettings(QSettings & settings) const {

	// save settings (this is needed for batch profiles)
	settings.beginGroup(name());
	mConfig.saveSettings(settings);
	settings.endGroup();
}

// tesseract config---------------------------------------------------------------------------
TesseractOCRConfig::TesseractOCRConfig() : ModuleConfig("TesseractPlugin") {
}

void TesseractOCRConfig::load(const QSettings & settings) {

	mTessdataDir = settings.value("TessdataDir", mTessdataDir).toString();
	mTextLevel = settings.value("TextLevel", mTextLevel).toInt();
}

void TesseractOCRConfig::save(QSettings & settings) const {
	settings.setValue("TessdataDir", mTessdataDir);
	settings.setValue("TextLevel", mTextLevel);
}

QString TesseractOCRConfig::TessdataDir() const {
	return mTessdataDir;
}

void TesseractOCRConfig::setTessdataDir(QString dir) {
	mTessdataDir = dir;
}

int TesseractOCRConfig::textLevel() const {
	return mTextLevel;
}

void TesseractOCRConfig::setTextLevel(int level) {
	mTextLevel = level;
}

QString TesseractOCRConfig::toString() const {

	QString msg = rdf::ModuleConfig::toString();
	msg += "  TessdataDir: " + mTessdataDir;
	msg += "  TextLevel: " + QString::number(mTextLevel);

	return msg;
}

};

