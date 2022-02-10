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
 ** File: SprdHandleLayer.cpp  DESCRIPTION                                    *
 **                                   Manage  SprdHWLayer information         *
 ******************************************************************************
 ******************************************************************************
 ** Author:         zhongjun.chen@spreadtrum.com                              *
 *****************************************************************************/


#include "SprdHandleLayer.h"
#include "SprdHWLayer.h"
#include "gralloc_public.h"

using namespace android;


int32_t /*hwc2_error_t*/SprdHandleLayer::SET_CURSOR_POSITION(
           hwc2_layer_t layer,
           int32_t x, int32_t y)
{
  /*  TODO:  */
  SprdHWLayer *sprdLayer = SprdHWLayer::remapFromAndroidLayer(layer);
  if (sprdLayer == NULL)
  {
    ALOGE("SprdHandleLayer::SET_CURSOR_POSITION sprdLayer is NULL");
    return ERR_BAD_LAYER;
  }

  if (sprdLayer->setCursorPosition(x, y) != 0)
  {
    ALOGE("prdPrimaryDisplayDevice setCursorPosition faiiled");
    return  ERR_BAD_LAYER;
  }

  return ERR_NONE;
}

int32_t /*hwc2_error_t*/SprdHandleLayer::SET_LAYER_BUFFER(
           hwc2_layer_t layer,
           buffer_handle_t buffer, int32_t acquireFence)
{
  const native_handle_t *pHandle = NULL;
  SprdHWLayer *sprdLayer = SprdHWLayer::remapFromAndroidLayer(layer);
  if (sprdLayer == NULL)
  {
    ALOGE("SprdHandleLayer::SET_LAYER_BUFFER sprdLayer is NULL");
    return ERR_BAD_LAYER;
  }

  if (buffer == NULL)
  {
    ALOGI("SprdHandleLayer::SET_LAYER_BUFFER buffer is NULL, SF may use Client composition");
    return ERR_NONE;
  }

  pHandle = static_cast<const native_handle_t *>(buffer);

  if (sprdLayer->setBuffer(const_cast<native_handle_t *>(pHandle), acquireFence) != 0)
  {
    ALOGE("prdPrimaryDisplayDevice setBuffer faiiled");
    return  ERR_BAD_LAYER;
  }

  return ERR_NONE;
}

int32_t /*hwc2_error_t*/SprdHandleLayer::SET_LAYER_SURFACE_DAMAGE(
           hwc2_layer_t layer,
           hwc_region_t damage)
{
  SprdHWLayer *sprdLayer = SprdHWLayer::remapFromAndroidLayer(layer);
  if (sprdLayer == NULL)
  {
    ALOGE("SprdHandleLayer::SET_LAYER_SURFACE_DAMAGE sprdLayer is NULL");
    return ERR_BAD_LAYER;
  }

  if (sprdLayer->setSurfaceDamage(damage) != 0)
  {
    ALOGE("prdPrimaryDisplayDevice setSurfaceDamage faiiled");
    return  ERR_BAD_LAYER;
  }

  return ERR_NONE;
}

int32_t /*hwc2_error_t*/SprdHandleLayer::SET_LAYER_BLEND_MODE(
           hwc2_layer_t layer,
           int32_t /*hwc2_blend_mode_t*/ mode)
{
  SprdHWLayer *sprdLayer = SprdHWLayer::remapFromAndroidLayer(layer);
  if (sprdLayer == NULL)
  {
    ALOGE("SprdHandleLayer::SET_LAYER_BLEND_MODE sprdLayer is NULL");
    return ERR_BAD_LAYER;
  }

  if (sprdLayer->setBlendMode(mode) != 0)
  {
    ALOGE("prdPrimaryDisplayDevice setBlendMode faiiled");
    return  ERR_BAD_LAYER;
  }

  return ERR_NONE;
}

int32_t /*hwc2_error_t*/SprdHandleLayer::SET_LAYER_COLOR(
           hwc2_layer_t layer,
           hwc_color_t color)
{
  SprdHWLayer *sprdLayer = SprdHWLayer::remapFromAndroidLayer(layer);
  if (sprdLayer == NULL)
  {
    ALOGE("SprdHandleLayer::SET_LAYER_COLOR sprdLayer is NULL");
    return ERR_BAD_LAYER;
  }

  if (sprdLayer->setColor(color) != 0)
  {
    ALOGE("prdPrimaryDisplayDevice setColor faiiled");
    return  ERR_BAD_LAYER;
  }

  return ERR_NONE;
}

int32_t /*hwc2_error_t*/SprdHandleLayer::SET_LAYER_COMPOSITION_TYPE(
           hwc2_layer_t layer,
           int32_t /*hwc2_composition_t*/ type)
{
  SprdHWLayer *sprdLayer = SprdHWLayer::remapFromAndroidLayer(layer);
  if (sprdLayer == NULL)
  {
    ALOGE("SprdHandleLayer::SET_LAYER_COMPOSITION_TYPE sprdLayer is NULL");
    return ERR_BAD_LAYER;
  }

  if (sprdLayer->setCompositionType(type) !=0 )
  {
    ALOGE("prdPrimaryDisplayDevice setCompositionType faiiled");
    return  ERR_BAD_LAYER;
  }

  return ERR_NONE;
}

