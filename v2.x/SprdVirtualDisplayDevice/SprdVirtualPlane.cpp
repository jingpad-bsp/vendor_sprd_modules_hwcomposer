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
 ** File:SprdVirtualPlane.cpp         DESCRIPTION                             *
 **                                   Responsible for Post display data to    *
 **                                   Virtual Display.                        *
 ******************************************************************************
 ******************************************************************************
 ** Author:         zhongjun.chen@spreadtrum.com                              *
 *****************************************************************************/


#include "SprdVirtualPlane.h"
#include "dump.h"
#include "AndroidFence.h"


using namespace android;


SprdVirtualPlane:: SprdVirtualPlane()
    : SprdDisplayPlane(),
      mPlaneWidth(1),
      mPlaneHeight(1),
      mPlaneFormat(-1),
      mDefaultPlaneFormat(HAL_PIXEL_FORMAT_YCbCr_420_SP),
      mVideoLayerCount(0),
      mOSDLayerCount(0),
      mVideoLayerList(0),
      mOSDLayerList(0),
      mFBTLayer(0),
      mDisplayBuffer(0),
      mDebugFlag(0),
      mDumpFlag(0)
{

}

SprdVirtualPlane:: ~SprdVirtualPlane()
{

}

bool SprdVirtualPlane:: open()
{
    return true;
}

bool SprdVirtualPlane:: close()
{
    return true;
}

void SprdVirtualPlane:: InvalidatePlane()
{

}

int SprdVirtualPlane:: setPlaneContext(void *context)
{
    HWC_IGNORE(context);
    return 0;
}

native_handle_t* SprdVirtualPlane:: dequeueBuffer(int *fenceFd)
{
    native_handle_t *outHandle = (mOutputLayer == NULL) ?
                                         NULL : mOutputLayer->getBufferHandle();
    if (outHandle == NULL)
    {
        ALOGE("SprdVirtualPlane:: dequeueBuffer outHandle is NULL");
        return NULL;
    }

    AttachDisplayBuffer(outHandle);

    if (mDisplayBuffer == NULL)
    {
        ALOGE("SprdVirtualPlane:: dequeueBuffer failed");
        return NULL;
    }
    native_handle_t* privateH = mDisplayBuffer;
    if (privateH == NULL)
    {
        ALOGE("SprdVirtualPlane:: dequeueBuffer Display Buffer handle is NULL");
        return NULL;
    }

    queryDebugFlag(&mDebugFlag);

    mPlaneWidth = ADP_WIDTH(privateH);
    mPlaneHeight = ADP_HEIGHT(privateH);
    mPlaneFormat = mDefaultPlaneFormat;

    ALOGI_IF(mDebugFlag, "SprdVirtualPlane::dequeueBuffer width:%d, height: %d, format: 0x%x fd:%d",
		mPlaneWidth, mPlaneHeight, mPlaneFormat, ADP_BUFFD(privateH));

    *fenceFd = -1;

    return mDisplayBuffer;
}

int SprdVirtualPlane:: queueBuffer(int fenceFd)
{
    ALOGI_IF(mDebugFlag, "SprdVirtualPlane:: queueBuffer HWC has finished the blit operation,fenceFd:%d",fenceFd);

    queryDumpFlag(&mDumpFlag);

    if (HWCOMPOSER_DUMP_VD_OVERLAY_FLAG & mDumpFlag)
    {
        const char *name = "VirtualDisplayYUV";

        dumpOverlayImage(mDisplayBuffer, name, fenceFd);
    }

    resetPlaneGeometry();

    return 0;
}

void SprdVirtualPlane:: resetPlaneGeometry()
{
    mPlaneWidth = 1;
    mPlaneHeight = 1;
    mPlaneFormat = -1;
}

int SprdVirtualPlane:: AttachVDFramebufferTargetLayer(SprdHWLayer *SprdFBTLayer)
{
    if (SprdFBTLayer == NULL)
    {
        ALOGE("SprdVirtualPlane:: AttachVDFramebufferTargetLayer input is NULL");
        return -1;
    }

    mFBTLayer = SprdFBTLayer;

    return 0;
}

void SprdVirtualPlane:: AttachDisplayBuffer(native_handle_t *outputBuffer)
{
    mDisplayBuffer = outputBuffer;
}

void SprdVirtualPlane:: getPlaneGeometry(unsigned int *width, unsigned int *height, int *format) const
{
    if (width == NULL || height == NULL || format == NULL)
    {
        ALOGE("SprdVirtualPlane:: getPlaneGeometry input para is NULL");
        return;
    }

    *width = mPlaneWidth;
    *height = mPlaneHeight;
    *format = mPlaneFormat;
}

void SprdVirtualPlane:: AttachVDLayer(SprdHWLayer **videoLayerList, int videoLayerCount, SprdHWLayer **osdLayerList, int osdLayerCount)
{
    if (videoLayerList == NULL || osdLayerList == NULL)
    {
        ALOGE("SprdVirtualPlane:: AttachVDLayer Sprd Layer List is NULL");
        mVideoLayerList = NULL;
        mOSDLayerList = NULL;
        mVideoLayerCount = 0;
        mOSDLayerCount = 0;
        return;
    }

    mVideoLayerList = videoLayerList;
    mVideoLayerCount = videoLayerCount;

    mOSDLayerList = osdLayerList;
    mOSDLayerCount = osdLayerCount;
}

native_handle_t* SprdVirtualPlane:: getPlaneBuffer() const
{
    return mDisplayBuffer;
}

PlaneContext *SprdVirtualPlane:: getPlaneContext() const
{
    return NULL;
}
