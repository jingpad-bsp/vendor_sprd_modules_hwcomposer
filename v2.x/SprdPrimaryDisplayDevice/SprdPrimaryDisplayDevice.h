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
 ** 10/07/2016    Hardware Composer v2.0   Responsible for processing some    *
 **                                   Hardware layers. These layers comply    *
 **                                   with display controller specification,  *
 **                                   can be displayed directly, bypass       *
 **                                   SurfaceFligner composition. It will     *
 **                                   improve system performance.             *
 ******************************************************************************
 ** File: SprdPrimaryDisplayDevice.h  DESCRIPTION                             *
 **                                   Manage the PrimaryDisplayDevice         *
 **                                   including prepare and commit            *
 ******************************************************************************
 ******************************************************************************
 ** Author:         zhongjun.chen@spreadtrum.com                              *
 *****************************************************************************/

#ifndef _SPRD_PRIMARY_DISPLAY_DEVICE_H_
#define _SPRD_PRIMARY_DISPLAY_DEVICE_H_

#include <hardware/hardware.h>
#include <hardware/hwcomposer2.h>
#include <fcntl.h>
#include <errno.h>

#include <EGL/egl.h>

#include <utils/RefBase.h>
#include <cutils/properties.h>
#include <cutils/atomic.h>
#include <cutils/log.h>

#include "SprdHWLayerList.h"
#include "SprdOverlayPlane.h"
#include "SprdPrimaryPlane.h"
#include "SprdFrameBufferHAL.h"
#include "../SprdDisplayDevice.h"
#include "../AndroidFence.h"
#include "../SprdHandleLayer.h"

#ifdef OVERLAY_COMPOSER_GPU
#include "../OverlayComposer/OverlayComposer.h"
#endif

#include "../SprdUtil.h"
#include "../dump.h"
#include "SprdHWC2DataType.h"

using namespace android;

class SprdHWLayerList;
class SprdUtil;
class SprdDisplayCore;
struct DisplayTrack;

#define MAX_DISPLAY_CLIENT 2

class SprdPrimaryDisplayDevice {
 public:
  SprdPrimaryDisplayDevice();

  ~SprdPrimaryDisplayDevice();

   /*
    *  Initialize the SprdPrimaryDisplayDevice member
    * */
   bool Init(SprdDisplayCore *core);

   /* Android ogrinal function */

   void getCapabilities(uint32_t* outCount,
                       int32_t* /*hwc2_capability_t*/ outCapabilities);

   void DUMP(uint32_t* outSize, char* outBuffer, String8& result);

   int32_t /*hwc2_error_t*/ ACCEPT_DISPLAY_CHANGES(SprdDisplayClient *Client);

   int32_t /*hwc2_error_t*/ CREATE_LAYER(SprdDisplayClient *Client,
                                         hwc2_layer_t* outLayer);

   int32_t /*hwc2_error_t*/ DESTROY_LAYER(SprdDisplayClient *Client, hwc2_layer_t layer);

   int32_t /*hwc2_error_t*/ GET_CHANGED_COMPOSITION_TYPES(
           SprdDisplayClient *Client,
           uint32_t* outNumElements, hwc2_layer_t* outLayers,
           int32_t* /*hwc2_composition_t*/ outTypes);

   int32_t /*hwc2_error_t*/ GET_CLIENT_TARGET_SUPPORT(
           SprdDisplayClient *Client,
           uint32_t width, uint32_t height, int32_t /*android_pixel_format_t*/ format,
           int32_t /*android_dataspace_t*/ dataspace);

   int32_t /*hwc2_error_t*/ GET_COLOR_MODES(
           SprdDisplayClient *Client,
           uint32_t* outNumModes,
           int32_t* /*android_color_mode_t*/ outModes);

   int32_t /*hwc2_error_t*/ GET_DISPLAY_NAME(
           SprdDisplayClient *Client,
           uint32_t* outSize,
           char* outName);

   int32_t /*hwc2_error_t*/ GET_DISPLAY_REQUESTS(
           SprdDisplayClient *Client,
           int32_t* /*hwc2_display_request_t*/ outDisplayRequests,
           uint32_t* outNumElements, hwc2_layer_t* outLayers,
           int32_t* /*hwc2_layer_request_t*/ outLayerRequests);

   int32_t /*hwc2_error_t*/ GET_DISPLAY_TYPE(
           SprdDisplayClient *Client,
           int32_t* /*hwc2_display_type_t*/ outType);

   int32_t /*hwc2_error_t*/ GET_DOZE_SUPPORT(
           SprdDisplayClient *Client,
           int32_t* outSupport);

   int32_t /*hwc2_error_t*/ GET_HDR_CAPABILITIES(
           SprdDisplayClient *Client,
           uint32_t* outNumTypes,
           int32_t* /*android_hdr_t*/ outTypes, float* outMaxLuminance,
           float* outMaxAverageLuminance, float* outMinLuminance);

   int32_t /*hwc2_error_t*/ GET_RELEASE_FENCES(
           SprdDisplayClient *Client,
           uint32_t* outNumElements,
           hwc2_layer_t* outLayers, int32_t* outFences);

   int32_t /*hwc2_error_t*/ SET_CLIENT_TARGET(
           SprdDisplayClient *Client,
           buffer_handle_t target,
           int32_t acquireFence, int32_t /*android_dataspace_t*/ dataspace,
           hwc_region_t damage);

