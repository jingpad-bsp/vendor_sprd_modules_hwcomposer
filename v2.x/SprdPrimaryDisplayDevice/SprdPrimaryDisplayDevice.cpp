/*
 * Copyright (C) 2016 The Android Open Source Project
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
 ** 10/07/2016    Hardware Composer v2.0  Responsible for processing some     *
 **                                   Hardware layers. These layers comply    *
 **                                   with display controller specification,  *
 **                                   can be displayed directly, bypass       *
 **                                   SurfaceFligner composition. It will     *
 **                                   improve system performance.             *
 ******************************************************************************
 ** File:SprdPrimaryDisplayDevice.cpp DESCRIPTION                             *
 **                                   Manage the PrimaryDisplayDevice         *
 **                                   including prepare and commit            *
 ******************************************************************************
 ******************************************************************************
 ** Author:         zhongjun.chen@spreadtrum.com                              *
 *****************************************************************************/

#include "SprdPrimaryDisplayDevice.h"
#include <utils/String8.h>
#include "../SprdTrace.h"
#include "../SprdHWC2DataType.h"
#include "../SprdDisplayCore.h"

using namespace android;

#define DEFAULT_PRESENT_LAYER_COUNT 10

SprdPrimaryDisplayDevice::SprdPrimaryDisplayDevice()
    : mFBInfo(0),
      mDispCore(NULL),
      mCurrentClient(0),
      mPrimaryDisplayContext(0),
      mHandleLayer(0),
      mOverlayPlane(0),
      mPrimaryPlane(0),
#ifdef OVERLAY_COMPOSER_GPU
      mWindow(NULL),
      mOverlayComposer(NULL),
#endif
      mFBTargetLayer(NULL),
      mComposedLayer(NULL),
      mPresentList(NULL),
      mUtil(0),
      mUtilSource(NULL),
      mUtilTarget(NULL),
      mInit(false),
      mDisplayFBTarget(false),
      mDisplayPrimaryPlane(false),
      mDisplayOverlayPlane(false),
      mDisplayOVC(false),
      mDisplayDispC(false),
      mDisplayNoData(false),
      mSchedualUtil(false),
      mFirstFrameFlag(true),
      mPresentState(false),
      mHasColorMatrix(false),
      mHWCDisplayFlag(HWC_DISPLAY_MASK),
      mAcceleratorMode(ACCELERATOR_NON),
      mPresentLayerCount(0),
      mClientCount(MAX_DISPLAY_CLIENT),
      mBlank(false),
#ifdef UPDATE_SYSTEM_FPS_FOR_POWER_SAVE
      mIsPrimary60Fps(true),
#endif
      mDebugFlag(0),
      mDumpFlag(0) {
}

bool SprdPrimaryDisplayDevice::Init(SprdDisplayCore *core) {
  int GXPAddrType = 0;

  if (core == NULL) {
    ALOGE("SprdPrimaryDisplayDevice:: Init adfData is NULL");
    mInit = false;
    return false;
  }

  mDispCore = core;

  mDispCore->setPrimaryDisplayDevice(this);

  for (int i = 0; i < mClientCount; i++)
  {
    mClient[i] = NULL;
  }

  mFBInfo = (FrameBufferInfo *)malloc(sizeof(FrameBufferInfo));
  if (mFBInfo == NULL) {
    ALOGE("Can NOT get FrameBuffer info");
    mInit = false;
    return false;
  }

  mUtil = new SprdUtil();
  if (mUtil == NULL) {
    ALOGE("new SprdUtil failed");
    mInit = false;
    return false;
  }

  mUtilSource = (SprdUtilSource *)malloc(sizeof(SprdUtilSource));
  if (mUtilSource == NULL) {
    ALOGE("malloc SprdUtilSource failed");
    mInit = false;
    return false;
  }

  mUtilTarget = (SprdUtilTarget *)malloc(sizeof(SprdUtilTarget));
  if (mUtilTarget == NULL) {
    ALOGE("malloc SprdUtilTarget failed");
    mInit = false;
    return false;
  }

  AcceleratorProbe();

  mHandleLayer = new SprdHandleLayer();
  if (mHandleLayer == NULL)
  {
    ALOGE("new SprdHandleLayer failed");
    mInit = false;
    return false;
  }

  mPrimaryPlane = new SprdPrimaryPlane();
  if (mPrimaryPlane == NULL) {
    ALOGE("new SprdPrimaryPlane failed");
    mInit = false;
    return false;
  }

#ifdef BORROW_PRIMARYPLANE_BUFFER
  mOverlayPlane = new SprdOverlayPlane(mPrimaryPlane);
#else
  mOverlayPlane = new SprdOverlayPlane();
#endif
  if (mOverlayPlane == NULL) {
    ALOGE("new SprdOverlayPlane failed");
    mInit = false;
    return false;
  }

  mInit = true;

  return true;
}

int32_t SprdPrimaryDisplayDevice::HotplugCallback(
               const char *displayName, int32_t displayType,
               bool connected, SprdDisplayClient **client)
{
  uint32_t index = 0;
  SprdHWLayerList *obj = NULL;
  static int count = 0;
  ALOGE("SprdPrimaryDisplayDevice::HotplugCallback count: %d", count++);

  if (client == NULL || displayName == NULL)
  {
    ALOGE("SprdPrimaryDisplayDevice::HotplugCallback input para is NULL");
    return -1;
  }

  index = displayType -1;

  if (connected && (mClient[index] != NULL)) {
    *client = mClient[index];
    return 0;
  }

  if (connected)
  {
    if (index >= MAX_DISPLAY_CLIENT)
    {
      ALOGE("SprdPrimaryDisplayDevice::HotplugCallback exceed max client count");
      return -1;
    }

    mClient[index] = new SprdDisplayClient(DISPLAY_PRIMARY_ID, displayType);
    if (mClient[index] == NULL)
    {
      ALOGE("SprdPrimaryDisplayDevice::HotplugCallback new SprdDisplayClient%d failed", index);
      return -1;
    }

    if (mClient[index]->Init(displayName) == false)
    {
      ALOGE("SprdPrimaryDisplayDevice::HotplugCallback DisplayClient Init failed");
      return -1;
    }
    
    obj = new SprdHWLayerList();
    if (obj == NULL) {
      ALOGE("new SprdHWLayerList failed");
      return -1;
    }

    mClient[index]->setUserData(static_cast<void *>(obj));

    *client = mClient[index];
  }
  else
  {
    obj = getHWLayerObj(*client);
    if (obj)
    {
      delete obj;
      obj = NULL;
    }
   
    delete *client;
    *client = NULL;
  }

  return 0;
}

SprdPrimaryDisplayDevice::~SprdPrimaryDisplayDevice() {

  if (mOverlayComposer){
	mOverlayComposer->requestThreadLoopExit();
  }

  if (mUtil != NULL) {
    delete mUtil;
    mUtil = NULL;
  }

  if (mUtilTarget != NULL) {
    free(mUtilTarget);
    mUtilTarget = NULL;
  }

  if (mUtilSource != NULL) {
    free(mUtilSource);
    mUtilSource = NULL;
  }

  if (mPrimaryPlane) {
    delete mPrimaryPlane;
    mPrimaryPlane = NULL;
  }

  if (mOverlayPlane) {
    delete mOverlayPlane;
    mOverlayPlane = NULL;
  }

  if (mPrimaryDisplayContext != NULL) {
    free(mPrimaryDisplayContext);
  }

  if (mHandleLayer)
  {
    delete mHandleLayer;
    mHandleLayer = NULL;
  }

  if (mFBInfo != NULL) {
    free(mFBInfo);
    mFBInfo = NULL;
  }

  for (int i = 0; i < mClientCount; i++)
  {
    if (mClient[i])
    {
      delete mClient[i];
      mClient[i] = NULL;
    }
  }

  if (mPresentList)
  {
    delete [] mPresentList;
    mPresentList = NULL;
  }
}

int SprdPrimaryDisplayDevice::AcceleratorProbe() {
  int accelerator = ACCELERATOR_NON;

#ifdef ACC_BY_DISPC
  if (mUtil->probeDPUDevice() == 0)
  {
    ALOGE("cannot find DPU devices");
  }
  else
  {
    accelerator |= ACCELERATOR_DISPC;
  }
#endif

#ifdef PROCESS_VIDEO_USE_GSP
  if (mUtil->probeGSPDevice() != 0) {
    ALOGE("cannot find GXP devices");
    goto Step2;
  }
  accelerator |= ACCELERATOR_GSP;
#endif

Step2:
#ifdef OVERLAY_COMPOSER_GPU
  accelerator |= ACCELERATOR_OVERLAYCOMPOSER;
#endif
  mAcceleratorMode |= accelerator;

  ALOGI_IF(mDebugFlag, "%s[%d] mAcceleratorMode:%x", __func__, __LINE__,
           mAcceleratorMode);
  return 0;
}

/*
 *  function:AcceleratorAdapt
 *  SprdPrimaryDisplayDevice::mAcceleratorMode record the actually probed
 *  available Accelerator type.
 *  DisplayDeviceAccelerator: is the Accelerator type that some special display
 *  have like primary have dispc-type,
 *  virtual do't have dispc-type.
 *  this function is used to get the intersection of these two set.
 * */
