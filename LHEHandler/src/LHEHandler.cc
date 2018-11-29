#include <FWCore/MessageLogger/interface/MessageLogger.h>
#include <FWCore/Utilities/interface/EDMException.h>
#include <MelaAnalytics/EventContainer/interface/HiggsComparators.h>
#include <iomanip>
#include <iostream>
#include <cstdlib>
#include <vector>
#include <string>
#include "TLorentzVector.h"
#include "TString.h"
#include "LHEHandler.h"

typedef std::vector<int> vectorInt;
typedef std::vector<std::pair<int, int>> vectorIntPair;

using namespace std;
using namespace PDGHelpers;


//if this is false, for 2017/18 MC, reweight from the default PDF, NNPDF31_nnlo_hessian_pdfas, to NNPDF31_nlo_hessian_pdfas and its variations
//                               and use the hessian  method to get the systematics.
//if this is true,               reweight                                                   to NNPDF30_nlo_nf_5_pdfas    and its variations
//                               and use the replicas method to get the systematics.
//FIXME: only applies to POWHEG samples, not to madgraph.  Madgraph is always reweighted to NNPDF31_nlo_hessian_pdfas.
//(LO samples like JHUGen and MCFM only have one pdf, NNPDF31_lo_as_0130.  Phantom only has NNPDF31_nnlo_hessian_pdfas.)
const constexpr static bool useNNPDF30 = true;

LHEHandler::LHEHandler(edm::Handle<LHEEventProduct>* lhe_evt_, int VVMode_, int VVDecayMode_, LHEHandler::KinematicsMode doKinematics_, int year_, LHEHandler::PDFChoice pdfChoice_, LHEHandler::QCDOrderChoice orderChoice_) :
VVMode(VVMode_),
VVDecayMode(VVDecayMode_),
doKinematics(doKinematics_),
year(year_),
pdfChoice(pdfChoice_),
orderChoice(orderChoice_),
genEvent(nullptr),
genCand(nullptr)
{
  setHandle(lhe_evt_); // Also calls clear()
  extract();
}
LHEHandler::LHEHandler(int VVMode_, int VVDecayMode_, LHEHandler::KinematicsMode doKinematics_, int year_, LHEHandler::PDFChoice pdfChoice_, LHEHandler::QCDOrderChoice orderChoice_) :
VVMode(VVMode_),
VVDecayMode(VVDecayMode_),
doKinematics(doKinematics_),
year(year_),
pdfChoice(pdfChoice_),
orderChoice(orderChoice_),
genEvent(nullptr),
genCand(nullptr)
{
  clear();
}
LHEHandler::~LHEHandler(){ clear(); }


void LHEHandler::setHandle(edm::Handle<LHEEventProduct>* lhe_evt_){ clear(); lhe_evt=lhe_evt_; }
void LHEHandler::clear(){
  lhe_evt=nullptr;

  if (genEvent){ delete genEvent; genEvent=nullptr; }
  genCand=nullptr;

  for (MELAParticle*& tmpPart:particleList){ if (tmpPart) delete tmpPart; }
  particleList.clear();

  LHEWeight.clear();
  LHEWeight_PDFVariationUpDn.clear();
  LHEWeight_AsMZUpDn.clear();
  PDFid.clear();
  PDFScale=0;
  LHEOriginalWeight=1;
  powhegOriginalWeight=1;
  defaultNLOweight=1;
}

MELACandidate* LHEHandler::getBestCandidate(){ return genCand; }
float const& LHEHandler::getLHEOriginalWeight() const{ return this->LHEOriginalWeight; }
float const& LHEHandler::getPowhegOriginalWeight() const{ return this->powhegOriginalWeight; }
float LHEHandler::getLHEWeight(unsigned int whichWeight, float defaultValue) const{
  if (whichWeight<LHEWeight.size()) return LHEWeight.at(whichWeight);
  else return defaultValue;
}
float LHEHandler::getLHEWeight_PDFVariationUpDn(int whichUpDn, float defaultValue) const{
  if (whichUpDn<0 && LHEWeight_PDFVariationUpDn.size()>1) return LHEWeight_PDFVariationUpDn.at(1);
  else if (whichUpDn>0 && LHEWeight_PDFVariationUpDn.size()>0) return LHEWeight_PDFVariationUpDn.at(0);
  else return defaultValue;
}
float LHEHandler::getLHEWeigh_AsMZUpDn(int whichUpDn, float defaultValue) const{
  if (whichUpDn<0 && LHEWeight_AsMZUpDn.size()>1) return LHEWeight_AsMZUpDn.at(1);
  else if (whichUpDn>0 && LHEWeight_AsMZUpDn.size()>0) return LHEWeight_AsMZUpDn.at(0);
  else return defaultValue;
}
float const& LHEHandler::getPDFScale() const{ return PDFScale; }
float LHEHandler::reweightNNLOtoNLO() const{
  if (year == 2017 || year == 2018) return defaultNLOweight; //note this is already divided by originalPowhegWeight
  throw cms::Exception("LHEWeights") << "Shouldn't be calling this function for " << year;
}

