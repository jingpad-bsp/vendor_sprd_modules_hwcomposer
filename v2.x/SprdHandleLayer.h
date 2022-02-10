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
 ** File: SprdHandleLayer.h  DESCRIPTION                                      *
 **                                   Manage  SprdHWLayer information         *
 ******************************************************************************
 ******************************************************************************
 ** Author:         zhongjun.chen@spreadtrum.com                              *
 *****************************************************************************/

#ifndef _SPRD_HANDLE_LAYER_H_
#define _SPRD_HANDLE_LAYER_H_

#include <hardware/hardware.h>
#include <hardware/hwcomposer2.h>
#include <errno.h>
#include <cutils/log.h>

#include "SprdHWC2DataType.h"

//using namespace android;

class SprdHandleLayer
{
public:
  SprdHandleLayer() { }
  ~SprdHandleLayer() { }

   int32_t /*hwc2_error_t*/ SET_CURSOR_POSITION(
           hwc2_layer_t layer,
           int32_t x, int32_t y);

   int32_t /*hwc2_error_t*/ SET_LAYER_BUFFER(
           hwc2_layer_t layer,
           buffer_handle_t buffer, int32_t acquireFence);

   int32_t /*hwc2_error_t*/ SET_LAYER_SURFACE_DAMAGE(
           hwc2_layer_t layer,
           hwc_region_t damage);

   int32_t /*hwc2_error_t*/ SET_LAYER_BLEND_MODE(
           hwc2_layer_t layer,
           int32_t /*hwc2_blend_mode_t*/ mode);

   int32_t /*hwc2_error_t*/ SET_LAYER_COLOR(
           hwc2_layer_t layer,
           hwc_color_t color);

   int32_t /*hwc2_error_t*/ SET_LAYER_COMPOSITION_TYPE(
           hwc2_layer_t layer,
           int32_t /*hwc2_composition_t*/ type);

   int32_t /*hwc2_error_t*/ SET_LAYER_DATASPACE(
           hwc2_layer_t layer,
           int32_t /*android_dataspace_t*/ dataspace);

   int32_t /*hwc2_error_t*/ SET_LAYER_DISPLAY_FRAME(
           hwc2_layer_t layer,
           hwc_rect_t frame);

   int32_t /*hwc2_error_t*/ SET_LAYER_PLANE_ALPHA(
           hwc2_layer_t layer,
           float alpha);

   int32_t /*hwc2_error_t*/ SET_LAYER_SIDEBAND_STREAM(
           hwc2_layer_t layer,
           const native_handle_t* stream);

   int32_t /*hwc2_error_t*/ SET_LAYER_SOURCE_CROP(
           hwc2_layer_t layer,
           hwc_frect_t crop);

   int32_t /*hwc2_error_t*/ SET_LAYER_TRANSFORM(
           hwc2_layer_t layer,
           int32_t /*hwc_transform_t*/ transform);

   int32_t /*hwc2_error_t*/ SET_LAYER_VISIBLE_REGION(
           hwc2_layer_t layer,
           hwc_region_t visible);

   int32_t /*hwc2_error_t*/ SET_LAYER_Z_ORDER(
           hwc2_layer_t layer,
           uint32_t z);
};

#endif