int SprdPrimaryDisplayDevice::AcceleratorAdapt(int DisplayDeviceAccelerator) {
  int value = ACCELERATOR_NON;
  HWC_IGNORE(DisplayDeviceAccelerator);
#ifdef FORCE_ADJUST_ACCELERATOR
  if (DisplayDeviceAccelerator & ACCELERATOR_DISPC) {
    if (mAcceleratorMode & ACCELERATOR_DISPC) {
      value |= ACCELERATOR_DISPC;
    }
  }

  if (DisplayDeviceAccelerator & ACCELERATOR_GSP) {
    if (mAcceleratorMode & ACCELERATOR_GSP) {
      value |= ACCELERATOR_GSP;
    }
  }

  if (DisplayDeviceAccelerator & ACCELERATOR_OVERLAYCOMPOSER) {
    if (mAcceleratorMode & ACCELERATOR_OVERLAYCOMPOSER) {
      value |= ACCELERATOR_OVERLAYCOMPOSER;
    }
  }
#else
  value |= mAcceleratorMode;
#endif

  ALOGI_IF(mDebugFlag,
           "SprdPrimaryDisplayDevice:: AcceleratorAdapt accelerator: 0x%x",
           value);
  return value;
}

#ifdef HWC_DUMP_CAMERA_SHAKE_TEST
void SprdPrimaryDisplayDevice::dumpCameraShakeTest(
    hwc_display_contents_1_t *list) {
  char value[PROPERTY_VALUE_MAX];
  if ((0 != property_get("persist.vendor.cam.performance_camera", value, "0")) &&
      (atoi(value) == 1)) {
    for (unsigned int i = 0; i < list->numHwLayers; i++) {
      hwc_layer_1_t *l = &(list->hwLayers[i]);

      if (l && ((l->flags & HWC_DEBUG_CAMERA_SHAKE_TEST) ==
                HWC_DEBUG_CAMERA_SHAKE_TEST)) {
        native_handle_t *privateH = (native_handle_t *)(l->handle);
        if (privateH == NULL) {
          continue;
        }

        void *cpuAddr = NULL;
        int offset = 0;
        int format = -1;
        int width = ADP_WIDTH(privateH);
        int height = ADP_STRIDE(privateH);
        cpuAddr = (void *)ADP_BASE(privateH);
        format = ADP_FORMAT(privateH);
        if (format == HAL_PIXEL_FORMAT_RGBA_8888) {
          int r = -1;
          int g = -1;
          int b = -1;
          int r2 = -1;
          int g2 = -1;
          int b2 = -1;
          int colorNumber = -1;

          /*
           *  read the pixel in the 1/4 of the layer
           * */
          offset = ((width >> 1) * (height >> 1)) << 2;
          uint8_t *inrgb = (uint8_t *)((int *)cpuAddr + offset);

          r = *(inrgb++);  // for r;
          g = *(inrgb++);  // for g;
          b = *(inrgb++);
          inrgb++;          // for a;
          r2 = *(inrgb++);  // for r;
          g2 = *(inrgb++);  // for g;
          b2 = *(inrgb++);

          if ((r == 205) && (g == 0) && (b == 252)) {
            colorNumber = 0;
          } else if ((r == 15) && (g == 121) && (b == 0)) {
            colorNumber = 1;
          } else if ((r == 31) && (g == 238) && (b == 0)) {
            colorNumber = 2;
          }

          ALOGD(
              "[HWComposer] will post camera shake test color:%d to LCD, 1st "
              "pixel in the middle of screen [r=%d, g=%d, b=%d], 2st "
              "pixel[r=%d, g=%d, b=%d]",
              colorNumber, r, g, b, r2, g2, b2);
        }
      }
    }
  }
}
#endif

/*
 *  Here, may has a bug: if Primary Display config changed, some struct of HWC
 * should be
 *  destroyed and then build them again.
 *  We assume that SurfaceFlinger just call syncAttributes once at bootup...
 */
int SprdPrimaryDisplayDevice::syncAttributes(SprdDisplayClient *Client, AttributesSet *dpyAttributes) {
  SprdHWLayerList *ListObj = NULL;
  if (Client == NULL || dpyAttributes == NULL || mFBInfo == NULL) {
    ALOGE("SprdPrimaryDisplayDevice:: syncAttributes Input parameter is NULL");
    return -1;
  }
#ifdef SPRD_SR
  bool resolutionChanged = ((mFBInfo->stride != dpyAttributes->xres)
                            ||(mFBInfo->fb_height != dpyAttributes->yres));
#endif
  mFBInfo->fb_width = dpyAttributes->xres;
  mFBInfo->fb_height = dpyAttributes->yres;
  // mFBInfo->format      =
  mFBInfo->stride = dpyAttributes->xres;
  mFBInfo->xdpi = dpyAttributes->xdpi;
  mFBInfo->ydpi = dpyAttributes->ydpi;

  ListObj = getHWLayerObj(Client);
  if (ListObj)
  {
    ListObj->updateFBInfo(mFBInfo);
    ListObj->setAccerlator(mUtil);
  }
  mPrimaryPlane->updateFBInfo(mFBInfo);
  mOverlayPlane->updateFBInfo(mFBInfo);
#ifdef PROCESS_VIDEO_USE_GSP
  mUtil->UpdateFBInfo(mFBInfo);
#endif
#ifdef OVERLAY_COMPOSER_GPU
  static bool OVCInit = false;
#ifdef SPRD_SR
  /*this is the better way to be compatible with MALI and IMG DDK*/
  if(OVCInit == true && resolutionChanged) {
    ALOGI("SPRD_SR mOverlayComposer->requestExitAndWait enter.");
    mOverlayComposer->requestThreadLoopExit();
    mOverlayComposer->requestExitAndWait();
    mOverlayComposer.clear();
    mOverlayComposer = NULL;
    mWindow = NULL;
    OVCInit = false;
    ALOGI("SPRD_SR mOverlayComposer->requestExitAndWait exit.");
  }
#endif

  if (OVCInit == false) {
    mWindow = new OverlayNativeWindow(mPrimaryPlane);
    if (mWindow == NULL) {
      ALOGE("Create Native Window failed, NO mem");
      return false;
    }

    if (!(mWindow->Init())) {
      ALOGE("Init Native Window failed");
      return false;
    }

    mOverlayComposer = new OverlayComposer(mPrimaryPlane, mWindow);
    if (mOverlayComposer == NULL) {
      ALOGE("new OverlayComposer failed");
      return false;
    }
    OVCInit = true;
  }
#endif

  return 0;
}

void SprdPrimaryDisplayDevice::getCapabilities(uint32_t* outCount,
                                               int32_t* /*hwc2_capability_t*/ outCapabilities)
{
  if (outCount == NULL)
  {
    ALOGE("SprdPrimaryDisplayDevice::getCapabilities outCount is NULL");
    return;
  }
  *outCount = 0;

  //if ((outCapabilities != nullptr) && (*outCount != 0))
  if (outCapabilities)
  {
    /* TODO: CAPABILITY_SIDEBAND_STREAM: implement later */
    outCapabilities[0] = CAPABILITY_INVALID;
  }
}

void SprdPrimaryDisplayDevice::DUMP(uint32_t* outSize, char* outBuffer, String8& result)
{
  //HWC_IGNORE(outSize);
  //HWC_IGNORE(outBuffer);
  if(outBuffer == NULL)
  {
    unsigned int layercount = 0;
    unsigned int i = 0;
    SprdHWLayerList *HWLayerList = NULL;
    SprdHWLayer **HWLayer = NULL;

    if(mCurrentClient)
    {
      HWLayerList = getHWLayerObj(mCurrentClient);
    }
    if(HWLayerList)
    {
      layercount = HWLayerList -> getLayerCount();
      HWLayer = HWLayerList->getLayerList();
    }

    headdump(result);

    for(i = 0; i < layercount; i++)
    {
        if(HWLayer[i])
        {
          switch (HWLayer[i]->getLayerType())
          {
            case (LAYER_OSD):
            case (LAYER_OVERLAY):
            case (LAYER_SURFACEFLINGER):
            case (LAYER_INVALIDE):
              dumpinput(HWLayer[i], result);
              break;
            default:
              ALOGE("SprdPrimaryDisplayDevice illegal layertype %d", HWLayer[i]->getLayerType());
              break;
          }
        }
      else if(HWLayer[i] == NULL && mHWCDisplayFlag == HWC_DISPLAY_FRAMEBUFFER_TARGET)
      {
        result.append("                                           ");
        result.append("Disable HWC - SF                                  \n");
        result.append("-------------------------------------------------------");
        result.append("------------------------------------------------------\n");
      }
      else
      {
        result.append("                                           ");
        result.append("DimLayer                                          \n");
        result.append("-------------------------------------------------------");
        result.append("------------------------------------------------------\n");
      }
    }

    if(mDisplayFBTarget && mCurrentClient)
    {
      mFBTargetLayer = mCurrentClient->getFBTargetLayer();
      if(mFBTargetLayer)
      {
        result.append("Output:GPU -");
        dumpout(mFBTargetLayer, result);
      }
    }
    else if(mComposedLayer)
    {
      result.append("Output:GSP -");
      dumpout(mComposedLayer, result);
    }
    *outSize = result.size();
  }

  else
  {
    memcpy(outBuffer, result.string(), result.size());
    result.clear();
  }
}

