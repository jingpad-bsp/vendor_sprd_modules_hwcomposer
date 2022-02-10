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
 ** File: SprdHWLayer.cpp             DESCRIPTION                             *
 **                                   Mainly responsible for filtering HWLayer*
 **                                   list, find layers that meet OverlayPlane*
 **                                   and PrimaryPlane specifications and then*
 **                                   mark them as HWC_OVERLAY.               *
 ******************************************************************************
 ******************************************************************************
 ** Author:         zhongjun.chen@spreadtrum.com                              *
 *****************************************************************************/

#include "SprdHWLayer.h"

using namespace android;

/*
 *  SprdHWLayer
 *  constract from a overlay buffer that used to send to dispc.
 * */
SprdHWLayer:: SprdHWLayer(native_handle_t *handle, int format, float planeAlpha,
                          int32_t blending, int32_t transform, int32_t fenceFd, uint32_t zorder)
    : mInit(false),
      mLayerType(LAYER_INVALIDE),
      mPrivateH(handle),
      mFormat(format),
      mLayerIndex(-1),
      mSprdLayerIndex(-1),
      mAccelerator(-1),
      mProtectedFlag(false),
      mPlaneAlpha(planeAlpha),
      mBlendMode(blending),
      mTransform(transform),
      mAcquireFenceFd(fenceFd),
      mCompositionType(COMPOSITION_INVALID),
      mCompositionChangedFlag(false),
      mLayerRequest(LAYER_REQUEST_NONE),
      mLayerRequestFlag(false),
      mZOrder(zorder),
      mDataSpace(0),
      mMagic(MAGIC_NUM),
      mDebugFlag(0)
{
    if (handle)
    {

        if ((mFormat == HAL_PIXEL_FORMAT_RGBA_8888) ||
            (mFormat == HAL_PIXEL_FORMAT_RGBX_8888) ||
            (mFormat == HAL_PIXEL_FORMAT_RGB_888) ||
            (mFormat == HAL_PIXEL_FORMAT_BGRA_8888) ||
            (mFormat == HAL_PIXEL_FORMAT_RGB_565))
        {
            setLayerType(LAYER_OSD);
        }
        else if ((mFormat == HAL_PIXEL_FORMAT_YCbCr_420_SP) ||
                 (mFormat == HAL_PIXEL_FORMAT_YCrCb_420_SP) ||
                 (mFormat == HAL_PIXEL_FORMAT_YCbCr_420_888) ||
                 (mFormat == HAL_PIXEL_FORMAT_YV12) ||
                 (mFormat == HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED))
       {
           setLayerType(LAYER_OVERLAY);
       }
        memset(&mColor, 0x00, sizeof(mColor));
        memset(&mDamageRegion, 0x00, sizeof(mDamageRegion));
        memset(&mVisibleRegion, 0x00, sizeof(mVisibleRegion));
        mInit = true;
    }
}

SprdHWLayer:: SprdHWLayer(native_handle_t *handle, int format, int32_t fenceFd,
                          int32_t dataspace, hwc_region_t damage, uint32_t zorder)
    : mInit(false),
      mLayerType(LAYER_INVALIDE),
      mPrivateH(handle),
      mFormat(format),
      mLayerIndex(-1),
      mSprdLayerIndex(-1),
      mAccelerator(-1),
      mProtectedFlag(false),
      mPlaneAlpha(1.0f),
      mBlendMode(BLEND_MODE_NONE),
      mTransform(0),
      mAcquireFenceFd(fenceFd),
      mCompositionType(COMPOSITION_INVALID),
      mCompositionChangedFlag(false),
      mLayerRequest(LAYER_REQUEST_NONE),
      mLayerRequestFlag(false),
      mZOrder(zorder),
      mDataSpace(dataspace),
      mMagic(MAGIC_NUM),
      mDebugFlag(0)
{
    if (handle)
    {

        if ((mFormat == HAL_PIXEL_FORMAT_RGBA_8888) ||
            (mFormat == HAL_PIXEL_FORMAT_RGBX_8888) ||
            (mFormat == HAL_PIXEL_FORMAT_RGB_888) ||
            (mFormat == HAL_PIXEL_FORMAT_BGRA_8888) ||
            (mFormat == HAL_PIXEL_FORMAT_RGB_565))
        {
            setLayerType(LAYER_OSD);
        }
        else if ((mFormat == HAL_PIXEL_FORMAT_YCbCr_420_SP) ||
                 (mFormat == HAL_PIXEL_FORMAT_YCrCb_420_SP) ||
                 (mFormat == HAL_PIXEL_FORMAT_YCbCr_420_888) ||
                 (mFormat == HAL_PIXEL_FORMAT_YV12) ||
                 (mFormat == HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED))
       {
           setLayerType(LAYER_OVERLAY);
       }
        memset(&mColor, 0x00, sizeof(mColor));
        memset(&mDamageRegion, 0x00, sizeof(mDamageRegion));
        memset(&mVisibleRegion, 0x00, sizeof(mVisibleRegion));

        setSurfaceDamage(damage);
        mInit = true;
    }
}

