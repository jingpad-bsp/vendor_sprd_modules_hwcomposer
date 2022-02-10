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
 ** File:SprdVirtualDisplayDevice.cpp DESCRIPTION                             *
 **                                   Manager Virtual Display device.         *
 ******************************************************************************
 ******************************************************************************
 ** Author:         zhongjun.chen@spreadtrum.com                              *
 *****************************************************************************/

#include "SprdVirtualDisplayDevice.h"
#include "../SprdTrace.h"

#include "SprdHWC2DataType.h"

#define HWC_NEGTIVE -1
using namespace android;

SprdVirtualDisplayDevice:: SprdVirtualDisplayDevice()
    : mDisplayPlane(0),
      mHandleLayer(0),
      mBlit(NULL),
      mClientCount(MAX_VDISPLAY_CLIENT),
      mHWCCopy(false),
      mDebugFlag(0),
      mDumpFlag(0)
{

}

SprdVirtualDisplayDevice:: ~SprdVirtualDisplayDevice()
{
    if (mDisplayPlane)
    {
        delete mDisplayPlane;
        mDisplayPlane = NULL;
    }

    if (mHandleLayer)
    {
      delete mHandleLayer;
      mHandleLayer = NULL;
    }

    for (uint32_t i = 0; i < mClientCount; i++)
    {
      SprdVDLayerList   *VDList = NULL;
      if (NULL != mClient[i])
      {
        VDList = getHWLayerObj(mClient[i]);
        if (VDList)
        {
          delete VDList;
          VDList = NULL;
        }

        delete mClient[i];
        mClient[i] = NULL;
      }
    }
}

int SprdVirtualDisplayDevice:: Init()
{
    mDisplayPlane = new SprdVirtualPlane();
    if (mDisplayPlane == NULL)
    {
        ALOGE("SprdVirtualDisplayDevice:: Init allocate SprdVirtualPlane failed");
        return -1;
    }
/*
    mBlit = new SprdWIDIBlit(mDisplayPlane);
    if (mBlit == NULL)
    {
        ALOGE("SprdVirtualDisplayDevice:: Init allocate SprdWIDIBlit failed");
        return -1;
    }
*/
    mHandleLayer = new SprdHandleLayer();
    if (mHandleLayer == NULL)
    {
      ALOGE("new SprdHandleLayer failed");
      return false;
    }

   for (uint32_t i = 0; i < mClientCount; i++)
   {
       mClient[i] = NULL;
   }

#ifdef FORCE_HWC_COPY_FOR_VIRTUAL_DISPLAYS
    mHWCCopy = true;
#else
    mHWCCopy = false;
#endif

    return 0;
}

void SprdVirtualDisplayDevice::DUMP(uint32_t* outSize, char* outBuffer, String8& result)
{
  HWC_IGNORE(outSize);
  HWC_IGNORE(outBuffer);
}

int32_t /*hwc2_error_t*/SprdVirtualDisplayDevice::CREATE_VIRTUAL_DISPLAY(
            uint32_t width, uint32_t height,
            int32_t* /*android_pixel_format_t*/ format, hwc2_display_t* outDisplay)
{
  char name[30];
  uint32_t index = 0;
  SprdDisplayClient *Client = NULL;
  DisplayAttributes *Att    = NULL;
  SprdVDLayerList   *VDList = NULL;

  if (format == NULL || outDisplay == NULL)
  {
    ALOGE("CREATE_VIRTUAL_DISPLAY input para is NULL");
    return ERR_BAD_PARAMETER;
  }

  for (uint32_t i = 0; i < mClientCount; i++)
  {
    if (NULL == mClient[i])
    {
      index = i;
      break;
    }
  }

  Client = new SprdDisplayClient(DISPLAY_VIRTUAL_ID, DISPLAY_TYPE_VIRTUAL);
  if (Client == NULL)
  {
    ALOGE("CREATE_VIRTUAL_DISPLAY[%d]", index);
    return ERR_NO_RESOURCES;
  }
  sprintf(name, "SprdVirtualDisplay%d", index);
  if (Client->Init(name) != true)
  {
    ALOGE("CREATE_VIRTUAL_DISPLAY[%d] Init failed", index);
    return ERR_NO_RESOURCES;
  }

  Att = Client->getDisplayAttributes();
  if (Att == NULL)
  {
    ALOGE("SprdVirtualDisplayDevice line: %d cannot get DisplayAttributes", __LINE__);
    return ERR_BAD_DISPLAY;
  }

  Att->configIndexSets[0]              = 0;
  Att->configsIndex                    = 0;
  Att->connected                       = false;
  Att->AcceleratorMode                 = ACCELERATOR_NON;
  Att->sets[Att->configsIndex].xres    = width;
  Att->sets[Att->configsIndex].yres    = height;
  Att->sets[Att->configsIndex].format  = HAL_PIXEL_FORMAT_YCrCb_420_SP;

  VDList = new SprdVDLayerList() ;
  if (VDList == NULL)
  {
    ALOGE("CREATE_VIRTUAL_DISPLAY[%d] new SprdVDLayerList failed", index);
    return ERR_NO_RESOURCES;
  }

  Client->setUserData(static_cast<void *>(VDList));

  *format     = Att->sets[Att->configsIndex].format;
  *outDisplay = Client->remapToAndroidDisplay(Client);

  mClient[index] = Client;

  return ERR_NONE;
}

