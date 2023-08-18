// Copyright CERN and copyright holders of ALICE O2. This software is
// distributed under the terms of the GNU General Public License v3 (GPL
// Version 3), copied verbatim in the file "COPYING".
//
// See http://alice-o2.web.cern.ch/license for full licensing information.
//
// In applying this license CERN does not waive the privileges and immunities
// granted to it by virtue of its status as an Intergovernmental Organization
// or submit itself to any jurisdiction.

///
/// \file   CheckOfPads.cxx
/// \author Laura Serksnyte, Maximilian Horst, Marcel Lesch
///

#include "TPC/CheckOfPads.h"
#include "QualityControl/MonitorObject.h"
#include "QualityControl/Quality.h"
#include "QualityControl/QcInfoLogger.h"
#include "Common/Utils.h"

// ROOT
#include <TCanvas.h>
#include <TH1.h>
#include <TH2.h>
#include <TPad.h>
#include <TList.h>
#include <TPaveText.h>

#include <iostream>

namespace o2::quality_control_modules::tpc
{
void CheckOfPads::configure()
{
  mMediumQualityLimit = common::getFromConfig<float>(mCustomParameters, "mediumQualityPercentageOfWorkingPads", 0.7);
  mBadQualityLimit = common::getFromConfig<float>(mCustomParameters, "badQualityPercentageOfWorkingPads", 0.3);
  const std::string checkChoiceString = common::getFromConfig<std::string>(mCustomParameters, "CheckChoice", "Mean");

  if (size_t finder = checkChoiceString.find("ExpectedValue"); finder != std::string::npos) {
    mExpectedValueCheck = true;
  }
  if (size_t finder = checkChoiceString.find("Mean"); finder != std::string::npos) {
    mMeanCheck = true;
  }
  if (size_t finder = checkChoiceString.find("Empty"); finder != std::string::npos) {
    mEmptyCheck = true;
  }
  if (size_t finder = checkChoiceString.find("ExpectedMean"); finder != std::string::npos) {
    mExpectedMeanCheck = true;
  }

  if (mExpectedValueCheck) {
    // load expectedValue and allowed standard deviations for medium/bad quality
    mExpectedValue = common::getFromConfig<float>(mCustomParameters, "ExpectedValue", 1.0);
    mExpectedValueMediumSigmas = common::getFromConfig<float>(mCustomParameters, "ExpectedValueSigmaMedium", 3.0);
    mExpectedValueBadSigmas = common::getFromConfig<float>(mCustomParameters, "ExpectedValueSigmaBad", 6.0);
  }
  // Check if Mean comparison is wished for:
  if (mMeanCheck) {
    // load Mean Sigma Medium
    mMeanMediumSigmas = common::getFromConfig<float>(mCustomParameters, "MeanSigmaMedium", 3.0);
    mMeanBadSigmas = common::getFromConfig<float>(mCustomParameters, "MeanSigmaBad", 6.0);
  }

  if (mExpectedMeanCheck) {
    // load expectedValue and allowed standard deviations for medium/bad quality
    mExpectedMean = common::getFromConfig<float>(mCustomParameters, "ExpectedMean", 1.0);
    mExpectedMeanMediumSigmas = common::getFromConfig<float>(mCustomParameters, "ExpectedMeanSigmaMedium", 3.0);
    mExpectedMeanBadSigmas = common::getFromConfig<float>(mCustomParameters, "ExpectedMeanSigmaBad", 6.0);
  }

  if (auto param = mCustomParameters.find("MOsNames2D"); param != mCustomParameters.end()) {
    auto temp = param->second.c_str();
    std::istringstream ss(temp);
    std::string token;
    while (std::getline(ss, token, ',')) {
      mMOsToCheck2D.emplace_back(token);
    }
  }

  mMetadataComment = common::getFromConfig<std::string>(mCustomParameters, "MetadataComment", "");
}

//______________________________________________________________________________
Quality CheckOfPads::check(std::map<std::string, std::shared_ptr<MonitorObject>>* moMap)
{
  for (int iROC = 1; iROC <= 72; iROC++) {
    mSectorsName[iROC - 1] = "";
    mSectorsQuality[iROC - 1] = Quality::Null;
    mROCExists[iROC - 1] = false;
    mPadMeans[iROC - 1] = 0.;
    mPadStdev[iROC - 1] = 0.;
    mEmptyPadPercent[iROC - 1] = 0.;
    mPadCounts[iROC - 1] = 0;
    mTotalPads[iROC - 1] = 0.;
  }

  mROCMetaData.clear();
  mROCMetaData[Quality::Null.getName()] = std::vector<std::string>();
  mROCMetaData[Quality::Bad.getName()] = std::vector<std::string>();
  mROCMetaData[Quality::Medium.getName()] = std::vector<std::string>();
  mROCMetaData[Quality::Good.getName()] = std::vector<std::string>();

  auto mo = moMap->begin()->second;
  if (!mo) {
    ILOG(Fatal, Support) << "Monitoring object not found" << ENDM;
  }
  const auto moName = mo->getName();
  if (auto it = std::find(mMOsToCheck2D.begin(), mMOsToCheck2D.end(), moName); it != mMOsToCheck2D.end()) {
    const size_t end = moName.find("_2D");
    const auto histSubName = moName.substr(7, end - 7);

    auto* canv = dynamic_cast<TCanvas*>(mo->getObject());
    if (!canv) {
      ILOG(Fatal, Support) << "Canvas not found" << ENDM;
    }
    // Check all histograms in the canvas

    mTotalMean = 0.;

    for (int iROC = 1; iROC <= 72; iROC++) { // loop over pads to fetch object and calculate mean per pad
      const auto padName = fmt::format("{:s}_{:d}", moName, iROC);
      const auto histName = fmt::format("h_{:s}_ROC_{:02d}", histSubName, iROC - 1);
      TPad* pad = (TPad*)canv->GetListOfPrimitives()->FindObject(padName.data());
      if (!pad) {
        mSectorsName[iROC - 1] = "notitle";
        mROCExists[iROC - 1] = false;
        continue;
      }
      TH2F* h = (TH2F*)pad->GetListOfPrimitives()->FindObject(histName.data());
      if (!h) {
        mSectorsName[iROC - 1] = "notitle";
        mROCExists[iROC - 1] = false;
        continue;
      }
      const std::string titleh = h->GetTitle();

      mSectorsName[iROC - 1] = titleh;

      // check if we are dealing with IROC or OROC
      const int MaximumXBin = h->GetNbinsX();
      const int MaximumYBin = h->GetNbinsY();
      if (titleh.find("IROC") != std::string::npos) {
        mTotalPads[iROC - 1] = 5280.;
      } else if (titleh.find("OROC") != std::string::npos) {
        mTotalPads[iROC - 1] = 9280.;
      } else {
        mROCExists[iROC - 1] = false;
        continue;
      }

      mROCExists[iROC - 1] = true;

      float padSum = 0.;
      int padsCount = 0;
      float padStdev = 0.;
      float padMean = 0.;

      // Run twice to get the mean and the standard deviation
      for (int runNo = 1; runNo <= 2; runNo++) {
        // Run1: calculate single Pad total->Mean
        // Run2: calculate standardDeviation from mean
        for (int xBin = 1; xBin <= MaximumXBin; xBin++) {
          for (int yBin = 1; yBin <= MaximumYBin; yBin++) {
            const float binvalue = h->GetBinContent(xBin, yBin);
            if (binvalue != 0) {
              if (runNo == 1) {
                padSum += binvalue;
                padsCount++;
              } else {
                padStdev += pow(binvalue - padMean, 2);
              }
            }
          }
        }

        if (runNo == 1 && padSum > 0.) { // no need for div by 0 check, because if padSum > 0 -> padsCount>0
          padMean = padSum / padsCount;
        }
      }

      mPadCounts[iROC - 1] = padsCount;
      if (mPadCounts[iROC - 1] > 2) {
        padStdev = sqrt(padStdev / (padsCount - 1));
      } else {
        padStdev = 0.;
      }

      mPadMeans[iROC - 1] = padMean;
      mPadStdev[iROC - 1] = padStdev;

    } // for (int iROC = 1; iROC <= 72; iROC++)

    // calculate the total mean and standard deviation
    float sumOfWeights = 0.;
    for (size_t iROC = 1; iROC <= 72; iROC++) { // loop over all pads to determine total mean
      if (mPadStdev[iROC - 1] == 0. || !mROCExists[iROC - 1]) {
        continue;
      }
      mTotalMean += mPadMeans[iROC - 1] / mPadStdev[iROC - 1];
      sumOfWeights += 1 / mPadStdev[iROC - 1];
    }

    mTotalStdev = sqrt(1 / sumOfWeights); // standard deviation of the weighted average.
    mTotalMean /= sumOfWeights;           // Weighted average (by standard deviation) of the total mean

    // calculate the Qualities:
    for (size_t iROC = 1; iROC <= 72; iROC++) { // loop over all pads to perform actual checks

      std::string padNullString = "";
      std::string padBadString = "";
      std::string padMediumString = "";
      std::string padGoodString = "";
      std::vector<Quality> qualitiesOfPad;

      if (!mROCExists[iROC - 1]) { // Exclude ROCs that do not exist
        qualitiesOfPad.push_back(Quality::Null);
      }

      if (mPadCounts[iROC - 1] == 0) {
        padNullString += "PadCounts \n";
        qualitiesOfPad.push_back(Quality::Null);
      }

      if (mEmptyCheck && mROCExists[iROC - 1]) {
        if (mPadCounts[iROC - 1] > mMediumQualityLimit * mTotalPads[iROC - 1]) {
          qualitiesOfPad.push_back(Quality::Good);
          padGoodString += "EmptyCheck \n";
        } else if (mPadCounts[iROC - 1] < mBadQualityLimit * mTotalPads[iROC - 1]) {
          qualitiesOfPad.push_back(Quality::Bad);
          padBadString += "EmptyCheck \n";
        } else {
          qualitiesOfPad.push_back(Quality::Medium);
          padMediumString += "EmptyCheck \n";
        }
        mEmptyPadPercent[iROC - 1] = 1. - (float)mPadCounts[iROC - 1] / mTotalPads[iROC - 1];
      }

      if (mExpectedValueCheck && mROCExists[iROC - 1]) {
        if (std::abs(mPadMeans[iROC - 1] - mExpectedValue) < mPadStdev[iROC - 1] * mExpectedValueMediumSigmas) {
          qualitiesOfPad.push_back(Quality::Good);
          padGoodString += "ExpectedValueCheck \n";
        } else if (std::abs(mPadMeans[iROC - 1] - mExpectedValue) >= mPadStdev[iROC - 1] * mExpectedValueMediumSigmas && std::abs(mPadMeans[iROC - 1] - mExpectedValue) < mPadStdev[iROC - 1] * mExpectedValueBadSigmas) {
          qualitiesOfPad.push_back(Quality::Medium);
          padMediumString += "ExpectedValueCheck \n";
        } else {
          qualitiesOfPad.push_back(Quality::Bad);
          padBadString += "ExpectedValueCheck \n";
        }
      }

      if (mMeanCheck && mROCExists[iROC - 1]) {
        if (std::abs(mPadMeans[iROC - 1] - mTotalMean) < mPadStdev[iROC - 1] * mMeanMediumSigmas) {
          qualitiesOfPad.push_back(Quality::Good);
          padGoodString += "MeanCheck \n";
        } else if (std::abs(mPadMeans[iROC - 1] - mTotalMean) >= mPadStdev[iROC - 1] * mMeanMediumSigmas && std::abs(mPadMeans[iROC - 1] - mTotalMean) < mPadStdev[iROC - 1] * mMeanBadSigmas) {
          qualitiesOfPad.push_back(Quality::Medium);
          padMediumString += "MeanCheck \n";
        } else {
          qualitiesOfPad.push_back(Quality::Bad);
          padBadString += "MeanCheck \n";
        }
      }

      if (qualitiesOfPad.size() > 0) {
        auto worst_Quality_Pad = std::max_element(qualitiesOfPad.begin(), qualitiesOfPad.end(),
                                                  [](const Quality& q1, const Quality& q2) {
                                                    return q1.isBetterThan(q2);
                                                  });
        mSectorsQuality[iROC - 1] = *worst_Quality_Pad;
      } else {
        mSectorsQuality[iROC - 1] = Quality::Null;
        padNullString += "NoQualities \n";
      }

      mROCMetaData[Quality::Null.getName()].push_back(padNullString);
      mROCMetaData[Quality::Bad.getName()].push_back(padBadString);
      mROCMetaData[Quality::Medium.getName()].push_back(padMediumString);
      mROCMetaData[Quality::Good.getName()].push_back(padGoodString);

    } // for iROC in vectors (pads)
  }   // if MO exists

  // Final Aggregation of qualities and metadata of all sectors
  Quality totalQuality = Quality::Good;
  std::string totalBadString = "";
  std::string totalMediumString = "";
  std::string totalGoodString = "";
  std::string totalNullString = "";

  for (int iROC = 1; iROC <= 72; iROC++) {
    if (mSectorsQuality[iROC - 1].isWorseThan(totalQuality)) {
      totalQuality = mSectorsQuality[iROC - 1];
    }
  }

  // MetaData aggregation from all ROCs
  totalBadString = createMetaData(mROCMetaData[Quality::Bad.getName()]);
  totalMediumString = createMetaData(mROCMetaData[Quality::Medium.getName()]);
  totalGoodString = createMetaData(mROCMetaData[Quality::Good.getName()]);
  totalNullString = createMetaData(mROCMetaData[Quality::Null.getName()]);

  Quality qualityMeanExpectedValue = Quality::Null;
  if (mExpectedMeanCheck) { // compare the total mean to the expected value. This is returned as the quality object
    if (std::abs(mTotalMean - mExpectedMean) < mTotalStdev * mExpectedMeanMediumSigmas) {
      qualityMeanExpectedValue = Quality::Good;
      totalGoodString += "MeanExpectedValue \n";
    } else if (std::abs(mTotalMean - mExpectedMean) > mTotalStdev * mExpectedMeanBadSigmas) {
      qualityMeanExpectedValue = Quality::Bad;
      totalBadString += "MeanExpectedValue \n";
    } else {
      qualityMeanExpectedValue = Quality::Medium;
      totalMediumString += "MeanExpectedValue \n";
    }
  }

  if (mExpectedMeanCheck && (mEmptyCheck || mExpectedValueCheck || mMeanCheck)) {
    if (qualityMeanExpectedValue.isWorseThan(totalQuality)) {
      totalQuality = qualityMeanExpectedValue;
    }
  } else { // mExpectedMeanCheck is the only check
    totalQuality = qualityMeanExpectedValue;
  }

  totalQuality.addMetadata(Quality::Bad.getName(), totalBadString);
  totalQuality.addMetadata(Quality::Medium.getName(), totalMediumString);
  totalQuality.addMetadata(Quality::Good.getName(), totalGoodString);
  totalQuality.addMetadata(Quality::Null.getName(), totalNullString);
  totalQuality.addMetadata("Comment", mMetadataComment);

  return totalQuality;
} // end of loop over moMap

//______________________________________________________________________________
std::string CheckOfPads::getAcceptedType() { return "TCanvas"; }

//______________________________________________________________________________
void CheckOfPads::beautify(std::shared_ptr<MonitorObject> mo, Quality)
{
  auto moName = mo->getName();
  if (auto it = std::find(mMOsToCheck2D.begin(), mMOsToCheck2D.end(), moName); it != mMOsToCheck2D.end()) {

    auto* tcanv = dynamic_cast<TCanvas*>(mo->getObject());

    const size_t end = moName.find("_2D");
    const auto histSubName = moName.substr(7, end - 7);
    const auto histNameS = fmt::format("h_{}_ROC", histSubName);
    for (int iROC = 1; iROC <= 72; iROC++) {

      if (!mROCExists[iROC - 1]) {
        continue;
      }

      const std::string padName = fmt::format("{:s}_{:d}", moName, iROC);
      TPad* pad = (TPad*)tcanv->GetListOfPrimitives()->FindObject(padName.data());
      if (!pad) {
        continue;
      }
      pad->cd();
      const auto histName = fmt::format("{}_{:02d}", histNameS, iROC - 1);
      const auto h = (TH1F*)pad->GetListOfPrimitives()->FindObject(histName.data());
      if (!h) {
        continue;
      }

      std::string checkMessage;
      Quality qualitySpecial = mSectorsQuality[iROC - 1];

      TPaveText* msgQuality = new TPaveText(0.1, 0.85, 0.81, 0.95, "NDC");
      msgQuality->SetBorderSize(1);

      msgQuality->SetName(Form("%s_msg", mo->GetName()));
      if (qualitySpecial == Quality::Good) {
        msgQuality->Clear();
        msgQuality->AddText("Good");
        msgQuality->SetFillColor(kGreen);
        checkMessage = mROCMetaData[Quality::Good.getName()][iROC - 1];
      } else if (qualitySpecial == Quality::Bad) {
        msgQuality->Clear();
        msgQuality->AddText("Bad");
        msgQuality->SetFillColor(kRed);
        checkMessage = mROCMetaData[Quality::Bad.getName()][iROC - 1];
      } else if (qualitySpecial == Quality::Medium) {
        msgQuality->Clear();
        msgQuality->AddText("Medium");
        checkMessage = mROCMetaData[Quality::Medium.getName()][iROC - 1];
        msgQuality->SetFillColor(kOrange);
      } else if (qualitySpecial == Quality::Null) {
        h->SetFillColor(0);
        checkMessage = mROCMetaData[Quality::Null.getName()][iROC - 1];
      }

      // Split lines by hand as \n does not work with TPaveText
      const std::string delimiter = "\n";
      size_t pos = 0;
      std::string subText;
      while ((pos = checkMessage.find(delimiter)) != std::string::npos) {
        subText = checkMessage.substr(0, pos);
        msgQuality->AddText(subText.c_str());
        checkMessage.erase(0, pos + delimiter.length());
      }
      msgQuality->AddText(mMetadataComment.data());

      h->SetLineColor(kBlack);
      msgQuality->Draw("same");
    }
  }
}

std::string CheckOfPads::createMetaData(const std::vector<std::string>& pointMetaData)
{

  std::string meanString = "";
  std::string expectedValueString = "";
  std::string emptyString = "";
  std::string noPadCountsString = "";
  std::string noQualityString = "";
  std::string noROCString = "";

  for (int i; i < pointMetaData.size(); i++) {
    if (pointMetaData.at(i).find("MeanCheck") != std::string::npos) {
      meanString += " " + std::to_string(i + 1) + ",";
    }
    if (pointMetaData.at(i).find("ExpectedValueCheck") != std::string::npos) {
      expectedValueString += " " + std::to_string(i + 1) + ",";
    }
    if (pointMetaData.at(i).find("EmptyCheck") != std::string::npos) {
      emptyString += " " + std::to_string(i + 1) + ",";
    }
    if (pointMetaData.at(i).find("PadCounts") != std::string::npos) {
      noPadCountsString += " " + std::to_string(i + 1) + ",";
    }
    if (pointMetaData.at(i).find("NoQualities") != std::string::npos) {
      noQualityString += " " + std::to_string(i + 1) + ",";
    }
    if (pointMetaData.at(i).find("NoROC") != std::string::npos) {
      noROCString += " " + std::to_string(i + 1) + ",";
    }
  }

  std::string totalString = "";
  if (meanString != "") {
    meanString.pop_back();
    meanString = "MeanCheck for ROCs:" + meanString + "\n";
    totalString += meanString;
  }
  if (expectedValueString != "") {
    expectedValueString.pop_back();
    expectedValueString = "ExpectedValueCheck for ROCs:" + expectedValueString + "\n";
    totalString += expectedValueString;
  }
  if (emptyString != "") {
    emptyString.pop_back();
    emptyString = "Empty Pads for ROCs:" + emptyString + "\n";
    totalString += emptyString;
  }
  if (noPadCountsString != "") {
    noPadCountsString.pop_back();
    noPadCountsString = "No counts for ROCs:" + noPadCountsString + "\n";
    totalString += noPadCountsString;
  }
  if (noQualityString != "") {
    noQualityString.pop_back();
    noQualityString = "No check performed for ROCs:" + noQualityString + "\n";
    totalString += noQualityString;
  }
  if (noROCString != "") {
    noROCString.pop_back();
    noROCString = "No ROCs found for " + noROCString + "\n";
    totalString += noROCString;
  }

  return totalString;
}

} // namespace o2::quality_control_modules::tpc
