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
 ** File: SprdHWLayerList.h           DESCRIPTION                             *
 **                                   Mainly responsible for filtering HWLayer*
 **                                   list, find layers that meet OverlayPlane*
 **                                   and PrimaryPlane specifications and then*
 **                                   mark them as HWC_OVERLAY.               *
 ******************************************************************************
 ******************************************************************************
 ** Author:         zhongjun.chen@spreadtrum.com                              *
 *****************************************************************************/


#ifndef _SPRD_HWLAYER_LIST_H_
#define _SPRD_HWLAYER_LIST_H_

#include <hardware/hardware.h>
#include <hardware/hwcomposer2.h>
#include <utils/RefBase.h>
#include <cutils/atomic.h>
#include <cutils/log.h>

#include "gralloc_public.h"
//#include "sc8825/dcam_hal.h"

#include "../SprdHWLayer.h"
#include "SprdFrameBufferHAL.h"
#include "SprdPrimaryDisplayDevice.h"
#include "../SprdUtil.h"


using namespace android;

class SprdPrimaryDisplayDevice;

/*
 *  Mainly responsible for traversaling HWLayer list,
 *  find layers that meet SprdDisplayPlane specification
 *  and then mark them as HWC_OVERLAY.
 * */
class SprdHWLayerList
{
public:
    SprdHWLayerList()
        : mFBInfo(NULL),
          mGXPLayerList(0),
          mOVCLayerList(0),
          mDispCLayerList(0),
          mLayerList(0),
          mAccerlator(NULL),
          mLayerCount(0),
          mOSDLayerCount(0), mVideoLayerCount(0),
          mDispCLayerCount(0), mGXPLayerCount(0),
          mYUVLayerCount(0),
          mFBLayerCount(0),
          mAcceleratorMode(ACCELERATOR_NON),
          mGXPSupport(false),
          mDisableHWCFlag(false),
          mSkipLayerFlag(false),
          mCompositionChangedNum(0),
          mRequestLayerNum(0),
          mGlobalProtectedFlag(false),
          mForceDisableHWC(false),
          mValidateDisplayed(false),
          mDebugFlag(0), mDumpFlag(0)
    {
#ifdef FORCE_DISABLE_HWC_OVERLAY
        mForceDisableHWC = true;
#else
        mForceDisableHWC = false;
#endif
    }
    ~SprdHWLayerList();

    /*
     *  For HWC v2.0 
     */
    int32_t acceptGeometryChanged();
    int32_t createSprdLayer(hwc2_layer_t* outLayer);
    int32_t destroySprdLayer(hwc2_layer_t layer);
    int32_t getChangedCompositionTypes(uint32_t* outNumElements, hwc2_layer_t* outLayers, int32_t* outTypes);
    int32_t getDisplayRequests(int32_t *outDisplayRequests, uint32_t* outNumElements,
                               hwc2_layer_t* outLayers, int32_t* outLayerRequests);
    int32_t validateDisplay(uint32_t* outNumTypes, uint32_t* outNumRequests,
                             int accelerator, int& DisplayFlag,
                             SprdPrimaryDisplayDevice *mPrimary);

    inline void updateFBInfo(FrameBufferInfo* fbInfo)
    {
        mFBInfo = fbInfo;
    }

    inline void setAccerlator(SprdUtil *acc)
    {
        mAccerlator = acc;
    }

    inline LIST& getHWCLayerList()
    {
      return mList;
    }

    inline SprdHWLayer **getDispCLayerList()
    {
        return mDispCLayerList;
    }

    inline int getDispLayerCount()
    {
        return mDispCLayerCount;
    }

    inline SprdHWLayer **getSprdGXPLayerList()
    {
        return mGXPLayerList;
    }

    inline SprdHWLayer **getOVCLayerList()
    {
        return mOVCLayerList;
    }

    inline SprdHWLayer **getLayerList()
    {
        return mLayerList;
    }

    inline int getGXPLayerCount() const
    {
        return mGXPLayerCount;
    }

    inline int getOVCLayerCount() const
    {
        return mOVCLayerCount;
    }

    inline int getOSDLayerCount() const
    {
        return mOSDLayerCount;
    }