int32_t /*hwc2_error_t*/SprdPrimaryDisplayDevice::ACCEPT_DISPLAY_CHANGES(SprdDisplayClient *Client)
{
  mCurrentClient  = Client;
  SprdHWLayerList *HWLayerList = NULL;

  if (mInit == false)
  {
    ALOGE("SprdPrimaryDisplayDevice l: %d mInit is false", __LINE__);
    return ERR_BAD_DISPLAY;
  }

  if (mCurrentClient == NULL)
  {
    ALOGE("SprdPrimaryDisplayDevice l: %d SprdDisplayclient is NULL", __LINE__);
    return ERR_BAD_DISPLAY;
  }

  HWLayerList = getHWLayerObj(mCurrentClient);
  if (HWLayerList == NULL)
  {
    ALOGE("SprdPrimaryDisplayDevice l: %d SprdHWLayerList obj is NULL", __LINE__);
    return ERR_BAD_DISPLAY;
  }

  return HWLayerList->acceptGeometryChanged();
}

int32_t /*hwc2_error_t*/SprdPrimaryDisplayDevice::CREATE_LAYER(SprdDisplayClient *Client, hwc2_layer_t* outLayer)
{
  mCurrentClient  = Client;
  SprdHWLayerList *HWLayerList = NULL;

  if (mInit == false)
  {
    ALOGE("SprdPrimaryDisplayDevice l: %d mInit is false", __LINE__);
    return ERR_BAD_DISPLAY;
  }

  if (mCurrentClient == NULL)
  {
    ALOGE("SprdPrimaryDisplayDevice l: %d SprdDisplayclient is NULL", __LINE__);
    return ERR_BAD_DISPLAY;
  }

  HWLayerList = getHWLayerObj(mCurrentClient);
  if (HWLayerList == NULL)
  {
    ALOGE("SprdPrimaryDisplayDevice l: %d SprdHWLayerList obj is NULL", __LINE__);
    return ERR_BAD_DISPLAY;
  }

  return HWLayerList->createSprdLayer(outLayer);
}

int32_t /*hwc2_error_t*/SprdPrimaryDisplayDevice::DESTROY_LAYER(SprdDisplayClient *Client, hwc2_layer_t layer)
{
  mCurrentClient  = Client;
  SprdHWLayerList *HWLayerList = NULL;

  if (mInit == false)
  {
    ALOGE("SprdPrimaryDisplayDevice l: %d mInit is false", __LINE__);
    return ERR_BAD_DISPLAY;
  }

  if (mCurrentClient == NULL)
  {
    ALOGE("SprdPrimaryDisplayDevice l: %d SprdDisplayclient is NULL", __LINE__);
    return ERR_BAD_DISPLAY;
  }

  HWLayerList = getHWLayerObj(mCurrentClient);
  if (HWLayerList == NULL)
  {
    ALOGE("SprdPrimaryDisplayDevice l: %d SprdHWLayerList obj is NULL", __LINE__);
    return ERR_BAD_DISPLAY;
  }

  return HWLayerList->destroySprdLayer(layer);
}

int32_t /*hwc2_error_t*/SprdPrimaryDisplayDevice::GET_CHANGED_COMPOSITION_TYPES(
           SprdDisplayClient *Client,
           uint32_t* outNumElements, hwc2_layer_t* outLayers,
           int32_t* /*hwc2_composition_t*/ outTypes)
{
  mCurrentClient  = Client;
  SprdHWLayerList *HWLayerList = NULL;

  if (mInit == false)
  {
    ALOGE("SprdPrimaryDisplayDevice l: %d mInit is false", __LINE__);
    return ERR_BAD_DISPLAY;
  }

  if (mCurrentClient == NULL)
  {
    ALOGE("SprdPrimaryDisplayDevice l: %d SprdDisplayclient is NULL", __LINE__);
    return ERR_BAD_DISPLAY;
  }

  HWLayerList = getHWLayerObj(mCurrentClient);
  if (HWLayerList == NULL)
  {
    ALOGE("SprdPrimaryDisplayDevice l: %d SprdHWLayerList obj is NULL", __LINE__);
    return ERR_BAD_DISPLAY;
  }

  Mutex::Autolock _l(mLock);
  if (mBlank) {
    ALOGI_IF(mDebugFlag, "we don't do prepare action when in blanke state");
    return 0;
  }


  return HWLayerList->getChangedCompositionTypes(outNumElements, outLayers, outTypes);
}

int32_t /*hwc2_error_t*/SprdPrimaryDisplayDevice::GET_CLIENT_TARGET_SUPPORT(
           SprdDisplayClient *Client,
           uint32_t width, uint32_t height, int32_t /*android_pixel_format_t*/ format,
           int32_t /*android_dataspace_t*/ dataspace)
{
  mCurrentClient  = Client;

  if (mInit == false)
  {
    ALOGE("SprdPrimaryDisplayDevice l: %d mInit is false", __LINE__);
    return ERR_BAD_DISPLAY;
  }

  if (mCurrentClient == NULL)
  {
    ALOGE("SprdPrimaryDisplayDevice l: %d SprdDisplayclient is NULL", __LINE__);
    return ERR_BAD_DISPLAY;
  }

  return mCurrentClient->GET_CLIENT_TARGET_SUPPORT(width, height, format, dataspace);
}

int32_t /*hwc2_error_t*/SprdPrimaryDisplayDevice::GET_COLOR_MODES(
           SprdDisplayClient *Client,
           uint32_t* outNumModes,
           int32_t* /*android_color_mode_t*/ outModes)
{
  mCurrentClient  = Client;
  if (mInit == false)
  {
    ALOGE("SprdPrimaryDisplayDevice l: %d mInit is false", __LINE__);
    return ERR_BAD_DISPLAY;
  }

  if (mCurrentClient == NULL)
  {
    ALOGE("SprdPrimaryDisplayDevice l: %d SprdDisplayclient is NULL", __LINE__);
    return ERR_BAD_DISPLAY;
  }
  return mCurrentClient->GET_COLOR_MODES(outNumModes, outModes);
}

int32_t /*hwc2_error_t*/SprdPrimaryDisplayDevice::GET_DISPLAY_NAME(
           SprdDisplayClient *Client,
           uint32_t* outSize,
           char* outName)
{
  mCurrentClient  = Client;

  if (mInit == false)
  {
    ALOGE("SprdPrimaryDisplayDevice l: %d mInit is false", __LINE__);
    return ERR_BAD_DISPLAY;
  }

  if (mCurrentClient == NULL)
  {
    ALOGE("SprdPrimaryDisplayDevice l: %d SprdDisplayclient is NULL", __LINE__);
    return ERR_BAD_DISPLAY;
  }

  return mCurrentClient->GET_DISPLAY_NAME(outSize, outName);
}

int32_t /*hwc2_error_t*/SprdPrimaryDisplayDevice::GET_DISPLAY_REQUESTS(
           SprdDisplayClient *Client,
           int32_t* /*hwc2_display_request_t*/ outDisplayRequests,
           uint32_t* outNumElements, hwc2_layer_t* outLayers,
           int32_t* /*hwc2_layer_request_t*/ outLayerRequests)
{
  mCurrentClient  = Client;
  SprdHWLayerList *HWLayerList = NULL;

  if (mInit == false)
  {
    ALOGE("SprdPrimaryDisplayDevice l: %d mInit is false", __LINE__);
    return ERR_BAD_DISPLAY;
  }

  if (mCurrentClient == NULL)
  {
    ALOGE("SprdPrimaryDisplayDevice l: %d SprdDisplayclient is NULL", __LINE__);
    return ERR_BAD_DISPLAY;
  }

  HWLayerList = getHWLayerObj(mCurrentClient);
  if (HWLayerList == NULL)
  {
    ALOGE("SprdPrimaryDisplayDevice l: %d SprdHWLayerList obj is NULL", __LINE__);
    return ERR_BAD_DISPLAY;
  }

  Mutex::Autolock _l(mLock);
  if (mBlank) {
    ALOGI_IF(mDebugFlag, "we don't do prepare action when in blanke state");
    return 0;
  }


  return HWLayerList->getDisplayRequests(outDisplayRequests, outNumElements, outLayers, outLayerRequests);
}

int32_t /*hwc2_error_t*/SprdPrimaryDisplayDevice::GET_DISPLAY_TYPE(
           SprdDisplayClient *Client,
           int32_t* /*hwc2_display_type_t*/ outType)
{
  mCurrentClient  = Client;
  if (mInit == false)
  {
    ALOGE("SprdPrimaryDisplayDevice l: %d mInit is false", __LINE__);
    return ERR_BAD_DISPLAY;
  }

  if (mCurrentClient == NULL)
  {
    ALOGE("SprdPrimaryDisplayDevice l: %d SprdDisplayclient is NULL", __LINE__);
    return ERR_BAD_DISPLAY;
  }

  if (outType == NULL)
  {
    ALOGE("SprdPrimaryDisplayDevice::GET_DISPLAY_TYPE outType is NULL");
    return ERR_BAD_PARAMETER;
  }

  return mCurrentClient->GET_DISPLAY_TYPE(outType);
}

int32_t /*hwc2_error_t*/SprdPrimaryDisplayDevice::GET_DOZE_SUPPORT(
           SprdDisplayClient *Client,
           int32_t* outSupport)
{
  mCurrentClient  = Client;
  if (mInit == false)
  {
    ALOGE("SprdPrimaryDisplayDevice l: %d mInit is false", __LINE__);
    return ERR_BAD_DISPLAY;
  }

  if (mCurrentClient == NULL)
  {
    ALOGE("SprdPrimaryDisplayDevice l: %d SprdDisplayclient is NULL", __LINE__);
    return ERR_BAD_DISPLAY;
  }

  if (outSupport == NULL)
  {
    ALOGE("SprdPrimaryDisplayDevice::GET_DOZE_SUPPORT outSupport is NULL");
    return ERR_BAD_PARAMETER;
  }

  return mCurrentClient->GET_DOZE_SUPPORT(outSupport);
}

