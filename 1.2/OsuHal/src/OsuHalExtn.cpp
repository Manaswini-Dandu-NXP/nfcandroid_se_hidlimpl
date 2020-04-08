/******************************************************************************
 *
 *  Copyright 2020 NXP
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 ******************************************************************************/
#include "OsuHalExtn.h"

#define LOG_TAG "OsuHalExtn"
const static hidl_vec<uint8_t> OSU_AID = {0x4F, 0x70, 0x80, 0x13, 0x04,
                                          0xDE, 0xAD, 0xBE, 0xEF};
const static uint8_t defaultSelectAid[] = {0x00, 0xA4, 0x04, 0x00, 0x00};
/*
 * INIT :- Will return OSU ongoing
 *
 * OPENBASIC :- Will set OSU mode ongoing by comparing AID
 *
 * OPNELOGICAL :- Will return OSU state
 *
 * CLOSE :- If Channel 0 reset OSU state
 *
 * TRANSMIT :- Check CLA if non-zero in OSU mode return
 *
 * */
OsuHalExtn::OsuApduMode OsuHalExtn::isOsuMode(const hidl_vec<uint8_t>& evt,
                                              uint8_t type,
                                              phNxpEse_data* pCmdData) {
  OsuApduMode osuSubState = (isAppOSUMode ? OSU_PROP_MODE : NON_OSU_MODE);
  checkAndUpdateOsuMode();

  switch (type) {
    case INIT:
      break;
    case OPENBASIC:
      if (!memcmp(&evt[0], &OSU_AID[0], OSU_AID.size())) {
        isAppOSUMode = true;
        osuSubState = OSU_PROP_MODE;
        LOG(ERROR) << "Dedicated mode is set !!!!!!!!!!!!!!!!!";
      } else if (isOsuMode()) {
        osuSubState = OSU_GP_MODE;
        LOG(ERROR) << "Non OSU AID Not allowed";
      } else {
      }
      break;
    case TRANSMIT:
      memcpy(pCmdData->p_data, evt.data(), evt.size());
      if (isOsuMode()) {
        osuSubState =
            checkTransmit(pCmdData->p_data, evt.size(), &pCmdData->len);
      } else {
        pCmdData->len = evt.size();
        osuSubState = OSU_GP_MODE;
      }
      break;
  }
  return osuSubState;
}

bool OsuHalExtn::isOsuMode(uint8_t type, uint8_t channel) {
  LOG(ERROR) << "Enter OSUMode2";
  checkAndUpdateOsuMode();
  switch (type) {
    case CLOSE:
      if (channel == ISO7816_BASIC_CHANNEL) {
        isAppOSUMode = false;
        isJcopOSUMode = false;
        LOG(ERROR) << "Setting to normal mode!!!";
      }
      break;
    case RESET:
      break;
    case INIT:
      break;
  }
  return isOsuMode();
}

void OsuHalExtn::checkAndUpdateOsuMode() {
  isJcopOSUMode = (phNxpEse_GetOsMode() == OSU_MODE);
}

OsuHalExtn& OsuHalExtn::getInstance() {
  static OsuHalExtn manager;
  return manager;
}
OsuHalExtn::OsuHalExtn() noexcept {
  isAppOSUMode = false;
  isJcopOSUMode = false;
}
OsuHalExtn::~OsuHalExtn() {}

bool OsuHalExtn::isOsuMode() { return (isAppOSUMode || isJcopOSUMode); }
OsuHalExtn::OsuApduMode OsuHalExtn::checkTransmit(uint8_t* input, size_t length,
                                                  uint32_t* outLength) {
  OsuHalExtn::OsuApduMode halMode = NON_OSU_MODE;
  if ((*input & ISO7816_CLA_CHN_MASK) != ISO7816_BASIC_CHANNEL ||
      (!memcmp(input, defaultSelectAid, length) && length == 5 &&
       isJcopOSUMode)) {
    phNxpEse_free(input);
    input = nullptr;
    halMode = NON_OSU_MODE;
  } else if (*input == OSU_PROP_CLA && *(input + 1) == OSU_PROP_INS &&
             *(input + 2) != OSU_PROP_RST_P1) {
    LOG(ERROR) << "checkTransmit in OSU_PROP_MODE";
    if( *(input + 4) != 0) {
      *outLength = length - 5;
      memcpy(input, input + 5, length - 5);
    } else {
      *outLength = length - 7;
      memcpy(input, input + 7, length - 7);
    }
    halMode = OSU_PROP_MODE;
  } else if (*input == OSU_PROP_CLA && *(input + 1) == OSU_PROP_INS &&
             *(input + 2) == OSU_PROP_RST_P1) {
    LOG(ERROR) << "checkTransmit in OSU_PROP_RST_INS";
    if (phNxpEse_SetEndPoint_Cntxt(0) != ESESTATUS_SUCCESS) {
      LOG(ERROR) << "phNxpEse_SetEndPoint_Cntxt failed!!!";
    }
    phNxpEse_resetJcopUpdate();
    if (phNxpEse_ResetEndPoint_Cntxt(0) !=  ESESTATUS_SUCCESS) {
      LOG(ERROR) << "phNxpEse_ResetEndPoint_Cntxt failed!!!";
    }
    phNxpEse_free(input);
    input = nullptr;
    halMode = OSU_RST_MODE;
  } else {
    *outLength = length;
    halMode = OSU_GP_MODE;
  }
  return halMode;
}
