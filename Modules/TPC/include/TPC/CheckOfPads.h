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
/// \file   CheckOfPads.h
/// \author Maximilian Horst, Marcel Lesch
///

#ifndef QC_MODULE_TPC_CheckOfPads_H
#define QC_MODULE_TPC_CheckOfPads_H

#include "QualityControl/CheckInterface.h"

namespace o2::quality_control_modules::tpc
{

/// \brief
///
/// \author Maximilian Horst
class CheckOfPads : public o2::quality_control::checker::CheckInterface
{

 public:
  /// Default constructor
  CheckOfPads() = default;
  /// Destructor
  ~CheckOfPads() override = default;

  // Override interface
  void configure() override;
  Quality check(std::map<std::string, std::shared_ptr<MonitorObject>>* moMap) override;
  void beautify(std::shared_ptr<MonitorObject> mo, Quality checkResult = Quality::Null) override;
  std::string getAcceptedType() override;

 private:
  ClassDefOverride(CheckOfPads, 1);
  std::string createMetaData(const std::vector<std::string>& pointMetaData);
  static constexpr std::string_view CheckChoiceMean = "Mean";
  static constexpr std::string_view CheckChoiceExpectedValue = "ExpectedValue";
  static constexpr std::string_view CheckChoiceBoth = "Both";

  /*std::vector<std::string> mSectorsName;
  std::vector<Quality> mSectorsQuality;
  */

  /* std::vector<float> mPadMeans;
  std::vector<float> mPadStdev;
  std::vector<float> mEmptyPadPercent;
  std::vector<int> mPadCounts;
  std::vector<float> mTotalPads;*/

  std::string mSectorsName[72];
  Quality mSectorsQuality[72];
  std::vector<std::string> mMOsToCheck2D;

  bool mROCExists[72];
  float mPadMeans[72] = { 0. };
  float mPadStdev[72] = { 0. };
  float mEmptyPadPercent[72] = { 0. };
  int mPadCounts[72] = { 0 };
  float mTotalPads[72] = { 0. };

  std::unordered_map<std::string, std::vector<std::string>> mROCMetaData;

  float mMediumQualityLimit;
  float mBadQualityLimit;

  float mExpectedValue;
  float mExpectedValueMediumSigmas;
  float mExpectedValueBadSigmas;

  float mMeanMediumSigmas;
  float mMeanBadSigmas;

  float mExpectedMean;
  float mExpectedMeanMediumSigmas;
  float mExpectedMeanBadSigmas;

  float mTotalMean;
  float mTotalStdev;

  bool mEmptyCheck = false;
  bool mExpectedValueCheck = false;
  bool mMeanCheck = false;
  bool mExpectedMeanCheck = false;
  std::string mMetadataComment;
};

} // namespace o2::quality_control_modules::tpc

#endif // QC_MODULE_TPC_CheckOfPads_H