int32_t /*hwc2_error_t*/SprdPrimaryDisplayDevice::GET_HDR_CAPABILITIES(
           SprdDisplayClient *Client,
           uint32_t* outNumTypes,
           int32_t* /*android_hdr_t*/ outTypes, float* outMaxLuminance,
           float* outMaxAverageLuminance, float* outMinLuminance)
{
  if (mInit == false)
  {
    ALOGE("SprdPrimaryDisplayDevice l: %d mInit is false", __LINE__);
    return ERR_BAD_DISPLAY;
  }

  mCurrentClient  = Client;
  if (mInit == false)
  {
    ALOGE("SprdPrimaryDisplayDevice l: %d mInit is false", __LINE__);
    return ERR_BAD_DISPLAY;
  }

  if (mCurrentClient == NULL)
  {
    ALOGE("SprdPrimaryDisplayDevice l: %d SprdDisplayclient is NULL", __LINE__);
    return ERR_BAD_DISPLAY;
  }

  if (outNumTypes == NULL)
  {
    ALOGE("SprdPrimaryDisplayDevice::GET_HDR_CAPABILITIES outNumTypes is NULL");
    return ERR_BAD_PARAMETER;
  }

  return mCurrentClient->GET_HDR_CAPABILITIES(outNumTypes, outTypes, outMaxLuminance,
                                       outMaxAverageLuminance, outMinLuminance);
}

int32_t /*hwc2_error_t*/SprdPrimaryDisplayDevice::GET_RELEASE_FENCES(
           SprdDisplayClient *Client,
           uint32_t* outNumElements,
           hwc2_layer_t* outLayers, int32_t* outFences)
{
  uint32_t i;
  uint32_t index = 0;
  SprdHWLayerList *HWLayerObj = NULL;

  if (mInit == false)
  {
    ALOGE("SprdPrimaryDisplayDevice l: %d mInit is false", __LINE__);
    return ERR_BAD_DISPLAY;
  }

  mCurrentClient  = Client;

  if (mCurrentClient == NULL)
  {
    ALOGE("SprdPrimaryDisplayDevice l: %d SprdDisplayclient is NULL", __LINE__);
    return ERR_BAD_DISPLAY;
  }

  HWLayerObj = getHWLayerObj(mCurrentClient);
  if (HWLayerObj == NULL)
  {
    ALOGE("SprdPrimaryDisplayDevice l: %d SprdHWLayerList obj is NULL", __LINE__);
    return ERR_BAD_DISPLAY;
  }

  uint32_t LayerCount  = HWLayerObj->getLayerCount();
  LIST HWCList         = HWLayerObj->getHWCLayerList();

  if (outNumElements == NULL)
  {
    ALOGE("SprdPrimaryDisplayDevice::GET_RELEASE_FENCES outNumElements is NULL");
    return ERR_BAD_PARAMETER;
  }

  *outNumElements = LayerCount;

  if (outLayers && outFences)
  {
    int fenceFd  = mCurrentClient->getReleseFence();
    int fenceFd2 = mCurrentClient->getReleseFence2();

#ifdef ENABLE_PENDING_RELEASE_FENCE_FEATURE
    ALOGI_IF(mDebugFlag, "current fenceFd = %d;fenceFd2 = %d;", fenceFd, fenceFd2);
    int prev_fence = -1;
    int dup_fence = -1;
    if (fenceFd >0)
    {
      dup_fence = dup(fenceFd);
    }
    else if (fenceFd2 > 0)
    {
      dup_fence = dup(fenceFd2);
    }

    prev_fence = mCurrentClient->processReleaseFence(dup_fence);
    mCurrentClient->setPreReleaseFence(prev_fence);

    fenceFd = prev_fence;
    fenceFd2 = prev_fence;
    ALOGI_IF(mDebugFlag, "prev_fence = %d;dup_fence = %d;", prev_fence, dup_fence);
#endif
    //ALOGE("DisplayClient::GET_RELEASE_FENCES fenceFd:%d, fenceFd2:%d", fenceFd, fenceFd2);

    for (i = 0; i < LayerCount; i++)
    {
      SprdHWLayer *l = HWCList[i];
      int tmp_fd = -1;

      if (l == NULL)
      {
        ALOGI_IF(mDebugFlag, "SprdPrimaryDisplayDevice::GET_RELEASE_FENCES layer:%d is NULL", i);
        continue;
      }

      if (l->InitCheck())
      {
        if (l->getAccelerator() & (ACCELERATOR_GSP | ACCELERATOR_OVERLAYCOMPOSER))
        {
          tmp_fd = fenceFd2;
        }
        else
        {
          tmp_fd = fenceFd;
        }
      }
      else
      {
          /* if layer do not go to overlay, Surfaceflinger
           * still get the release fd, just give rel
           */
          tmp_fd = fenceFd2;
      }

      outLayers[index] = SprdHWLayer::remapToAndroidLayer(l);
      outFences[index] = (tmp_fd >= 0) ? dup(tmp_fd) : -1;
      index++;
      ALOGI_IF(mDebugFlag, "SprdPrimaryDisplayDevice::GET_RELEASE_FENCES layerId: 0x%p, fence[org:%d, dup:%d], Accelerator type is %x",
                (void *)outLayers[i], fenceFd, outFences[i], l->getAccelerator());
    }
  }

  return ERR_NONE;
}

int32_t /*hwc2_error_t*/SprdPrimaryDisplayDevice::SET_CLIENT_TARGET(
           SprdDisplayClient *Client,
           buffer_handle_t target,
           int32_t acquireFence, int32_t /*android_dataspace_t*/ dataspace,
           hwc_region_t damage)
{
  if (mInit == false)
  {
    ALOGE("SprdPrimaryDisplayDevice l: %d mInit is false", __LINE__);
    return ERR_BAD_DISPLAY;
  }

  mCurrentClient  = Client;

  if (mCurrentClient == NULL)
  {
    ALOGE("SprdPrimaryDisplayDevice l: %d SprdDisplayclient is NULL", __LINE__);
    return ERR_BAD_DISPLAY;
  }

  return mCurrentClient->SET_CLIENT_TARGET(target, acquireFence, dataspace, damage); 
}

int32_t /*hwc2_error_t*/SprdPrimaryDisplayDevice::SET_COLOR_MODE(
           SprdDisplayClient *Client,
           int32_t /*android_color_mode_t*/ mode)
{
  if (mInit == false)
  {
    ALOGE("SprdPrimaryDisplayDevice l: %d mInit is false", __LINE__);
    return ERR_BAD_DISPLAY;
  }

  mCurrentClient  = Client;
  if (mInit == false)
  {
    ALOGE("SprdPrimaryDisplayDevice l: %d mInit is false", __LINE__);
    return ERR_BAD_DISPLAY;
  }

  if (mCurrentClient == NULL)
  {
    ALOGE("SprdPrimaryDisplayDevice l: %d SprdDisplayclient is NULL", __LINE__);
    return ERR_BAD_DISPLAY;
  }

  return mCurrentClient->SET_COLOR_MODE(mode);
}

int32_t /*hwc2_error_t*/SprdPrimaryDisplayDevice::SET_COLOR_TRANSFORM(
           SprdDisplayClient *Client,
           const float* matrix,
           int32_t /*android_color_transform_t*/ hint)
{
  int32_t ret;

  if (mInit == false)
  {
    ALOGE("SprdPrimaryDisplayDevice l: %d mInit is false", __LINE__);
    return ERR_BAD_DISPLAY;
  }

  mCurrentClient  = Client;
  if (mInit == false)
  {
    ALOGE("SprdPrimaryDisplayDevice l: %d mInit is false", __LINE__);
    return ERR_BAD_DISPLAY;
  }

  if (mCurrentClient == NULL)
  {
    ALOGE("SprdPrimaryDisplayDevice l: %d SprdDisplayclient is NULL", __LINE__);
    return ERR_BAD_DISPLAY;
  }

  ret = mCurrentClient->SET_COLOR_TRANSFORM(matrix, hint);
  if (ret  == ERR_NONE) {
	setHasColorMatrix(false);
  } else {
	setHasColorMatrix(true);
  }
  return ret;
}

int32_t /*hwc2_error_t*/SprdPrimaryDisplayDevice::SET_POWER_MODE(
           SprdDisplayClient *Client,
           int32_t /*hwc2_power_mode_t*/ mode)
{
  HWC_IGNORE(mode);
  HWC_IGNORE(Client);

  if (mInit == false)
  {
    ALOGE("SprdPrimaryDisplayDevice l: %d mInit is false", __LINE__);
    return ERR_BAD_DISPLAY;
  }

  int32_t sprdMode = -1;

  if ((mode == HWC2_POWER_MODE_DOZE)
     || (mode == HWC2_POWER_MODE_DOZE_SUSPEND))
  {
    ALOGI_IF(mDebugFlag, "SprdPrimaryDisplayDevice::SET_POWER_MODE do not support power mode: %d", mode);
    return ERR_UNSUPPORTED;
  }
  if (mode == -1)
  {
    return ERR_BAD_PARAMETER;
  }

  Mutex::Autolock _l(mLock);
  mBlank = (mode == HWC_POWER_MODE_OFF ? 1 : 0);
  mDispCore->Blank(DISPLAY_PRIMARY, mBlank);

  return ERR_NONE;
}

