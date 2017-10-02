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

#pragma once

#include "DkPluginInterface.h"
#include "DkBatchInfo.h"

// read includes
#include "BaseModule.h"
#include "Elements.h"


//tesseract includes
#include <baseapi.h> // tesseract main header

// opencv defines
namespace cv {
	class Mat;
}

namespace rdf {
	class PageXmlParser;
}

namespace rdm {

class TesseractOCRConfig : public rdf::ModuleConfig {

public:
	TesseractOCRConfig();

	virtual QString toString() const override;
	QString TessdataDir() const;
	void setTessdataDir(QString dir);
	int textLevel() const;
	void setTextLevel(int level);

private:

	QString mTessdataDir = QString("E:\\dev\\CVL\\ReadModules\\build-x64\\Modules\\TesseractOCR");
	int mTextLevel = 1;

	void load(const QSettings& settings) override;
	void save(QSettings& settings) const override;
};


class TesseractOCR : public QObject, nmc::DkBatchPluginInterface {
	Q_OBJECT
		Q_INTERFACES(nmc::DkBatchPluginInterface)
		Q_PLUGIN_METADATA(IID "com.nomacs.ImageLounge.TesseractOCR/3.2" FILE "TesseractOCR.json")

public:
	TesseractOCR(QObject* parent = 0);
	~TesseractOCR();

	QImage image() const override;

	QList<QAction*> createActions(QWidget* parent) override;
	QList<QAction*> pluginActions() const override;
	QSharedPointer<nmc::DkImageContainer> runPlugin(
		const QString &runID, 
		QSharedPointer<nmc::DkImageContainer> imgC,
		const nmc::DkSaveInfo& saveInfo,
		QSharedPointer<nmc::DkBatchInfo>& info) const override;

	void preLoadPlugin() const override;
	void postLoadPlugin(const QVector<QSharedPointer<nmc::DkBatchInfo> >& batchInfo) const override;
	QString name() const override;

	QString settingsFilePath() const override;
	void loadSettings(QSettings& settings) override;
	void saveSettings(QSettings& settings) const override;

	enum {
		id_perform_ocr,
		// add actions here

		id_end
	};

protected:
	QList<QAction*> mActions;
	QStringList mRunIDs;
	QStringList mMenuNames;
	QStringList mMenuStatusTips;

	TesseractOCRConfig mConfig;
	QString mModuleName;

	tesseract::TessBaseAPI* initTesseract() const;
	void performOCR(tesseract::TessBaseAPI*, QImage) const;
	QSharedPointer<rdf::PageElement> saveResultsToXML(tesseract::TessBaseAPI*, QSharedPointer<rdf::PageElement>) const;
	void addTextToXML(QImage& img, QSharedPointer<rdf::PageElement> xmlPage, tesseract::TessBaseAPI* tessAPI) const;
	QVector<rdf::Rect> TesseractOCR::getOCRBoxes(QSize& imgSize, QVector<QSharedPointer<rdf::Region>> tRegions) const;
	rdf::Rect polygonToOCRBox(QSize imgSize, rdf::Polygon poly) const;
	QVector<rdf::Rect> mergeOverlappingBoxes(QVector<rdf::Rect> tBoxes) const;
	void TesseractOCR::processRectResults(tesseract::TessBaseAPI* tessAPI, rdf::Rect b, QVector<QSharedPointer<rdf::Region>> tRegions, QVector<rdf::Rect> tBoxes) const;
	void writeTextToRegions(char* lineText, rdf::Rect lineRect, QVector<QSharedPointer<rdf::Region>>& tRegions, QVector<rdf::Rect> tBoxes) const;
};
};
