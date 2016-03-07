/*******************************************************************************************************
 ReadFramework is the basis for modules developed at CVL/TU Wien for the EU project READ. 
  
 Copyright (C) 2016 Markus Diem <diem@caa.tuwien.ac.at>
 Copyright (C) 2016 Stefan Fiel <fiel@caa.tuwien.ac.at>
 Copyright (C) 2016 Florian Kleber <kleber@caa.tuwien.ac.at>

 This file is part of ReadFramework.

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

#pragma warning(push, 0)	// no warnings from includes
// Qt Includes
#include <QString>
#include <QVector>
#include "opencv2/imgproc/imgproc.hpp"
#include "opencv2/ml/ml.hpp"
#pragma warning(pop)

// TODO: add DllExport magic

// Qt defines

namespace rdm {
	class WIVocabulary {
	public:
		WIVocabulary();

		void loadVocabulary(const QString filePath);
		void saveVocabulary(const QString filePath) const;

		bool isEmpty() const;

		enum type {
			WI_GMM,
			WI_BOW,

			WI_UNDEFINED
		};

		void setVocabulary(cv::Mat voc);
		cv::Mat vocabulary() const;
		void setEM(cv::Ptr<cv::ml::EM> em);
		cv::Ptr<cv::ml::EM> em() const;
		void setPcaMean(cv::Mat mean);
		cv::Mat pcaMean() const;
		void setPcaSigma(cv::Mat pcaSigma);
		cv::Mat pcaSigma() const;
		void setPcaEigenvectors(cv::Mat ev);
		cv::Mat pcaEigenvectors() const;
		void setPcaEigenvalues(cv::Mat ev);
		cv::Mat pcaEigenvalues() const;
		void setL2Mean(const cv::Mat l2mean);
		cv::Mat l2Mean() const;
		void setL2Sigma(const cv::Mat l2sigma);
		cv::Mat l2Sigma() const;
		void setNumberOfCluster(const int number);
		int numberOfCluster() const;
		void setNumberOfPCA(const int number);
		int numberOfPCA() const;
		void setType(const int type);
		int type() const;
		void setNote(QString note);
		QString note() const;


	private:
		cv::Mat mVocabulary;
		cv::Ptr<cv::ml::EM> mEM;
		cv::Mat mPcaMean;
		cv::Mat mPcaSigma;
		cv::Mat mPcaEigenvectors;
		cv::Mat mPcaEigenvalues;
		cv::Mat mL2Mean;
		cv::Mat mL2Sigma;

		int mNumberOfClusters;
		int mNumberPCA;
		int mType;

		QString mNote;
	};

// read defines
	class WIDatabase {
	public:
		WIDatabase();

		void addFile(const QString filePath);
		void generateVocabulary();

		void setVocabulary(const WIVocabulary voc);
		WIVocabulary vocabulary() const;
		void saveVocabulary(QString filePath) const;

		void evaluateDatabase(QStringList classLabels, QStringList filePaths, QString filePath = QString()) const;
		cv::Mat generateHist(cv::Mat desc) const;

	private:
		QString debugName() const;
		cv::Mat calculatePCA(cv::Mat desc);
		void generateBOW(cv::Mat desc);
		void generateGMM(cv::Mat desc);
		cv::Mat applyPCA(cv::Mat desc) const;
		cv::Mat generateHistBOW(cv::Mat desc) const;
		cv::Mat generateHistGMM(cv::Mat desc) const;
		cv::Mat l2Norm(cv::Mat desc) const;
		QVector<QVector<cv::KeyPoint> > mKeyPoints;
		QVector<cv::Mat> mDescriptors;
		WIVocabulary mVocabulary;
	};
};