    inline int getVideoLayerCount()
    {
        return mVideoLayerCount;
    }

    inline int getYuvLayerCount()
    {
        return mYUVLayerCount;
    }

    inline unsigned int getLayerCount()
    {
        return mLayerCount;
    }

    inline unsigned int getFBLayerCount()
    {
        return mFBLayerCount;
    }

    inline bool& getDisableHWCFlag()
    {
        return mDisableHWCFlag;
    }

private:
    FrameBufferInfo* mFBInfo;
    LIST        mList;
    SprdHWLayer **mGXPLayerList;
    SprdHWLayer **mOVCLayerList;
    SprdHWLayer **mDispCLayerList;
    SprdHWLayer **mLayerList;

    /*
     *  mFBTargetLayer:it's the dst buffer, but in sprd hwc,
     *  we have independant overlay buffer, so just leave it alone.
     * */
    SprdUtil    *mAccerlator;
    /*
     *  mLayerCount:total layer cnt of this composition, including fb target layer.
     * */
    unsigned int mLayerCount;
    unsigned int mOSDLayerCount;
    unsigned int mVideoLayerCount;
    unsigned int mDispCLayerCount;
    unsigned int mGXPLayerCount;
    unsigned int mOVCLayerCount;
    unsigned int mYUVLayerCount;
    unsigned int mSprdLayerCount;
    /*
     *  mFBLayerCount:layer cnt that should be composited by GPU in SF.
     * */
    unsigned int mFBLayerCount;
    /*
     *  mAcceleratorMode:available accelerator.
     * */
    int mAcceleratorMode;
    bool mGXPSupport;
    bool mDisableHWCFlag;
    bool mSkipLayerFlag;
    uint32_t mPrivateFlag[2];
    uint32_t mCompositionChangedNum;
    uint32_t mRequestLayerNum;
    bool mGlobalProtectedFlag;
    bool mForceDisableHWC;
    bool mValidateDisplayed;
    int mDebugFlag;
    int mDumpFlag;

    /*
     *  traversal HWLayer list
     *  and change some geometry.
     * */
    int updateGeometry(int accelerator);

    /*
     *  traversal HWLayer list again,
     *  mainly judge whether upper layer and bottom layer
     *  is consistent with SprdDisplayPlane Hardware requirements.
     * */
    int revisitGeometry(int& DisplayFlag, SprdPrimaryDisplayDevice *mPrimary);

    /*
     *  Filter OSD layer
     * */
    int prepareOSDLayer(SprdHWLayer *l);

    /*
     *  Filter video layer
     * */
    int prepareVideoLayer(SprdHWLayer *l);

    /*
     *  Prepare for Display Controller.
     *  return value:
     *      0: use DispC to accelerate.
     *      1: use OverlayComposer to accelerate.
     *      -1: cannot find available accelerator.
     * */
    int prepareForDispC(SprdHWLayer *l);

    int prepareOVCLayer(SprdHWLayer *l);

    int revisitOVCLayers(int& DisplayFlag);

#ifdef TRANSFORM_USE_DCAM
    int DCAMTransformPrepare(hwc_layer_1_t *layer, struct sprdRectF *srcRect, struct sprdRect *FBRect);
#endif

    bool IsHWCLayer(SprdHWLayer *layer);

    /*
     * set a HW layer as Overlay flag.
     * */
    void setOverlayFlag(SprdHWLayer *l, unsigned int index);

    /*
     *  reset a HW layer as normal framebuffer flag
     * */
    void resetOverlayFlag(SprdHWLayer *l);

    /*
     *  Force to set a layer to Overlay flag.
     * */
    void forceOverlay(SprdHWLayer *l, int32_t compositionType);

    /*
     *  Clear framebuffer content to black color.
     * */
    void ClearFrameBuffer(SprdHWLayer *l, unsigned int index);

    void HWCLayerPreCheck();

    void dump_layer(SprdHWLayer *l);
    void dump_yuv(uint8_t* pBuffer, uint32_t aInBufSize);

    inline int MIN(int x, int y)
    {
        return ((x < y) ? x : y);
    }

    inline int MAX(int x, int y)
    {
        return ((x > y) ? x : y);
    }
};
#endif
