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
#include "Utils.h"
#include "Drawer.h"

//tesseract
#include <allheaders.h> // leptonica main header for image io

// openCV
#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>

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
	menuNames[id_white_space_analysis] = tr("white space based layout segmentation");
	mMenuNames = menuNames.toList();

	// create menu status tips
	QVector<QString> statusTips;
	statusTips.resize(id_end);

	statusTips[id_perform_ocr] = tr("Computes optical character recognition results for an image and saves them as a PAGE xml file.");
	statusTips[id_white_space_analysis] = tr("Computes layout segmentation results based on a white space analysis and saves them as a PAGE xml file.");
	mMenuStatusTips = statusTips.toList();

	// this line adds the settings to the config
	// everything else is done by nomacs - so no worries
	rdf::DefaultSettings s;
	s.beginGroup(name());
	mConfig.saveDefaultSettings(s);

	mConfig.saveDefaultSettings(s);
	rdf::WhiteSpaceAnalysisConfig wsc;
	wsc.saveDefaultSettings(s);

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
		
		if (!xml_found) {

			qInfo() << "Tesseract plugin: OCR on current image";
			rdf::Timer dt;

			// compute tesseract OCR results
			tesseract::ResultIterator* pageResults;
			pageResults = tessEngine.processPage(img);

			// convert results to PAGE xml regions
			convertPageResults(pageResults, xmlPage);

			qInfo() << "Tesseract plugin: OCR results computed in" << dt;
		}
		else {
			
			qInfo() << "Tesseract plugin: OCR on current image - using existing PAGE xml";
			rdf::Timer dt;

			// extract list of text regions that should be processed by tesseract
			QVector<QSharedPointer<rdf::Region>> textRegions = extractTextRegions(xmlPage);
			QImage result = tessEngine.processTextRegions(img, textRegions);
			
			qInfo() << "Tesseract plugin: OCR results computed in" << dt;

			// drawing result boxes
			if (mConfig.drawResults()) {

				qDebug() << "Tesseract plugin: Drawing OCR boxes that have been recognized.";
				imgC->setImage(result, "OCR boxes");
			}
		}
		
		// write xml output
		QString saveXmlPath = rdf::PageXmlParser::imagePathToXmlPath(saveInfo.outputFilePath());
		parser.write(saveXmlPath, xmlPage);
	}

	if (runID == mRunIDs[id_white_space_analysis]) {

		//get currrent imagescale
		QImage img = imgC->image();
		cv::Mat imgCv = nmc::DkImage::qImage2Mat(img);

		rdf::WhiteSpaceAnalysis wsa(imgCv);
		
		//TODO transfer/set parameters in another way
		wsa.config()->setDebugDraw(mWsaConfig.debugDraw());
		wsa.config()->setDebugPath(mWsaConfig.debugPath());
		wsa.config()->setMaxImgSide(mWsaConfig.maxImgSide());
		wsa.config()->setMserMaxArea(mWsaConfig.mserMaxArea());
		wsa.config()->setMserMinArea(mWsaConfig.mserMinArea());
		wsa.config()->setNumErosionLayers(mWsaConfig.numErosionLayers());
		wsa.config()->setScaleInput(mWsaConfig.scaleInput());
		
		//qDebug() << "wsa.config() debugPath = " << wsa.config()->debugPath();
		//qDebug() << "wsa.config() maxImgSide = " << QString::number(wsa.config()->maxImgSide());
		wsa.compute();
		
		// load existing XML or create new one
		QString loadXmlPath = rdf::PageXmlParser::imagePathToXmlPath(saveInfo.inputFilePath());
		rdf::PageXmlParser parser;
		bool xml_found = parser.read(loadXmlPath);

		// set xml header info
		auto xmlPage = parser.page();
		xmlPage->setCreator(QString("CVL"));
		xmlPage->setImageSize(QSize(img.size()));
		xmlPage->setImageFileName(imgC->fileName());

		// set up output xml
		QString saveXmlPath = rdf::PageXmlParser::imagePathToXmlPath(saveInfo.outputFilePath());
		//saveXmlPath = rdf::Utils::createFilePath(saveXmlPath, "-wsa_lines");

		////-------------------------eval xml text block regions
		saveXmlPath = rdf::PageXmlParser::imagePathToXmlPath(saveInfo.inputFilePath());

		xmlPage->rootRegion()->removeAllChildren();
		for (auto tr : wsa.evalTextBlockRegions()) {
			xmlPage->rootRegion()->addChild(tr);
		}
		parser.write(saveXmlPath, xmlPage);

		////add text line results to page and save as xml
		//xmlPage->rootRegion()->removeAllChildren();
		//for (auto tr : wsa.textLineRegions()) {
		//	xmlPage->rootRegion()->addChild(tr);
		//}
		//parser.write(saveXmlPath, xmlPage);

		////write text block results to page and save as xml
		//saveXmlPath = rdf::PageXmlParser::imagePathToXmlPath(saveInfo.outputFilePath());
		//saveXmlPath = rdf::Utils::createFilePath(saveXmlPath, "-wsa_block");

		////add results to xml
		//xmlPage->rootRegion()->removeAllChildren();
		//xmlPage->rootRegion()->addChild(wsa.textBlockRegions());

		//parser.write(saveXmlPath, xmlPage);

		// drawing debug image
		if (mConfig.drawResults()) {

			QImage result = rdf::Image::mat2QImage(wsa.draw(imgCv), true);
			qDebug() << "Tesseract plugin: Drawing white segmentation results.";
			imgC->setImage(result, "visualising white space based layout segmentation");
		}
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

	if (mConfig.singleLevelOutput()) {
		QVector<QSharedPointer<rdf::Region>> regions;

		if (ol == 2) {
			regions = rdf::Region::filter(xmlPage->rootRegion().data(), rdf::Region::type_text_line);
		}
		else if (ol == 3) {
			regions = rdf::Region::filter(xmlPage->rootRegion().data(), rdf::Region::type_word);
		}
		else {
			regions = rdf::Region::filter(xmlPage->rootRegion().data(), rdf::Region::type_text_region);
		}

		if (!regions.isEmpty()){
			xmlPage->rootRegion()->removeAllChildren();
			for (QSharedPointer<rdf::Region> c : regions) {
				if (c->children().isEmpty()) {	// do not add RIL_BLOCK elements
					xmlPage->rootRegion()->addChild(c);
				}
			}
		}
	}
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

QSharedPointer<rdf::TextRegion> TesseractPlugin::createTextRegion(const tesseract::ResultIterator* ri, const tesseract::PageIteratorLevel riLevel, 
	const tesseract::PageIteratorLevel outputLevel, bool textAtAllLevels) const {

	// TODO find a more general way to create all kinds of text region in one function

	//create text region element
	QSharedPointer<rdf::TextRegion> textRegion(new rdf::TextRegion());

	int x1, y1, x2, y2;
	ri->BoundingBox(riLevel, &x1, &y1, &x2, &y2);
	rdf::Rect r(QRect(QPoint(x1, y1), QPoint(x2, y2)));
	textRegion->setPolygon(rdf::Polygon::fromRect(r));
	
	if (riLevel == outputLevel || textAtAllLevels) {
		char* text = ri->GetUTF8Text(riLevel);
		//qDebug("new block found: %s", text);
		textRegion->setText(QString::fromUtf8(text));
		delete[] text;
	}

	textRegion->setId(textRegion->id().remove("{").remove("}"));	// remove parentheses to please Aletheia and avoid errors

	if (riLevel == tesseract::PageIteratorLevel::RIL_WORD) {
		textRegion->setType(rdf::Region::type_word);
	}

	//if (riLevel == tesseract::RIL_WORD) {
		// NOTE word font attributes currently not available for tess 4.0 LSTM mode

		// TODO test if legacy engine provides font attributes while maintaining recognition performance of lstm
		//float wConfidence = ri->Confidence(riLevel);
		//qDebug() << "Word recognition confidence = " << wConfidence;

		//bool* is_bold = false;
		//bool* is_italic = false;
		//bool* is_underlined = false;
		//bool* is_monospace = false;
		//bool* is_serif = false;
		//bool* is_smallcaps = false;
		//int* pointsize = 0;
		//int* font_id = 0;

		//ri->WordFontAttributes(is_bold, is_italic, is_underlined, is_monospace, is_serif, is_smallcaps, pointsize, font_id);
		//
		//qDebug() << "Word Font Attributes: " << is_bold << ", " << is_italic  << ", " << is_underlined << ", " << is_monospace << ", " << is_serif << ", " << is_smallcaps << ", " << pointsize << ", " << font_id << ", " << wConfidence;
		
	//}

	return textRegion;
}

QSharedPointer<rdf::TextLine> TesseractPlugin::createTextLine(const tesseract::ResultIterator* ri, const tesseract::PageIteratorLevel outputLevel, bool textAtAllLevels) const {

	//create text region element
	QSharedPointer<rdf::TextLine> textLine(new rdf::TextLine());

	int x1, y1, x2, y2;
	ri->BoundingBox(tesseract::RIL_TEXTLINE, &x1, &y1, &x2, &y2);
	rdf::Rect r(QRect(QPoint(x1, y1), QPoint(x2, y2)));
	textLine->setPolygon(rdf::Polygon::fromRect(r));

	if (outputLevel == tesseract::RIL_TEXTLINE || textAtAllLevels) {
		char* text = ri->GetUTF8Text(tesseract::RIL_TEXTLINE);
		//qDebug("new block found: %s", text);
		textLine->setText(QString::fromUtf8(text));
		delete[] text;
	}

	textLine->setId(textLine->id().remove("{").remove("}"));	// remove parentheses to please Aletheia and avoid errors

	return textLine;
}

// extract text region containing no text results
QVector<QSharedPointer<rdf::Region>> TesseractPlugin::extractTextRegions(const QSharedPointer<rdf::PageElement> xmlPage) const {

	// get text regions from existing xml (type_text_region + type_text_line)
	QVector<QSharedPointer<rdf::Region>> tRegions = rdf::Region::filter(xmlPage->rootRegion().data(), rdf::Region::type_text_region);
	tRegions = tRegions + rdf::Region::filter(xmlPage->rootRegion().data(), rdf::Region::type_text_line);
	
	QVector<QSharedPointer<rdf::Region>> emptyTextRegions;

	for (auto r : tRegions) {

		if (!r->isEmpty() && !r->polygon().isEmpty()) {

			if (r->type() == rdf::Region::type_text_region) {
				auto trc = qSharedPointerCast<rdf::TextRegion>(r);
				if (trc->text().isEmpty()) {
					emptyTextRegions.append(r);
				}
			}
			else if (r->type() == rdf::Region::type_text_line) {
				auto trc = qSharedPointerCast<rdf::TextLine>(r);
				if (trc->text().isEmpty()) {
					emptyTextRegions.append(r);
				}
			}
		}
	}

	qWarning() << "Tesseract plugin: Found" << emptyTextRegions.size() << "empty text regions where text recognition results will be added.";

	return emptyTextRegions;
}

// TesseractEngine functions--------------------------------------------------------------------------

TesseractEngine::TesseractEngine() {	
	mTessAPI = new tesseract::TessBaseAPI();
}

TesseractEngine::~TesseractEngine() {
	mTessAPI->End();
	qDebug() << "Tesseract plugin: destroying tesseract engine...";
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
		
		tesseract::OcrEngineMode cOEM =  mTessAPI->oem();
		qInfo() << "Tesseract plugin: Using OCR engine mode: " << cOEM;
	}

	return true;
}

