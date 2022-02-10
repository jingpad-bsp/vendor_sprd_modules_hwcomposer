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
 ** File:SprdVDLayerList.h            DESCRIPTION                             *
 **                                   Responsible for traverse Virtual Display*
 **                                   Layer list and mark the layers as       *
 **                                   Overlay which                           *
 **                                   comply with Sprd Virtual Display spec.  *
 ******************************************************************************
 ******************************************************************************
 ** Author:         zhongjun.chen@spreadtrum.com                              *
 *****************************************************************************/


#ifndef _SPRD_VD_LAYER_LIST_H_
#define _SPRD_VD_LAYER_LIST_H_

#include <hardware/hardware.h>
#include <hardware/hwcomposer2.h>
#include <utils/RefBase.h>
#include <cutils/atomic.h>
#include <cutils/log.h>
#include "gralloc_public.h"

#include "../SprdHWLayer.h"
#include "../dump.h"

using namespace android;

class SprdVDLayerList
{
public:
    SprdVDLayerList()
        : mOSDLayerList(0),
          mVideoLayerList(0),
          mLayerCount(0),
          mOSDLayerCount(0),
          mVideoLayerCount(0),
          mFBLayerCount(0),
          mCompositionChangedNum(0),
          mRequestLayerNum(0),
          mValidateDisplayed(false),
          mSkipMode(false),
          mDebugFlag(0),
          mDumpFlag(0)
    {

    }
    virtual ~SprdVDLayerList();

    int32_t acceptGeometryChanged();

    int32_t createSprdLayer(hwc2_layer_t* outLayer);

    int32_t destroySprdLayer(hwc2_layer_t layer);

    int32_t getDisplayRequests(int32_t* outDisplayRequests,
                               uint32_t* outNumElements, hwc2_layer_t* outLayers,
                               int32_t* outLayerRequests);

    int32_t getChangedCompositionTypes(uint32_t* outNumElements, hwc2_layer_t* outLayers,
                                       int32_t* outTypes);

    int32_t validateDisplay(uint32_t* outNumTypes, uint32_t* outNumRequests);

    inline LIST& getHWCLayerList()
    {
      return mList;
    }

    inline unsigned int getSprdLayerCount()
    {
        return mLayerCount;
    }

    inline SprdHWLayer **getSprdOSDLayerList()
    {
        return mOSDLayerList;
    }

    inline int getOSDLayerCount()
    {
        return mOSDLayerCount;
    }

    inline SprdHWLayer **getSprdVideoLayerList()
    {
        return mVideoLayerList;
    }

    inline int getVideoLayerCount()
    {
        return mVideoLayerCount;
    }

    inline unsigned int getFBLayerCount()
    {
        return mFBLayerCount;
    }

    inline bool getSkipMode() const
    {
        return mSkipMode;
    }

private:
    LIST        mList;
    SprdHWLayer **mOSDLayerList;
    SprdHWLayer **mVideoLayerList;
    unsigned int mLayerCount;
    int mOSDLayerCount;
    int mVideoLayerCount;
    int mFBLayerCount;
    uint32_t mCompositionChangedNum;
    uint32_t mRequestLayerNum;
    bool mValidateDisplayed;
    bool mSkipMode;
    int mDebugFlag;
    int mDumpFlag;

    int updateGeometry();
    int revisitGeometry();


    int prepareOSDLayer(SprdHWLayer *l);
    int prepareVideoLayer(SprdHWLayer *l);

    void reclaimSprdHWLayer();

    void ClearFrameBuffer(SprdHWLayer *l, unsigned int index) ;
    void setOverlayFlag(SprdHWLayer *l, unsigned int index);
    void forceOverlay(SprdHWLayer *l, int32_t compositionType);
    void resetOverlayFlag(SprdHWLayer *l);

    void dump_layer(SprdHWLayer *l);
    bool IsHWCLayer(SprdHWLayer *layer);
};

#endif
