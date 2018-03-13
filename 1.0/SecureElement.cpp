/******************************************************************************
 *
 *  Copyright 2018 NXP
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
#include "SecureElement.h"
#include <android-base/logging.h>
#include "phNxpEse_Apdu_Api.h"
#include "phNxpEse_Api.h"
#include "LsClient.h"
#include <android-base/stringprintf.h>

/* Mutex to synchronize multiple transceive */
ThreadMutex sLock;

namespace android {
namespace hardware {
namespace secure_element {
namespace V1_0 {
namespace implementation {

#define LOG_TAG "nxpese@1.0-service"

#define DEFAULT_BASIC_CHANNEL 0x00
#define MAX_LOGICAL_CHANNELS 0x04

typedef struct gsTransceiveBuffer {
  phNxpEse_data cmdData;
  phNxpEse_data rspData;
  hidl_vec<uint8_t>* pRspDataBuff;
} sTransceiveBuffer_t;

static sTransceiveBuffer_t gsTxRxBuffer;
static hidl_vec<uint8_t> gsRspDataBuff(256);
static android::sp<ISecureElementHalCallback> cCallback;
SecureElement::SecureElement()
    : mOpenedchannelCount(0),
      mIsEseInitialized(false),
      mOpenedChannels{false, false, false, false} {}


Return<void> SecureElement::init(
    const sp<
        ::android::hardware::secure_element::V1_0::ISecureElementHalCallback>&
        clientCallback) {
  ESESTATUS status = ESESTATUS_SUCCESS;
  phNxpEse_initParams initParams;
  gsTxRxBuffer.pRspDataBuff = &gsRspDataBuff;
  memset(&initParams, 0x00, sizeof(phNxpEse_initParams));
  initParams.initMode = ESE_MODE_NORMAL;

  if (clientCallback == nullptr) {
    return Void();
  } else {
    clientCallback->linkToDeath(this, 0 /*cookie*/);
  }
  if (mIsEseInitialized) {
    clientCallback->onStateChange(true);
    return Void();
  }

  status = phNxpEse_open(initParams);
  if (status != ESESTATUS_SUCCESS) {
    //return Void();
  }

  status = phNxpEse_SetEndPoint_Cntxt(0);
  if (status != ESESTATUS_SUCCESS) {
    //return Void(); TODO
  }
  status = phNxpEse_init(initParams);
  if (status != ESESTATUS_SUCCESS) {
    //return Void(); TODO
  }
  status = phNxpEse_ResetEndPoint_Cntxt(0);
  if (status != ESESTATUS_SUCCESS) {
    //return Void(); TODO
  }
  mIsEseInitialized = true;
  LOG(ERROR) << "Mr Robot says ESE SPI init complete !!!";
  if (status == ESESTATUS_SUCCESS) clientCallback->onStateChange(true);
  cCallback = clientCallback;
  return Void();
}

Return<void> SecureElement::getAtr(getAtr_cb _hidl_cb) {
  hidl_vec<uint8_t> response;
  _hidl_cb(response);
  return Void();
}

Return<bool> SecureElement::isCardPresent() { return true; }

Return<void> SecureElement::transmit(const hidl_vec<uint8_t>& data,
                                     transmit_cb _hidl_cb) {
  ESESTATUS status = ESESTATUS_FAILED;
  phNxpEse_memset(&gsTxRxBuffer.cmdData, 0x00, sizeof(phNxpEse_data));
  phNxpEse_memset(&gsTxRxBuffer.rspData, 0x00, sizeof(phNxpEse_data));
  gsTxRxBuffer.cmdData.len = data.size();
  gsTxRxBuffer.cmdData.p_data =
      (uint8_t*)phNxpEse_memalloc(data.size() * sizeof(uint8_t));

  memcpy(gsTxRxBuffer.cmdData.p_data, data.data(), gsTxRxBuffer.cmdData.len);
    LOG(ERROR) << "Mr Robot acquiring lock SPI";
  AutoThreadMutex a(sLock);
    LOG(ERROR) << "Mr Robot acquired lock for SPI";
  status = phNxpEse_SetEndPoint_Cntxt(0);
  if (status != ESESTATUS_SUCCESS) {
    //return Void(); TODO
  }
  status =
      phNxpEse_Transceive(&gsTxRxBuffer.cmdData, &gsTxRxBuffer.rspData);

  hidl_vec<uint8_t> result;
  if (status != ESESTATUS_SUCCESS) {
    LOG(ERROR) << "transmit failed!!!";
  } else {
    result.resize(gsTxRxBuffer.rspData.len);
    memcpy(&result[0], gsTxRxBuffer.rspData.p_data, gsTxRxBuffer.rspData.len);
  }
  status = phNxpEse_ResetEndPoint_Cntxt(0);
  if (status != ESESTATUS_SUCCESS) {
    //return Void(); TODO
  }


  _hidl_cb(result);
  phNxpEse_free(gsTxRxBuffer.cmdData.p_data);

  return Void();
}

Return<void> SecureElement::openLogicalChannel(const hidl_vec<uint8_t>& aid,
                                               uint8_t p2,
                                               openLogicalChannel_cb _hidl_cb) {
  hidl_vec<uint8_t> manageChannelCommand = {0x00, 0x70, 0x00, 0x00, 0x01};

  LogicalChannelResponse resApduBuff;
  resApduBuff.channelNumber = 0xff;
  memset(&resApduBuff, 0x00, sizeof(resApduBuff));
  if (!mIsEseInitialized) {
    ESESTATUS status = seHalInit();
    if (status != ESESTATUS_SUCCESS) {
      LOG(ERROR) << "%s: seHalInit Failed!!!"<< __func__;
      _hidl_cb(resApduBuff, SecureElementStatus::IOERROR);
      return Void();
    }
  }

  SecureElementStatus sestatus = SecureElementStatus::IOERROR;
  ESESTATUS status = ESESTATUS_FAILED;
  phNxpEse_data cmdApdu;
  phNxpEse_data rspApdu;

  phNxpEse_memset(&cmdApdu, 0x00, sizeof(phNxpEse_data));
  phNxpEse_memset(&rspApdu, 0x00, sizeof(phNxpEse_data));

  cmdApdu.len = manageChannelCommand.size();
  cmdApdu.p_data = (uint8_t*)phNxpEse_memalloc(manageChannelCommand.size() *
                                               sizeof(uint8_t));
  memcpy(cmdApdu.p_data, manageChannelCommand.data(), cmdApdu.len);

  LOG(ERROR) << "Mr Robot Open Logical Channel from SPI!!!";
  LOG(ERROR) << "Robot about to acquire lock from SPI";
  AutoThreadMutex a(sLock);
  LOG(ERROR) << "Robot acquired the lock from SPI";
  status = phNxpEse_SetEndPoint_Cntxt(0);
  if (status != ESESTATUS_SUCCESS) {
    //return Void(); TODO
  }
  status = phNxpEse_Transceive(&cmdApdu, &rspApdu);
  if (status != ESESTATUS_SUCCESS) {
    resApduBuff.channelNumber = 0xff;
    if (rspApdu.len > 0 &&
        (rspApdu.p_data[0] == 0x64 && rspApdu.p_data[1] == 0xFF)) {
      sestatus = SecureElementStatus::IOERROR;
    }
    _hidl_cb(resApduBuff, sestatus);
    phNxpEse_free(cmdApdu.p_data);
    return Void();
  } else if (rspApdu.p_data[rspApdu.len - 2] == 0x6A &&
             rspApdu.p_data[rspApdu.len - 1] == 0x81) {
    resApduBuff.channelNumber = 0xff;
    sestatus = SecureElementStatus::CHANNEL_NOT_AVAILABLE;
    _hidl_cb(resApduBuff, sestatus);
    phNxpEse_free(cmdApdu.p_data);
    return Void();
  } else if (rspApdu.p_data[rspApdu.len - 2] == 0x90 &&
             rspApdu.p_data[rspApdu.len - 1] == 0x00) {
    resApduBuff.channelNumber = rspApdu.p_data[0];
    mOpenedchannelCount++;
    mOpenedChannels[resApduBuff.channelNumber] = true;
    sestatus = SecureElementStatus::SUCCESS;
  }else if (((rspApdu.p_data[rspApdu.len - 2] == 0x6E) ||
            (rspApdu.p_data[rspApdu.len - 2] == 0x6D)) &&
             rspApdu.p_data[rspApdu.len - 1] == 0x00) {
    sestatus = SecureElementStatus::UNSUPPORTED_OPERATION;
  }
  /*Free the allocations*/
  phNxpEse_free(cmdApdu.p_data);
  phNxpEse_free(rspApdu.p_data);

  if (sestatus != SecureElementStatus::SUCCESS) {
    /*If manageChanle is failed in any of above cases
    send the callback and return*/
    _hidl_cb(resApduBuff, sestatus);
    return Void();
  }
  LOG(INFO) << "openLogicalChannel Sending selectApdu";
  sestatus = SecureElementStatus::IOERROR;
  status = ESESTATUS_FAILED;

  phNxpEse_7816_cpdu_t cpdu;
  phNxpEse_7816_rpdu_t rpdu;
  phNxpEse_memset(&cpdu, 0x00, sizeof(phNxpEse_7816_cpdu_t));
  phNxpEse_memset(&rpdu, 0x00, sizeof(phNxpEse_7816_rpdu_t));

  cpdu.cla = resApduBuff.channelNumber; /* Class of instruction */
  cpdu.ins = 0xA4;                      /* Instruction code */
  cpdu.p1 = 0x04;                       /* Instruction parameter 1 */
  cpdu.p2 = p2;                         /* Instruction parameter 2 */
  cpdu.lc = aid.size();
  cpdu.le_type = 0x01;
  cpdu.pdata = (uint8_t*)phNxpEse_memalloc(aid.size() * sizeof(uint8_t));
  memcpy(cpdu.pdata, aid.data(), cpdu.lc);
  cpdu.le = 256;

  rpdu.len = 0x02;
  rpdu.pdata = (uint8_t*)phNxpEse_memalloc(cpdu.le * sizeof(uint8_t));

  
  status = phNxpEse_7816_Transceive(&cpdu, &rpdu);
  
  
  if (status != ESESTATUS_SUCCESS) {
    /*Transceive failed*/
    if (rpdu.len > 0 && (rpdu.sw1 == 0x64 && rpdu.sw2 == 0xFF)) {
      _hidl_cb(resApduBuff, SecureElementStatus::IOERROR);
    } else {
      _hidl_cb(resApduBuff, SecureElementStatus::FAILED);
    }
  } else {
    /*Status word to be passed as part of response
    So include additional length*/
    uint16_t responseLen = rpdu.len + 2;
    resApduBuff.selectResponse.resize(responseLen);
    memcpy(&resApduBuff.selectResponse[0], rpdu.pdata, rpdu.len);
    resApduBuff.selectResponse[responseLen - 1] = rpdu.sw2;
    resApduBuff.selectResponse[responseLen - 2] = rpdu.sw1;

    /*Status is success*/
    if (rpdu.sw1 == 0x90 && rpdu.sw2 == 0x00) {
      sestatus = SecureElementStatus::SUCCESS;
    }
    /*AID provided doesn't match any applet on the secure element*/
    else if (rpdu.sw1 == 0x67 && rpdu.sw2 == 0x00) {
      sestatus = SecureElementStatus::NO_SUCH_ELEMENT_ERROR;
    }
    /*Operation provided by the P2 parameter is not permitted by the applet.*/
    else if (rpdu.sw1 == 0x6A && rpdu.sw2 == 0x86) {
      sestatus = SecureElementStatus::UNSUPPORTED_OPERATION;
    } else {
      sestatus = SecureElementStatus::FAILED;
    }
    _hidl_cb(resApduBuff, sestatus);
  }
  status = phNxpEse_ResetEndPoint_Cntxt(0);
  if (status != ESESTATUS_SUCCESS) {
    //return Void(); TODO
  }

  phNxpEse_free(cpdu.pdata);
  phNxpEse_free(rpdu.pdata);

  return Void();
}

Return<void> SecureElement::openBasicChannel(const hidl_vec<uint8_t>& aid,
                                             uint8_t p2,
                                             openBasicChannel_cb _hidl_cb) {
  ESESTATUS status = ESESTATUS_SUCCESS;
  phNxpEse_7816_cpdu_t cpdu;
  phNxpEse_7816_rpdu_t rpdu;
  hidl_vec<uint8_t> result;
  hidl_vec<uint8_t> ls_aid = {0xA0, 0x00, 0x00, 0x03, 0x96, 0x41, 0x4C,
              0x41, 0x01, 0x43, 0x4F, 0x52, 0x01};
  if (!mIsEseInitialized) {
    ESESTATUS status = seHalInit();
    if (status != ESESTATUS_SUCCESS) {
      LOG(ERROR) << "%s: seHalInit Failed!!!"<< __func__;
      _hidl_cb(result, SecureElementStatus::IOERROR);
      return Void();
    }
  }
  phNxpEse_memset(&cpdu, 0x00, sizeof(phNxpEse_7816_cpdu_t));
  phNxpEse_memset(&rpdu, 0x00, sizeof(phNxpEse_7816_rpdu_t));

  cpdu.cla = 0x00; /* Class of instruction */
  cpdu.ins = 0xA4; /* Instruction code */
  cpdu.p1 = 0x04;  /* Instruction parameter 1 */
  cpdu.p2 = p2;    /* Instruction parameter 2 */
  cpdu.lc = aid.size();
  cpdu.le_type = 0x01;
  cpdu.pdata = (uint8_t*)phNxpEse_memalloc(aid.size() * sizeof(uint8_t));
  memcpy(cpdu.pdata, aid.data(), cpdu.lc);
  cpdu.le = 256;

  rpdu.len = 0x02;
  rpdu.pdata = (uint8_t*)phNxpEse_memalloc(cpdu.le * sizeof(uint8_t));

  LOG(ERROR) << "Robot about to acquire lock in VISO ";
  AutoThreadMutex a(sLock);
  LOG(ERROR) << "Robot acquired the lock in VISO ";
  status = phNxpEse_SetEndPoint_Cntxt(0);
  if (status != ESESTATUS_SUCCESS) {
    //return Void(); TODO
  }
  if(!memcmp(ls_aid.data(), aid.data(), cpdu.lc))
  {
      LOG(ERROR) << "LSDownload aid matches";
      tLSC_STATUS lsStatus = performLSDownload(cCallback);
      /*performLSDownload returns STATUS_FAILED in case thread creation fails.
      So return callback as false.
      Otherwise callback will be called in LSDownload module.*/
      if(lsStatus != STATUS_SUCCESS) {
          LOG(ERROR) << "LSDownload thread creation failed!!!";
          status = ESESTATUS_FAILED;
          rpdu.len = 2;
          rpdu.sw1 = 0x6A;
          rpdu.sw2 = 0x86;
          cCallback->onStateChange(false);
      }else {
          status = ESESTATUS_SUCCESS;
          rpdu.len = 2;
          rpdu.sw1 = 0x90;
          rpdu.sw2 = 0x00;
      }
  }else {
        status = phNxpEse_7816_Transceive(&cpdu, &rpdu);
  }

  SecureElementStatus sestatus;
  memset(&sestatus, 0x00, sizeof(sestatus));

  if (status != ESESTATUS_SUCCESS) {
    /* Transceive failed */
    if (rpdu.len > 0 && (rpdu.sw1 == 0x64 && rpdu.sw2 == 0xFF)) {
      _hidl_cb(result, SecureElementStatus::IOERROR);
    } else {
      _hidl_cb(result, SecureElementStatus::FAILED);
    }
  } else {
    /*Status word to be passed as part of response
    So include additional length*/
    uint16_t responseLen = rpdu.len + 2;
    result.resize(responseLen);
    memcpy(&result[0], rpdu.pdata, rpdu.len);
    result[responseLen - 1] = rpdu.sw2;
    result[responseLen - 2] = rpdu.sw1;

    /*Status is success*/
    if ((rpdu.sw1 == 0x90) && (rpdu.sw2 == 0x00)) {
      /*Set basic channel reference if it is not set */
      if (!mOpenedChannels[0]) {
        mOpenedChannels[0] = true;
        mOpenedchannelCount++;
      }

      sestatus = SecureElementStatus::SUCCESS;
    }
    /*AID provided doesn't match any applet on the secure element*/
    else if (rpdu.sw1 == 0x67 && rpdu.sw2 == 0x00) {
      sestatus = SecureElementStatus::NO_SUCH_ELEMENT_ERROR;
    }
    /*Operation provided by the P2 parameter is not permitted by the applet.*/
    else if (rpdu.sw1 == 0x6A && rpdu.sw2 == 0x86) {
      sestatus = SecureElementStatus::UNSUPPORTED_OPERATION;
    } else {
      sestatus = SecureElementStatus::FAILED;
    }
    _hidl_cb(result, sestatus);
  }
  status = phNxpEse_ResetEndPoint_Cntxt(0);
  if (status != ESESTATUS_SUCCESS) {
    //return Void(); TODO
  }
  phNxpEse_free(cpdu.pdata);
  phNxpEse_free(rpdu.pdata);
  return Void();
}

Return<::android::hardware::secure_element::V1_0::SecureElementStatus>
SecureElement::closeChannel(uint8_t channelNumber) {
  ESESTATUS status = ESESTATUS_SUCCESS;
  SecureElementStatus sestatus = SecureElementStatus::FAILED;
  phNxpEse_7816_cpdu_t cpdu;
  phNxpEse_7816_rpdu_t rpdu;

  if (channelNumber < DEFAULT_BASIC_CHANNEL ||
      channelNumber >= MAX_LOGICAL_CHANNELS) {
    LOG(ERROR) << StringPrintf("invalid channel!!! %d for %d",channelNumber,mOpenedChannels[channelNumber]);
    sestatus = SecureElementStatus::FAILED;
  } else if (channelNumber > DEFAULT_BASIC_CHANNEL){
    phNxpEse_memset(&cpdu, 0x00, sizeof(phNxpEse_7816_cpdu_t));
    phNxpEse_memset(&rpdu, 0x00, sizeof(phNxpEse_7816_rpdu_t));
    cpdu.cla = channelNumber; /* Class of instruction */
    cpdu.ins = 0x70;          /* Instruction code */
    cpdu.p1 = 0x80;           /* Instruction parameter 1 */
    cpdu.p2 = channelNumber;  /* Instruction parameter 2 */
    cpdu.lc = 0x00;
    cpdu.le = 0x9000;
  LOG(ERROR) << "Robot about to acquire lock in SPI ";
  AutoThreadMutex a(sLock);
  LOG(ERROR) << "Robot acquired the lock in SPI ";
    status = phNxpEse_SetEndPoint_Cntxt(0);
    if (status != ESESTATUS_SUCCESS) {
      //return Void(); TODO
    }
    status = phNxpEse_7816_Transceive(&cpdu, &rpdu);
    if (status != ESESTATUS_SUCCESS) {
      if (rpdu.len > 0 && (rpdu.sw1 == 0x64 && rpdu.sw2 == 0xFF)) {
        sestatus = SecureElementStatus::FAILED;
      } else {
        sestatus = SecureElementStatus::FAILED;
      }
    } else {
      if ((rpdu.sw1 == 0x90) && (rpdu.sw2 == 0x00)) {
        sestatus = SecureElementStatus::SUCCESS;
      } else {
        sestatus = SecureElementStatus::FAILED;
      }
    }
    status = phNxpEse_ResetEndPoint_Cntxt(0);
    if (status != ESESTATUS_SUCCESS) {
      //return Void(); TODO
    }
  }
  if ((channelNumber == DEFAULT_BASIC_CHANNEL) ||
      (sestatus == SecureElementStatus::SUCCESS)) {
    mOpenedChannels[channelNumber] = false;
    mOpenedchannelCount--;
    /*If there are no channels remaining close secureElement*/
    if (mOpenedchannelCount == 0) {
      sestatus = seHalDeInit();
    } else {
      sestatus = SecureElementStatus::SUCCESS;
    }
  }
  return sestatus;
}
void SecureElement::serviceDied(uint64_t /*cookie*/, const wp<IBase>& /*who*/) {
    LOG(ERROR) << " SecureElement serviceDied!!!";
    phNxpEse_deInit();
    phNxpEse_close();
  }
ESESTATUS SecureElement::seHalInit() {
  ESESTATUS status = ESESTATUS_SUCCESS;
  phNxpEse_initParams initParams;
  memset(&initParams, 0x00, sizeof(phNxpEse_initParams));
  initParams.initMode = ESE_MODE_NORMAL;

  //status = phNxpEse_open(initParams);
  if (status != ESESTATUS_SUCCESS) {
    LOG(ERROR) << "%s: SecureElement open failed!!!"<<__func__;
  } else {
    status = phNxpEse_SetEndPoint_Cntxt(0);
     if (status != ESESTATUS_SUCCESS) {
       //return Void(); TODO
     }
    status = phNxpEse_init(initParams);
    if (status != ESESTATUS_SUCCESS) {
      LOG(ERROR) << "%s: SecureElement init failed!!!"<< __func__;
    } else {
        status = phNxpEse_ResetEndPoint_Cntxt(0);
       if (status != ESESTATUS_SUCCESS) {
       //return Void(); TODO
      }
      mIsEseInitialized = true;
    }
  }
  return status;
}

Return<::android::hardware::secure_element::V1_0::SecureElementStatus>
SecureElement::seHalDeInit() {
  ESESTATUS status = ESESTATUS_SUCCESS;
  SecureElementStatus sestatus = SecureElementStatus::FAILED;
  status = phNxpEse_SetEndPoint_Cntxt(0);
  if (status != ESESTATUS_SUCCESS) {
    //return Void(); TODO
  }
  status = phNxpEse_deInit();
  status = phNxpEse_ResetEndPoint_Cntxt(0);
  if (status != ESESTATUS_SUCCESS) {
    //return Void(); TODO
  }
  if (status != ESESTATUS_SUCCESS) {
    sestatus = SecureElementStatus::FAILED;
  } else {
    //status = phNxpEse_close();
    if (status != ESESTATUS_SUCCESS) {
      sestatus = SecureElementStatus::FAILED;
    } else {
      mIsEseInitialized = false;
      sestatus = SecureElementStatus::SUCCESS;

      for (uint8_t xx = 0; xx < MAX_LOGICAL_CHANNELS; xx++) {
        mOpenedChannels[xx] = false;
      }
      mOpenedchannelCount = 0;
    }
  }
  return sestatus;
}

}  // namespace implementation
}  // namespace V1_0
}  // namespace secure_element
}  // namespace hardware
}  // namespace android
