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
 **                                   with Virtual Display specification,     *
 **                                   can be displayed directly, bypass       *
 **                                   SurfaceFligner composition. It will     *
 **                                   improve system performance.             *
 ******************************************************************************
 ** File:SprdVDLayerList.cpp          DESCRIPTION                             *
 **                                   Responsible for traverse Virtual Display*
 **                                   Layer list and mark the layers as       *
 **                                   Overlay which                           *
 **                                   comply with Sprd Virtual Display spec.  *
 ******************************************************************************
 ******************************************************************************
 ** Author:         zhongjun.chen@spreadtrum.com                              *
 *****************************************************************************/

#include "SprdVDLayerList.h"


using namespace android;

SprdVDLayerList:: ~SprdVDLayerList()
{
    if (!(mList.isEmpty()))
    {
      for (size_t i = 0; i < mList.size(); i++)
      {
        if (mList[i])
        {
          delete mList[i];
        }
      }
      mList.clear();
    }

    reclaimSprdHWLayer();
}

int32_t SprdVDLayerList:: acceptGeometryChanged()
{
  if (mValidateDisplayed == false)
  {
    ALOGE("SprdVDLayerList:: acceptGeometryChanged validateDisplay should be called first");
    return ERR_NOT_VALIDATED;
  }

  for (size_t i = 0; i < mList.size(); i++)
  {
    SprdHWLayer *l = mList[i];

    if (l == NULL)
    {
      continue;
    }

    /*
     *  After ValidateDisplay, the layer may have been set to
     *  use Overlay flag, but SurfaceFlinger may forcibly use
     *  COMPOSITION_CLIENT for GPU comopistion, so here revert
     *  the Overlay flag first.
     * */
    if ((l->getCompositionType() == COMPOSITION_CLIENT)
        && ((l->getLayerType() == LAYER_OSD)
            || (l->getLayerType() == LAYER_OVERLAY)))
    {
        resetOverlayFlag(l);
        mFBLayerCount++;
    }
  }

  mValidateDisplayed = false;

  return ERR_NONE;
}

int32_t SprdVDLayerList:: createSprdLayer(hwc2_layer_t* outLayer)
{
  SprdHWLayer *sprdLayer = NULL;

  sprdLayer = new SprdHWLayer();
  if (sprdLayer == NULL)
  {
    ALOGE("SprdVDLayerList:: createSprdLayer failed");
    return ERR_NO_RESOURCES;
  }

  mList.add(sprdLayer);
  *outLayer = SprdHWLayer::remapToAndroidLayer(sprdLayer);

  return ERR_NONE;
}

int32_t SprdVDLayerList:: destroySprdLayer(hwc2_layer_t layer)
{
  size_t i;
  SprdHWLayer *sprdLayer = NULL;

  sprdLayer = SprdHWLayer::remapFromAndroidLayer(layer);
  if (sprdLayer == NULL)
  {
    ALOGE("SprdVDLayerList:: destroySprdLayer BAD hwc2 layer");
    return ERR_BAD_LAYER;
  }

  for (i = 0; i < mList.size(); i++)
  {
    if (sprdLayer == mList[i])
    {
      delete mList[i];
      mList.removeAt(i);
      break;
    }
  }

  sprdLayer = NULL;

  return ERR_NONE;  

}

int32_t SprdVDLayerList:: getDisplayRequests(int32_t* outDisplayRequests,
                                             uint32_t* outNumElements, hwc2_layer_t* outLayers,
                                             int32_t* outLayerRequests)
{
  if (mValidateDisplayed == false)
  {
    ALOGE("SprdVDLayerList:: getDisplayRequests validateDisplay should be called first");
    return ERR_NOT_VALIDATED;
  }

  if (outDisplayRequests)
  {
    *outDisplayRequests = 0;
  }

  if (outNumElements == NULL)
  {
    ALOGE("SprdVDLayerList:: getDisplayRequests outNumElements is NULL");
    return ERR_BAD_PARAMETER;
  }

  *outNumElements = mCompositionChangedNum;

  if (outLayers && outLayerRequests)
  {
    uint32_t ChangedNum = 0;
    for (unsigned int i = 0; i < mLayerCount; i++)
    {
      SprdHWLayer *l = mList[i];
      if (l)
      {
        outLayers[i]        = SprdHWLayer::remapToAndroidLayer(l);
        outLayerRequests[i] = l->getLayerRequest();
        ChangedNum++;
      }
    }

    if (ChangedNum > mCompositionChangedNum)
    {
      ALOGE("SprdVDLayerList:: getDisplayRequests mCompositionChangedNum error");
    }
  }

  return ERR_NONE;
}