int32_t /*hwc2_error_t*/SprdVirtualDisplayDevice::DESTROY_VIRTUAL_DISPLAY(SprdDisplayClient *Client)
{
  uint32_t index = 0;
  SprdVDLayerList   *VDList = NULL;

  if (Client == NULL || reinterpret_cast<unsigned long>(Client) == HWC_NEGTIVE)
  {
    ALOGE("DESTROY_VIRTUAL_DISPLAY SprdDisplayClient is NULL or a invalid display");
    return ERR_BAD_DISPLAY;
  }

  for (uint32_t i = 0; i < mClientCount; i++)
  {
    if (Client == mClient[i])
    {
      index = i;
      break;
    }
  }

  VDList = getHWLayerObj(Client);
  if (VDList)
  {
    delete VDList;
    VDList = NULL;
  } 

  delete Client;
  Client = NULL;

  mClient[index] = NULL;

  return ERR_NONE;
}

uint32_t SprdVirtualDisplayDevice::GET_MAX_VIRTUAL_DISPLAY_COUNT(void)
{
  return mClientCount;
}

int32_t /*hwc2_error_t*/SprdVirtualDisplayDevice::ACCEPT_DISPLAY_CHANGES(SprdDisplayClient *Client)
{
  SprdVDLayerList   *VDList = NULL;

  if (Client == NULL)
  {
    ALOGE("SprdVirtualDisplayDevice line: %d cannot get the SprdDisplayClient", __LINE__);
    return ERR_BAD_DISPLAY;
  }

  VDList = getHWLayerObj(Client);
  if (VDList == NULL)
  {
    ALOGE("SprdVirtualDisplayDevice line: %d SprdVDLayerList is NULL", __LINE__);
    return ERR_BAD_DISPLAY;
  }

  return VDList->acceptGeometryChanged();
}

int32_t /*hwc2_error_t*/SprdVirtualDisplayDevice::CREATE_LAYER(SprdDisplayClient *Client,
                                                               hwc2_layer_t* outLayer)
{
  SprdVDLayerList   *VDList = NULL;

  if (Client == NULL)
  {
    ALOGE("SprdVirtualDisplayDevice line: %d cannot get the SprdDisplayClient", __LINE__);
    return ERR_BAD_DISPLAY;
  }

  VDList = getHWLayerObj(Client);
  if (VDList == NULL)
  {
    ALOGE("SprdVirtualDisplayDevice line: %d SprdVDLayerList is NULL", __LINE__);
    return ERR_BAD_DISPLAY;
  }

  return VDList->createSprdLayer(outLayer);
}

int32_t /*hwc2_error_t*/SprdVirtualDisplayDevice::DESTROY_LAYER(SprdDisplayClient *Client,
                                                                hwc2_layer_t layer)
{
  SprdVDLayerList   *VDList = NULL;

  if (Client == NULL)
  {
    ALOGE("SprdVirtualDisplayDevice line: %d cannot get the SprdDisplayClient", __LINE__);
    return ERR_BAD_DISPLAY;
  }

  VDList = getHWLayerObj(Client);
  if (VDList == NULL)
  {
    ALOGE("SprdVirtualDisplayDevice line: %d SprdVDLayerList is NULL", __LINE__);
    return ERR_BAD_DISPLAY;
  }

  return VDList->destroySprdLayer(layer);
}

