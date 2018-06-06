/*******************************************************************************
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

#include <android/hardware/secure_element/1.0/ISecureElementHalCallback.h>
#ifndef JCOPCLIENT_H_
#define JCOPCLIENT_H_

typedef enum {
  SESTATUS_SUCCESS = (0x0000),
  SESTATUS_FAILED = (0x0003),
  SESTATUS_FILE_NOT_FOUND = (0x0005)
} SESTATUS;

using ::android::hardware::secure_element::V1_0::ISecureElementHalCallback;

/*******************************************************************************
**
** Function:        LSC_doDownload
**
** Description:     Perform LS during hal init
**
** Returns:         SUCCESS of ok
**
*******************************************************************************/
SESTATUS JCOS_doDownload();

#endif /* LSCLIENT_H_ */