int32_t /*hwc2_error_t*/SprdHandleLayer::SET_LAYER_DATASPACE(
           hwc2_layer_t layer,
           int32_t /*android_dataspace_t*/ dataspace)
{
  SprdHWLayer *sprdLayer = SprdHWLayer::remapFromAndroidLayer(layer);
  if (sprdLayer == NULL)
  {
    ALOGE("SprdHandleLayer::SET_LAYER_DATASPACE sprdLayer is NULL");
    return ERR_BAD_LAYER;
  }

  if (sprdLayer->setDataSpace(dataspace) != 0)
  {
    ALOGE("prdPrimaryDisplayDevice setDataSpace faiiled");
    return  ERR_BAD_LAYER;
  }

  return ERR_NONE;
}

int32_t /*hwc2_error_t*/SprdHandleLayer::SET_LAYER_DISPLAY_FRAME(
           hwc2_layer_t layer,
           hwc_rect_t frame)
{
  SprdHWLayer *sprdLayer = SprdHWLayer::remapFromAndroidLayer(layer);
  if (sprdLayer == NULL)
  {
    ALOGE("SprdHandleLayer::SET_LAYER_DISPLAY_FRAME sprdLayer is NULL");
    return ERR_BAD_LAYER;
  }

  if (sprdLayer->setDisplayFrame(frame) != 0)
  {
    ALOGE("prdPrimaryDisplayDevice setDisplayFrame faiiled");
    return  ERR_BAD_LAYER;
  }

  return ERR_NONE;

}

int32_t /*hwc2_error_t*/SprdHandleLayer::SET_LAYER_PLANE_ALPHA(
           hwc2_layer_t layer,
           float alpha)
{
  SprdHWLayer *sprdLayer = SprdHWLayer::remapFromAndroidLayer(layer);
  if (sprdLayer == NULL)
  {
    ALOGE("SprdHandleLayer::SET_LAYER_PLANE_ALPHA sprdLayer is NULL");
    return ERR_BAD_LAYER;
  }

  sprdLayer->setPlaneAlpha(alpha);

  return ERR_NONE;
}

int32_t /*hwc2_error_t*/SprdHandleLayer::SET_LAYER_SIDEBAND_STREAM(
           hwc2_layer_t layer,
           const native_handle_t* stream)
{
  const native_handle_t *pHandle = NULL;
  SprdHWLayer *sprdLayer = SprdHWLayer::remapFromAndroidLayer(layer);
  if (sprdLayer == NULL)
  {
    ALOGE("SprdHandleLayer::SET_LAYER_SIDEBAND_STREAM sprdLayer is NULL");
    return ERR_BAD_LAYER;
  }

  if (stream == NULL)
  {
    ALOGE("SprdHandleLayer::SET_LAYER_SIDEBAND_STREAM stream is NULL");
    return ERR_BAD_PARAMETER;
  }

  pHandle = static_cast<const native_handle_t *>(stream);

  if (sprdLayer->setSidebandStream(const_cast<native_handle_t *>(pHandle)) != 0)
  {
    ALOGE("prdPrimaryDisplayDevice setSidebandStream faiiled");
    return  ERR_BAD_LAYER;
  }

  return ERR_NONE;
}

int32_t /*hwc2_error_t*/SprdHandleLayer::SET_LAYER_SOURCE_CROP(
           hwc2_layer_t layer,
           hwc_frect_t crop)
{
  SprdHWLayer *sprdLayer = SprdHWLayer::remapFromAndroidLayer(layer);
  if (sprdLayer == NULL)
  {
    ALOGE("SprdHandleLayer::SET_LAYER_SOURCE_CROP sprdLayer is NULL");
    return ERR_BAD_LAYER;
  }

  if (sprdLayer->setSourceCrop(crop) != 0)
  {
    ALOGE("prdPrimaryDisplayDevice setSourceCrop faiiled");
    return  ERR_BAD_LAYER;
  }

  return ERR_NONE;
}

int32_t /*hwc2_error_t*/SprdHandleLayer::SET_LAYER_TRANSFORM(
           hwc2_layer_t layer,
           int32_t /*hwc_transform_t*/ transform)
{
  SprdHWLayer *sprdLayer = SprdHWLayer::remapFromAndroidLayer(layer);
  if (sprdLayer == NULL)
  {
    ALOGE("SprdHandleLayer::SET_LAYER_TRANSFORM sprdLayer is NULL");
    return ERR_BAD_LAYER;
  }

  sprdLayer->setTransform(transform);

  return ERR_NONE;
}

int32_t /*hwc2_error_t*/SprdHandleLayer::SET_LAYER_VISIBLE_REGION(
           hwc2_layer_t layer,
           hwc_region_t visible)
{
  SprdHWLayer *sprdLayer = SprdHWLayer::remapFromAndroidLayer(layer);
  if (sprdLayer == NULL)
  {
    ALOGE("SprdHandleLayer::SET_LAYER_VISIBLE_REGION sprdLayer is NULL");
    return ERR_BAD_LAYER;
  }

  if (sprdLayer->setVisibleRegion(visible) != 0)
  {
    ALOGE("prdPrimaryDisplayDevice setVisibleRegion faiiled");
    return  ERR_BAD_LAYER;
  }

  return ERR_NONE;
}

int32_t /*hwc2_error_t*/SprdHandleLayer::SET_LAYER_Z_ORDER(
           hwc2_layer_t layer,
           uint32_t z)
{
  SprdHWLayer *sprdLayer = SprdHWLayer::remapFromAndroidLayer(layer);
  if (sprdLayer == NULL)
  {
    ALOGE("SprdHandleLayer::SET_LAYER_Z_ORDER sprdLayer is NULL");
    return ERR_BAD_LAYER;
  }

  if (sprdLayer->setZOrder(z) != 0)
  {
    ALOGE("prdPrimaryDisplayDevice::SET_LAYER_Z_ORDER setZOrder faiiled");
    return  ERR_BAD_LAYER;
  }

  return ERR_NONE;
}
