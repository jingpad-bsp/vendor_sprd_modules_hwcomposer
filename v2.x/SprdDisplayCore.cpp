/*
 * Copyright (C) 2010 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 *  SprdDisplayCore: surpport several display framework, such as
 *                    traditional Frame buffer device, Android ADF,
 *                    and the DRM in the future.
 *  It pass display data from HWC to Display driver.
 *  Author: zhongjun.chen@spreadtrum.com
*/

#include "SprdDisplayCore.h"
#include "SprdPrimaryDisplayDevice/SprdPrimaryDisplayDevice.h"
#include "SprdExternalDisplayDevice/SprdExternalDisplayDevice.h"


int32_t SprdDisplayCore::REGISTER_CALLBACK(int32_t descriptor, hwc2_callback_data_t callbackData,
                                           hwc2_function_pointer_t pointer)
{
  if (callbackData == NULL || pointer == NULL)
  {
      ALOGE("SprdDisplayCore REGISTER_CALLBACK input para is NULL");
      return ERR_BAD_PARAMETER;
  }

  switch (descriptor)
  {
      case HWC2_CALLBACK_HOTPLUG:
        mCallbackData[HWC2_HOTPLUG_CB].pFn   = pointer;
        mCallbackData[HWC2_HOTPLUG_CB].cData = callbackData;
        mAndroidHotplug = reinterpret_cast<AndroidHotplugCB_t>(pointer);

        /*
         * TODO: WORKAROUND here for PrimaryDisplay hotplug event here
         * depends on SurfaceFlinger do not use lock to lock the main thread,
         * If the lock exist, the dead lock will happen when call hotplug in the same main thread,
         * should move it to another thread.
         */
        SprdEventHandle::SprdHandleHotPlugReport(static_cast<void *>(this), DISPLAY_PRIMARY, true);

        break;
      case HWC2_CALLBACK_REFRESH:
        mCallbackData[HWC2_REFRESH_CB].pFn   = pointer;
        mCallbackData[HWC2_REFRESH_CB].cData = callbackData;
        mAndroidRefresh = reinterpret_cast<AndroidRefreshCB_t>(pointer);
        break;
      case HWC2_CALLBACK_VSYNC:
        mCallbackData[HWC2_VSYNC_CB].pFn     = pointer;
        mCallbackData[HWC2_VSYNC_CB].cData   = callbackData;
        mAndroidVsync   = reinterpret_cast<AndroidVsyncCB_t>(pointer);
        break;
  }

  return ERR_NONE;
}

void SprdDisplayCore::setPrimaryDisplayDevice(SprdPrimaryDisplayDevice *display)
{
  mPrimaryDisplay = display;
}

SprdPrimaryDisplayDevice *SprdDisplayCore::getPrimaryDisplayDevice()
{
  return mPrimaryDisplay;
}

void SprdDisplayCore::setExternalDisplayDevice(SprdExternalDisplayDevice *display)
{
  mExternalDisplay = display;
}

SprdExternalDisplayDevice *SprdDisplayCore::getExternalDisplayDevice()
{
  return mExternalDisplay;
}

void SprdEventHandle::SprdHandleVsyncReport(void *data, int disp, uint64_t timestamp)
{
  hwc2_display_t displayId = 0;
  SprdDisplayCore *core = static_cast<SprdDisplayCore *>(data);
  SprdDisplayClient **ClientsRef = NULL;
  SprdDisplayClient *client = NULL;
  AndroidVsyncCB_t pfn = NULL;
  if (core == NULL) {
    ALOGE("SprdHandleVsyncReport cannot get the SprdDisplayCore reference");
    return;
  }

  ClientsRef = core->getClientReference();
  if (ClientsRef == NULL)
  {
    ALOGE("SprdHandleVsyncReport cannot get ClientsRef");
    return;
  }

  switch (disp)
  {
      case DISPLAY_PRIMARY:
          client = ClientsRef[0];
          break;
      case DISPLAY_EXTERNAL:
          client = ClientsRef[1];
          break;
      case DISPLAY_VIRTUAL:
          break;
      default:
          break;
  }

  displayId = (client == NULL) ? 0 : SprdDisplayClient::remapToAndroidDisplay(client);

  pfn = core->getAndroidVsyncPFN();
  HWC2CallbackData *pCBD = core->getHWC2CBData(HWC2_VSYNC_CB);
  if (pCBD && pfn) {
    static int count = 0;
    pfn(pCBD->cData, displayId, timestamp);
  }
}

