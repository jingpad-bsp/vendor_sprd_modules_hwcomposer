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
 ** 16/06/2014    Hardware Composer   Responsible for processing some         *
 **                                   Hardware layers. These layers comply    *
 **                                   with Virtual Display specification,     *
 **                                   can be displayed directly, bypass       *
 **                                   SurfaceFligner composition. It will     *
 **                                   improve system performance.             *
 ******************************************************************************
 ** File:SprdVirtualPlane.h           DESCRIPTION                             *
 **                                   Responsible for Post display data to    *
 **                                   Virtual Display.                        *
 ******************************************************************************
 ******************************************************************************
 ** Author:         zhongjun.chen@spreadtrum.com                              *
 *****************************************************************************/


#ifndef _SPRD_VIRTUAL_PLANE_H_
#define _SPRD_VIRTUAL_PLANE_H_

#include <hardware/hardware.h>
#include <hardware/gralloc.h>
#include <hardware/hwcomposer2.h>
#include <cutils/log.h>
#include "../SprdDisplayPlane.h"
#include "../SprdHWLayer.h"
#include "../dump.h"


using namespace android;


class SprdVirtualPlane: public SprdDisplayPlane
{
public:
    SprdVirtualPlane();
    virtual ~SprdVirtualPlane();

    /*
     *  These interfaces are from the father class SprdDisplayPlane.
     *  dequeuebuffer: gain a available buffer for SprdVirtualPlane.
     *  queueBuffer: display a buffer.
     * */
    virtual native_handle_t *dequeueBuffer(int *fenceFd);
    virtual int queueBuffer(int fenceFd);
    /*
     *  Restore the plane info to unused state.
     * */
    virtual void InvalidatePlane();

    /*
     *  setup the DisplayPlane context.
     * */
    virtual int setPlaneContext(void *context);
    virtual native_handle_t* getPlaneBuffer() const;
    virtual void getPlaneGeometry(unsigned int *width, unsigned int *height, int *format) const;
    virtual PlaneContext *getPlaneContext() const;
    /*****************************************************************************/

    virtual bool open();
    virtual bool close();

    /*
     *  Bind HWC layers to SprdVirtualPlane
     * */
    void AttachVDLayer(SprdHWLayer **videoLayerList, int videoLayerCount, SprdHWLayer **osdLayerList, int osdLayerCount);

    /*
     *  Attach HWC_FRAMEBUFFER_TARGET layer to SprdVirtualPlane.
     * */
    int AttachVDFramebufferTargetLayer(SprdHWLayer *SprdFBTLayer);
    /*
     *  Set output Layer list to SprdVirtualPlane.
     * */
    int AttachOutputLayer(SprdHWLayer *outputLayer)
    {
        mOutputLayer = outputLayer;
        return 0;
    }

    inline int getPlaneFormat() const
    {
        return mPlaneFormat;
    }

    inline SprdHWLayer *getSprdHWSourceLayer() const
    {
        return mFBTLayer;
    }

    inline SprdHWLayer **getSourceVideoLayerList() const
    {
        return mVideoLayerList;
    }

    inline int getSourceVideoLayerCount() const
    {
        return mVideoLayerCount;
    }

    inline SprdHWLayer **getSourceOSDLayerList() const
    {
        return mOSDLayerList;
    }

    inline int getSourceOSDLayerCount() const
    {
        return mOSDLayerCount;
    }



private:
    int mPlaneWidth;
    int mPlaneHeight;
    int mPlaneFormat;
    int mDefaultPlaneFormat;
    int mVideoLayerCount;
    int mOSDLayerCount;
    SprdHWLayer **mVideoLayerList;
    SprdHWLayer **mOSDLayerList;
    SprdHWLayer *mFBTLayer;
    SprdHWLayer *mOutputLayer;
    native_handle_t *mDisplayBuffer;
    int mDebugFlag;
    int mDumpFlag;

    void resetPlaneGeometry();

    /*
     *  Attach DisplayBuffer to SprdVirtualPlane. 
     * */
    void AttachDisplayBuffer(native_handle_t *outputBuffer);
};



#endif