int32_t /*hwc2_error_t*/SprdVirtualDisplayDevice::GET_CHANGED_COMPOSITION_TYPES(
           SprdDisplayClient *Client,
           uint32_t* outNumElements, hwc2_layer_t* outLayers,
           int32_t* /*hwc2_composition_t*/ outTypes)
{
  SprdVDLayerList   *VDList = NULL;

  if (Client == NULL)
  {
    ALOGE("SprdVirtualDisplayDevice line: %d cannot get the SprdDisplayClient", __LINE__);
    return ERR_BAD_DISPLAY;
  }

  VDList = getHWLayerObj(Client);
  if (VDList == NULL)
  {
    ALOGE("SprdVirtualDisplayDevice line: %d SprdVDLayerList is NULL", __LINE__);
    return ERR_BAD_DISPLAY;
  }

  return VDList->getChangedCompositionTypes(outNumElements, outLayers, outTypes);
}

int32_t /*hwc2_error_t*/SprdVirtualDisplayDevice::GET_CLIENT_TARGET_SUPPORT(
           SprdDisplayClient *Client,
           uint32_t width, uint32_t height, int32_t /*android_pixel_format_t*/ format,
           int32_t /*android_dataspace_t*/ dataspace)
{
  int32_t err = ERR_NONE;

  if (Client == NULL)
  {
    ALOGE("SprdVirtualDisplayDevice line: %d cannot get the SprdDisplayClient", __LINE__);
    return ERR_BAD_DISPLAY;
  }

  /* TODO: Implement this function lator, SurfaceFlinger seems do not use it */
  HWC_IGNORE(Client);
  HWC_IGNORE(width);
  HWC_IGNORE(height);
  HWC_IGNORE(format);
  HWC_IGNORE(dataspace);

  return err;
}

int32_t /*hwc2_error_t*/SprdVirtualDisplayDevice::GET_COLOR_MODES(
           SprdDisplayClient *Client,
           uint32_t* outNumModes,
           int32_t* /*android_color_mode_t*/ outModes)
{

  if (Client == NULL)
  {
    ALOGE("SprdVirtualDisplayDevice line: %d cannot get the SprdDisplayClient", __LINE__);
    return ERR_BAD_DISPLAY;
  }

  HWC_IGNORE(outNumModes);
  HWC_IGNORE(outModes);

  return ERR_NONE;
}

int32_t /*hwc2_error_t*/SprdVirtualDisplayDevice::GET_DISPLAY_NAME(
           SprdDisplayClient *Client,
           uint32_t* outSize,
           char* outName)
{
  if (Client == NULL)
  {
    ALOGE("SprdVirtualDisplayDevice line: %d cannot get the SprdDisplayClient", __LINE__);
    return ERR_BAD_DISPLAY;
  }

   return Client->GET_DISPLAY_NAME(outSize, outName);
}

int32_t /*hwc2_error_t*/SprdVirtualDisplayDevice::GET_DISPLAY_REQUESTS(
           SprdDisplayClient *Client,
           int32_t* /*hwc2_display_request_t*/ outDisplayRequests,
           uint32_t* outNumElements, hwc2_layer_t* outLayers,
           int32_t* /*hwc2_layer_request_t*/ outLayerRequests)
{
  SprdVDLayerList   *VDList = NULL;

  if (Client == NULL)
  {
    ALOGE("SprdVirtualDisplayDevice line: %d cannot get the SprdDisplayClient", __LINE__);
    return ERR_BAD_DISPLAY;
  }

  VDList = getHWLayerObj(Client);
  if (VDList == NULL)
  {
    ALOGE("SprdVirtualDisplayDevice line: %d SprdVDLayerList is NULL", __LINE__);
    return ERR_BAD_DISPLAY;
  }

  return VDList->getDisplayRequests(outDisplayRequests, outNumElements, outLayers, outLayerRequests);
}

int32_t /*hwc2_error_t*/SprdVirtualDisplayDevice::GET_DISPLAY_TYPE(
           SprdDisplayClient *Client,
           int32_t* /*hwc2_display_type_t*/ outType)
{
  if (Client == NULL)
  {
    ALOGE("SprdVirtualDisplayDevice line: %d cannot get the SprdDisplayClient", __LINE__);
    return ERR_BAD_DISPLAY;
  }

  if (outType == NULL)
  {
    ALOGE("SprdVirtualDisplayDevice::GET_DISPLAY_TYPE outType is NULL");
    return ERR_BAD_PARAMETER;
  }

  return Client->GET_DISPLAY_TYPE(outType);
}