int32_t /*hwc2_error_t*/SprdPrimaryDisplayDevice::VALIDATE_DISPLAY(
           SprdDisplayClient *Client,
           uint32_t* outNumTypes, uint32_t* outNumRequests, int accelerator)
{
  int32_t err = ERR_NONE;
  int displayFlag = HWC_DISPLAY_MASK;
  int acceleratorLocal = ACCELERATOR_NON;

  queryDebugFlag(&mDebugFlag);

  mCurrentClient  = Client;
  SprdHWLayerList *HWLayerList = NULL;

  if (mInit == false)
  {
    ALOGE("SprdPrimaryDisplayDevice l: %d mInit is false", __LINE__);
    return ERR_BAD_DISPLAY;
  }

  if (mCurrentClient == NULL)
  {
    ALOGE("SprdPrimaryDisplayDevice l: %d SprdDisplayclient is NULL", __LINE__);
    return ERR_BAD_DISPLAY;
  }

  HWLayerList = getHWLayerObj(mCurrentClient);
  if (HWLayerList == NULL)
  {
    ALOGE("SprdPrimaryDisplayDevice l: %d SprdHWLayerList obj is NULL", __LINE__);
    return ERR_BAD_DISPLAY;
  }

  ALOGI_IF(mDebugFlag, "HWC start prepare");

  Mutex::Autolock _l(mLock);
  if (mBlank) {
    ALOGI_IF(mDebugFlag, "we don't do prepare action when in blanke state");
    return 0;
  }

  acceleratorLocal = AcceleratorAdapt(accelerator);

#ifdef UPDATE_SYSTEM_FPS_FOR_POWER_SAVE
  UpdateSystemFps(HWLayerList->getLayerCount());
#endif

  err = HWLayerList->validateDisplay(outNumTypes, outNumRequests,
                                     acceleratorLocal, displayFlag, this);
  if (err != ERR_NONE)
  {
    ALOGE("SprdPrimaryDisplayDevice::VALIDATE_DISPLAY failed err: %d", err);
  }

  err = attachToDisplayPlane(displayFlag, HWLayerList);
  if (err != 0) {
    ALOGE("SprdPrimaryDisplayDevice:: attachToDisplayPlane failed");
  }

  return err;
}


int SprdPrimaryDisplayDevice::ActiveConfig(SprdDisplayClient *Client, DisplayAttributes *dpyAttributes) {
  if (dpyAttributes == NULL) {
    ALOGE("SprdPrimaryDisplayDevice:: ActiveConfig input para is NULL");
    return -1;
  }

  AttributesSet *attr = &(dpyAttributes->sets[dpyAttributes->configsIndex]);
  dpyAttributes->connected = true;
  syncAttributes(Client, attr);

  return 0;
}

void SprdPrimaryDisplayDevice::setHasColorMatrix(bool hasColorMatrix)
{
  mHasColorMatrix = hasColorMatrix;
}

bool SprdPrimaryDisplayDevice::getHasColorMatrix()
{
  return mHasColorMatrix;
}

int SprdPrimaryDisplayDevice::reclaimPlaneBuffer(bool condition) {
  static int ret = -1;
  enum PlaneRunStatus status = PLANE_STATUS_INVALID;

  if (condition == false) {
    mPrimaryPlane->recordPlaneIdleCount();

    status = mPrimaryPlane->queryPlaneRunStatus();
    if (status == PLANE_SHOULD_CLOSED) {
      mPrimaryPlane->close();
#ifdef OVERLAY_COMPOSER_GPU
      mWindow->releaseNativeBuffer();
#endif
    }

    ret = 0;
  } else {
    mPrimaryPlane->resetPlaneIdleCount();

    status = mPrimaryPlane->queryPlaneRunStatus();
    if (status == PLANE_CLOSED) {
      bool value = false;
      value = mPrimaryPlane->open();
      if (value == false) {
        ALOGE("open PrimaryPlane failed");
        ret = 1;
      } else {
        ret = 0;
      }
    }
  }

  return ret;
}

int SprdPrimaryDisplayDevice::attachToDisplayPlane(int DisplayFlag, SprdHWLayerList *HWLayerList) {
  int displayType = HWC_DISPLAY_MASK;
  mHWCDisplayFlag = HWC_DISPLAY_MASK;

  if (mInit == false)
  {
    ALOGE("SprdPrimaryDisplayDevice l: %d mInit is false", __LINE__);
    return ERR_BAD_DISPLAY;
  }

  if (HWLayerList == NULL)
  {
    ALOGE("SprdPrimaryDisplayDevice l: %d SprdHWLayerList obj is NULL", __LINE__);
    return ERR_BAD_DISPLAY;
  }

  unsigned int OSDLayerCount = HWLayerList->getOSDLayerCount();
  unsigned int VideoLayerCount = HWLayerList->getVideoLayerCount();
  unsigned int FBLayerCount = HWLayerList->getFBLayerCount();
  unsigned int DispCLayerCount = HWLayerList->getDispLayerCount();
  unsigned int GXPLayerCount = HWLayerList->getGXPLayerCount();
  bool &disableHWC = HWLayerList->getDisableHWCFlag();

  if (disableHWC) {
    ALOGI_IF(
        mDebugFlag,
        "SprdPrimaryDisplayDevice:: attachToDisplayPlane HWC is disabled now");
    return 0;
  }

  if ((DispCLayerCount > 0 && (DispCLayerCount == HWLayerList->getLayerCount()))) {
    displayType &= ~(HWC_DISPLAY_PRIMARY_PLANE | HWC_DISPLAY_OVERLAY_PLANE);
    displayType |= HWC_DISPLAY_DISPC;
    ALOGI_IF(mDebugFlag, "attachToDisplayPlane choose DPC, DPC L count:%d", DispCLayerCount);
  } else if ((DispCLayerCount > 0 &&
              DispCLayerCount < HWLayerList->getLayerCount()) &&
             (OSDLayerCount > 0) &&
             (GXPLayerCount > 0)) {
    displayType = HWC_DISPLAY_DISPC | HWC_DISPLAY_PRIMARY_PLANE;
    ALOGI_IF(mDebugFlag, "attachToDisplayPlane choose DPC&PP, DPC L count:%d, PP L count:%d",
              DispCLayerCount, OSDLayerCount);
  } else if ((DispCLayerCount > 0 &&
              DispCLayerCount < HWLayerList->getLayerCount()) &&
             (VideoLayerCount > 0) &&
             (GXPLayerCount > 0)) {
    displayType = HWC_DISPLAY_DISPC | HWC_DISPLAY_OVERLAY_PLANE;
    ALOGI_IF(mDebugFlag, "attachToDisplayPlane choose DPC&OP, DPC L count:%d, OP L count:%d",
              DispCLayerCount, VideoLayerCount);
  } else if ((DispCLayerCount > 0 &&
              DispCLayerCount < HWLayerList->getLayerCount()) &&
             (OSDLayerCount > 0) &&
             (VideoLayerCount > 0) &&
             (GXPLayerCount > 0)) {
    displayType = HWC_DISPLAY_DISPC | HWC_DISPLAY_OVERLAY_PLANE |
                  HWC_DISPLAY_PRIMARY_PLANE;
    ALOGI_IF(mDebugFlag, "attachToDisplayPlane choose DPC&OP&OP, DPC L count:%d, PPLcount:%d, OPLcount:%d",
              DispCLayerCount, OSDLayerCount, VideoLayerCount);
  } else if ((VideoLayerCount > 0) && (GXPLayerCount > 0)) {
    displayType = HWC_DISPLAY_OVERLAY_PLANE;

    ALOGI_IF(mDebugFlag, "attachToDisplayPlane choose OP, DPC L count:%d, PPLcount:%d, OPLcount:%d",
              DispCLayerCount, OSDLayerCount, VideoLayerCount);
  } else if (DisplayFlag & HWC_DISPLAY_OVERLAY_COMPOSER_GPU) {
    displayType &= ~(HWC_DISPLAY_PRIMARY_PLANE | HWC_DISPLAY_OVERLAY_PLANE |
                     HWC_DISPLAY_DISPC);
    displayType |= DisplayFlag;
    ALOGI_IF(mDebugFlag, "attachToDisplayPlane choose OVC");
  } else if (DispCLayerCount > 0) {
    displayType |= HWC_DISPLAY_DISPC;
  } else if (GXPLayerCount > 0) {
    displayType = HWC_DISPLAY_PRIMARY_PLANE;
  } else if (FBLayerCount > 0) {
    ALOGI_IF(mDebugFlag, "attachToDisplayPlane choose SF");
    displayType |= (0x1) & HWC_DISPLAY_FRAMEBUFFER_TARGET;
  } else if (HWLayerList->getLayerCount() < 1) {
    ALOGI_IF(mDebugFlag, "attachToDisplayPlane, no visible layers, total layer count:%d",
              HWLayerList->getLayerCount());
    displayType |= HWC_DISPLAY_NO_DATA;
  } else {
    displayType &= ~HWC_DISPLAY_FRAMEBUFFER_TARGET;
  }

  mHWCDisplayFlag |= displayType;

  return 0;
}