void TesseractEngine::setImage(const QImage img) {	
	mTessAPI->SetImage(img.bits(), img.width(), img.height(), img.bytesPerLine() / img.width(), img.bytesPerLine());
}

void TesseractEngine::setRectangle(const rdf::Rect rect) {

	mTessAPI->SetRectangle((int)rect.topLeft().x(), (int)rect.topLeft().y(), (int)rect.width(), (int)rect.height());
}

tesseract::ResultIterator* TesseractEngine::processPage(const QImage img) {

	setImage(img);
	
	//set tess parameters
	mTessAPI->SetPageSegMode(tesseract::PageSegMode::PSM_AUTO);
	mTessAPI->SetVariable("save_best_choices", "T");

	mTessAPI->Recognize(0);

	tesseract::ResultIterator* ri = mTessAPI->GetIterator();

	return ri;
}

QImage TesseractEngine::processTextRegions(QImage img, QVector<QSharedPointer<rdf::Region>> textRegions){

	// TODO fix coloring of drawn items
	QImage result = img.copy();
	QPainter myPainter(&result);
	myPainter.setPen(QPen(QBrush(rdf::ColorManager::blue()), 3));
	myPainter.setBrush(Qt::NoBrush);

	//for (int i = 0; i < tBoxes.size(); ++i) {
	for (auto r : textRegions) {
		
		if (r.isNull()) {
			continue;
		}

		// removes isNull() points from polygon - rdf::Rect::fromPoints() method also ignores them
		//QPolygon poly = r->polygon().polygon().toPolygon();
		//for (QPoint p : poly) {
		//	if (p.isNull())
		//		poly.removeOne(p);
		//}
		//r->polygon().setPolygon(QPolygonF(poly));
		//r->setPolygon(rdf::Polygon(QPolygonF(poly)));

		if (isAARect(r->polygon())) {
			rdf::Rect rRect = polygonToOCRBox(img.size(), r->polygon());
			addTextToRegion(img, r, rRect);
		}
		else {
			QImage rImg = getRegionImage(img, r);
			addTextToRegion(rImg, r);
		}
		

		rdf::Rect rR = polygonToOCRBox(img.size(), r->polygon());
		myPainter.drawPolyline(r->polygon().closedPolygon().begin(), r->polygon().closedPolygon().size());
	}

	myPainter.end();

	return result;
}

