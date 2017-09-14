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

// read framework
#include "PageParser.h"
#include "Shapes.h"


//tesseract
#include <allheaders.h> // leptonica main header for image io

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

	menuNames[id_ocr_image] = tr("OCR of current image");
	menuNames[id_ocr_page] = tr("OCR using given segmentation");
	mMenuNames = menuNames.toList();

	// create menu status tips
	QVector<QString> statusTips;
	statusTips.resize(id_end);

	statusTips[id_ocr_image] = tr("Computes optical character recognition results for an image and saves them as PAGE xml file.");
	statusTips[id_ocr_page] = tr("Computes optical character recognition results for provided segmentation (PAGE file) and image.");
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


	if (runID == mRunIDs[id_ocr_image]) {

		qInfo() << "image ocr to PAGE xml";

		auto img = imgC->image();

		// Init Tesseract API
		tesseract::TessBaseAPI* tessAPI = initTesseract();	//fixed path, only english
		if (!tessAPI) {	//init failed
			return imgC;
		}
		// Prepare PAGE xml output
		rdf::PageXmlParser parser;
		auto xmlPage = createOutputPAGE(parser, imgC);
		
		//compute tesseract OCR results
		performOCR(tessAPI, imgC->image());
		
		saveOCRtoPAGE(tessAPI, xmlPage);
		
		// save xml output
		//QString saveXmlPath = QDir::currentPath() + "/xml/output.xml";
		QString saveXmlPath = rdf::PageXmlParser::imagePathToXmlPath(saveInfo.outputFilePath());
		parser.write(saveXmlPath, xmlPage);

		// close tesseract api
		tessAPI->End();

		
		//alternative output format, just an idea
		//tesseract::TessPDFRenderer pdfR = new tesseract::TessPDFRenderer(outputbase, api->GetDatapath(), textonly));

		return imgC;

	}
	else if (runID == mRunIDs[id_ocr_page]) {

		qDebug() << "OCR on PAGE file";

		QString inputXmlPath = rdf::PageXmlParser::imagePathToXmlPath(saveInfo.inputFilePath(), "gt");
		rdf::PageXmlParser parser;
		parser.read(inputXmlPath);

		addOCRtoPAGE();

		return imgC;
	}

	// wrong runID? - do nothing
	return imgC;
}

// OCR & PAGE functions------------------------------------------------------------------------------
tesseract::TessBaseAPI* TesseractOCR::initTesseract() const{
	
	tesseract::TessBaseAPI* tessAPI = new tesseract::TessBaseAPI();

	//TO DO detailed warnings/error messages
	//TO DO allow setting different languages
	//TO DO fix call to tessdataDir path 

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

QSharedPointer<rdf::PageElement> TesseractOCR::createOutputPAGE(
	rdf::PageXmlParser parser, 
	QSharedPointer<nmc::DkImageContainer> imgC) const{

	//auto xmlPage = parser.page();
	parser.setPage(parser.page().create());
	auto xmlPage = parser.page();

											// set our header info
	xmlPage->setCreator(QString("CVL"));
	xmlPage->setImageSize(QSize(imgC->image().size()));
	xmlPage->setImageFileName(imgC->fileName());

	return xmlPage;
}

void TesseractOCR::performOCR(tesseract::TessBaseAPI* tessAPI, QImage img) const{

	//TO DO allow setting different segmentation modes

	tessAPI->SetPageSegMode(tesseract::PageSegMode::PSM_AUTO);
	tessAPI->SetVariable("save_best_choices", "T");
	tessAPI->SetImage(img.bits(), img.width(), img.height(), img.bytesPerLine() / img.width(), img.bytesPerLine());
	tessAPI->Recognize(0);

}

QSharedPointer<rdf::PageElement> TesseractOCR::saveOCRtoPAGE(
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

void TesseractOCR::addOCRtoPAGE() const {

	//TODO

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

