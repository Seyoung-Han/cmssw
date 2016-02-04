// $Id: SoapUtils.h,v 1.3 2009/07/20 13:06:11 mommsen Exp $
/// @file: SoapUtils.h 

#ifndef StorageManager_SoapUtils_h
#define StorageManager_SoapUtils_h

#include "xdaq/Application.h"
#include "xoap/MessageReference.h"

#include <string>

namespace stor {

  namespace soaputils {

    /**
     * Collection of utility functions for handling SOAP messages
     *
     * $Author: mommsen $
     * $Revision: 1.3 $
     * $Date: 2009/07/20 13:06:11 $
     */

    /**
     * Extract parameters and FSM command from SOAP message
     */
    std::string extractParameters
    (
      xoap::MessageReference,
      xdaq::Application*
    );

    /**
     * Create a SOAP FSM response message
     */
    xoap::MessageReference createFsmSoapResponseMsg
    (
      const std::string commandName,
      const std::string currentState
    );

  } // namespace soaputils
  
} // namespace stor

#endif // StorageManager_SoapUtils_h 


/// emacs configuration
/// Local Variables: -
/// mode: c++ -
/// c-basic-offset: 2 -
/// indent-tabs-mode: nil -
/// End: -