void TesseractEngine::addTextToRegion(const QImage img, QSharedPointer<rdf::Region> region, const rdf::Rect regionRect, const tesseract::PageSegMode psm) {

	// set rect region and do OCR
	setImage(img);

	if (!regionRect.isNull()) {
		setRectangle(regionRect);
	}

	mTessAPI->SetPageSegMode(psm);
	mTessAPI->SetVariable("save_best_choices", "T");

	mTessAPI->Recognize(0);
	char* boxText = mTessAPI->GetUTF8Text();

	//write text to regions
	auto r = region;
	if (r->type() == rdf::Region::type_text_region) {
		auto rc = qSharedPointerCast<rdf::TextRegion>(r);
		rc->setText(QString::fromUtf8(boxText));
	}
	else if (r->type() == rdf::Region::type_text_line) {
		auto rc = qSharedPointerCast<rdf::TextLine>(r);
		rc->setText(QString::fromUtf8(boxText));
	}

	delete[] boxText;

}

// get a cropped and masked image of the text region
QImage TesseractEngine::getRegionImage(const QImage img, const QSharedPointer<rdf::Region> region, const QColor fillColor) const {

	// check for axis aligned rect
	
	// TODO test results and find out if there is a better method for masking a polygonal area
	
	// TODO check why img.format() results in black background sometimes
	//QImage regionImg = QImage(img.size(), img.format());
	QImage regionImg = QImage(img.size(), QImage::Format_RGB32);
	regionImg.fill(fillColor);

	QPainter myPainter(&regionImg);

	myPainter.setClipRegion(QRegion(region->polygon().polygon().toPolygon()));
	myPainter.drawImage(0, 0, img);

	rdf::Rect regionRect = polygonToOCRBox(img.size(), region->polygon());	//this will add some additional padding
	QImage croppedRI = regionImg.copy(regionRect.toQRect());

	// debug ocr images
	//cv::Mat cri = nmc::DkImage::qImage2Mat(croppedRI);
	//cv::Mat ri = nmc::DkImage::qImage2Mat(regionImg);
	//cv::String winName1 = "image of the cropped text region ";
	//cv::String winName2 = "image of masked text region";
	//winName1 = winName1 + region->id().toStdString();
	//winName2 = winName2 + region->id().toStdString();
	//
	//cv::namedWindow(winName1, cv::WINDOW_AUTOSIZE);// Create a window for display.
	//cv::imshow(winName1, cri);

	//cv::namedWindow(winName2, cv::WINDOW_AUTOSIZE);// Create a window for display.
	//cv::imshow(winName2, ri);
	
	return croppedRI;

}