int32_t /*hwc2_error_t*/SprdVirtualDisplayDevice::GET_DOZE_SUPPORT(
           SprdDisplayClient *Client,
           int32_t* outSupport)
{
  if (outSupport == NULL)
  {
    ALOGE("SprdVirtualDisplayDevice::GET_DOZE_SUPPORT outSupport is NULL");
    return ERR_BAD_PARAMETER;
  }

  if (Client == NULL)
  {
    ALOGE("SprdVirtualDisplayDevice line: %d cannot get the SprdDisplayClient", __LINE__);
    return ERR_BAD_DISPLAY;
  }

  return Client->GET_DOZE_SUPPORT(outSupport);
}

int32_t /*hwc2_error_t*/SprdVirtualDisplayDevice::GET_HDR_CAPABILITIES(
           SprdDisplayClient *Client,
           uint32_t* outNumTypes,
           int32_t* /*android_hdr_t*/ outTypes, float* outMaxLuminance,
           float* outMaxAverageLuminance, float* outMinLuminance)
{
  if (outNumTypes == NULL)
  {
    ALOGE("SprdVirtualDisplayDevice::GET_HDR_CAPABILITIES outNumTypes is NULL");
    return ERR_BAD_PARAMETER;
  }

  if (Client == NULL)
  {
    ALOGE("SprdVirtualDisplayDevice line: %d cannot get the SprdDisplayClient", __LINE__);
    return ERR_BAD_DISPLAY;
  }

  return Client->GET_HDR_CAPABILITIES(outNumTypes, outTypes, outMaxLuminance,
                                       outMaxAverageLuminance, outMinLuminance);
}

int32_t /*hwc2_error_t*/SprdVirtualDisplayDevice::GET_RELEASE_FENCES(
           SprdDisplayClient *Client,
           uint32_t* outNumElements,
           hwc2_layer_t* outLayers, int32_t* outFences)
{
  uint32_t i;
  uint32_t LayerCount = 0;
  SprdVDLayerList   *VDList = NULL;

  if (outNumElements == NULL)
  {
    ALOGE("SprdVirtualDisplayDevice::GET_RELEASE_FENCES outNumElements is NULL");
    return ERR_BAD_PARAMETER;
  }

  if (Client == NULL)
  {
    ALOGE("SprdVirtualDisplayDevice line: %d cannot get the SprdDisplayClient", __LINE__);
    return ERR_BAD_DISPLAY;
  }

  VDList = getHWLayerObj(Client);
  if (VDList == NULL)
  {
    ALOGE("SprdVirtualDisplayDevice line: %d SprdVDLayerList is NULL", __LINE__);
    return ERR_BAD_DISPLAY;
  }


  LayerCount        = VDList->getSprdLayerCount();
  LIST& HWCList     = VDList->getHWCLayerList();

  *outNumElements = LayerCount;

  if (outLayers && outFences)
  {
    for (i = 0; i < LayerCount; i++)
    {
      if (HWCList[i])
      {
        outLayers[i] = SprdHWLayer::remapToAndroidLayer(HWCList[i]);
        outFences[i] = dup(Client->getReleseFence());
      }
      else
      {
        ALOGE("SprdVirtualDisplayDevice::GET_RELEASE_FENCES PresentLayerList[%d] is NULL", i);
      }
    }
  }

  return ERR_NONE;
}

int32_t /*hwc2_error_t*/SprdVirtualDisplayDevice::SET_CLIENT_TARGET(
           SprdDisplayClient *Client,
           buffer_handle_t target,
           int32_t acquireFence, int32_t /*android_dataspace_t*/ dataspace,
           hwc_region_t damage)
{
  if (Client == NULL)
  {
    ALOGE("SprdVirtualDisplayDevice line: %d cannot get the SprdDisplayClient", __LINE__);
    return ERR_BAD_DISPLAY;
  }

  return Client->SET_CLIENT_TARGET(target, acquireFence, dataspace, damage); 
}

int32_t /*hwc2_error_t*/SprdVirtualDisplayDevice::SET_COLOR_MODE(
           SprdDisplayClient *Client,
           int32_t /*android_color_mode_t*/ mode)
{
  if (Client == NULL)
  {
    ALOGE("SprdVirtualDisplayDevice line: %d cannot get the SprdDisplayClient", __LINE__);
    return ERR_BAD_DISPLAY;
  }

  return Client->SET_COLOR_MODE(mode);
}