/*
 *  checkRGBLayerFormat
 *  if it's rgb format,init SprdHWLayer from hwc_layer_1_t.
 * */
bool SprdHWLayer:: checkRGBLayerFormat()
{
    native_handle_t *privateH = mPrivateH;

    if (privateH == NULL)
    {
        return false;
    }

    ALOGI_IF(mDebugFlag, "function = %s, line = %d, privateH -> format = %x", __FUNCTION__, __LINE__, ADP_FORMAT(privateH));
    bool result = false;
    switch (ADP_FORMAT(privateH)) {
	case HAL_PIXEL_FORMAT_RGBA_8888:
	case HAL_PIXEL_FORMAT_RGBX_8888:
	case HAL_PIXEL_FORMAT_RGB_888:
	case HAL_PIXEL_FORMAT_RGB_565:
	case HAL_PIXEL_FORMAT_BGRA_8888:
	//case HAL_PIXEL_FORMAT_BGRX_8888:
	case 0x101:
	    if (!mInit) {
		mInit = true;
	    }
	    result = true;
	    break;
	default:
	    result = false;
	}
    ALOGI_IF(mDebugFlag,"function = %s, line = %d, privateH -> format = %x, result = %u", __FUNCTION__, __LINE__, ADP_FORMAT(privateH), result);
    return result;
}

/*
 *  checkYUVLayerFormat
 *  if it's yuv format,init SprdHWLayer from hwc_layer_1_t.
 * */
bool SprdHWLayer:: checkYUVLayerFormat()
{
    native_handle_t *privateH = mPrivateH;
    if (privateH == NULL)
    {
        return false;
    }
    bool result = false;
    switch (ADP_FORMAT(privateH)) {
	case HAL_PIXEL_FORMAT_YCbCr_420_SP:
	case HAL_PIXEL_FORMAT_YCrCb_420_SP:
	case HAL_PIXEL_FORMAT_YCbCr_420_888:
	case HAL_PIXEL_FORMAT_YV12:
	case HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED:
	    if (!mInit) {
		mInit = true;
	    }
	    result = true;
	    break;
	default:
	    result = false;
    }
    return result;
}

SprdHWLayer *SprdHWLayer::remapFromAndroidLayer(hwc2_layer_t layer)
{
    SprdHWLayer *sprdLayer = NULL;

    /*
     *  TODO: this convert method may has a risk?
     */
    sprdLayer = reinterpret_cast<SprdHWLayer *>(layer);
    if (sprdLayer == NULL || reinterpret_cast<unsigned long>(sprdLayer) == 0x1 || (sprdLayer->mMagic != MAGIC_NUM))
    {
      ALOGE("SprdHWLayer::remapFromAndroidLayer invalid layer");
      return NULL;
    }

    return sprdLayer;
}

hwc2_layer_t SprdHWLayer::remapToAndroidLayer(SprdHWLayer *l)
{
  return reinterpret_cast<hwc2_layer_t>(l);
}

int32_t SprdHWLayer::setSurfaceDamage(hwc_region_t damage)
{
  size_t i = 0;

  if (mDamageRegion.rects)
  {
    free(mDamageRegion.rects);
    mDamageRegion.rects = NULL;
  }

  if (damage.numRects > 0)
  {
    mDamageRegion.rects = (sprdRegion_t *)malloc(damage.numRects * sizeof(sprdRegion_t));
    if (mDamageRegion.rects == NULL)
    {
      ALOGE("SprdHWLayer::setSurfaceDamage malloc sprdRect_t failed");
      return -1;
    }

    mDamageRegion.numRects = damage.numRects;

    for (i = 0; i < damage.numRects; i++)
    {
      mDamageRegion.rects->left   = damage.rects->left;
      mDamageRegion.rects->top    = damage.rects->top;
      mDamageRegion.rects->right  = damage.rects->right;
      mDamageRegion.rects->bottom = damage.rects->bottom;
    }
  }

  return 0;
}

int32_t SprdHWLayer::setColor(hwc_color_t color)
{
  mColor.r = color.r;
  mColor.g = color.g;
  mColor.b = color.b;
  mColor.a = color.a;

  return 0;
}

int32_t SprdHWLayer::setVisibleRegion(hwc_region_t visible)
{
  size_t i = 0;

  if (mVisibleRegion.rects)
  {
    free(mVisibleRegion.rects);
    mVisibleRegion.rects = NULL;
  }

  if (visible.numRects > 0)
  {
    mVisibleRegion.rects = (sprdRegion_t *)malloc(visible.numRects * sizeof(sprdRegion_t));
    if (mVisibleRegion.rects == NULL)
    {
      ALOGE("SprdHWLayer::setVisibleRegion malloc sprdRect_t failed");
      return -1;
    }

    mVisibleRegion.numRects = visible.numRects;

    for (i = 0; i < visible.numRects; i++)
    {
      mVisibleRegion.rects->left   = visible.rects->left;
      mVisibleRegion.rects->top    = visible.rects->top;
      mVisibleRegion.rects->right  = visible.rects->right;
      mVisibleRegion.rects->bottom = visible.rects->bottom;
    }
  }

  return 0;
}