rdf::Rect TesseractEngine::polygonToOCRBox(const QSize imgSize, const rdf::Polygon poly) const {

	rdf::Rect ocrBox = rdf::Rect::fromPoints(poly.toPoints());		// warning: fromPoints ignores point (0,0)
	ocrBox.expand(10); // expanding image improves OCR results - avoids errors if text is connected to the image border
	ocrBox = ocrBox.clipped(rdf::Vector2D(imgSize));

	return ocrBox;
}

// returns true if polygon is an axis aligned rectangle
bool TesseractEngine::isAARect(rdf::Polygon poly) {

	// TODO test/debug function
	
	if (poly.size() == 4) {

		QVector<rdf::Vector2D> pts;

		for (const QPointF& p : poly.polygon()) {
			pts << rdf::Vector2D(p);
		}

		rdf::Vector2D l1 = pts[0] - pts[1];
		rdf::Vector2D l2 = pts[2] - pts[3];
		
		if (l1.length() == l2.length())
			return true;
		else
			return false;
	}
	else {
		return false;
	}
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
	mWsaConfig.loadSettings(settings);
	settings.endGroup();
}

void TesseractPlugin::saveSettings(QSettings & settings) const {

	// save settings (this is needed for batch profiles)
	settings.beginGroup(name());
	mConfig.saveSettings(settings);
	mWsaConfig.saveSettings(settings);
	settings.endGroup();
}

