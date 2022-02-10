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

#ifndef _SPRD_DISPLAY_CORE_H_
#define _SPRD_DISPLAY_CORE_H_

#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <hardware/hwcomposer2.h>
#include <cutils/log.h>

#include "SprdHWLayer.h"
#include "SprdDisplayDevice.h"

using namespace android;

class SprdPrimaryDisplayDevice;
class SprdExternalDisplayDevice;

#define DEFAULT_DISPLAY_TYPE_NUM 3

struct DisplayTrack {
  int releaseFenceFd;
  int retiredFenceFd;
};

typedef void (*AndroidHotplugCB_t)(hwc2_callback_data_t callbackData,
                                 hwc2_display_t displayId, int32_t intConnected);

typedef void (*AndroidRefreshCB_t)(hwc2_callback_data_t callbackData,
                                 hwc2_display_t displayId);

typedef void (*AndroidVsyncCB_t)(hwc2_callback_data_t callbackData,
                               hwc2_display_t displayId, int64_t timestamp);

typedef struct _HWC2CallbackData {
  hwc2_function_pointer_t pFn;
  hwc2_callback_data_t cData;
} HWC2CallbackData;

#define HWC2_CALLBACK_NUM  3
#define HWC2_HOTPLUG_CB    0
#define HWC2_REFRESH_CB    1
#define HWC2_VSYNC_CB      2
#define MAX_HW_CLIENT 2

class SprdDisplayCore {
public:
  SprdDisplayCore()
      : mInitFlag(false),
        mActiveContextCount(0),
        mLayerCount(0),
        mClient(NULL),
        mPrimaryDisplay(NULL),
        mExternalDisplay(NULL)
        {}

  virtual ~SprdDisplayCore() {
    mInitFlag = false;
    mActiveContextCount = 0;
    mLayerCount = 0;

    if (mClient)
    {
      delete [] mClient;
      mClient = NULL;
    }
  }

  /*
   *  Init SprdDisplayCore, open interface, and get some configs.
   */
  virtual bool Init() {
    for (int i = 0; i < HWC2_CALLBACK_NUM; i++)
    {
      mCallbackData[i].pFn   = NULL;
      mCallbackData[i].cData = NULL;
    }

    if (mClient == NULL)
    {
      mClient = new SprdDisplayClient*[MAX_HW_CLIENT];
      if (mClient == NULL)
      {
        ALOGE("SprdDisplayCore:: SprdDisplayClient is NULL");
        return false;
      }
    }

    mInitFlag = true;

    return mInitFlag;
  }


  int32_t REGISTER_CALLBACK(int32_t descriptor, hwc2_callback_data_t callbackData,
                            hwc2_function_pointer_t pointer);

  inline AndroidHotplugCB_t getAndroidHotplugPFN()
  {
    return mAndroidHotplug;
  }

  inline AndroidRefreshCB_t getAndroidRefreshPFN()
  {
    return mAndroidRefresh;
  }

  inline AndroidVsyncCB_t getAndroidVsyncPFN()
  {
    return mAndroidVsync;
  }

  inline SprdDisplayClient **getClientReference()
  {
    return mClient;
  }

  void setPrimaryDisplayDevice(SprdPrimaryDisplayDevice *display);

  SprdPrimaryDisplayDevice *getPrimaryDisplayDevice();

  void setExternalDisplayDevice(SprdExternalDisplayDevice *display);

  SprdExternalDisplayDevice *getExternalDisplayDevice();

  inline HWC2CallbackData *getHWC2CBData(uint32_t index) { return &(mCallbackData[index]); }

  virtual int AddFlushData(int DisplayType, SprdHWLayer **list,
                           int LayerCount) = 0;

  virtual int PostDisplay(DisplayTrack *tracker) = 0;

  virtual int QueryDisplayInfo(uint32_t *DisplayNum) = 0;

  virtual int GetConfigs(int DisplayType, uint32_t *Configs,
                         size_t *NumConfigs) = 0;

  virtual int setActiveConfig(int DisplayType, uint32_t Config) = 0;

  virtual int getActiveConfig(int DisplayType, uint32_t *pConfig) = 0;

  virtual int GetConfigAttributes(int DisplayType, uint32_t Config,
                                  const uint32_t *attributes,
                                  int32_t *values) = 0;

  virtual int EventControl(int DisplayType, int event, bool enabled) = 0;

  virtual int Blank(int DisplayType, bool enabled) = 0;

  virtual int Dump(char *buffer) = 0;

 protected:
  bool mInitFlag;
  int32_t mActiveContextCount;
  int32_t mLayerCount;
  HWC2CallbackData mCallbackData[HWC2_CALLBACK_NUM];
  AndroidHotplugCB_t mAndroidHotplug;
  AndroidRefreshCB_t mAndroidRefresh;
  AndroidVsyncCB_t   mAndroidVsync;
  SprdDisplayClient **mClient;
  SprdPrimaryDisplayDevice  *mPrimaryDisplay;
  SprdExternalDisplayDevice *mExternalDisplay;
};

class SprdEventHandle {
 public:
  SprdEventHandle() {}
  ~SprdEventHandle() {}

  static void SprdHandleVsyncReport(void *data, int disp, uint64_t timestamp);

  static void SprdHandleHotPlugReport(void *data, int disp, bool connected);

  static void SprdHandleCustomReport(void *data, int disp,
                                     struct adf_event *event);
};

#endif  // #ifndef _SPRD_DISPLAY_CORE_H_