float LHEHandler::getWeightRescale() const{
  if (year == 2016) return 1.f;
  else if (year == 2017 || year == 2018) return this->reweightNNLOtoNLO();
  throw cms::Exception("LHEWeights") << "Year " << year << " not yet implemented in LHEHandler::getWeightRescale";
  return 0.f;
}

void LHEHandler::extract(){
  if (!lhe_evt) cerr << "LHEHandler::extract: lhe_evt==0" << endl;
  else if (!lhe_evt->isValid()) cerr << "LHEHandler::extract: lhe_evt invalid!" << endl;
  else{

    readEvent();

    genCand=nullptr;
    genEvent=nullptr;
    if (doKinematics>=doBasicKinematics){

      genEvent = new MELAEvent();
      vectorInt writtenGenCandIndex;

      {
        int p=0;
        for (MELAParticle* genPart:particleList){
          if (doKinematics==doHiggsKinematics && isAHiggs(genPart->id)){
            writtenGenCandIndex.push_back(p);
            if (VVMode==-1 && (genPart->genStatus==1 || genPart->genStatus==2)) genEvent->addIntermediate(genPart);
          }
          if (genPart->genStatus==1){
            if (isALepton(genPart->id)) genEvent->addLepton(genPart);
            else if (isANeutrino(genPart->id)) genEvent->addNeutrino(genPart);
            else if (isAPhoton(genPart->id)) genEvent->addPhoton(genPart);
            else if (isAGluon(genPart->id) || isAQuark(genPart->id)) genEvent->addJet(genPart);
          }
          else if (genPart->genStatus==-1) genEvent->addMother(genPart);
          p++;
        }
      }

      if (doKinematics==doHiggsKinematics){
        genEvent->constructVVCandidates(VVMode, VVDecayMode);
        genEvent->addVVCandidateAppendages();
        if (!writtenGenCandIndex.empty()){
          for (const int& iC:writtenGenCandIndex){
            MELACandidate* tmpCand = HiggsComparators::matchAHiggsToParticle(*genEvent, particleList.at(iC));
            if (tmpCand){
              if (!genCand) genCand = tmpCand;
              else genCand = HiggsComparators::candComparator(genCand, tmpCand, HiggsComparators::BestZ1ThenZ2ScSumPt, VVMode);
            }
          }
        }
        if (!genCand) genCand = HiggsComparators::candidateSelector(*genEvent, HiggsComparators::BestZ1ThenZ2ScSumPt, VVMode);
      }

    }

  }
}