// tesseract config---------------------------------------------------------------------------
TesseractPluginConfig::TesseractPluginConfig() : ModuleConfig("TesseractPlugin") {
}

void TesseractPluginConfig::load(const QSettings & settings) {

	mTessdataDir = settings.value("TessdataDir", mTessdataDir).toString();
	mTextLevel = settings.value("TextLevel", mTextLevel).toInt();
	mDrawResults = settings.value("DrawResults", mDrawResults).toBool();
	mSingleLevelOutput = settings.value("SingleLevelOutput", mSingleLevelOutput).toBool();
}

void TesseractPluginConfig::save(QSettings & settings) const {

	settings.setValue("TessdataDir", mTessdataDir);
	settings.setValue("TextLevel", mTextLevel);
	settings.setValue("DrawResults", mDrawResults);
	settings.setValue("SingleLevelOutput", mSingleLevelOutput);
}

QString TesseractPluginConfig::TessdataDir() const {
	return mTessdataDir;
}

int TesseractPluginConfig::textLevel() const {
	return mTextLevel;
}

bool TesseractPluginConfig::drawResults() const {
	return mDrawResults;
}

bool TesseractPluginConfig::singleLevelOutput() const {
	return mSingleLevelOutput;
}

QString TesseractPluginConfig::toString() const {

	QString msg = rdf::ModuleConfig::toString();
	msg += "  TessdataDir: " + mTessdataDir;
	msg += "  TextLevel: " + QString::number(mTextLevel);
	msg += drawResults() ? " drawing results\n" : " not drawing results\n";
	msg += singleLevelOutput() ? " single text level exported\n" : " all text levels exported\n";

	return msg;
}

};