int SprdPrimaryDisplayDevice::WrapOverlayLayer(native_handle_t *buf,
                                               int format, float planeAlpha, int fenceFd,
                                               int32_t blendMode, uint32_t zorder,
                                               SprdHWLayer *l) {
  struct sprdRectF *src = NULL;
  struct sprdRect *fb = NULL;

  if (buf == NULL) {
    ALOGE("WrapOverlayLayer buf is NULL");
    return -1;
  }

  if (mComposedLayer)
  {
     delete mComposedLayer;
     mComposedLayer = NULL;
  }

  mComposedLayer =
      new SprdHWLayer(buf, format, planeAlpha, blendMode, 0x00, fenceFd, zorder);

  src = mComposedLayer->getSprdSRCRectF();
  fb  = mComposedLayer->getSprdFBRect();

  if (l)
  {
#if 0
    src->x = l->getSprdSRCRect()->x;
    src->y = l->getSprdSRCRect()->y;
    src->w = l->getSprdSRCRect()->right  - src->x;
    src->h = l->getSprdSRCRect()->bottom - src->y;
#endif
    src->x = l->getSprdFBRect()->x;
    src->y = l->getSprdFBRect()->y;
    src->w = l->getSprdFBRect()->right  - src->x;
    src->h = l->getSprdFBRect()->bottom - src->y;
    src->right  = l->getSprdFBRect()->right;
    src->bottom = l->getSprdFBRect()->bottom;

    fb->x  = l->getSprdFBRect()->left;
    fb->y  = l->getSprdFBRect()->top;
    fb->w  = l->getSprdFBRect()->right  - fb->x;
    fb->h  = l->getSprdFBRect()->bottom - fb->y;
    fb->right  = l->getSprdFBRect()->right;
    fb->bottom = l->getSprdFBRect()->bottom;
  }
  else
  {
    src->x = 0; 
    src->y = 0;
    src->w = ADP_WIDTH(buf);
    src->h = ADP_HEIGHT(buf);
    src->right  = src->x + src->w;
    src->bottom = src->y + src->h;

    fb->x  = 0;
    fb->y  = 0;
    fb->w  = ADP_WIDTH(buf);
    fb->h  = ADP_HEIGHT(buf);
    fb->right  = fb->x + fb->w;
    fb->bottom = fb->y + fb->h;
  }

  if (AddPresentLayerList(&mComposedLayer, 1) != 0)
  {
    ALOGE("WrapOverlayLayer AddPresentLayerList failed");
    return -1;
  }

  return 0;
}

int SprdPrimaryDisplayDevice::AddPresentLayerList(SprdHWLayer **list, int32_t count)
{
  static bool initFlag = false;
  static int32_t MAX_COUNT  = DEFAULT_PRESENT_LAYER_COUNT;
  int32_t currentCount = 0;
  int recordCount = 0;
  SprdHWLayer **tmpList = NULL;

  if (list == NULL || count <= 0)
  {
    ALOGE("SprdPrimaryDisplayDevice::AddPresentLayerList input para is NULL");
    return -1;
  }

  currentCount = mPresentLayerCount;
  mPresentLayerCount += count;

  if (currentCount > mPresentLayerCount)
  {
    ALOGE("SprdPrimaryDisplayDevice::AddPresentLayerList Present layer count error");
    return -1;
  }

  if (mPresentLayerCount > MAX_COUNT)
  {
    initFlag  = false;
    MAX_COUNT = mPresentLayerCount;
  }

  if (initFlag == false)
  {
    tmpList = new SprdHWLayer*[MAX_COUNT];
    if (tmpList == NULL)
    {
      ALOGE("SprdPrimaryDisplayDevice::AddPresentLayerList new SprdHWLayer* failed");
      return -1;
    }

    if (currentCount > 0 && mPresentList)
    {
      for (int32_t j = 0; j < currentCount; j++)
      {
        if (mPresentList[j])
        {
          tmpList[j] = mPresentList[j];
        }
      }
    }

    if (mPresentList)
    {
      delete [] mPresentList;
      mPresentList = NULL;
    }

    mPresentList = tmpList;

    initFlag = true;
  }

  for (uint32_t i = 0; i < ((uint32_t)count); i++)
  {
    SprdHWLayer *inL = list[i];

    if (inL == NULL)
    {
      ALOGI_IF(mDebugFlag, "AddPresentLayerList input layer is NULL");
      continue;
    }

    if (i == inL->getZOrder())
    {
      /* TODO: merged z order if a layer has been in PresentList. */
      if (mPresentList[i])
      {
        /* TODO: Implement z order merged function, use replacement temporarily */
      }

      mPresentList[i] = list[i];
      recordCount++;
    } else {
      if (inL->getAccelerator() == ACCELERATOR_DISPC) {
        mPresentList[i + 1] = list[i];
      } else {
        mPresentList[inL->getZOrder()] = list[i];
      }
      recordCount++;

    }
    ALOGI_IF(mDebugFlag, "AddPresentLayerList L:%p, zorder:%d", (void *)inL, inL->getZOrder());
  }

  if (recordCount != count)
  {
    ALOGE("AddPresentLayerList failed, user req count:%d, comsume count:%d", count, recordCount);
    mPresentLayerCount -= (count > recordCount) ? (count - recordCount) : 0;
  }

  //ALOGI("AddPresentLayerList L:%p, zorder:%d", (void *)inL, inL->getZOrder());

  return 0;
}

int SprdPrimaryDisplayDevice::RemoveLayerInPresentList()
{
  uint32_t count = getPresentLayerCount();

  for (uint32_t i = 0; i < count; i++)
  {
    mPresentList[i] = NULL;
    mPresentLayerCount--;
  }


  return 0;
}

int SprdPrimaryDisplayDevice::SprdUtilScheldule(SprdUtilSource *Source,
                                                SprdUtilTarget *Target) {
  if (Source == NULL || Target == NULL) {
    ALOGE("SprdUtilScheldule input source/target is NULL");
    return -1;
  }

#ifdef TRANSFORM_USE_DCAM
  mUtil->transformLayer(Source, Target);
#endif

#ifdef PROCESS_VIDEO_USE_GSP
  if (mOverlayPlane->online()) {
    Target->format = mOverlayPlane->getPlaneFormat();
  } else if (mPrimaryPlane->online()) {
    Target->format = mPrimaryPlane->getPlaneFormat();
  }

  // if(mUtil->composerLayers(Source, Target))
  if (mUtil->composeLayerList(Source, Target)) {
    ALOGE("%s[%d],composerLayers ret err!!", __func__, __LINE__);
  } else {
    ALOGI_IF(mDebugFlag, "%s[%d],composerLayers success", __func__, __LINE__);
  }
#endif

  return 0;
}

#if OVERLAY_COMPOSER_GPU
int SprdPrimaryDisplayDevice::OverlayComposerScheldule(
    SprdHWLayer **list, uint32_t layerCount, SprdDisplayPlane *DisplayPlane, SprdHWLayer *FBTargetLayer) {
  int acquireFenceFd = -1;
  int format = -1;
  native_handle_t *buf = NULL;

  if (list == NULL || DisplayPlane == NULL) {
    ALOGE("OverlayComposerScheldule input para is NULL");
    return -1;
  }

  ALOGI_IF(mDebugFlag, "Start OverlayComposer composition misson");

  mOverlayComposer->onComposer(list, layerCount, FBTargetLayer);
  buf = DisplayPlane->flush(&acquireFenceFd);
  format = DisplayPlane->getPlaneFormat();

  if (WrapOverlayLayer(buf, format, 1.0, acquireFenceFd, BLEND_MODE_NONE, 0, NULL)) {
    ALOGE("OverlayComposerScheldule WrapOverlayLayer failed");
    return -1;
  }

  return 0;
}
#endif

#ifdef UPDATE_SYSTEM_FPS_FOR_POWER_SAVE
#include "../../FileOp.h"

void SprdPrimaryDisplayDevice::UpdateSystemFps(int systemLayerCount) {
  bool isReduceFps = false;
  int layerCount = 0;
  FileOp fileop;

  char value[PROPERTY_VALUE_MAX];
  property_get("vendor.cam.lowpower.display.30fps", value, "false");
  if (!strcmp(value, "true")) {
    isReduceFps = true;
    ALOGI_IF(mDebugFlag, "try reduce system fps");
  }

  layerCount = systemLayerCount;
  if (layerCount == 1 && isReduceFps == true) {
    if (mIsPrimary60Fps == true) {
      ALOGI_IF(mDebugFlag, "set primary device to 30fps");
      fileop.SetFPS(30);
    }
    mIsPrimary60Fps = false;
  } else if (mIsPrimary60Fps == false) {
    mIsPrimary60Fps = true;
    ALOGI_IF(mDebugFlag, "set primary device to 60fps");
    fileop.SetFPS(60);
  }
}
#endif

