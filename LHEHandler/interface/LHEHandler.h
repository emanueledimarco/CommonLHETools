/** \class LHEHandler
 *
 *
 *  \author N. Amapane - Torino
 *  \author U. Sarica - UCSB/JHU
 *  \author H. Roskes - JHU
 *
 *
 */
#ifndef LHEHADNLER_H
#define LHEHANDLER_H

#include <DataFormats/Common/interface/Handle.h>
#include <SimDataFormats/GeneratorProducts/interface/LHERunInfoProduct.h>
#include <SimDataFormats/GeneratorProducts/interface/LHEEventProduct.h>
#include <JHUGenMELA/MELA/interface/Mela.h>
#include <MelaAnalytics/EventContainer/interface/MELAEvent.h>

#include <vector>
#include <cmath>


class LHEHandler{
protected:
  static int maxlines_print_header;

public:
  enum KinematicsMode{
    noKinematics=0,
    doBasicKinematics=1,
    doHiggsKinematics=2
  };
  enum PDFChoice{
    keepDefaultPDF=0,
    tryNNPDF30,
    tryNNPDF31
  };
  enum QCDOrderChoice{
    keepDefaultQCDOrder=0,
    tryLO,
    tryNLO,
    tryNNLO
  };
  enum RunMode{
    CMS_Run2_preUL,
    CMS_Run2_UL
  };

  LHEHandler(MELAEvent::CandidateVVMode VVMode_, int VVDecayMode_, LHEHandler::KinematicsMode doKinematics_, int year_, LHEHandler::PDFChoice pdfChoice_, LHEHandler::QCDOrderChoice orderChoice_, LHEHandler::RunMode runMode_);
  LHEHandler(edm::Handle<LHEEventProduct>* lhe_evt_, MELAEvent::CandidateVVMode VVMode_, int VVDecayMode_, LHEHandler::KinematicsMode doKinematics_, int year_, LHEHandler::PDFChoice pdfChoice_, LHEHandler::QCDOrderChoice orderChoice_, LHEHandler::RunMode runMode_);
  virtual ~LHEHandler();

  void setHandle(edm::Handle<LHEEventProduct>* lhe_evt_);
  void setHeaderFromRunInfo(edm::Handle<LHERunInfoProduct>* lhe_run);
  void extract();
  void clear();

  MELAEvent* getEvent();
  MELACandidate* getBestCandidate();
  float const& getLHEOriginalWeight() const; // Weight written in the <event> block
  float const& getMemberZeroWeight() const; // Weight from POWHEG before JHUGen reweighting, taken from alternate weight 1001. If there are no alternative weights, this is the same as the LHEOriginalWeight.
  float const& getWeightRescale() const; // The weight rescale accounts for your choice of the PDFs relative to the nominal PDF used in MC generation. Nominal weight should be getLHEOriginalWeight() * getWeightRescale().
  float getLHEWeight(unsigned int whichWeight, float defaultValue=1) const; // = {Weights written in LHE weight variations} / getLHEOriginalWeight()
  float getLHEWeight_PDFVariationUpDn(char const& whichUpDn, float defaultValue=1) const; // = {Weights written in LHE weight variations} / getLHEOriginalWeight()
  float getLHEWeigh_AsMZUpDn(char const& whichUpDn, float defaultValue=1) const; // = {Weights written in LHE weight variations} / getLHEOriginalWeight()
  float const& getPDFScale() const;
  bool hasHeader() const;
  std::vector<std::string> const& getHeader() const;
  std::vector<MELAParticle*> const& getParticleList() const;

  // Misc. functions needed for ordering the PDF weights
  static bool compareAbsIsLess(float const& val1, float const& val2);
  static void suppressLargeWeights(std::vector<float>& wgt_array);
  static float findNearestOneSigma(float const& ref, char const& lowhigh, std::vector<float> const& wgt_array);
  static float safeDivide(float const& numerator, float const& denominator){ return (!(std::isfinite(numerator) && std::isfinite(denominator)) || denominator==0.f ? 0.f : numerator/denominator); }

  // Misc. functions for the operation of printouts
  static void set_maxlines_print_header(int nlines){ LHEHandler::maxlines_print_header=nlines; }
  void printHeader(bool error) const;

protected:
  enum PDFVariationMode{
    useNone=0,
    useMC=1,
    useHessian=2,
    useAlternativePDFs=3
  };
  enum AlternateWeightsType{
    unknown,
    powheg,
    madgraph_0offset,
    madgraph_1000offset
  };

  // VVMode and VVDecayMode: See comment lines within MELAEvent::constructVVCandidates
  const MELAEvent::CandidateVVMode VVMode;
  const int VVDecayMode;
  const LHEHandler::KinematicsMode doKinematics;
  const int year;

  // These options influence which pdf sets/members are selected
  PDFChoice pdfChoice;
  QCDOrderChoice orderChoice;
  RunMode runMode;

  edm::Handle<LHEEventProduct>* lhe_evt;
  std::vector<MELAParticle*> particleList;
  MELAEvent* genEvent;
  MELACandidate* genCand;

  float defaultMemberZeroWeight;
  float defaultWeightScale;
  float LHEOriginalWeight;
  std::vector<float> LHEWeight;
  std::vector<float> LHEWeight_PDFVariationUpDn;
  std::vector<float> LHEWeight_AsMZUpDn;
  std::vector<int> PDFid;
  float PDFScale;

  std::vector<std::string> LHEHeader;

  void readEvent();

  bool check_Run2_2016_preULconfig() const;
  bool check_Run2_201718_preULconfig() const;
  bool check_Run2_20161718_ULconfig() const;

  // Include functions for exceptional cases from a separate file here
  // so that these functions are members of the LHEHandler class.
#include "LHEHandler_ExceptionalCases.h"

};


#endif
