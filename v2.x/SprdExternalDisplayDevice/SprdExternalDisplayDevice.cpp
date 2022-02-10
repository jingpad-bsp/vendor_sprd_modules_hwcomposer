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
 ** 10/07/2016    Hardware Composer v2.0  Responsible for processing some     *
 **                                   Hardware layers. These layers comply    *
 **                                   with display controller specification,  *
 **                                   can be displayed directly, bypass       *
 **                                   SurfaceFligner composition. It will     *
 **                                   improve system performance.             *
 ******************************************************************************
 ** File:SprdExternalDisplayDevice.cpp DESCRIPTION                            *
 **                                   Manager External Display device.        *
 ******************************************************************************
 ******************************************************************************
 ** Author:         zhongjun.chen@spreadtrum.com                              *
 *****************************************************************************/

#include "SprdExternalDisplayDevice.h"
#include "SprdDisplayCore.h"

using namespace android;

SprdExternalDisplayDevice::SprdExternalDisplayDevice()
    : mHandleLayer(NULL),
      mDebugFlag(0), mDumpFlag(0) {}

SprdExternalDisplayDevice::~SprdExternalDisplayDevice() {
  if (mHandleLayer)
  {
    delete mHandleLayer;
  }
}

bool SprdExternalDisplayDevice::Init(SprdDisplayCore *core) {
  if (core == NULL)
  {
    ALOGE("SprdExternalDisplayDevice::Init SprdDisplayCore is NULL");
    return false;
  }

  ALOGI_IF(mDebugFlag, "core:%p", core);

  core->setExternalDisplayDevice(this);

  mHandleLayer = new SprdHandleLayer();

  return true;
}

void SprdExternalDisplayDevice::DUMP(uint32_t* outSize, char* outBuffer, String8& result)
{
  HWC_IGNORE(outSize);
  HWC_IGNORE(outBuffer);
}

int32_t /*hwc2_error_t*/SprdExternalDisplayDevice::ACCEPT_DISPLAY_CHANGES(SprdDisplayClient *Client)
{
  HWC_IGNORE(Client);
  return ERR_NONE;
}

int32_t /*hwc2_error_t*/SprdExternalDisplayDevice::CREATE_LAYER(SprdDisplayClient *Client,
                                                                hwc2_layer_t* outLayer)
{
  HWC_IGNORE(Client);
  HWC_IGNORE(outLayer);
  return ERR_NONE;
}

int32_t /*hwc2_error_t*/SprdExternalDisplayDevice::DESTROY_LAYER(SprdDisplayClient *Client,
                                                                 hwc2_layer_t layer)
{
  HWC_IGNORE(Client);
  HWC_IGNORE(layer);
  return ERR_NONE;
}

int32_t /*hwc2_error_t*/SprdExternalDisplayDevice::GET_CHANGED_COMPOSITION_TYPES(
           SprdDisplayClient *Client,
           uint32_t* outNumElements, hwc2_layer_t* outLayers,
           int32_t* /*hwc2_composition_t*/ outTypes)
{
  HWC_IGNORE(Client);
  HWC_IGNORE(outNumElements);
  HWC_IGNORE(outLayers);
  HWC_IGNORE(outTypes);
  return ERR_NONE;
}

int32_t /*hwc2_error_t*/SprdExternalDisplayDevice::GET_CLIENT_TARGET_SUPPORT(
           SprdDisplayClient *Client,
           uint32_t width, uint32_t height, int32_t /*android_pixel_format_t*/ format,
           int32_t /*android_dataspace_t*/ dataspace)
{
  HWC_IGNORE(Client);
  HWC_IGNORE(width);
  HWC_IGNORE(height);
  HWC_IGNORE(format);
  HWC_IGNORE(dataspace);
  return ERR_NONE;
}

int32_t /*hwc2_error_t*/SprdExternalDisplayDevice::GET_COLOR_MODES(
           SprdDisplayClient *Client,
           uint32_t* outNumModes,
           int32_t* /*android_color_mode_t*/ outModes)
{
  HWC_IGNORE(Client);
  HWC_IGNORE(outNumModes);
  HWC_IGNORE(outModes);
  return ERR_NONE;
}

int32_t /*hwc2_error_t*/SprdExternalDisplayDevice::GET_DISPLAY_NAME(
           SprdDisplayClient *Client,
           uint32_t* outSize,
           char* outName)
{
  HWC_IGNORE(Client);
  HWC_IGNORE(outSize);
  HWC_IGNORE(outName);
  return ERR_NONE;
}

int32_t /*hwc2_error_t*/SprdExternalDisplayDevice::GET_DISPLAY_REQUESTS(
           SprdDisplayClient *Client,
           int32_t* /*hwc2_display_request_t*/ outDisplayRequests,
           uint32_t* outNumElements, hwc2_layer_t* outLayers,
           int32_t* /*hwc2_layer_request_t*/ outLayerRequests)
{
  HWC_IGNORE(Client);
  HWC_IGNORE(outDisplayRequests);
  HWC_IGNORE(outNumElements);
  HWC_IGNORE(outLayers);
  HWC_IGNORE(outLayerRequests);
  return ERR_NONE;
}

int32_t /*hwc2_error_t*/SprdExternalDisplayDevice::GET_DISPLAY_TYPE(
           SprdDisplayClient *Client,
           int32_t* /*hwc2_display_type_t*/ outType)
{
  HWC_IGNORE(Client);
  HWC_IGNORE(outType);
  return ERR_NONE;
}