int SprdPrimaryDisplayDevice::commit(SprdDisplayClient *Client) {
  HWC_TRACE_CALL;
  int ret = -1;
  int OverlayBufferFenceFd = -1;
  int PrimaryBufferFenceFd = -1;
  bool PrimaryPlane_Online = false;
  PlaneContext *PrimaryContext = NULL;
  PlaneContext *OverlayContext = NULL;
  SprdDisplayPlane *DisplayPlane = NULL;
  int totalLayerCount = 0;
  int32_t blendMode = BLEND_MODE_NONE;
  int OVCFormat = 0;
  uint32_t zorder = 0;
  int DumpFlag = 0;
  int i = 0;
  float GXPPlaneAlpha = 1.0;

  mCurrentClient  = Client;
  SprdHWLayerList *HWLayerList = NULL;

  if (mInit == false)
  {
    ALOGE("SprdPrimaryDisplayDevice l: %d mInit is false", __LINE__);
    return ERR_BAD_DISPLAY;
  }

  if (mCurrentClient == NULL)
  {
    ALOGE("SprdPrimaryDisplayDevice l: %d SprdDisplayclient is NULL", __LINE__);
    return ERR_BAD_DISPLAY;
  }
  mCurrentClient->closeRelFence();
#ifdef ENABLE_PENDING_RELEASE_FENCE_FEATURE
  mCurrentClient->closePreRelFence();
#endif

  HWLayerList = getHWLayerObj(mCurrentClient);
  if (HWLayerList == NULL)
  {
    ALOGE("SprdPrimaryDisplayDevice l: %d SprdHWLayerList obj is NULL", __LINE__);
    return ERR_BAD_DISPLAY;
  }


  int GXPLayerCount = HWLayerList->getGXPLayerCount();
  SprdHWLayer **GXPLayerList = HWLayerList->getSprdGXPLayerList();

  int DispCLayerCount = HWLayerList->getDispLayerCount();
  SprdHWLayer **DispCLayerList = HWLayerList->getDispCLayerList();

  totalLayerCount = HWLayerList->getLayerCount();

  mDisplayFBTarget = false;
  mDisplayPrimaryPlane = false;
  mDisplayOverlayPlane = false;
  mDisplayOVC = false;
  mDisplayDispC = false;
  mDisplayNoData = false;
  mSchedualUtil = false;
  mUtilSource->LayerList = NULL;
  mUtilSource->LayerCount = 0;
  mUtilSource->releaseFenceFd = -1;
  mUtilTarget->buffer = NULL;
  mUtilTarget->acquireFenceFd = -1;
  mUtilTarget->releaseFenceFd = -1;

  Mutex::Autolock _l(mLock);
  if (mBlank) {
    ALOGI_IF(mDebugFlag, "we don't do commit action when in blanke state");
    return ERR_NO_JOB;
  }

  if (totalLayerCount < 1 && mFirstFrameFlag) {
    ALOGI_IF(mDebugFlag, "we don't do commit action when only has FBT");
    return 0;
  }

  if (mFirstFrameFlag) {
    ALOGI_IF(mDebugFlag, "set mFirstFrameFlag to false");
    mFirstFrameFlag = false;
  }

  ALOGI_IF(mDebugFlag, "HWC start commit display flag:0x%x", mHWCDisplayFlag);

  switch ((mHWCDisplayFlag & ~HWC_DISPLAY_MASK)) {
    case (HWC_DISPLAY_FRAMEBUFFER_TARGET):
      mDisplayFBTarget = true;
      break;
    case (HWC_DISPLAY_PRIMARY_PLANE):
      mDisplayPrimaryPlane = true;
      break;
    case (HWC_DISPLAY_OVERLAY_PLANE):
      mDisplayOverlayPlane = true;
      break;
    case (HWC_DISPLAY_PRIMARY_PLANE | HWC_DISPLAY_OVERLAY_PLANE):
      mDisplayPrimaryPlane = true;
      mDisplayOverlayPlane = true;
      break;
    case (HWC_DISPLAY_OVERLAY_COMPOSER_GPU):
      mDisplayOVC = true;
      break;
    case (HWC_DISPLAY_DISPC):
      mDisplayDispC = true;
      break;
    case (HWC_DISPLAY_DISPC | HWC_DISPLAY_PRIMARY_PLANE):
      mDisplayDispC = true;
      mDisplayPrimaryPlane = true;
      break;
    case (HWC_DISPLAY_DISPC | HWC_DISPLAY_OVERLAY_PLANE):
      mDisplayDispC = true;
      mDisplayOverlayPlane = true;
      break;
    case (HWC_DISPLAY_DISPC | HWC_DISPLAY_OVERLAY_PLANE |
          HWC_DISPLAY_PRIMARY_PLANE):
      mDisplayDispC = true;
      mDisplayOverlayPlane = true;
      mDisplayPrimaryPlane = true;
      break;
    case (HWC_DISPLAY_FRAMEBUFFER_TARGET | HWC_DISPLAY_OVERLAY_PLANE):
      mDisplayFBTarget = true;
      mDisplayOverlayPlane = true;
      break;
    case (HWC_DISPLAY_NO_DATA):
      mDisplayNoData = true;
      break;
    default:
      ALOGI_IF(mDebugFlag, "Display type: %d, use FBTarget",
               (mHWCDisplayFlag & ~HWC_DISPLAY_MASK));
      mDisplayFBTarget = true;
      break;
  }

  mPresentState = false;
  RemoveLayerInPresentList();

  if (mComposedLayer) {
    delete mComposedLayer;
    mComposedLayer = NULL;
  }

  if (mDisplayNoData) {
    return 0;
  }

  if (mDisplayDispC)
  {
    AddPresentLayerList(DispCLayerList, DispCLayerCount);
  }

  /*
   *  This is temporary methods for displaying Framebuffer target layer, has
   * some bug in FB HAL.
   *  ====     start   ================
   * */
  if (mDisplayFBTarget) {
    mFBTargetLayer = mCurrentClient->getFBTargetLayer();
    if (mFBTargetLayer == NULL)
    {
      ALOGI_IF(mDebugFlag, "SprdPrimaryDisplayDevice::commit FBTarget layer is NULL, no job to do");
      return ERR_NO_JOB;
    }

    struct sprdRectF *src = NULL;
    struct sprdRect *fb = NULL;

    src = mFBTargetLayer->getSprdSRCRectF();
    fb  = mFBTargetLayer->getSprdFBRect();
    src->x = 0;
    src->y = 0;
    src->w = mFBInfo->fb_width;
    src->h = mFBInfo->fb_height;
    src->right  = src->x + src->w;
    src->bottom = src->y + src->h;

    fb->x = 0;
    fb->y = 0;
    fb->w = mFBInfo->fb_width;
    fb->h = mFBInfo->fb_height;
    fb->right  = fb->x + fb->w;
    fb->bottom = fb->y + fb->h;

    bool  hasColorMatrix;
    hasColorMatrix = getHasColorMatrix();
    mFBTargetLayer->setHasColorMatrix(hasColorMatrix);

    if (AddPresentLayerList(&mFBTargetLayer, 1) != 0)
    {
      ALOGE("SprdPrimaryDisplayDevice::commit AddPresentLayerList failed");
    }

    queryDumpFlag(&DumpFlag);
    if (DumpFlag & HWCOMPOSER_DUMP_FRAMEBUFFER_FLAG)
    {
      dumpFrameBuffer(mFBTargetLayer);
    }

    goto DisplayDone;
  }
  /*
   *  ==== end ========================
   * */

  /*
  static int64_t now = 0, last = 0;
  static int flip_count = 0;
  flip_count++;
  now = systemTime();
  if ((now - last) >= 1000000000LL)
  {
      float fps = flip_count*1000000000.0f/(now-last);
      ALOGI("HWC post FPS: %f", fps);
      flip_count = 0;
      last = now;
  }
  */

  OverlayContext = mOverlayPlane->getPlaneContext();
  PrimaryContext = mPrimaryPlane->getPlaneContext();

#ifdef OVERLAY_COMPOSER_GPU
  if (mDisplayOVC) {
    DisplayPlane = mOverlayComposer->getDisplayPlane();
    OverlayComposerScheldule(HWLayerList->getOVCLayerList(), 
                             HWLayerList->getOVCLayerCount(),
                             DisplayPlane, mFBTargetLayer);
    goto DisplayDone;
  }
#endif

  if (mDisplayOverlayPlane && (!OverlayContext->DirectDisplay)) {
    mUtilTarget->buffer = mOverlayPlane->dequeueBuffer(&OverlayBufferFenceFd);
    mUtilTarget->releaseFenceFd = OverlayBufferFenceFd;

    mSchedualUtil = true;
  } else {
    mOverlayPlane->disable();
  }

#ifdef PROCESS_VIDEO_USE_GSP
  PrimaryPlane_Online = (mDisplayPrimaryPlane && (!mDisplayOverlayPlane));
#else
  PrimaryPlane_Online = mDisplayPrimaryPlane;
#endif

  if (PrimaryPlane_Online) {
    mPrimaryPlane->dequeueBuffer(&PrimaryBufferFenceFd);

    if (PrimaryContext->DirectDisplay == false) {
      mUtilTarget->buffer2 = mPrimaryPlane->getPlaneBuffer();
      mUtilTarget->releaseFenceFd = PrimaryBufferFenceFd;

      mSchedualUtil = true;
    }
  } else {
    /*
     *  Use GSP to do 2 layer blending, so if PrimaryLayer is not NULL,
     *  disable DisplayPrimaryPlane.
     * */
    mPrimaryPlane->disable();
    mDisplayPrimaryPlane = false;
  }

  if (mSchedualUtil) {
    mUtilSource->LayerList = GXPLayerList;
    mUtilSource->LayerCount = GXPLayerCount;

    SprdUtilScheldule(mUtilSource, mUtilTarget);
    ALOGI_IF(mDebugFlag,
             "<02-1> SprdUtilScheldule() return, src rlsFd:%d, dst "
             "acqFd:%d,dst rlsFd:%d",
             mUtilSource->releaseFenceFd, mUtilTarget->acquireFenceFd,
             mUtilTarget->releaseFenceFd);

#ifdef HWC_DUMP_CAMERA_SHAKE_TEST
    dumpCameraShakeTest(list);
#endif

    if (mWindow != NULL)
    {
      mWindow->notifyDirtyTarget(true);
    }
  }

  if (mOverlayPlane->online()) {
    int acquireFenceFd = -1;
    native_handle_t *buf = NULL;
    SprdHWLayer *l = NULL;

    if (GXPLayerCount > 0)
    {
      l = GXPLayerList[0];
    }

    ret = mOverlayPlane->queueBuffer(mUtilTarget->acquireFenceFd);
    buf = mOverlayPlane->flush(&acquireFenceFd);

    if (ret != 0) {
      ALOGE("OverlayPlane::queueBuffer failed");
      return -1;
    }

    if (GXPLayerCount > 1) {
      GXPPlaneAlpha = GXPLayerList[1]->getPlaneAlphaF() + \
                             (1 - GXPLayerList[1]->getPlaneAlphaF()) * \
                             GXPLayerList[0]->getPlaneAlphaF();
      for (i = 2; i < GXPLayerCount; i++) {
        GXPPlaneAlpha = GXPLayerList[i]->getPlaneAlphaF() + \
                               (1 - GXPLayerList[i]->getPlaneAlphaF()) * \
                               GXPPlaneAlpha;
      }
    } else if (GXPLayerCount == 1) {
      GXPPlaneAlpha = GXPLayerList[0]->getPlaneAlphaF();
    }

    if (GXPLayerCount == 1 && GXPLayerList[0]) {
        blendMode = l->getBlendMode();
        OVCFormat = mUtilTarget->format;
        zorder    = l->getZOrder();
        WrapOverlayLayer(buf, OVCFormat, GXPPlaneAlpha, acquireFenceFd,
           blendMode, zorder, l);
    }
    else {
        blendMode = SPRD_HWC_BLENDING_PREMULT;
        OVCFormat = mUtilTarget->format;
        zorder    = l->getZOrder();
        WrapOverlayLayer(buf, OVCFormat, GXPPlaneAlpha, acquireFenceFd,
           blendMode, zorder, NULL);
    }

    if (mComposedLayer == NULL) {
      ALOGE("OverlayPlane Wrap ComposedLayer failed");
      return -1;
    }
  } else if (mPrimaryPlane->online()) {
    int acquireFenceFd = -1;
    native_handle_t *buf = NULL;
    SprdHWLayer *l = NULL;

    if (GXPLayerCount > 0)
    {
      l = GXPLayerList[0];
    }

    ret = mPrimaryPlane->queueBuffer(mUtilTarget->acquireFenceFd);

    buf = mPrimaryPlane->flush(&acquireFenceFd);

    if (ret != 0) {
      ALOGE("PrimaryPlane::queueBuffer failed");
      return -1;
    }

    if (GXPLayerCount > 1) {
      GXPPlaneAlpha = GXPLayerList[1]->getPlaneAlphaF() + \
                             (1 - GXPLayerList[1]->getPlaneAlphaF()) * \
                             GXPLayerList[0]->getPlaneAlphaF();
      for (i = 2; i < GXPLayerCount; i++) {
        GXPPlaneAlpha = GXPLayerList[i]->getPlaneAlphaF() + \
                               (1 - GXPLayerList[i]->getPlaneAlphaF()) * \
                               GXPPlaneAlpha;
      }
    } else if (GXPLayerCount == 1) {
      GXPPlaneAlpha = GXPLayerList[0]->getPlaneAlphaF();
    }

    if (GXPLayerCount == 1 && GXPLayerList[0]) {
        blendMode = l->getBlendMode();
        OVCFormat = mUtilTarget->format;
        zorder    = l->getZOrder();
        WrapOverlayLayer(buf, OVCFormat, GXPPlaneAlpha, acquireFenceFd,
           blendMode, zorder, l);
    }
    else {
        blendMode = SPRD_HWC_BLENDING_PREMULT;
        OVCFormat = mUtilTarget->format;
        zorder    = l->getZOrder();
        WrapOverlayLayer(buf, OVCFormat, GXPPlaneAlpha, acquireFenceFd,
           blendMode, zorder, NULL);
    }

    if (mComposedLayer == NULL) {
      ALOGE("PrimaryPlane Wrap ComposedLayer failed");
      return -1;
    }
  }

DisplayDone:
  mPresentState = true;

  mDispCore->AddFlushData(DISPLAY_PRIMARY,
                            getPresentLayerList(), getPresentLayerCount());

  return 0;
}