int32_t SprdVDLayerList:: getChangedCompositionTypes(uint32_t* outNumElements, hwc2_layer_t* outLayers,
                                                     int32_t* outTypes)
{
  if (outNumElements == NULL)
  {
    ALOGE("SprdVDLayerList:: getChangedCompositionTypes input is NULL");
    return ERR_BAD_PARAMETER;
  }

  if (mValidateDisplayed == false)
  {
    ALOGE("SprdVDLayerList:: getChangedCompositionTypes validateDisplay should be called first");
    return ERR_NOT_VALIDATED;
  }

  *outNumElements = mCompositionChangedNum;

  if (outLayers && outTypes)
  {
    uint32_t ChangedNum = 0;
    for (unsigned int i = 0; i < mLayerCount; i++)
    {
      SprdHWLayer *l = mList[i];
      if (l)
      {
        outLayers[i] = SprdHWLayer::remapToAndroidLayer(l);
        outTypes[i]  = l->getCompositionType();
        ChangedNum++;
      }
    }

    if (ChangedNum != mCompositionChangedNum)
    {
      ALOGE("SprdVDLayerList:: getChangedCompositionTypes mCompositionChangedNum error");
    }
  }

  return ERR_NONE;
}

int32_t SprdVDLayerList:: validateDisplay(uint32_t* outNumTypes, uint32_t* outNumRequests)
{
  int32_t err = ERR_NONE;

  if (outNumTypes == NULL || outNumRequests == NULL)
  {
    ALOGE("SprdVDLayerList:: validate_display outNumTypes/outNumRequests is NULL");
    return ERR_BAD_PARAMETER;
  }

  if (updateGeometry() !=0)
  {
    ALOGE("SprdVDLayerList:: validate_display updateGeometry failed");
    return ERR_NO_RESOURCES;
  }

  if (revisitGeometry() != 0)
  {
    ALOGE("SprdVDLayerList:: validate_display revisitGeometry failed");
  }

  mValidateDisplayed = true;

  *outNumTypes    = mCompositionChangedNum;
  *outNumRequests = mRequestLayerNum;

  if (mCompositionChangedNum > 0)
  {
    err = ERR_HAS_CHANGE;
  }

  return ERR_NONE;
}

int SprdVDLayerList:: updateGeometry()
{
    mOSDLayerCount = 0;
    mVideoLayerCount = 0;
    mLayerCount = 0;
    mSkipMode = false;
    mCompositionChangedNum = 0;
    mRequestLayerNum = 0;

    queryDebugFlag(&mDebugFlag);
    queryDumpFlag(&mDumpFlag);

    /*
     *  Should we reclaim it here?
     * */
    reclaimSprdHWLayer();

    if (HWCOMPOSER_DUMP_ORIGINAL_VD_LAYERS & mDumpFlag)
    {
        dumpImage(mList);
    }

    mLayerCount = mList.size();
    if (mLayerCount <= 0)
    {
        ALOGI_IF(mDebugFlag, "SprdVirtualDisplayDevice:: updateGeometry mLayerCount < 0");
        return 0;
    }

    /*
     *  mOSDLayerList and mVideoLayerList should not include
     *  FramebufferTarget layer.
     * */
    mOSDLayerList = new SprdHWLayer*[mLayerCount];
    if (mOSDLayerList == NULL)
    {
        ALOGE("SprdVirtualDisplayDevice:: updateGeometry Cannot create OSD Layer list");
        return -1;
    }

    mVideoLayerList = new SprdHWLayer*[mLayerCount];
    if (mVideoLayerList == NULL)
    {
        ALOGE("SprdVirtualDisplayDevice:: updateGeometry Cannot create Video Layer list");
        return -1;
    }

    mFBLayerCount = mLayerCount;

    for (unsigned int i = 0; i < mLayerCount; i++)
    {
         SprdHWLayer *layer = mList[i];

         ALOGI_IF(mDebugFlag, "VirtualDisplay process LayerList[%d/%d]", i , mLayerCount);

         dump_layer(layer);

         if (!IsHWCLayer(layer))
         {
             ALOGI_IF(mDebugFlag, "NOT HWC layer");
             mSkipMode = true;
             continue;
         }

         prepareOSDLayer(layer);

         prepareVideoLayer(layer);
    }

    return 0;
}