   int32_t /*hwc2_error_t*/ SET_COLOR_MODE(
           SprdDisplayClient *Client,
           int32_t /*android_color_mode_t*/ mode);

   int32_t /*hwc2_error_t*/ SET_COLOR_TRANSFORM(
           SprdDisplayClient *Client,
           const float* matrix,
           int32_t /*android_color_transform_t*/ hint);

   int32_t /*hwc2_error_t*/ SET_POWER_MODE(
           SprdDisplayClient *Client,
           int32_t /*hwc2_power_mode_t*/ mode);

   int32_t /*hwc2_error_t*/ VALIDATE_DISPLAY(
           SprdDisplayClient *Client,
           uint32_t* outNumTypes, uint32_t* outNumRequests,
           int accelerator);

  int32_t HotplugCallback(const char *displayName, int32_t displayType,
                                 bool connected, SprdDisplayClient **client);

  /*
   *  Post layers to SprdDisplayPlane.
   * */
  int commit(SprdDisplayClient *Client);

  /*
   *  Build Sync data for SurfaceFligner
   * */
  int buildSyncData(SprdDisplayClient *Client, struct DisplayTrack *tracker, int32_t* outRetireFence);


  /*
   *  Display configure attribution.
   * */
  int syncAttributes(SprdDisplayClient *Client, AttributesSet *dpyAttributes);

  int ActiveConfig(SprdDisplayClient *Client, DisplayAttributes *dpyAttributes);

  /*
   *  Recycle DispalyPlane buffer for saving memory.
   * */
  int reclaimPlaneBuffer(bool condition);

  inline SprdHandleLayer *getHandleLayer()
  {
    return mHandleLayer;
  }

  bool getHasColorMatrix();
  void setHasColorMatrix(bool hasColorMatrix);

 private:
  FrameBufferInfo          *mFBInfo;
  SprdDisplayCore          *mDispCore;
  SprdDisplayClient        *mClient[MAX_DISPLAY_CLIENT];
  SprdDisplayClient        *mCurrentClient;
  void *mPrimaryDisplayContext;
  SprdHandleLayer *mHandleLayer;
  SprdOverlayPlane *mOverlayPlane;
  SprdPrimaryPlane *mPrimaryPlane;
#ifdef OVERLAY_COMPOSER_GPU
  sp<OverlayNativeWindow> mWindow;
  sp<OverlayComposer> mOverlayComposer;
#endif
  SprdHWLayer *mFBTargetLayer;
  SprdHWLayer *mComposedLayer;
  SprdHWLayer **mPresentList;
  SprdUtil *mUtil;
  SprdUtilSource *mUtilSource;
  SprdUtilTarget *mUtilTarget;
  bool mInit;
  bool mDisplayFBTarget;
  bool mDisplayPrimaryPlane;
  bool mDisplayOverlayPlane;
  bool mDisplayOVC;
  bool mDisplayDispC;
  bool mDisplayNoData;
  bool mSchedualUtil;
  bool mFirstFrameFlag;
  bool mPresentState;
  bool mHasColorMatrix;
  int mHWCDisplayFlag;
  unsigned int mAcceleratorMode;
  int32_t mPresentLayerCount;
  int32_t mClientCount;

  bool mBlank;
  Mutex mLock;

#ifdef UPDATE_SYSTEM_FPS_FOR_POWER_SAVE
  bool mIsPrimary60Fps;
#endif

  int mDebugFlag;
  int mDumpFlag;

  typedef struct sprdRect DisplayRect;

  /*
   *  And then attach these HWC_OVERLAY layers to SprdDisplayPlane.
   * */
  int attachToDisplayPlane(int DisplayFlag, SprdHWLayerList *HWLayerList);

  int WrapOverlayLayer(native_handle_t *buf, int format, float planeAlpha, int fenceFd,
                       int32_t blendMode, uint32_t zorder, SprdHWLayer *l);

  int AddPresentLayerList(SprdHWLayer **list, int32_t count);

  int RemoveLayerInPresentList();

  inline SprdHWLayer **getPresentLayerList() const
  {
    return mPresentList;
  }

  inline uint32_t getPresentLayerCount() const
  {
    return mPresentLayerCount;
  }

  /*
   *  Compute the DirtyRegion for PrimaryDisplay.
   *  DirtyRegion: the region will be read by
   *  Display Controller.
   *  the region will be smaller than Screen size,
   *  it is usefull for Partial update.
   * */
  int computeDisplayDirtyRegion(DisplayRect &DirtyRegion);

  int AcceleratorProbe();

  int AcceleratorAdapt(int DisplayDeviceAccelerator);

  static inline SprdHWLayerList *getHWLayerObj(SprdDisplayClient *client)
  {
    return static_cast<SprdHWLayerList *>(client->getUserData());
  }

#ifdef HWC_DUMP_CAMERA_SHAKE_TEST
  void dumpCameraShakeTest(hwc_display_contents_1_t *list);
#endif

  int SprdUtilScheldule(SprdUtilSource *Source, SprdUtilTarget *Target);

#ifdef OVERLAY_COMPOSER_GPU
  int OverlayComposerScheldule(SprdHWLayer **list,
                               uint32_t layerCount,
                               SprdDisplayPlane *DisplayPlane, SprdHWLayer *FBTargetLayer);
#endif

#ifdef UPDATE_SYSTEM_FPS_FOR_POWER_SAVE
void UpdateSystemFps(int systemLayerCount);
#endif
};

#endif  // #ifndef _SPRD_PRIMARY_DISPLAY_DEVICE_H_