int SprdPrimaryDisplayDevice::buildSyncData(SprdDisplayClient *Client,
                                            struct DisplayTrack *tracker,
                                            int32_t* outRetireFence) {
  int HWCReleaseFenceFd = -1;      // src rel
  int fenceFd = -1;
  SprdDisplayPlane *DisplayPlane = NULL;
  PlaneContext *PrimaryContext = NULL;
  PlaneContext *OverlayContext = NULL;
  OverlayContext = mOverlayPlane->getPlaneContext();
  PrimaryContext = mPrimaryPlane->getPlaneContext();
  SprdHWLayer **PresentLayerList = getPresentLayerList();
#ifdef OVERLAY_COMPOSER_GPU
  if(mOverlayComposer)
          DisplayPlane = mOverlayComposer->getDisplayPlane();
#endif
  mCurrentClient  = Client;
  SprdHWLayerList *HWLayerList = NULL;

  if (mInit == false)
  {
    ALOGE("SprdPrimaryDisplayDevice l: %d mInit is false", __LINE__);
    return ERR_BAD_DISPLAY;
  }

  if (mCurrentClient == NULL)
  {
    ALOGE("SprdPrimaryDisplayDevice l: %d SprdDisplayclient is NULL", __LINE__);
    return ERR_BAD_DISPLAY;
  }

  HWLayerList = getHWLayerObj(mCurrentClient);
  if (HWLayerList == NULL)
  {
    ALOGE("SprdPrimaryDisplayDevice l: %d SprdHWLayerList obj is NULL", __LINE__);
    return ERR_BAD_DISPLAY;
  }

  if (tracker == NULL) {
    ALOGE("SprdPrimaryDisplayDevice:: buildSyncData input para is NULL");
    return -1;
  }

  if (tracker->releaseFenceFd == -1 || tracker->retiredFenceFd == -1) {
    ALOGI_IF(mDebugFlag, "SprdPrimaryDisplayDevice:: buildSyncData input fencefd illegal");
  }

  HWCReleaseFenceFd = tracker->releaseFenceFd;
  if (mDisplayFBTarget) {
    goto FBTPath;
  }

  if ((mUtilSource->releaseFenceFd >= 0) && mSchedualUtil) {
     if (HWCReleaseFenceFd >= 0) {
      fenceFd = FenceMerge("DPUGSP", HWCReleaseFenceFd,
                                mUtilSource->releaseFenceFd);  // OV-GSP/GPP
      HWCReleaseFenceFd = fenceFd;
      closeFence(&mUtilSource->releaseFenceFd);
    }
  }

  if (mDisplayDispC) {
    mCurrentClient->setReleaseFence(dup(tracker->releaseFenceFd));
  }

  if (mDisplayOverlayPlane) {
    mOverlayPlane->InvalidatePlane();
    mOverlayPlane->addFlushReleaseFence(tracker->releaseFenceFd);
  }

  if (mDisplayPrimaryPlane) {
    mPrimaryPlane->InvalidatePlane();
    mPrimaryPlane->addFlushReleaseFence(tracker->releaseFenceFd);
  }

  if (mDisplayOVC && DisplayPlane != NULL && !mSchedualUtil) {
    DisplayPlane->InvalidatePlane();
    DisplayPlane->addFlushReleaseFence(tracker->releaseFenceFd);
    int  getReleaseFenceFd = mOverlayComposer->getReleaseFence();
    /* NOTE: We cannot use GSP/DPU and OVC at the same time */
    if ((HWCReleaseFenceFd >= 0) && (getReleaseFenceFd >= 0)) {
      fenceFd = FenceMerge("OVCGSP", HWCReleaseFenceFd,
                getReleaseFenceFd);
      close(getReleaseFenceFd);
      getReleaseFenceFd = -1;
      mOverlayComposer->closeRelFence();
      HWCReleaseFenceFd = fenceFd;
    }
    else if (getReleaseFenceFd >= 0) {
      HWCReleaseFenceFd = getReleaseFenceFd;
    }
  }

FBTPath:
  if (HWCReleaseFenceFd >= 0)
  {
    mCurrentClient->setReleaseFence2(dup(HWCReleaseFenceFd));
  }

  if(tracker->retiredFenceFd > 0)
    *outRetireFence = mCurrentClient->processRetiredFence(
                    dup(tracker->retiredFenceFd));

  closeAcquireFDs(HWLayerList->getHWCLayerList(), mDebugFlag);

  if (HWCReleaseFenceFd >= 0) {
    ALOGI_IF(mDebugFlag, "<10> close src rel parent:%d.", HWCReleaseFenceFd);
    closeFence(&HWCReleaseFenceFd);
  }

  if (mFBTargetLayer)
  {
    if (mFBTargetLayer->getAcquireFence() >= 0)
    {
      ALOGI_IF(mDebugFlag, "close FBT layer acquirefence:%d", mFBTargetLayer->getAcquireFence());
      closeFence(mFBTargetLayer->getAcquireFencePointer());
    }
  }

  return 0;
}