int SprdVDLayerList:: revisitGeometry()
{
#ifdef FORCE_HWC_COPY_FOR_VIRTUAL_DISPLAYS
    SprdHWLayer *l = mList[0];
    if (l == NULL)
    {
        return 0;
    }

    if (!IsHWCLayer(l))
    {
        ALOGI_IF(mDebugFlag, "SprdVDLayerList:: revistGeometry NOT HWC layer");
        mSkipMode = true;
        return 0;
    }

    native_handle_t *privateH = (native_handle_t *)(layer->handle);
    if (privateH == NULL)
    {
        ALOGI_IF(mDebugFlag, "SprdVDLayerList:: revistGeometry privateH is NULL");
        return 0;
    }

    if ((ADP_FORMAT(privateH) != HAL_PIXEL_FORMAT_YCbCr_420_SP)
        && (ADP_FORMAT(privateH) != HAL_PIXEL_FORMAT_RGBA_8888)
        && (ADP_FORMAT(privateH) != HAL_PIXEL_FORMAT_RGBX_8888)
        && (ADP_FORMAT(privateH) != HAL_PIXEL_FORMAT_BGRA_8888)
        && (ADP_FORMAT(privateH) != HAL_PIXEL_FORMAT_RGB_888)
        && (ADP_FORMAT(privateH) != HAL_PIXEL_FORMAT_RGB_565))
    {
        ALOGI_IF(mDebugFlag, "SprdVDLayerList:: revistGeometry not support format");
        return 0;
    }

    if ((mLayerCount == 1)
        && (l->InitCheck()))
    {
        mLayerList[0].setLayerType(LAYER_OSD);
        setOverlayFlag(&(mLayerList[0]), 0);
        ALOGI_IF(mDebugFlag, "SprdVDLayerList:: revistGeometry find single layer, force goto Overlay");
    }
#endif

    return 0;
}

int SprdVDLayerList:: prepareOSDLayer(SprdHWLayer *l)
{
    HWC_IGNORE(l);
    return 0;
}

int SprdVDLayerList:: prepareVideoLayer(SprdHWLayer *l)
{
    HWC_IGNORE(l);
    return 0;
}

void SprdVDLayerList:: reclaimSprdHWLayer()
{
    if (mOSDLayerList)
    {
       delete [] mOSDLayerList;
       mOSDLayerList = NULL;
    }

    if (mVideoLayerList)
    {
        delete [] mVideoLayerList;
        mVideoLayerList = NULL;
    }

}

void SprdVDLayerList:: ClearFrameBuffer(SprdHWLayer *l, unsigned int index)
{
    int32_t request = LAYER_REQUEST_NONE;
    if (l == NULL)
    {
      ALOGE("SprdHWLayerList:: ClearFrameBuffer SprdHWLayer is NULL");
      return;
    }

    if (index != 0)
    {
        request = CLEAR_CLIENT_TARGET;
    }

    if (l->getLayerRequest() == CLEAR_CLIENT_TARGET)
    {
        mRequestLayerNum++;
    }

    l->setLayerRequest(request);
}

