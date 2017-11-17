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
#include "Shapes.h"
#include <QXmlStreamReader>

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

	class TesseractPluginConfig : public rdf::ModuleConfig {

		public:
			TesseractPluginConfig();

			virtual QString toString() const override;

			QString TessdataDir() const;
			//void setTessdataDir(QString dir);

			int textLevel() const;
			//void setTextLevel(int level);

			bool drawResults() const;
			//void setDrawResults(bool draw);

		private:

			QString mTessdataDir = QString("E:\\dev\\CVL\\ReadModules\\ReadModules\\Modules\\TesseractOCR");
			int mTextLevel = 2;
			bool mDrawResults = false;

			void load(const QSettings& settings) override;
			void save(QSettings& settings) const override;
	};

	class TessWord : public rdf::Region {

	public:
		TessWord(const Type& type = Type::type_unknown);

		void setText(const QString& text);
		QString text() const;

		virtual bool read(QXmlStreamReader& reader) override;
		virtual void write(QXmlStreamWriter& writer) const override;

		virtual QString toString(bool withChildren = false) const override;

		virtual void draw(QPainter& p, const rdf::RegionTypeConfig& config) const override;

	protected:
		rdf::TextEquiv mTextEquiv;
	};

	class TesseractEngine {

		public:
			TesseractEngine();
			~TesseractEngine();

			bool init(const QString tessdataDir);
			tesseract::ResultIterator* processPage(const QImage img);
			QImage processTextRegions(QImage img, QVector<QSharedPointer<rdf::Region>> textRegions);

			QImage getRegionImage(const QImage img, const QSharedPointer<rdf::Region>, const QColor fillColor = QColor(Qt::white)) const;
			void addTextToRegion(const QImage img, QSharedPointer<rdf::Region> region, 
				const rdf::Rect regionRect = rdf::Rect(), const tesseract::PageSegMode psm = tesseract::PageSegMode::PSM_AUTO);
			rdf::Rect polygonToOCRBox(const QSize imgSize, const rdf::Polygon poly) const;
			bool isAARect(rdf::Polygon poly);

		private:
			tesseract::TessBaseAPI* mTessAPI;
			void setImage(const QImage img);
			void setRectangle(const rdf::Rect rect);
	};

	class TesseractPlugin : public QObject, nmc::DkBatchPluginInterface {
		Q_OBJECT
			Q_INTERFACES(nmc::DkBatchPluginInterface)
			Q_PLUGIN_METADATA(IID "com.nomacs.ImageLounge.TesseractOCR/3.2" FILE "TesseractOCR.json")

	public:
		TesseractPlugin(QObject* parent = 0);
		~TesseractPlugin();

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
			id_end
		};

	protected:
		QList<QAction*> mActions;
		QStringList mRunIDs;
		QStringList mMenuNames;
		QStringList mMenuStatusTips;

		TesseractPluginConfig mConfig;
		QString mModuleName;

		void convertPageResults(tesseract::ResultIterator * ri, const QSharedPointer<rdf::PageElement> xmlPage) const;
		QVector<QSharedPointer<rdf::Region>> extractTextRegions(const QSharedPointer<rdf::PageElement> xmlPage) const;

		void convertRegion(const tesseract::PageIteratorLevel cil, const tesseract::PageIteratorLevel fil, tesseract::ResultIterator* ri, QSharedPointer<rdf::Region> parent) const;
		QSharedPointer<rdf::TextRegion> createTextRegion(const tesseract::ResultIterator* ri, const tesseract::PageIteratorLevel level, const tesseract::PageIteratorLevel outputLevel) const;
		QSharedPointer<rdf::TextLine> createTextLine(const tesseract::ResultIterator* ri, const tesseract::PageIteratorLevel outputLevel) const;
	};

};