void LHEHandler::readEvent(){
  // Particles
  if (doKinematics>=doBasicKinematics){
    const lhef::HEPEUP hepeup_ = (*lhe_evt)->hepeup();
    const int nup = hepeup_.NUP;
    const vectorInt istup = hepeup_.ISTUP;
    const vectorIntPair mothup = hepeup_.MOTHUP;
    const vectorInt idup = hepeup_.IDUP;
    const std::vector<lhef::HEPEUP::FiveVector> pup = hepeup_.PUP;
    vectorInt motherIDs_first;
    vectorInt motherIDs_second;
    for (int ipart = 0; ipart<nup; ipart++){
      SimpleParticle_t simplePart(idup.at(ipart), TLorentzVector(pup.at(ipart)[0], pup.at(ipart)[1], pup.at(ipart)[2], pup.at(ipart)[3]));
      MELAParticle* onePart = new MELAParticle(simplePart.first, simplePart.second);
      onePart->setGenStatus(istup.at(ipart));
      particleList.push_back(onePart);

      motherIDs_first.push_back(mothup.at(ipart).first);
      motherIDs_second.push_back(mothup.at(ipart).second);
    }
    // Link the mothers
    for (int a=0; a<nup; a++){
      if (motherIDs_first.at(a)>0) particleList.at(a)->addMother(particleList.at(motherIDs_first.at(a)-1));
      if (motherIDs_second.at(a)>0 && motherIDs_first.at(a)!=motherIDs_second.at(a)) particleList.at(a)->addMother(particleList.at(motherIDs_second.at(a)-1)); // Refrain from adding the same mothers twice
    }
  }
  //

  // PDF scale
  PDFScale = -1;
  if ((*lhe_evt)->pdf()!=NULL){
    PDFScale = (*lhe_evt)->pdf()->scalePDF;
    PDFid.push_back((*lhe_evt)->pdf()->id.first);
    if (PDFid.at(0) != (*lhe_evt)->pdf()->id.second) PDFid.push_back((*lhe_evt)->pdf()->id.second);
  }
  //

  // LHE weights (Maximum 9 or size of weights array for QCD variations, 2 for PDF variations and 2 for alphas(mZ) variations)
  LHEOriginalWeight = (*lhe_evt)->originalXWGTUP();
  vector<float> LHEPDFAlphaSMZWgt;
  vector<float> LHEPDFVariationWgt;
  bool founddefaultNLOweight = false;
  bool foundpowhegOriginalWeight = false;
  AlternateWeightsType weightstype = unknown;
  powhegOriginalWeight = LHEOriginalWeight;
  //first find the main powheg weight (should be one of the first)
  for (const auto& weight : (*lhe_evt)->weights()){
    if (weight.id == "1001"){
      powhegOriginalWeight = weight.wgt;
      foundpowhegOriginalWeight = true;
      break;
    }
  }

  for (const auto& weight : (*lhe_evt)->weights()){
    int wgtid;
    try{
      wgtid = stoi(weight.id.c_str());
    }
    catch (std::invalid_argument& e){
      continue;  //we don't use non-numerical indices, but they exist in some 2016 MC samples
    }
    float wgtval = safeDivide(weight.wgt, powhegOriginalWeight);
    //cout << "PDF id = " << PDFid.at(0) << " " << wgtid << " -> " << wgtval << endl;
    if (year == 2016){
      if (wgtid<2000) LHEWeight.push_back(wgtval);
      else if (wgtid<3000) LHEPDFVariationWgt.push_back(wgtval); // Add PDF replicas and alphas(mZ) variations from the same pdf
    }
    else if (year == 2017 || year == 2018){
      //Madgraph 0 offset
      if (weightstype == unknown && wgtid == 1){ weightstype = madgraph_0offset; LHEWeight.push_back(wgtval); }
      else if (weightstype == madgraph_0offset && 2 <= wgtid && wgtid <= 9) LHEWeight.push_back(wgtval);
      else if (weightstype == madgraph_0offset && 10 <= wgtid && wgtid <= 120){/*do nothing, these are the NNLO variations*/ }
      else if (weightstype == madgraph_0offset && wgtid == 121){ founddefaultNLOweight = true; defaultNLOweight = wgtval; }
      else if (weightstype == madgraph_0offset && 122 <= wgtid && wgtid <= 223) LHEPDFVariationWgt.push_back(wgtval);
      else if (weightstype == madgraph_0offset && 224 <= wgtid && wgtid <= 1080){/*do nothing, these are other various weights*/ }

      //QCD variations for all the other weightstypes
      else if (weightstype == unknown && 1001 <= wgtid && wgtid <= 1009) LHEWeight.push_back(wgtval);

      //Madgraph 1000 offset
      else if (wgtid == 1010 && weightstype == unknown) weightstype = madgraph_1000offset;
      else if (weightstype == madgraph_1000offset && 1011 <= wgtid && wgtid <= 1120){/*do nothing, these are the NNLO variations*/ }
      else if (weightstype == madgraph_1000offset && wgtid == 1121){ founddefaultNLOweight = true; defaultNLOweight = wgtval; }
      else if (weightstype == madgraph_1000offset && 1122 <= wgtid && wgtid <= 1223) LHEPDFVariationWgt.push_back(wgtval);
      else if (weightstype == madgraph_1000offset && 1224 <= wgtid && wgtid <= 2080){/*do nothing, these are other various weights*/ }

      //powheg
      else if (weightstype == unknown && wgtid == 2000){ weightstype = powheg; /*but do nothing, this is an NNLO variation*/ }
      else if (weightstype == powheg && 1500 <= wgtid && wgtid <= 1602){ //these are the NLO pdf for NNPDF30 and variations
        if (useNNPDF30){
          if (wgtid == 1500){ founddefaultNLOweight = true; defaultNLOweight = wgtval; }
          else LHEPDFVariationWgt.push_back(wgtval);
        }
        //else do nothing
      }

      else if (weightstype == powheg && wgtid == 1700)                  {/*do nothing, this is the NNLO pdf for NNPDF30*/ }
      else if (weightstype == powheg && (wgtid == 1800 || wgtid == 1850 || wgtid == 1900 || wgtid == 1950)){/*do nothing, these are LO pdfs*/ }
      else if (weightstype == powheg && 2001 <= wgtid && wgtid <= 2111){/*do nothing, these are more NNLO variations*/ }
      else if (weightstype == powheg && wgtid >= 4000)                  {/*do nothing, these are other various weights*/ }

      else if (weightstype == powheg && 3000 <= wgtid && wgtid <= 3102){ //these are the NLO pdf for NNPDF31 and variations
        if (!useNNPDF30){
          if (wgtid == 3000){ founddefaultNLOweight = true; defaultNLOweight = wgtval; }
          else LHEPDFVariationWgt.push_back(wgtval);
        }
      }

      else throw cms::Exception("LHEWeights") << "Don't know what to do with alternate weight id = " << wgtid << "(weightstype == " << weightstype << ")";
    }
    else throw cms::Exception("LHEWeights") << "Unknown year " << year;
  }

  if ((year == 2017 || year == 2018) && !(*lhe_evt)->weights().empty() && !(foundpowhegOriginalWeight && founddefaultNLOweight && LHEWeight.size() == 9 && LHEPDFVariationWgt.size() == 102)){
    throw cms::Exception("LHEWeights")
      << "For " << year << " MC, expect to find either\n"
      << " - no alternate LHE weights, or\n"
      << " - all of the following:\n"
      << "   - muR and muF variations (1001-1009, found " << LHEWeight.size() << " of them, " << (foundpowhegOriginalWeight ? "" : "not ") << "including 1001)\n"
      << "   - the default NLO PDF weight (3000, " << (founddefaultNLOweight ? "found" : "didn't find") << " it)\n"
      << "   - NLO PDF weight variations (3001-3102, found " << LHEPDFVariationWgt.size() << " of them)";
  }

  if ((year == 2017 || year == 2018) && weightstype == madgraph_1000offset){   //but not 0offset!  0offset does it the same way as powheg
    LHEWeight ={ LHEWeight[0], LHEWeight[3], LHEWeight[6],   //note LHEWeight[8] is always defined here,
                 LHEWeight[1], LHEWeight[4], LHEWeight[7],   //because weightstype != unknown implies that there are weights present
                 LHEWeight[2], LHEWeight[5], LHEWeight[8] }; //so if LHEweight.size() != 9 you already got an exception
  }                                                          //in the previous block

  // Handle LO samples 
  if (year == 2016 && LHEPDFVariationWgt.empty()){
    for (int iw=0; iw<(int) ((*lhe_evt)->weights().size()); iw++){
      int wgtid;
      try{
        wgtid = stoi((*lhe_evt)->weights().at(iw).id.c_str());
      }
      catch (std::invalid_argument& e){
        continue;  //we don't use non-numerical indices, but they exist in some 2016 MC samples
      }
      if (wgtid>=10 && wgtid<=110){
        float wgtval=safeDivide((*lhe_evt)->weights().at(iw).wgt, LHEOriginalWeight);
        LHEPDFVariationWgt.push_back(wgtval);
      }
    }
  }

  if (LHEPDFVariationWgt.size() > 100){
    auto firstalphasweight = LHEPDFVariationWgt.begin() + 100; //= iterator to LHEPDFVariationWgt[100] = 101st entry
    LHEPDFAlphaSMZWgt.assign(firstalphasweight, LHEPDFVariationWgt.end());
    LHEPDFVariationWgt.erase(firstalphasweight, LHEPDFVariationWgt.end());
  }

  // Find the proper PDF and alphas(mZ) variations
  if (!LHEWeight.empty()){
    if (year == 2016 || ((year == 2017 || year == 2018) && useNNPDF30)){
      float centralWeight, divideby;
      if (year == 2016){
        centralWeight = LHEWeight.at(0);
        divideby = 1;
      }
      else if (year == 2017 || year == 2018){
        centralWeight = defaultNLOweight;
        divideby = this->reweightNNLOtoNLO();
      }
      else throw cms::Exception("LHEWeights") << "Unknown year " << year;

      // LHE weight suppresssion not needed
      //suppressLargeWeights(LHEPDFVariationWgt);
      std::sort(LHEPDFVariationWgt.begin(), LHEPDFVariationWgt.end(), compareAbsIsLess);
      LHEWeight_PDFVariationUpDn.push_back(safeDivide(findNearestOneSigma(centralWeight, 1, LHEPDFVariationWgt), divideby));
      LHEWeight_PDFVariationUpDn.push_back(safeDivide(findNearestOneSigma(centralWeight, -1, LHEPDFVariationWgt), divideby));
      if (LHEPDFAlphaSMZWgt.size()>1){
        float asdn = LHEPDFAlphaSMZWgt.at(0);
        float asup = LHEPDFAlphaSMZWgt.at(1);
        // Rescale alphas(mZ) variations from 0.118+-0.001 to 0.118+-0.0015
        // 0.001 is valid both for the alternates used in 2016 and for NNPDF30_nlo_nf_5_pdfas in 2017/18
        LHEWeight_AsMZUpDn.push_back(safeDivide((centralWeight + (asup-centralWeight)*1.5), divideby));
        LHEWeight_AsMZUpDn.push_back(safeDivide((centralWeight + (asdn-centralWeight)*1.5), divideby));
      }
    }
    else if ((year == 2017 || year == 2018) && !useNNPDF30){
      //https://arxiv.org/pdf/1706.00428v2.pdf page 88
      //we are working with NNPDF31_nlo_hessian_pdfas
      //see the routine starting on line 101 of PDFSet.cc in LHAPDF-6.2.1
      //based on NNPDF31_nlo_hessian_pdfas.info, which confirms that errorType() is symmhessian+as
      float centralWeight = defaultNLOweight;
      if (centralWeight == 0){
        LHEWeight_PDFVariationUpDn = { 0, 0 };
        LHEWeight_AsMZUpDn = { 0, 0 };
        edm::LogWarning warning("ZeroWeight");
        warning << "Default NLO PDF weight is 0\nIncoming particle id and pz:";
        for (const auto& p:particleList) if (p->genStatus==-1) warning << "\n" << p->id << " " << p->z();
      }
      else{
        float errorsquared = 0;
        for (const auto& wt : LHEPDFVariationWgt){
          float difference = wt - centralWeight;
          errorsquared += difference*difference;
        }
        float error = sqrt(errorsquared);
        float divideby = this->reweightNNLOtoNLO();
        LHEWeight_PDFVariationUpDn = { safeDivide((centralWeight + error), divideby), safeDivide((centralWeight - error), divideby) };

        if (LHEPDFAlphaSMZWgt.size()>1){
          float asdn = LHEPDFAlphaSMZWgt.at(0);
          float asup = LHEPDFAlphaSMZWgt.at(1);
          // Rescale alphas(mZ) variations from 0.118+-0.002 to 0.118+-0.0015
          //                           Note this number ^ is different than 2016!
          LHEWeight_AsMZUpDn.push_back(safeDivide((centralWeight + (asup-centralWeight)*0.75), divideby));
          LHEWeight_AsMZUpDn.push_back(safeDivide((centralWeight + (asdn-centralWeight)*0.75), divideby));
        }
      }
    }
    else throw cms::Exception("LHEWeights") << "Unknown year " << year;
  }
}