void SprdVDLayerList:: setOverlayFlag(SprdHWLayer *l, unsigned int index)
{
    if (l == NULL)
    {
      ALOGE("SprdVDLayerList:: setOverlayFlag SprdHWLayer is NULL");
      return;
    }

    switch (l->getLayerType())
    {
        case LAYER_OSD:
            l->setSprdLayerIndex(mOSDLayerCount);
            mOSDLayerList[mOSDLayerCount] = l;
            mOSDLayerCount++;
            forceOverlay(l, COMPOSITION_DEVICE);
            ClearFrameBuffer(l, index);
            break;
        case LAYER_OVERLAY:
            l->setSprdLayerIndex(mVideoLayerCount);
            mVideoLayerList[mVideoLayerCount] = l;
            mVideoLayerCount++;
            forceOverlay(l, COMPOSITION_DEVICE);
            ClearFrameBuffer(l, index);
            break;
        case LAYER_SURFACEFLINGER:
        default:
            forceOverlay(l, COMPOSITION_CLIENT);
            ClearFrameBuffer(l, 100);
            break;
    }

    l->setLayerIndex(index);
}

void SprdVDLayerList:: forceOverlay(SprdHWLayer *l, int32_t compositionType)
{
    if (l == NULL)
    {
        ALOGE("Input parameters SprdHWLayer is NULL");
        return;
    }

    if (l->InitCheck())
    {
        ALOGI_IF(mDebugFlag, "setOverlayFlag, Overlay has been marked");
        return;
    }

    /* TODO: should check the composition type first */
    //if (l->getCompositionType() != compositionType)
    {
      mCompositionChangedNum++;
    }

    l->setCompositionType(compositionType);
}

void SprdVDLayerList:: resetOverlayFlag(SprdHWLayer *l)
{
    if (l == NULL)
    {
        ALOGE("SprdHWLayer is NULL");
        return;
    }

    //if (l->getCompositionType() != COMPOSITION_CLIENT)
    {
      mCompositionChangedNum++;
    }

    l->mCompositionType = COMPOSITION_CLIENT;

    int index = l->getSprdLayerIndex();

    if (index < 0)
    {
        return;
    }

    switch (l->getLayerType())
    {
        case LAYER_OSD:
            mOSDLayerList[index] = NULL;
            mOSDLayerCount--;
            break;
        case LAYER_OVERLAY:
            mVideoLayerList[index] = NULL;
            mVideoLayerCount--;
            break;
        default:
            return;
    }
}

void SprdVDLayerList:: dump_layer(SprdHWLayer *l)
{
    ALOGI_IF(mDebugFlag , "\ttype=%d, format=0x%x, handle=%p, tr=%02x, blend=%04x, {%f,%f,%f,%f}, {%d,%d,%d,%d}, planeAlpha: %d",
             l->getCompositionType(), l->getLayerFormat(), l->getBufferHandle(),
             l->getTransform(), l->getBlendMode(),
             l->getSprdSRCRectF()->x,
             l->getSprdSRCRectF()->y,
             l->getSprdSRCRectF()->right,
             l->getSprdSRCRectF()->bottom,
             l->getSprdFBRect()->left,
             l->getSprdFBRect()->top,
             l->getSprdFBRect()->right,
             l->getSprdFBRect()->bottom,
             l->getPlaneAlpha());
}

bool SprdVDLayerList::IsHWCLayer(SprdHWLayer *layer)
{
  if (layer == NULL)
  {
    return false;
  }

  if (layer->InitCheck() == false)
  {
    ALOGI_IF(mDebugFlag, "SprdVDLayerList::IsHWCLayer not HWC layer");
    return false;
  }
/*
    if (AndroidLayer->flags & HWC_SKIP_LAYER)
    {
        ALOGI_IF(mDebugFlag, "Skip layer");
        return false;
    }
*/

    /*
     *  Here should check buffer usage
     * */

    return true;
}
