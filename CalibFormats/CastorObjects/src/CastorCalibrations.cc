#include "CalibFormats/CastorObjects/interface/CastorCalibrations.h"

/** \class CastorCalibrations
    
    Container for retrieved calibration constants for Castor
   $Author: katsas
   $Date: 2008/03/04 10:01:27 $
   $Revision: 1.1 $
*/

CastorCalibrations::CastorCalibrations (const float fGain [4], const float fPedestal [4]) {
  int iCap = 4;
  while (--iCap >= 0) {
    mGain [iCap] = fGain [iCap];
    mPedestal [iCap] = fPedestal [iCap];
  }
}