int32_t /*hwc2_error_t*/SprdExternalDisplayDevice::GET_DOZE_SUPPORT(
           SprdDisplayClient *Client,
           int32_t* outSupport)
{
  HWC_IGNORE(Client);
  HWC_IGNORE(outSupport);
  return ERR_NONE;
}


int32_t /*hwc2_error_t*/SprdExternalDisplayDevice::GET_HDR_CAPABILITIES(
           SprdDisplayClient *Client,
           uint32_t* outNumTypes,
           int32_t* /*android_hdr_t*/ outTypes, float* outMaxLuminance,
           float* outMaxAverageLuminance, float* outMinLuminance)
{
  HWC_IGNORE(Client);
  HWC_IGNORE(outNumTypes);
  HWC_IGNORE(outTypes);
  HWC_IGNORE(outMaxLuminance);
  HWC_IGNORE(outMaxAverageLuminance);
  HWC_IGNORE(outMinLuminance);
  return ERR_NONE;
}

int32_t /*hwc2_error_t*/SprdExternalDisplayDevice::GET_RELEASE_FENCES(
           SprdDisplayClient *Client,
           uint32_t* outNumElements,
           hwc2_layer_t* outLayers, int32_t* outFences)
{
  HWC_IGNORE(Client);
  HWC_IGNORE(outNumElements);
  HWC_IGNORE(outLayers);
  HWC_IGNORE(outFences);
  return ERR_NONE;
}

int32_t /*hwc2_error_t*/SprdExternalDisplayDevice::SET_CLIENT_TARGET(
           SprdDisplayClient *Client,
           buffer_handle_t target,
           int32_t acquireFence, int32_t /*android_dataspace_t*/ dataspace,
           hwc_region_t damage)
{
  HWC_IGNORE(Client);
  HWC_IGNORE(target);
  HWC_IGNORE(acquireFence);
  HWC_IGNORE(dataspace);
  HWC_IGNORE(damage);
  return ERR_NONE;
}

int32_t /*hwc2_error_t*/SprdExternalDisplayDevice::SET_COLOR_MODE(
           SprdDisplayClient *Client,
           int32_t /*android_color_mode_t*/ mode)
{
  HWC_IGNORE(Client);
  HWC_IGNORE(mode);
  return ERR_NONE;
}

int32_t /*hwc2_error_t*/SprdExternalDisplayDevice:: SET_COLOR_TRANSFORM(
           SprdDisplayClient *Client,
           const float* matrix,
           int32_t /*android_color_transform_t*/ hint)
{
  HWC_IGNORE(Client);
  HWC_IGNORE(matrix);
  HWC_IGNORE(hint);
  return ERR_NONE;
}

int32_t /*hwc2_error_t*/SprdExternalDisplayDevice::SET_POWER_MODE(
        SprdDisplayClient *Client,
        int32_t /*hwc2_power_mode_t*/ mode)
{
  HWC_IGNORE(Client);
  HWC_IGNORE(mode);
  return ERR_NONE;
}

int32_t /*hwc2_error_t*/SprdExternalDisplayDevice::VALIDATE_DISPLAY(
        SprdDisplayClient *Client,
        uint32_t* outNumTypes, uint32_t* outNumRequests,
        int accelerator)
{
  HWC_IGNORE(Client);
  HWC_IGNORE(outNumTypes);
  HWC_IGNORE(outNumRequests);
  HWC_IGNORE(accelerator);
  return ERR_NONE;
}

int32_t SprdExternalDisplayDevice::HotplugCallback(const char *displayName, int32_t displayType,
                              bool connected, SprdDisplayClient **client)
{
  HWC_IGNORE(displayName);
  HWC_IGNORE(displayType);
  HWC_IGNORE(client);
  HWC_IGNORE(connected);
  return ERR_NONE;
}


int SprdExternalDisplayDevice::syncAttributes(SprdDisplayClient *Client, AttributesSet *dpyAttributes) {
  HWC_IGNORE(Client);
  int index = 0;
  float refreshRate = 60.0;

  if (dpyAttributes == NULL) {
    ALOGE("Input parameter is NULL");
    return -1;
  }

  dpyAttributes->vsync_period = 0;
  dpyAttributes->xres = 0;
  dpyAttributes->yres = 0;
  dpyAttributes->stride = 0;
  dpyAttributes->xdpi = 0;
  dpyAttributes->ydpi = 0;

  return 0;
}

int SprdExternalDisplayDevice::ActiveConfig(SprdDisplayClient *Client, DisplayAttributes *dpyAttributes) {
  HWC_IGNORE(Client);
  ALOGI_IF(mDebugFlag, "dpyAttributes:%p", dpyAttributes);
  return 0;
}

int SprdExternalDisplayDevice::setPowerMode(int mode) {
  int ret = -1;

  switch (mode) {
    case POWER_MODE_NORMAL:
      /*
       *  Turn on the display (if it was previously off),
       *  and take it out of low power mode.
       * */

      break;
    case POWER_MODE_DOZE:
      /*
       *  Turn on the display (if it was previously off),
       *  and put the display in a low power mode.
       * */

      break;
    case POWER_MODE_OFF:
      /*
       *  Turn the display off.
       * */

      break;
    default:
      return 0;
  }

  return 0;
}

int SprdExternalDisplayDevice::commit(SprdDisplayClient *Client) {
  HWC_IGNORE(Client);
  return 0;
}

int SprdExternalDisplayDevice::buildSyncData(SprdDisplayClient *Client,
                                             struct DisplayTrack *tracker,
                                             int32_t* outRetireFence) {
  HWC_IGNORE(Client);
  HWC_IGNORE(tracker);
  HWC_IGNORE(outRetireFence);
  return 0;
}