bool LHEHandler::compareAbsIsLess(float val1, float val2){ return std::abs(val1) < std::abs(val2); }

float LHEHandler::findNearestOneSigma(float ref, int lowhigh, std::vector<float> const& wgt_array){
  int nrep = wgt_array.size();
  int pos_low=-1, pos_high=nrep;
  for (int irep=0; irep<nrep; irep++){ // Assume ordered from low to high in absolute value
    float tmp = fabs(wgt_array.at(irep));
    if (fabs(tmp)<fabs(ref)) pos_low=irep;
    else if (fabs(tmp)>fabs(ref)){
      pos_high=irep;
      break;
    }
  }
  if (lowhigh==-1 && nrep>0){ // Low
    float ninst = pos_low+1;
    if (ninst==0) return ref;
    int nprogress = (ninst*0.6827+0.5); // truncate down
    int taken = ninst-nprogress;
    return wgt_array.at(taken);
  }
  else if (lowhigh==1 && nrep>0){
    float ninst = nrep-pos_high;
    if (ninst==0) return ref;
    int nprogress = (ninst*0.6827+0.5); // truncate down
    int taken = nprogress+pos_high-1;
    return wgt_array.at(taken);
  }
  else return ref;
}

void LHEHandler::suppressLargeWeights(std::vector<float>& wgt_array){
  constexpr float c_wgt_thr=5.f;
  constexpr float c_trunc_min=0.35f;
  constexpr float c_trunc_max=0.7f;
  if (wgt_array.empty()) return;

  std::vector<float> weightListNP[2];
  for (float const& weight:wgt_array){
    if (weight<0.) weightListNP[0].push_back(fabs(weight));
    else if (weight>0.) weightListNP[1].push_back(fabs(weight));
  }

  float weight_meanlow=0;
  float weight_meanhigh=0;
  float weight_thrlow=0;
  float weight_thrhigh=0;
  for (unsigned int inp=0; inp<2; inp++){
    std::vector<float>& weightList = weightListNP[inp];
    float& weight_mean = (inp==0 ? weight_meanlow : weight_meanhigh);
    float& weight_thr = (inp==0 ? weight_thrlow : weight_thrhigh);
    const unsigned int wsize=weightList.size();
    if (wsize>0){
      std::sort(weightList.begin(), weightList.end());
      const unsigned int ibegin=float(wsize)*c_trunc_min;
      const unsigned int iend=float(wsize)*c_trunc_max+1;
      std::vector<float>::const_iterator it_begin=weightList.cbegin()+ibegin;
      std::vector<float>::const_iterator it_end=weightList.cbegin()+iend;
      // Find mean and rms
      for (std::vector<float>::const_iterator it=it_begin; it!=it_end; it++){
        float const& wgt = *it;
        weight_mean += wgt;
      }
      weight_mean /= float(iend-ibegin);
      unsigned int nhigh=0;
      for (std::vector<float>::const_iterator it=it_begin; it!=it_end; it++){
        float const& wgt = *it;
        if (wgt>=weight_mean){ weight_thr += pow(wgt, 2); nhigh++; }
      }
      if (nhigh>0){
        weight_thr /= float(nhigh);
        weight_thr = sqrt(fabs(weight_thr-pow(weight_mean, 2)));
        weight_thr = c_wgt_thr*weight_thr + weight_mean;
      }
      else weight_thr = c_wgt_thr*weight_mean;
      //cout << "Weight threshold [" << inp << "]: " << weight_mean << " +" << weight_thr << "/-0" << endl;
    }
  }
  weight_thrlow = -weight_thrlow;
  weight_meanlow = -weight_meanlow;

  {
    float const diffthrlow = weight_thrlow - weight_meanlow;
    float const diffthrhigh = weight_thrhigh - weight_meanhigh;
    for (float& weight:wgt_array){
      if (weight==0.) continue;
      if (weight<weight_thrlow){
        float weightdiff = weight - weight_meanlow;
        weightdiff = pow(diffthrlow, 2)/weightdiff;
        weight = weightdiff + weight_meanlow;
      }
      else if (weight>weight_thrhigh){
        float weightdiff = weight - weight_meanhigh;
        weightdiff = pow(diffthrhigh, 2)/weightdiff;
        weight = weightdiff + weight_meanhigh;
      }
    }
  }

}