int32_t /*hwc2_error_t*/SprdVirtualDisplayDevice::SET_COLOR_TRANSFORM(
           SprdDisplayClient *Client,
           const float* matrix,
           int32_t /*android_color_transform_t*/ hint)
{
  if (Client == NULL)
  {
    ALOGE("SprdVirtualDisplayDevice line: %d cannot get the SprdDisplayClient", __LINE__);
    return ERR_BAD_DISPLAY;
  }

  return Client->SET_COLOR_TRANSFORM(matrix, hint);
}

int32_t /*hwc2_error_t*/SprdVirtualDisplayDevice::SET_OUTPUT_BUFFER(
           SprdDisplayClient *Client, buffer_handle_t buffer,
           int32_t releaseFence)
{
  if (Client == NULL)
  {
    ALOGE("SprdVirtualDisplayDevice line: %d cannot get the SprdDisplayClient", __LINE__);
    return ERR_BAD_DISPLAY;
  }

  return Client->SET_OUTPUT_BUFFER(buffer, releaseFence);
}
    

int32_t /*hwc2_error_t*/SprdVirtualDisplayDevice::SET_POWER_MODE(
           SprdDisplayClient *Client,
           int32_t /*hwc2_power_mode_t*/ mode)
{
  HWC_IGNORE(Client);
  HWC_IGNORE(mode);

  return ERR_NONE;
}

int32_t /*hwc2_error_t*/SprdVirtualDisplayDevice::VALIDATE_DISPLAY(
           SprdDisplayClient *Client,
           uint32_t* outNumTypes, uint32_t* outNumRequests, int accelerator)
{
  int32_t err = ERR_NONE;
  SprdVDLayerList   *VDList = NULL;

  HWC_IGNORE(accelerator);

  if (Client == NULL)
  {
    ALOGE("SprdVirtualDisplayDevice line: %d cannot get the SprdDisplayClient", __LINE__);
    return ERR_BAD_DISPLAY;
  }

  VDList = getHWLayerObj(Client);
  if (VDList == NULL)
  {
    ALOGE("SprdVirtualDisplayDevice line: %d SprdVDLayerList is NULL", __LINE__);
    return ERR_BAD_DISPLAY;
  }

  err = VDList->validateDisplay(outNumTypes, outNumRequests);
  if (err != ERR_NONE)
  {
    ALOGE("SprdVirtualDisplayDevice::VALIDATE_DISPLAY failed err: %d", err);
  }

  return err;
}

int SprdVirtualDisplayDevice:: syncAttributes(SprdDisplayClient *Client,
                                              AttributesSet *dpyAttributes)
{
    HWC_IGNORE(Client);
    HWC_IGNORE(dpyAttributes);
    return 0;
}

int SprdVirtualDisplayDevice:: ActiveConfig(SprdDisplayClient *Client, DisplayAttributes *dpyAttributes)
{
    HWC_IGNORE(Client);
    HWC_IGNORE(dpyAttributes);
    return 0;
}

