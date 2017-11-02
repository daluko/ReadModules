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

#include "Drawer.h"



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
TesseractPlugin::TesseractPlugin(QObject* parent) : QObject(parent) {

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
TesseractPlugin::~TesseractPlugin() {

	qDebug() << "destroying tesseract plugin...";
}

/**
* Returns descriptive iamge for every ID
* @param plugin ID
**/
QImage TesseractPlugin::image() const {

	return QImage(":/TesseractOCR/img/read.png");
};

QList<QAction*> TesseractPlugin::createActions(QWidget* parent) {

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


QList<QAction*> TesseractPlugin::pluginActions() const {
	return mActions;
}

/**
* Main function: runs plugin based on its ID
* @param plugin ID
* @param image to be processed
**/
QSharedPointer<nmc::DkImageContainer> TesseractPlugin::runPlugin(
	const QString &runID,
	QSharedPointer<nmc::DkImageContainer> imgC,
	const nmc::DkSaveInfo& saveInfo,
	QSharedPointer<nmc::DkBatchInfo>& info) const {

	if (!imgC)
		return imgC;


	if (runID == mRunIDs[id_perform_ocr]) {

		//get currrent image
		QImage img = imgC->image();

		// load existing XML or create new one
		QString loadXmlPath = rdf::PageXmlParser::imagePathToXmlPath(saveInfo.inputFilePath());
		rdf::PageXmlParser parser;
		bool xml_found = parser.read(loadXmlPath);

		// set xml header info
		auto xmlPage = parser.page();
		xmlPage->setCreator(QString("CVL"));
		xmlPage->setImageSize(QSize(img.size()));
		xmlPage->setImageFileName(imgC->fileName());

		// init tesseract
		TesseractEngine tessEngine;
		bool initialized = tessEngine.init(mConfig.TessdataDir());

		if (!initialized) {
			return imgC;
		}

		// TODO remove after refactoring xml processing
		tesseract::TessBaseAPI* tessAPI = tessEngine.tessAPI();
		
		if (!xml_found) {

			qInfo() << "Tesseract plugin: OCR on current image";

			// compute tesseract OCR results
			tesseract::ResultIterator* pageResults;
			pageResults = tessEngine.processPage(img);

			// convert results to PAGE xml regions
			convertPageResults(pageResults, xmlPage);
		}
		else {
			
			// TODO remove after refactoring xml processing
			setUpOCR(img, tessAPI);

			qInfo() << "Tesseract plugin: OCR on current image - using existing PAGE xml";

			// add OCR results to xml file
			QImage result = addTextToXML(img, xmlPage, tessAPI);

			// drawing result boxes
			if (mConfig.drawResults()) {

				qDebug() << "Tesseract plugin: Drawing OCR boxes that have been recognized.";
				imgC->setImage(result, "OCR boxes");

			}
		}
		
		// write xml output
		QString saveXmlPath = rdf::PageXmlParser::imagePathToXmlPath(saveInfo.outputFilePath());
		parser.write(saveXmlPath, xmlPage);

		// close tesseract api
		tessAPI->End();
	}

	// wrong runID? - do nothing
	return imgC;
}

// OCR related functions------------------------------------------------------------------------------

void TesseractPlugin::convertPageResults(tesseract::ResultIterator* pageResults, const QSharedPointer<rdf::PageElement> xmlPage) const {

	int ol = mConfig.textLevel();

	if (ol < 0 || ol>3) {
		ol = 1;
		qWarning() << "Tesseract plugin: TextLevel has to be an Integer from 0-3: 0(block), 1(paragraph), 2(line), 3(word))";
		qInfo() << "Tesseract plugin: TextLevel set to 1.";
	}

	tesseract::PageIteratorLevel outputLevel = static_cast<tesseract::PageIteratorLevel>(ol);
	tesseract::PageIteratorLevel currentLevel = tesseract::RIL_BLOCK;

	convertRegion(currentLevel, outputLevel, pageResults, xmlPage->rootRegion());
}

void TesseractPlugin::convertRegion(const tesseract::PageIteratorLevel cil, const tesseract::PageIteratorLevel fil, tesseract::ResultIterator* ri, QSharedPointer<rdf::Region> parent) const {

	if (!ri->Empty(cil)) {
		if (PTIsTextType(ri->BlockType())) {
			if (cil <= fil) {

				QSharedPointer<rdf::Region> child;

				if (ri->IsAtBeginningOf(cil)) {

					if (cil == tesseract::RIL_TEXTLINE) {
						child = createTextLine(ri, fil);
					}
					else {
						child = createTextRegion(ri, cil, fil);
					}

					//process text regions of the current child
					convertRegion(static_cast<tesseract::PageIteratorLevel>(cil + 1), fil, ri, child);
					parent->addChild(child);
				}

				if (cil != tesseract::RIL_BLOCK) {
					if (ri->IsAtFinalElement(static_cast<tesseract::PageIteratorLevel>(cil - 1), cil)) {
						return;
					}
				}

				if (ri->Next(cil)) {
					convertRegion(cil, fil, ri, parent);
				}
			}
			else {
				//qDebug() << "Reached final iterator level!";
				return;
			}
		}
		else {
			//qDebug() << "Found a non text block!";
			ri->Next(tesseract::RIL_BLOCK);
			convertRegion(cil, fil, ri, parent);
		}
	}
	//qDebug() << "Finished processing text regions!";
	return;
}

QSharedPointer<rdf::TextRegion> TesseractPlugin::createTextRegion(const tesseract::ResultIterator* ri, const tesseract::PageIteratorLevel riLevel, const tesseract::PageIteratorLevel outputLevel) const {

	// TODO find a more general way to create all kinds of text region in one function

	//create text region element
	QSharedPointer<rdf::TextRegion> textRegion(new rdf::TextRegion());

	int x1, y1, x2, y2;
	ri->BoundingBox(riLevel, &x1, &y1, &x2, &y2);
	rdf::Rect r(QRect(QPoint(x1, y1), QPoint(x2, y2)));
	textRegion->setPolygon(rdf::Polygon::fromRect(r));

	if (riLevel == outputLevel) {
		char* text = ri->GetUTF8Text(riLevel);
		//qDebug("new block found: %s", text);
		textRegion->setText(QString::fromUtf8(text));
		delete[] text;
	}

	//textRegion->setId(textRegion->id().remove(QRegExp("{}")));
	textRegion->setId(textRegion->id().remove("{").remove("}"));	// remove parentheses to please Aletheia and avoid errors

	return textRegion;
}

QSharedPointer<rdf::TextLine> TesseractPlugin::createTextLine(const tesseract::ResultIterator* ri, const tesseract::PageIteratorLevel outputLevel) const {

	//create text region element
	QSharedPointer<rdf::TextLine> textLine(new rdf::TextLine());

	int x1, y1, x2, y2;
	ri->BoundingBox(tesseract::RIL_TEXTLINE, &x1, &y1, &x2, &y2);
	rdf::Rect r(QRect(QPoint(x1, y1), QPoint(x2, y2)));
	textLine->setPolygon(rdf::Polygon::fromRect(r));

	if (outputLevel == tesseract::RIL_TEXTLINE) {
		char* text = ri->GetUTF8Text(tesseract::RIL_TEXTLINE);
		//qDebug("new block found: %s", text);
		textLine->setText(QString::fromUtf8(text));
		delete[] text;
	}

	textLine->setId(textLine->id().remove("{").remove("}"));	// remove parentheses to please Aletheia and avoid errors

	return textLine;
}

void TesseractPlugin::setUpOCR(const QImage img, tesseract::TessBaseAPI* tessAPI) const {

	tessAPI->SetPageSegMode(tesseract::PageSegMode::PSM_AUTO);
	tessAPI->SetVariable("save_best_choices", "T");
	tessAPI->SetImage(img.bits(), img.width(), img.height(), img.bytesPerLine() / img.width(), img.bytesPerLine());
}

QImage TesseractPlugin::addTextToXML(QImage img, const QSharedPointer<rdf::PageElement> xmlPage, tesseract::TessBaseAPI* tessAPI) const {

	// TODO best value/method for padding of text boxes for OCR
	// TODO remove merging of overlapping regions - assume input is polygonal region and process only the specified area (masking image if needed)
	// TODO check if additional line breaks are caused by processing text

	rdf::Timer dt;

	// get text regions from existing xml (type_text_region + type_text_line)
	QVector<QSharedPointer<rdf::Region>> tRegions = rdf::Region::filter(xmlPage->rootRegion().data(), rdf::Region::type_text_region);
	tRegions = tRegions + rdf::Region::filter(xmlPage->rootRegion().data(), rdf::Region::type_text_line);

	//get boxes representing OCR regions
	QVector<rdf::Rect> tBoxes = getOCRBoxes(img.size(), tRegions);

	// TODO fix coloring of drawn items

	QPainter myPainter(&img);

	myPainter.setPen(QPen(QBrush(rdf::ColorManager::blue()), 3));
	myPainter.setBrush(Qt::NoBrush);

	for (int i = 0; i < tBoxes.size(); i++) {
		QRect tmpBox = tBoxes[i].toQRect();
		myPainter.drawRect(tmpBox);
	}

	myPainter.end();

	// merge overlapping regions - avoid misclassification and double recognition
	//QVector<rdf::Rect> oBoxes = tBoxes;
	//oBoxes = mergeOverlappingBoxes(oBoxes);

	// compute OCR results and add them to the corresponding text regions
	for (int i = 0; i < tBoxes.size(); ++i) {

		rdf::Rect b = tBoxes[i];

		if (b.isNull()) {
			continue;
		}

		// set up and apply tesseract
		tessAPI->SetRectangle((int)b.topLeft().x(), (int)b.topLeft().y(), (int)b.width(), (int)b.height());
		tessAPI->Recognize(0);

		//ignore merging and processing of rects
		char* boxText = tessAPI->GetUTF8Text();

		auto r = tRegions[i];
		if (r->type() == rdf::Region::type_text_region) {
			auto rc = qSharedPointerCast<rdf::TextRegion>(r);
			rc->setText(QString::fromUtf8(boxText));
			tRegions[i] = rc;
		}
		else if (r->type() == rdf::Region::type_text_line) {
			auto rc = qSharedPointerCast<rdf::TextLine>(r);
			rc->setText(QString::fromUtf8(boxText));
			tRegions[i] = rc;
		}

		delete[] boxText;

		//iterate through result data and save to text regions
		//processRectResults(b, tBoxes, tessAPI, tRegions);

		//debug ocr images
		//cv::Mat src = nmc::DkImage::qImage2Mat(img);
		//cv::Mat trImg = cv::Mat(src, b.toCvRect());
		//cv::String winName = "image of text region ";
		//winName = winName + std::to_string(tBoxes.indexOf(b));
		//cv::namedWindow(winName, cv::WINDOW_AUTOSIZE);// Create a window for display.
		//cv::imshow(winName, trImg);
	}

	//qInfo() << "There are " << tBoxes.size() << " text regions in the XML";
	qInfo() << "ocr results computed in" << dt;

	return img;
}

// get boxes representing text regions where OCR should be applied
QVector<rdf::Rect> TesseractPlugin::getOCRBoxes(const QSize imgSize, const QVector<QSharedPointer<rdf::Region>> tRegions) const {

	QVector<rdf::Rect> tBoxes = QVector<rdf::Rect>(tRegions.size());

	for (int i = 0; i < tRegions.size(); ++i) {

		auto r = tRegions[i];

		if (!r->isEmpty() && !r->polygon().isEmpty()) {

			if (r->type() == rdf::Region::type_text_region) {
				auto trc = qSharedPointerCast<rdf::TextRegion>(r);
				if (trc->text().isEmpty()) {
					rdf::Rect tmp = polygonToOCRBox(imgSize, trc->polygon());
					tBoxes[i] = tmp;
				}
			}
			else if (r->type() == rdf::Region::type_text_line) {
				auto trc = qSharedPointerCast<rdf::TextLine>(r);
				if (trc->text().isEmpty()) {
					rdf::Rect tmp = polygonToOCRBox(imgSize, trc->polygon());
					tBoxes[i] = tmp;
				}
			}
		}
	}

	return tBoxes;
}

rdf::Rect TesseractPlugin::polygonToOCRBox(const QSize imgSize, const rdf::Polygon poly) const {

	rdf::Rect tmp = rdf::Rect::fromPoints(poly.toPoints());
	tmp.expand(10); // expanding image improves OCR results - avoids errors if text is connected to the image border
	tmp = tmp.clipped(rdf::Vector2D(imgSize));

	return tmp;
}

QVector<rdf::Rect> TesseractPlugin::mergeOverlappingBoxes(QVector<rdf::Rect> boxes) const {

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

void TesseractPlugin::processRectResults(const rdf::Rect box, const QVector<rdf::Rect> tBoxes,
	tesseract::TessBaseAPI* tessAPI, QVector<QSharedPointer<rdf::Region>> tRegions) const {

	// TODO remove in next update

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
					rdf::Rect lr(QRect(QPoint(x1, y1), QPoint(x2, y2)));

					// make sure that the result rect created is smaller than the actual ROI
					if (!box.contains(lr)) {
						lr = lr.intersected(box);
					}

					//save OCR results to text region
					writeTextToRegions(lineText, lr, tBoxes, tRegions);

					delete[] lineText;

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

void TesseractPlugin::writeTextToRegions(const char* lineText, const rdf::Rect lineRect,
	const QVector<rdf::Rect> tBoxes, QVector<QSharedPointer<rdf::Region>>& tRegions) const {
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

	if (rIdx != -1) {
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

// TesseractEngine functions--------------------------------------------------------------------------
TesseractEngine::TesseractEngine() {
	
	mTessAPI = new tesseract::TessBaseAPI();
}

TesseractEngine::~TesseractEngine() {

	mTessAPI->End();
	qDebug() << "Tesseract plugin: destroying tesseract engine...";
}

tesseract::TessBaseAPI* TesseractEngine::tessAPI() {

	return mTessAPI;

}

bool TesseractEngine::init(const QString tessdataDir) {

	// TODO allow different languages
	char * lang = "eng";
	
	if (mTessAPI->Init(tessdataDir.toStdString().c_str(), lang)==-1)
	{
		qWarning() << "Tesseract plugin: Could not initialize tesseract API!";
		qWarning() << "Tesseract plugin: Set path to directory containing \"tessdata\" folder using config plugin!";
		qWarning() << "Tesseract plugin: If tessdata folder is missing, create it and download eng.traineddata from github https:\\\\github.com\\tesseract-ocr\\tessdata";

		return false;
	}
	else {
		qInfo() << "Tesseract plugin: Initialized tesseract API.";
		qInfo() << "Tesseract plugin: Using tesseract version: " << mTessAPI->Version();
	}

	return true;
}

void TesseractEngine::setImage(const QImage img) {	
	mTessAPI->SetImage(img.bits(), img.width(), img.height(), img.bytesPerLine() / img.width(), img.bytesPerLine());
}

void TesseractEngine::setTessVariables(const tesseract::PageSegMode psm) {
	
	mTessAPI->SetPageSegMode(psm);
	mTessAPI->SetVariable("save_best_choices", "T");
}

tesseract::ResultIterator* TesseractEngine::processPage(const QImage img) {

	setImage(img);
	setTessVariables(tesseract::PageSegMode::PSM_AUTO);

	mTessAPI->Recognize(0);

	tesseract::ResultIterator* ri = mTessAPI->GetIterator();

	return ri;
}


// plugin functions----------------------------------------------------------------------------------
void TesseractPlugin::preLoadPlugin() const {

	//qDebug() << "[PRE LOADING] Batch Test";
}

void TesseractPlugin::postLoadPlugin(const QVector<QSharedPointer<nmc::DkBatchInfo>>& batchInfo) const {
	int runIdx = mRunIDs.indexOf(batchInfo.first()->id());

	for (auto bi : batchInfo) {
		qDebug() << bi->filePath() << "computed...";

	}
}

QString TesseractPlugin::name() const {
	return "Tesseract OCR";
}

QString TesseractPlugin::settingsFilePath() const {
	return rdf::Config::instance().settingsFilePath();
}

void TesseractPlugin::loadSettings(QSettings & settings) {

	// update settings
	settings.beginGroup(name());
	mConfig.loadSettings(settings);
	settings.endGroup();
}

void TesseractPlugin::saveSettings(QSettings & settings) const {

	// save settings (this is needed for batch profiles)
	settings.beginGroup(name());
	mConfig.saveSettings(settings);
	settings.endGroup();
}

// tesseract config---------------------------------------------------------------------------
TesseractPluginConfig::TesseractPluginConfig() : ModuleConfig("TesseractPlugin") {
}

void TesseractPluginConfig::load(const QSettings & settings) {

	mTessdataDir = settings.value("TessdataDir", mTessdataDir).toString();
	mTextLevel = settings.value("TextLevel", mTextLevel).toInt();
	mDrawResults = settings.value("DrawResults", mDrawResults).toBool();
}

void TesseractPluginConfig::save(QSettings & settings) const {

	settings.setValue("TessdataDir", mTessdataDir);
	settings.setValue("TextLevel", mTextLevel);
	settings.setValue("DrawResults", mDrawResults);
}

QString TesseractPluginConfig::TessdataDir() const {
	return mTessdataDir;
}

//void TesseractPluginConfig::setTessdataDir(const QString dir) {
//	mTessdataDir = dir;
//}

int TesseractPluginConfig::textLevel() const {
	return mTextLevel;
}

//void TesseractPluginConfig::setTextLevel(int level) {
//	mTextLevel = level;
//}

bool TesseractPluginConfig::drawResults() const {
	return mDrawResults;
}

//void TesseractPluginConfig::setDrawResults(bool draw) {
//	mDrawResults = draw;
//}

QString TesseractPluginConfig::toString() const {

	QString msg = rdf::ModuleConfig::toString();
	msg += "  TessdataDir: " + mTessdataDir;
	msg += "  TextLevel: " + QString::number(mTextLevel);
	msg += drawResults() ? " drawing results\n" : " not drawing results\n";

	return msg;
}

};


