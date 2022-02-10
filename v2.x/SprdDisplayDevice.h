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


/******************************************************************************
 **                   Edit    History                                         *
 **---------------------------------------------------------------------------*
 ** DATE          Module              DESCRIPTION                             *
 ** 10/07/2016    Hardware Composer v2.0 Responsible for processing some      *
 **                                   Hardware layers. These layers comply    *
 **                                   with display controller specification,  *
 **                                   can be displayed directly, bypass       *
 **                                   SurfaceFligner composition. It will     *
 **                                   improve system performance.             *
 ******************************************************************************
 ** File: SprdDisplayDevice.h         DESCRIPTION                             *
 **                                   Define some display device structures.  *
 ******************************************************************************
 ******************************************************************************
 ** Author:         zhongjun.chen@spreadtrum.com                              *
 *****************************************************************************/

#ifndef _SPRD_DISPLAY_DEVICE_H_
#define _SPRD_DISPLAY_DEVICE_H_

#include <hardware/hwcomposer2.h>
#include <hardware/hwcomposer.h>
#include "SprdHWLayer.h"
#include <string.h>
#include "AndroidFence.h"

/*
 *  We follow SurfaceFlinger
 * */
#define MAX_NUM_CONFIGS 128

enum DisplayType {
    DISPLAY_ID_INVALIDE       = -1,
    DISPLAY_PRIMARY           = HWC_DISPLAY_PRIMARY,
    DISPLAY_EXTERNAL          = HWC_DISPLAY_EXTERNAL,
    DISPLAY_VIRTUAL           = HWC_DISPLAY_VIRTUAL,
    NUM_BUILDIN_DISPLAY_TYPES = HWC_NUM_PHYSICAL_DISPLAY_TYPES,
};

enum DisplayID {
    DISPLAY_PRIMARY_ID   = 0x10001,
    DISPLAY_EXTERNAL_ID  = 0x10002,
    DISPLAY_VIRTUAL_ID   = 0x10003,
};

enum DisplayPowerMode {
    POWER_MODE_NORMAL = HWC_POWER_MODE_NORMAL,
    POWER_MODE_DOZE   = HWC_POWER_MODE_DOZE,
    POWER_MODE_OFF    = HWC_POWER_MODE_OFF,
#ifdef __LP64__
    POWER_MODE_DOZE_SUSPEND = HWC_POWER_MODE_DOZE_SUSPEND,
#endif
};

#define MAX_DISPLAYS HWC_NUM_DISPLAY_TYPES

typedef struct _DisplayAttributesSet
{
    uint32_t vsync_period; //nanos
    uint32_t xres;
    uint32_t yres;
    uint32_t stride;
    float xdpi;
    float ydpi;
    int32_t format;
    bool fillFlag;
} AttributesSet;

typedef struct _DisplayAttributes {
    AttributesSet      sets[MAX_NUM_CONFIGS];
    uint32_t           configIndexSets[MAX_NUM_CONFIGS];
    size_t             numConfigs;
    uint32_t           configsIndex; // active config index
    bool               sprdSF; // sprd sf vs GSI sf, GSI sf only need one config to pass VTS
    bool               connected;
    unsigned int       AcceleratorMode;
} DisplayAttributes;

static const uint32_t DISPLAY_ATTRIBUTES[] = {
    HWC_DISPLAY_VSYNC_PERIOD,
    HWC_DISPLAY_WIDTH,
    HWC_DISPLAY_HEIGHT,
    HWC_DISPLAY_DPI_X,
    HWC_DISPLAY_DPI_Y,
    HWC_DISPLAY_NO_ATTRIBUTE,
};

#define NUM_DISPLAY_ATTRIBUTES (sizeof(DISPLAY_ATTRIBUTES) / (sizeof(DISPLAY_ATTRIBUTES[0])))

enum HWC2_ATTRIBUTE {
  ATT_INVALID       = HWC2_ATTRIBUTE_INVALID,
  ATT_WIDTH         = HWC2_ATTRIBUTE_WIDTH,
  ATT_HEIGHT        = HWC2_ATTRIBUTE_HEIGHT,
  ATT_VSYNC_PERIOD  = HWC2_ATTRIBUTE_VSYNC_PERIOD,
  ATT_DPI_X         = HWC2_ATTRIBUTE_DPI_X,
  ATT_DPI_Y         = HWC2_ATTRIBUTE_DPI_Y,
};

#define COLORMATRIX_NUM 16

#define DISPLAY_NAME_SIZE 30

#define RETIRED_THRESHOLD 2

class SprdDisplayClient
{
public:
  SprdDisplayClient(int32_t displayId, int32_t displayType);
  ~SprdDisplayClient();

  bool Init(const char *displayName);

  int32_t /*hwc2_error_t*/ GET_CLIENT_TARGET_SUPPORT(
          uint32_t width, uint32_t height, int32_t /*android_pixel_format_t*/ format,
          int32_t /*android_dataspace_t*/ dataspace);