int SprdVirtualDisplayDevice:: commit(SprdDisplayClient *Client)
{
  HWC_TRACE_CALL;

  bool BlitCond = false;
  int AndroidLayerCount      = 0;
  SprdHWLayer *SprdFBTLayer  = NULL;
  SprdHWLayer *SprdLayer     = NULL;
  SprdHWLayer *OutputLayer   = NULL;
  SprdHWLayer **OSDLayerList = NULL;

  SprdVDLayerList   *VDList = NULL;

  if (Client == NULL)
  {
    ALOGE("SprdVirtualDisplayDevice line: %d cannot get the SprdDisplayClient", __LINE__);
    return ERR_BAD_DISPLAY;
  }

  VDList = getHWLayerObj(Client);
  if (VDList == NULL)
  {
    ALOGE("SprdVirtualDisplayDevice line: %d SprdVDLayerList is NULL", __LINE__);
    return ERR_BAD_DISPLAY;
  }

  AndroidLayerCount = VDList->getSprdLayerCount();
  int OSDLayerCount = VDList->getOSDLayerCount();

  queryDebugFlag(&mDebugFlag);

  SprdFBTLayer = Client->getFBTargetLayer();
  if (SprdFBTLayer == NULL)
  {
      ALOGE("SprdVirtualDisplayDevice:: commit cannot get SprdFBTLayer");
      return ERR_NO_RESOURCES;
  }

  ALOGI_IF(mDebugFlag, "Start Display VirtualDisplay FBT layer");

  if (SprdFBTLayer->getAcquireFence() >= 0)
  {
      String8 name("HWCFBTVirtual::Post");

      FenceWaitForever(name, SprdFBTLayer->getAcquireFence());

      if (SprdFBTLayer->getAcquireFence() >= 0)
      {
          closeFence(SprdFBTLayer->getAcquireFencePointer());
      }
  }

  closeAcquireFDs(VDList->getHWCLayerList(), mDebugFlag);

  if (mHWCCopy)
  {
      OutputLayer = Client->getOutputLayer();
      if (OutputLayer)
      {
        native_handle_t *outHandle = OutputLayer->getBufferHandle();
        bool TargetIsRGBFormat = false;
        if (outHandle
            && (ADP_FORMAT(outHandle) != HAL_PIXEL_FORMAT_YCbCr_420_SP)
            && (ADP_FORMAT(outHandle) != HAL_PIXEL_FORMAT_YCrCb_420_SP))
        {
            TargetIsRGBFormat = true;
        }

        if (TargetIsRGBFormat)
        {
            BlitCond = VDList->getSkipMode() ? false : true;
        }
        else
        {
            BlitCond = true;
        }

        mDisplayPlane->AttachOutputLayer(OutputLayer);
      }
  }

  if ((mHWCCopy == false)
      || (BlitCond == false))
  {
      ALOGI_IF(mDebugFlag, "SprdVirtualDisplayDevice:: commit do not need COPY");
  }
  else if (mHWCCopy && BlitCond)
  {
      OSDLayerList = VDList->getSprdOSDLayerList();
      SprdLayer = OSDLayerList[0];
      if ((AndroidLayerCount - 1 == 1) && (OSDLayerCount == 1)
          && SprdLayer)
      {
          ALOGI_IF(mDebugFlag, "SprdVirtualDisplayDevice:: commit attach Overlay layer[OSD]");
          mDisplayPlane->AttachVDFramebufferTargetLayer(SprdLayer);
      }
      else
      {
          ALOGI_IF(mDebugFlag, "SprdVirtualDisplayDevice:: commit attach FBT layer");
          mDisplayPlane->AttachVDFramebufferTargetLayer(SprdFBTLayer);
      }

      if (OutputLayer && OutputLayer->getAcquireFence() >= 0)
      {
          String8 name("HWCFBTVirtual::outbuf");

          FenceWaitForever(name, OutputLayer->getAcquireFence());

          if (OutputLayer->getAcquireFence() >= 0)
          {
              closeFence(OutputLayer->getAcquireFencePointer());
          }
      }

      /*
       *  Blit buffer for Virtual Display
       * */
      //mBlit->onStart();

      /* TODO: */
      Client->setReleaseFence(-1);

      //mBlit->onDisplay();
  }

  return 0;
}

int SprdVirtualDisplayDevice:: buildSyncData(SprdDisplayClient *Client,
                                             DisplayTrack *tracker,
                                             int32_t* outRetireFence)
{
  HWC_IGNORE(tracker);
  SprdHWLayer *OutputLayer  = NULL;

  if (Client == NULL)
  {
    ALOGE("SprdVirtualDisplayDevice line: %d cannot get the SprdDisplayClient", __LINE__);
    return ERR_BAD_DISPLAY;
  }

  OutputLayer = Client->getOutputLayer();

  if (mHWCCopy == false)
  {
    if (outRetireFence)
    {
      if (*outRetireFence >= 0)
      {
        closeFence(outRetireFence);
      }

      if (OutputLayer && (OutputLayer->getAcquireFence() >= 0))
      {
        /*
         *  Virtual display just have outbufAcquireFenceFd.
         *  We do not touch this outbuf, and do not need
         *  wait this fence, so just send this acquireFence
         *  back to SurfaceFlinger as retireFence.
         * */
        *outRetireFence = OutputLayer->getAcquireFence();
      }
    }
  }
  else
  {
    /* TODO: */
  }

  return 0;
}

void SprdVirtualDisplayDevice:: WaitForDisplay()
{
    HWC_TRACE_CALL;
/*
    if (mReleaseFence >= 0)
    {
        String8 name("HWCVDREL");
        FenceWaitForever(name, mReleaseFence);
        close(mReleaseFenceFd);
        mReleaseFenceFd = -1;
    }
*/
}