void SprdEventHandle::SprdHandleHotPlugReport(void *data, int disp, bool connected)
{
  SprdDisplayClient **ClientsRef = NULL;
  SprdDisplayClient *client = NULL;
  hwc2_display_t displayId = 0;
  AndroidHotplugCB_t pfn = NULL;

  SprdDisplayCore *core = static_cast<SprdDisplayCore *>(data);
  if (core == NULL) {
    ALOGE("SprdHandleHotPlugReport cannot get the SprdDisplayCore reference");
    return;
  }

  ClientsRef = core->getClientReference();
  if (ClientsRef == NULL)
  {
    ALOGE("SprdHandleHotPlugReport  cannot get ClientsRef");
    return;
  }

  if (disp == DISPLAY_PRIMARY)
  {
    SprdPrimaryDisplayDevice *PD = core->getPrimaryDisplayDevice();

    if (PD)
    {
      PD->HotplugCallback("SprdBuiltInDisplay", DISPLAY_TYPE_PHYSICAL,
                          connected, &ClientsRef[0]);
    }
    client = ClientsRef[0];
  }
  else if (disp == DISPLAY_EXTERNAL)
  {
    SprdExternalDisplayDevice *ED = core->getExternalDisplayDevice();

    if (ED)
    {
      ED->HotplugCallback("SprdHotplugDisplay", DISPLAY_TYPE_PHYSICAL,
                          connected, &ClientsRef[1]);
    }
    client = ClientsRef[1];
  }

  displayId = (client == NULL) ? 0 : SprdDisplayClient::remapToAndroidDisplay(client);

  pfn = core->getAndroidHotplugPFN();
  HWC2CallbackData *pCBD = core->getHWC2CBData(HWC2_HOTPLUG_CB);
  if (pCBD && pfn) {
    static int count = 0;
    pfn(pCBD->cData, displayId, connected);
  }
}

void SprdEventHandle::SprdHandleCustomReport(void *data, int disp,
                                     struct adf_event *event)
{
  SprdDisplayClient *client = NULL;
  SprdDisplayClient **ClientsRef = NULL;
  hwc2_display_t displayId = 0;
  AndroidRefreshCB_t pfn = NULL;
  SprdDisplayCore *core = static_cast<SprdDisplayCore *>(data);
  if (core == NULL) {
    ALOGE("SprdHandleHotPlugReport cannot get the SprdDisplayCore reference");
    return;
  }

  ClientsRef = core->getClientReference();
  if (ClientsRef == NULL)
  {
    ALOGE("SprdHandleHotPlugReport  cannot get ClientsRef");
    return;
  }

  switch (disp)
  {
      case DISPLAY_PRIMARY:
          client = ClientsRef[0];
          break;
      case DISPLAY_EXTERNAL:
          client = ClientsRef[1];
          break;
      case DISPLAY_VIRTUAL:
          break;
      default:
          break;
  }

  if (event == NULL)
  {
    ALOGE("SprdHandleCustomReport event is NULL");
  }

  displayId = (client == NULL) ? 0 : SprdDisplayClient::remapToAndroidDisplay(client);

  pfn = core->getAndroidRefreshPFN();
  HWC2CallbackData *pCBD = core->getHWC2CBData(HWC2_REFRESH_CB);
  if (pCBD && pfn) {
    static int count = 0;
    pfn(pCBD->cData, displayId);
  }

}
