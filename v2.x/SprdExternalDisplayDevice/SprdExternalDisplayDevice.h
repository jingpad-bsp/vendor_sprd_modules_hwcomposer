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
 ** File: SprdExternalDisplayDevice.h DESCRIPTION                             *
 **                                   Manager External Display device.        *
 ******************************************************************************
 ******************************************************************************
 ** Author:         zhongjun.chen@spreadtrum.com                              *
 *****************************************************************************/

#ifndef _SPRD_EXTERNAL_DISPLAY_DEVICE_H_
#define _SPRD_EXTERNAL_DISPLAY_DEVICE_H_

#include <stdio.h>
#include <stdlib.h>
#include <hardware/hardware.h>
#include <hardware/gralloc.h>
#include <hardware/hwcomposer2.h>

#include <utils/RefBase.h>
#include <cutils/log.h>

#include "../SprdDisplayDevice.h"
#include "../AndroidFence.h"
#include "../dump.h"

#include "SprdHandleLayer.h"

using namespace android;

class SprdDisplayCore;
struct DisplayTrack;

class SprdExternalDisplayDevice {
 public:
  SprdExternalDisplayDevice();
  ~SprdExternalDisplayDevice();

   /* Android ogrinal function */
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

  static int32_t HotplugCallback(const char *displayName, int32_t displayType,
                                 bool connected, SprdDisplayClient **client);

  /*
   *  Display configure attribution.
   * */
  int syncAttributes(SprdDisplayClient *Client, AttributesSet *dpyAttributes);

  int ActiveConfig(SprdDisplayClient *Client, DisplayAttributes *dpyAttributes);

  int setPowerMode(int mode);

  /*
   *  Post layers to SprdDisplayPlane.
   * */
  int commit(SprdDisplayClient *Client);

  /*
   *  Build Sync data for SurfaceFligner
   * */
  int buildSyncData(SprdDisplayClient *Client, struct DisplayTrack *tracker, int32_t* outRetireFence);

  bool Init(SprdDisplayCore *core);

  inline SprdHandleLayer *getHandleLayer()
  {
    return mHandleLayer;
  }


 private:
  SprdHandleLayer *mHandleLayer;
  int mDebugFlag;
  int mDumpFlag;
};

#endif   // #ifndef _SPRD_EXTERNAL_DISPLAY_DEVICE_H_
