// Copyright 2019-2020 CERN and copyright holders of ALICE O2.
// See https://alice-o2.web.cern.ch/copyright for details of the copyright holders.
// All rights not expressly granted are reserved.
//
// This software is distributed under the terms of the GNU General Public
// License v3 (GPL Version 3), copied verbatim in the file "COPYING".
//
// In applying this license CERN does not waive the privileges and immunities
// granted to it by virtue of its status as an Intergovernmental Organization
// or submit itself to any jurisdiction.

/// \file   BoostHistogramUtils.h
/// \author Hannah Bossi, hannah.bossi@yale.edu

#include <cmath>
#include <numeric>
#include <algorithm>
#include <vector>
#include <array>
#include <TH1.h>
#include <TH2.h>
#include <TFile.h>

#include "Rtypes.h"
#include "TLinearFitter.h"
#include "TVectorD.h"
#include "TMath.h"
#include "TF1.h"
#include "Foption.h"
#include "HFitInterface.h"
#include "TFitResultPtr.h"
#include "TFitResult.h"
#include "Fit/Fitter.h"
#include "Fit/BinData.h"
#include "Math/WrappedMultiTF1.h"
#include <boost/histogram.hpp>
#include <boost/histogram/histogram.hpp>
#include <boost/format.hpp>
#include <boost/histogram/axis.hpp>
#include <boost/histogram/make_histogram.hpp>
#include <boost/histogram/accumulators/mean.hpp>

namespace o2
{
namespace utils
{

/// \class BinCenterView
/// \brief  Axis iterator over bin centers.
template <typename AxisIterator>
class BinCenterView
{
 public:
  BinCenterView(AxisIterator iter) : mBaseIterator(iter) {}
  ~BinCenterView() = default;
  AxisIterator&
    operator++()
  {
    ++mBaseIterator;
    return mBaseIterator;
  }
  AxisIterator operator++(int)
  {
    AxisIterator result(mBaseIterator);
    mBaseIterator++;
    return result;
  }

  decltype(auto) operator*() { return mBaseIterator->center(); }

 private:
  AxisIterator mBaseIterator;
};

/// \class BinUpperView
/// \brief  Axis iterator over bin upper edges.
template <typename AxisIterator>
class BinUpperView
{
 public:
  BinUpperView(AxisIterator iter) : mBaseIterator(iter) {}
  ~BinUpperView() = default;
  AxisIterator&
    operator++()
  {
    ++mBaseIterator;
    return mBaseIterator;
  }
  AxisIterator operator++(int)
  {
    AxisIterator result(mBaseIterator);
    mBaseIterator++;
    return result;
  }

  decltype(auto) operator*() { return mBaseIterator->upper(); }

 private:
  AxisIterator mBaseIterator;
};

/// \class BinLowerView
/// \brief  Axis iterator over bin lower edges.
template <typename AxisIterator>
class BinLowerView
{
 public:
  BinLowerView(AxisIterator iter) : mBaseIterator(iter) {}
  ~BinLowerView() = default;
  AxisIterator&
    operator++()
  {
    ++mBaseIterator;
    return mBaseIterator;
  }
  AxisIterator operator++(int)
  {
    AxisIterator result(mBaseIterator);
    mBaseIterator++;
    return result;
  }

  decltype(auto) operator*() { return mBaseIterator->lower(); }

 private:
  AxisIterator mBaseIterator;
};

template <typename AxisIterator>
BinCenterView<AxisIterator> operator+(BinCenterView<AxisIterator> lhs, int n)
{
  BinCenterView<AxisIterator> result(lhs);
  for (int i = 0; i < n; i++) {
    ++result;
  }
  return result;
}

template <typename AxisIterator>
BinUpperView<AxisIterator> operator+(BinUpperView<AxisIterator> lhs, int n)
{
  BinUpperView<AxisIterator> result(lhs);
  for (int i = 0; i < n; i++) {
    ++result;
  }
  return result;
}

template <typename AxisIterator>
BinLowerView<AxisIterator> operator+(BinLowerView<AxisIterator> lhs, int n)
{
  BinLowerView<AxisIterator> result(lhs);
  for (int i = 0; i < n; i++) {
    ++result;
  }
  return result;
}

template <typename T, int nparams>
class fitResult
{
 public:
  fitResult() = default;

  fitResult(T chi2, const T (&list)[nparams]) : mChi2(chi2),
                                                mFitParameters()
  {
    memcpy(mFitParameters.data(), list, sizeof(T) * nparams);
  }

  /// \brief Get the paratmeters of a fit result. Ex:  result.getParameter<1>();
  template <int index>
  T getParameter() const
  {
    static_assert(index <= nparams, "Acessing invalid parameter");
    return mFitParameters[index];
  }