  int32_t /*hwc2_error_t*/ GET_COLOR_MODES(
          uint32_t* outNumModes,
          int32_t* /*android_color_mode_t*/ outModes);

  int32_t /*hwc2_error_t*/ GET_DISPLAY_NAME(
          uint32_t* outSize,
           char* outName);

  int32_t /*hwc2_error_t*/ GET_DISPLAY_TYPE(
          int32_t* /*hwc2_display_type_t*/ outType);

  int32_t /*hwc2_error_t*/ GET_DOZE_SUPPORT(
          int32_t* outSupport);

  int32_t /*hwc2_error_t*/ GET_HDR_CAPABILITIES(
          uint32_t* outNumTypes,
          int32_t* /*android_hdr_t*/ outTypes, float* outMaxLuminance,
          float* outMaxAverageLuminance, float* outMinLuminance);

  int32_t /*hwc2_error_t*/ SET_CLIENT_TARGET(
          buffer_handle_t target,
          int32_t acquireFence, int32_t /*android_dataspace_t*/ dataspace,
          hwc_region_t damage);

  int32_t /*hwc2_error_t*/ SET_COLOR_MODE(
          int32_t /*hwc2_power_mode_t*/ mode);

  int32_t SET_COLOR_TRANSFORM(const float* matrix,
                              int32_t /*android_color_transform_t*/ hint);

  int32_t /*hwc2_error_t*/ SET_OUTPUT_BUFFER(
           buffer_handle_t buffer,
           int32_t releaseFence);

  int processRetiredFence(int fd);
#ifdef ENABLE_PENDING_RELEASE_FENCE_FEATURE
  int processReleaseFence(int fd);
#endif

  inline int32_t getDisplayId()
  {
    return mDisplayId;
  }

  inline void setUserData(void *data)
  {
    mData = data;
  }

  inline void *getUserData()
  {
    return mData;
  }

  inline int setReleaseFence(int fd)
  {
    if (mReleaseFence >= 0)
    {
      closeFence(&mReleaseFence);
    }

    mReleaseFence = fd;

    return 0;
  }

#ifdef ENABLE_PENDING_RELEASE_FENCE_FEATURE
  inline int setPreReleaseFence(int fd)
  {
    if (mPreReleaseFence >= 0)
    {
      closeFence(&mPreReleaseFence);
    }

    mPreReleaseFence = fd;

    return 0;
  }
#endif

  inline int setReleaseFence2(int fd)
  {
    if (mReleaseFence2 >= 0)
    {
      closeFence(&mReleaseFence2);
    }

    mReleaseFence2 = fd;

    return 0;
  }

  inline int closeRelFence()
  {
    if (mReleaseFence >= 0)
    {
      closeFence(&mReleaseFence);
    }

    if (mReleaseFence2 >= 0)
    {
      closeFence(&mReleaseFence2);
    }

    return 0;
  }

#ifdef ENABLE_PENDING_RELEASE_FENCE_FEATURE
  inline int closePreRelFence()
  {
    if (mPreReleaseFence >= 0)
    {
      closeFence(&mPreReleaseFence);
    }

    return 0;
  }
#endif

  inline int getReleseFence()
  {
    return mReleaseFence;
  }

  inline int getReleseFence2()
  {
    return mReleaseFence2;
  }

  DisplayAttributes *getDisplayAttributes()
  {
    return mDisplayAttributes;
  }

  inline SprdHWLayer *getFBTargetLayer()
  {
    return mFBTargetLayer;
  }

  inline SprdHWLayer *getOutputLayer()
  {
    return mOutputLayer;
  }

  inline int32_t getDisplayId() const
  {
    return mDisplayId;
  }

  static inline SprdDisplayClient *getDisplayClient(hwc2_display_t display)
  {
    /* this method may has a risk */
    return reinterpret_cast<SprdDisplayClient *>(display);
  }

  inline static hwc2_display_t remapToAndroidDisplay(SprdDisplayClient *client)
  {
    return reinterpret_cast<hwc2_display_t>(client);
  }


private:
  bool mInitFlag;
  int32_t mDisplayId;
  int32_t mDisplayType;
  char mDisplayName[DISPLAY_NAME_SIZE];
  DisplayAttributes *mDisplayAttributes;
  void *mData;
  SprdHWLayer *mFBTargetLayer;
  SprdHWLayer *mOutputLayer; // only used for Virtual display
  int mReleaseFence;
#ifdef ENABLE_PENDING_RELEASE_FENCE_FEATURE
  int mPreReleaseFence;
#endif
  int mReleaseFence2;
  int32_t mColorTransformHit;
  int32_t mColorMatrix[COLORMATRIX_NUM]; // 4 * 4 matrix
  int mRetiredFences[RETIRED_THRESHOLD];
#ifdef ENABLE_PENDING_RELEASE_FENCE_FEATURE
  int mReleaseFences[RETIRED_THRESHOLD];
#endif

  int WrapFBTargetLayer(native_handle_t *buf, int32_t acquireFence,
                        int32_t dataspace, hwc_region_t damage);
};

#endif