  /// \brief Set the paratmeters of a fit result. Ex:  result.setParameter<1>(T(value)));
  template <int index>
  void setParameter(T parameter)
  {
    static_assert(index <= nparams, "Attempting to set invalid parameter");
    mFitParameters[index] = parameter;
  }

  /// \brief Set the chi2 of the fit result.
  void setChi2(double chi2In)
  {
    mChi2 = chi2In;
  }
  T getChi2() const { return mChi2; }

 private:
  T mChi2;                               ///< chi2 of the fit
  std::array<T, nparams> mFitParameters; ///< parameters of the fit (0-Constant, 1-Mean, 2-Sigma,  3-Sum)
};

/**
   * \enum FitGausError_t
   * \brief Error code for invalid result in the fitGaus process
   */
enum class FitGausError_t {
  FIT_ERROR, ///< Fit procedure returned invalid result
};

/// \brief Printing an error message when then fit returns an invalid result
/// \param errorcode Error of the type FitGausError_t, thrown when fit result is invalid.
std::string createErrorMessage(FitGausError_t errorcode)
{
  return "[Error]: Fit return an invalid result.";
}

/// \brief Function to fit histogram to a gaussian using iterators.
/// \param first begin iterator of the histogram
/// \param last end iterator of the histogram
/// \param axisFirst axis iterator over the bin centers
/// \return result
///      result is of the type fitResult, which contains 4 parameters (0-Constant, 1-Mean, 2-Sigma,  3-Sum)
///
/// ** Temp Note: For now we forgo the templated struct in favor of a std::vector in order to
/// have this compile while we are working out the details
template <typename T, typename Iterator, typename BinCenterView>
std::vector<double> fitGaus(Iterator first, Iterator last, BinCenterView axisfirst)
{
  TLinearFitter fitter(3, "pol2");
  TMatrixD mat(3, 3);
  Double_t kTol = mat.GetTol();
  fitter.StoreData(kFALSE);
  fitter.ClearPoints();
  TVectorD par(3);
  TVectorD sigma(3);
  TMatrixD A(3, 3);
  TMatrixD b(3, 1);
  T rms = TMath::RMS(first, last);
  // Markus: return type of std::max_element is an iterator, cannot cast implicitly to double
  // better use auto
  // pointer needs to be dereferenced afterwards
  auto xMax = std::max_element(first, last);
  auto xMin = std::min_element(first, last);
  auto nbins = last - first;
  const double binWidth = double(*xMax - *xMin) / double(nbins);

  Float_t meanCOG = 0;
  Float_t rms2COG = 0;
  Float_t sumCOG = 0;

  Float_t entries = 0;
  Int_t nfilled = 0;

  for (auto iter = first; iter != last; iter++) {
    entries += *iter;
    if (*iter > 0) {
      nfilled++;
    }
  }

  // TODO: Check why this is needed
  if (*xMax < 4) {
    throw FitGausError_t::FIT_ERROR;
  }
  if (entries < 12) {
    throw FitGausError_t::FIT_ERROR;
  }

  if (rms < kTol) {
    throw FitGausError_t::FIT_ERROR;
  }

  /*
  fitResult<T, 4> result;
  result.setParameter<3>(entries);
  */
  // create the result, first fill it with all 0's
  std::vector<double> result;
  for (int r = 0; r < 5; r++) {
    result.push_back(0);
  }
  // then set the third parameter to entries
  result.at(3) = entries;

  int ibin = 0;
  Int_t npoints = 0;
  for (auto iter = first, axisiter = axisfirst; iter != last; iter++, axisiter++) {
    if (nbins > 1) {
      // Markus: For this we implemented the BinCenterView
      double x = *axisfirst;
      double y = *iter;
      double ey = std::sqrt(y);
      fitter.AddPoint(&x, y, ey);
      if (npoints < 3) {
        A(npoints, 0) = 1;
        A(npoints, 1) = x;
        A(npoints, 2) = x * x;
        b(npoints, 0) = y;
        meanCOG += x * nbins;
        rms2COG += x * nbins * x;
        sumCOG += nbins;
      }
      npoints++;
    }
  }

  Double_t chi2 = 0;
  if (npoints >= 3) {
    if (npoints == 3) {
      //analytic calculation of the parameters for three points
      A.Invert();
      TMatrixD res(1, 3);
      res.Mult(A, b);
      par[0] = res(0, 0);
      par[1] = res(0, 1);
      par[2] = res(0, 2);
      chi2 = -3.;
    } else {
      // use fitter for more than three points
      fitter.Eval();
      fitter.GetParameters(par);
      fitter.GetCovarianceMatrix(mat);
      //result.setChi2(fitter.GetChisquare() / Double_t(npoints));
      result.at(4) = (fitter.GetChisquare() / Double_t(npoints));
    }
    if (TMath::Abs(par[1]) < kTol) {
      throw FitGausError_t::FIT_ERROR;
      ;
    }
    if (TMath::Abs(par[2]) < kTol) {
      throw FitGausError_t::FIT_ERROR;
      ;
    }

    T param1 = T(par[1] / (-2. * par[2]));
    //result.setParameter<1>(param1);
    //result.setParameter<2>(T(1. / TMath::Sqrt(TMath::Abs(-2. * par[2]))));
    result.at(1) = param1;
    result.at(2) = T(1. / TMath::Sqrt(TMath::Abs(-2. * par[2])));
    auto lnparam0 = par[0] + par[1] * param1 + par[2] * param1 * param1;
    if (lnparam0 > 307) {
      throw FitGausError_t::FIT_ERROR;
      ;
    }
    //result.setParameter<0>(TMath::Exp(lnparam0));
    result.at(0) = TMath::Exp(lnparam0);
    return result;
  }

  if (npoints == 2) {
    //use center of gravity for 2 points
    meanCOG /= sumCOG;
    rms2COG /= sumCOG;
    /*
    result.setParameter<0>(xMax);
    result.setParameter<1>(meanCOG);
    result.setParameter<2>(TMath::Sqrt(TMath::Abs(meanCOG * meanCOG - rms2COG)));
    result.setChi2(-2);
    */
    result.at(0) = *xMax;
    result.at(1) = meanCOG;
    result.at(2) = TMath::Sqrt(TMath::Abs(meanCOG * meanCOG - rms2COG));
    result.at(4) = -2;
  }
  if (npoints == 1) {
    meanCOG /= sumCOG;
    /*
    result.setParameter<0>(xMax);
    result.setParameter<1>(meanCOG);
    result.setParameter<2>(binWidth / TMath::Sqrt(12));
    result.setChi2(-1);
    */
    result.at(0) = *xMax;
    result.at(1) = meanCOG;
    result.at(2) = binWidth / TMath::Sqrt(12);
    result.at(4) = -1;
  }
  return result;
}

template <typename valuetype, typename... axes>
std::vector<double> fitBoostHistoWithGaus(boost::histogram::histogram<axes...>& hist)
{
  return fitGaus<valuetype>(hist.begin(), hist.end(), BinCenterView(hist.axis(0).begin()));
}

/// \brief Convert a 1D root histogram to a Boost histogram
auto boosthistoFromRoot_1D(TH1D* inHist1D)
{
  // first setup the proper boost histogram
  int nBins = inHist1D->GetNbinsX();
  int xMin = inHist1D->GetXaxis()->GetXmin();
  int xMax = inHist1D->GetXaxis()->GetXmax();
  const char* title = inHist1D->GetXaxis()->GetTitle();
  auto mHisto = boost::histogram::make_histogram(boost::histogram::axis::regular<>(nBins, xMin, xMax, title));

  // trasfer the acutal values
  for (Int_t x = 1; x < nBins + 1; x++) {
    mHisto.at(x) = inHist1D->GetBinContent(x);
  }
  return mHisto;
}

// \brief Convert a 2D root histogram to a Boost histogram
auto boostHistoFromRoot_2D(TH2D* inHist2D)
{
  // first setup the proper boost histogram
  int nBinsX = inHist2D->GetNbinsX();
  int xMin = inHist2D->GetXaxis()->GetXmin();
  int xMax = inHist2D->GetXaxis()->GetXmax();
  const char* xTitle = inHist2D->GetXaxis()->GetTitle();
  int nBinsY = inHist2D->GetNbinsY();
  int yMin = inHist2D->GetYaxis()->GetXmin();
  int yMax = inHist2D->GetYaxis()->GetXmax();
  const char* yTitle = inHist2D->GetYaxis()->GetTitle();
  auto mHisto = boost::histogram::make_histogram(boost::histogram::axis::regular<>(nBinsX, xMin, xMax, xTitle), boost::histogram::axis::regular<>(nBinsY, yMin,
                                                                                                                                                  yMax, yTitle));
  // trasfer the acutal values
  for (Int_t x = 1; x < nBinsX + 1; x++) {
    for (Int_t y = 1; y < nBinsY + 1; y++) {
      mHisto.at(x, y) = inHist2D->GetBinContent(x, y);
    }
  }
  return mHisto;
}

} // end namespace utils
} // end namespace o2
