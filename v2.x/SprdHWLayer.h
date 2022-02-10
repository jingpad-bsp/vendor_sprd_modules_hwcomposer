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
 ** File: SprdHWLayer.h               DESCRIPTION                             *
 **                                   Mainly responsible for filtering HWLayer*
 **                                   list, find layers that meet OverlayPlane*
 **                                   and PrimaryPlane specifications and then*
 **                                   mark them as HWC_OVERLAY.               *
 ******************************************************************************
 ******************************************************************************
 ** Author:         zhongjun.chen@spreadtrum.com                              *
 *****************************************************************************/


#ifndef _SPRD_HWLAYER_H_
#define _SPRD_HWLAYER_H_

#include <hardware/hardware.h>
#include <hardware/hwcomposer2.h>
#include <utils/RefBase.h>
#include <cutils/atomic.h>
#include <cutils/log.h>
#include <utils/Vector.h>
#include "gralloc_public.h"

#include "SprdHWC2DataType.h"

#include "SprdPrimaryDisplayDevice/SprdFrameBufferHAL.h"

using namespace android;

/*
 *  Accelerator mode
 * */
#define ACCELERATOR_NON             (0x00000000)
#define ACCELERATOR_DISPC           (0x00000001)
#define ACCELERATOR_GSP             (0x00000010)
#define ACCELERATOR_OVERLAYCOMPOSER (0x00000100)  // GPU
#define ACCELERATOR_DCAM            (0x00010000)
#define ACCELERATOR_DISPC_BACKUP    (0x00100000)


/*
 * Blend modes, corresponds to hwc1.x
 */
#define SPRD_HWC_BLENDING_NONE      0x0100
#define SPRD_HWC_BLENDING_PREMULT   0x0105
#define SPRD_HWC_BLENDING_COVERAGE  0x0405

/*
 *  YUV format layer info.
 * */
struct sprdYUV {
    uint32_t w;
    uint32_t h;
    uint32_t format;
    uint32_t y_addr;
    uint32_t u_addr;
    uint32_t v_addr;
};

/*
 *  Available layer rectangle.
 * */
struct sprdRect {
    union {
      uint32_t    x;
      uint32_t left;
    };
    union {
      uint32_t   y;
      uint32_t top;
    };
    uint32_t w;
    uint32_t h;
    uint32_t right;
    uint32_t bottom;
};

struct sprdRectF {
    union {
      float    x;
      float left;
    };
    union {
      float   y;
      float   top;
    };
    float   w;
    float   h;
    float   right;
    float   bottom;
};

struct sprdPoint {
    uint32_t x;
    uint32_t y;
};

typedef struct _color {
  uint8_t r;
  uint8_t g;
  uint8_t b;
  uint8_t a;
} color_t;

typedef struct sprdRect sprdRegion_t;
struct _Rects {
  uint32_t       numRects;
  sprdRegion_t   *rects;
};
typedef struct _Rects DamageRegion_t;
typedef struct _Rects VisibleRegion_t;

enum layerType {
    LAYER_OSD = 1,// means rgb format, should bind to OSD layer of dispc
    LAYER_OVERLAY,// means yuv format, should bind to IMG layer of dispc
    LAYER_SURFACEFLINGER, // means go to SurfaceFlinger for composition
    LAYER_INVALIDE
};


#define MAGIC_NUM 0x456982

/*
 *  SprdHWLayer wrapped the hwc_layer_1_t which come from SF.
 *  SprdHWLayer is a local layer abstract, include src-rect,
 *  dst-rect, overlay buffer layer's handle,etc.
 * */
class SprdHWLayer
{
public:
    /*
     *  SprdHWLayer
     *  default constractor used to wrap src layer.
     * */
    SprdHWLayer()
        : mInit(false),
          mLayerType(LAYER_INVALIDE),
          mPrivateH(NULL),
          mSideBandStream(NULL),
          mFormat(-1),
          mLayerIndex(-1),
          mSprdLayerIndex(-1),
          mAccelerator(-1),
          mProtectedFlag(false),
          mPlaneAlpha(1.0),
          mBlendMode(HWC_BLENDING_NONE),
          mTransform(0x0),
          mAcquireFenceFd(-1),
          mCompositionType(COMPOSITION_INVALID),
          mCompositionChangedFlag(false),
          mLayerRequest(LAYER_REQUEST_NONE),
          mLayerRequestFlag(false),
          mZOrder(0),
          mDataSpace(0),
          mMagic(MAGIC_NUM),
          mDebugFlag(0),
          mHasColorMatrix(false)
    {
        memset(&mColor, 0x00, sizeof(mColor));
        memset(&mDamageRegion, 0x00, sizeof(mDamageRegion));
        memset(&mVisibleRegion, 0x00, sizeof(mVisibleRegion));
    }

    SprdHWLayer(native_handle_t *handle, int format, float planeAlpha,
                int32_t blending, int32_t transform, int32_t fenceFd, uint32_t zorder);

    SprdHWLayer(native_handle_t *handle, int format, int32_t fenceFd,
                int32_t dataspace, hwc_region_t damage, uint32_t zorder);

    ~SprdHWLayer()
    {
      if (mDamageRegion.rects)
      {
        free(mDamageRegion.rects);
        mDamageRegion.rects = NULL;
      }

      if (mVisibleRegion.rects)
      {
        free(mVisibleRegion.rects);
        mVisibleRegion.rects = NULL;
      }
    }

    inline bool InitCheck()
    {
        bool flag = false;
        if ((mCompositionType != COMPOSITION_CLIENT)
            && (mCompositionType != COMPOSITION_INVALID))
        {
          flag = true;
        }

        return flag;
    }

    inline void setLayerAccelerator(int flag)
    {
        mAccelerator = flag;
    }

    void setHasColorMatrix(bool hasColorMatrix)
    {
        mHasColorMatrix = hasColorMatrix;
    }

    bool getHasColorMatrix()
    {
        return mHasColorMatrix;
    }

    inline int getLayerIndex() const
    {
        return mLayerIndex;
    }

    inline int getSprdLayerIndex() const
    {
        return mSprdLayerIndex;
    }

    inline enum layerType getLayerType() const
    {
        return mLayerType;
    }

    inline int getLayerFormat() const
    {
        return mFormat;
    }

    inline struct sprdRect *getSprdSRCRect()
    {
        return &srcRect;
    }

    inline struct sprdRectF *getSprdSRCRectF()
    {
        return &srcRectF;
    }

    inline struct sprdRect *getSprdFBRect()
    {
        return &FBRect;
    }

/* //no reference
    inline bool checkContiguousPhysicalAddress(struct private_handle_t *privateH)
    {
        return (privateH->flags & private_handle_t::PRIV_FLAGS_USES_PHY);
    }
*/

    inline int getAccelerator() const
    {
        return mAccelerator;
    }

    inline void resetAccelerator()
    {
        mAccelerator = 0;
    }

    inline bool getProtectedFlag() const
    {
        return mProtectedFlag;
    }

    inline int getPlaneAlpha() const
    {
        return mPlaneAlpha * 255;
    }

    inline float getPlaneAlphaF() const
    {
        return mPlaneAlpha;
    }

    inline int32_t getBlendMode() const
    {
        return mBlendMode;
    }

    inline uint32_t getTransform() const
    {
        return  mTransform;
    }

    inline native_handle_t *getBufferHandle() const
    {
        return mPrivateH;
    }

    inline int getAcquireFence() const
    {
        return mAcquireFenceFd;
    }

    inline int *getAcquireFencePointer()
    {
        return &mAcquireFenceFd;
    }

    inline struct sprdRect *getCursorPosition()
    {
      return &srcRect;
    }

    inline DamageRegion_t *getDamageRegion()
    {
      return &mDamageRegion;
    }

    inline color_t *getColor()
    {
      return &mColor;
    }

    inline int32_t getCompositionType() const
    {
      return mCompositionType;
    }

    inline bool getCompositionChangedFlag() const
    {
        return mCompositionChangedFlag;
    }

    inline int32_t getLayerRequest() const
    {
      return mLayerRequest;
    }

    inline bool getLayerRequestFlag() const
    {
        return mLayerRequestFlag;
    }

    inline int32_t getDataSpace() const
    {
      return mDataSpace;
    }

    inline native_handle_t *getSidebandStream() const
    {
      return mSideBandStream;
    }

    inline VisibleRegion_t *getVisibleRegion()
    {
      return &mVisibleRegion;
    }

    inline uint32_t getZOrder() const
    {
      return mZOrder;
    }

    bool checkRGBLayerFormat();
    bool checkYUVLayerFormat();

    static SprdHWLayer *remapFromAndroidLayer(hwc2_layer_t layer);
    static hwc2_layer_t remapToAndroidLayer(SprdHWLayer *l);

private:
    friend class SprdHWLayerList;
    friend class SprdVDLayerList;
    friend class SprdHandleLayer;

    bool mInit;
    enum layerType mLayerType;// indicate this layer should bind to OSD/IMG layer of dispc
    native_handle_t *mPrivateH;
    native_handle_t *mSideBandStream;
    int mFormat;
    int mLayerIndex;
    int mSprdLayerIndex;
    struct sprdRect srcRect;
    struct sprdRectF srcRectF; // float, used for OVC
    struct sprdRect FBRect;
    int mAccelerator;//default is ACCELERATOR_OVERLAYCOMPOSER, then check dispc&gsp can process or not.
    bool mProtectedFlag;
    float  mPlaneAlpha;
    int32_t  mBlendMode;
    int32_t  mTransform;
    int32_t  mAcquireFenceFd;
    int32_t  mCompositionType;
    bool     mCompositionChangedFlag;
    int32_t  mLayerRequest;
    bool     mLayerRequestFlag;
    uint32_t mZOrder;
    int32_t  mDataSpace;
    color_t  mColor;
    DamageRegion_t mDamageRegion;
    VisibleRegion_t mVisibleRegion;
    int32_t mMagic;
    int mDebugFlag;
    bool mHasColorMatrix;


    inline void setLayerIndex(unsigned int index)
    {
        mLayerIndex = index;
    }

    inline void setSprdLayerIndex(unsigned int index)
    {
        mSprdLayerIndex = index;
    }

    inline void setLayerType(enum layerType t)
    {
        mLayerType = t;
    }

    inline void setLayerFormat(int f)
    {
        mFormat = f;
    }

    inline void setProtectedFlag(bool flag)
    {
        mProtectedFlag = flag;
    }

    inline void setPlaneAlpha(float alpha)
    {
        mPlaneAlpha = alpha;
    }

    inline void setTransform(int32_t transform)
    {
        mTransform = transform;
    }

    inline void setAcquireFenceFd(int fd)
    {
        mAcquireFenceFd = fd;
    }

    inline int32_t setCursorPosition(int32_t x, int32_t y)
    {
      srcRect.x   = x;
      srcRect.y   = y;
      return 0;
    }

    inline int32_t setBuffer(native_handle_t *buf, int32_t acquireFence)
    {
      mPrivateH       = buf;
      mAcquireFenceFd = acquireFence;
      return 0;
    }

    inline int32_t setDisplayFrame(hwc_rect_t frame)
    {
      FBRect.x      = frame.left;
      FBRect.y      = frame.top;
      FBRect.right  = frame.right;
      FBRect.bottom = frame.bottom;
      FBRect.w      = frame.right  - frame.left;
      FBRect.h      = frame.bottom - frame.top;
      return 0;
    }

    int32_t setSurfaceDamage(hwc_region_t damage);

    inline int32_t setBlendMode(int32_t mode)
    {
      switch (mode) {
	  case HWC2_BLEND_MODE_NONE:
		  mBlendMode = SPRD_HWC_BLENDING_NONE;
		  break;
	  case HWC2_BLEND_MODE_PREMULTIPLIED:
		  mBlendMode = SPRD_HWC_BLENDING_PREMULT;
		  break;
	  case HWC2_BLEND_MODE_COVERAGE:
		  mBlendMode = SPRD_HWC_BLENDING_COVERAGE;
		  break;
	  default:
		  mBlendMode = SPRD_HWC_BLENDING_NONE;
		  break;
      }
      return 0;
    }

    int32_t setColor(hwc_color_t color);

    inline int32_t setCompositionType(int32_t type)
    {
      if (mCompositionType == COMPOSITION_INVALID)
      {
        mCompositionChangedFlag = false;
      }
      else
      {
        mCompositionChangedFlag = (mCompositionType == type) ? false : true;
      }
      mCompositionType        = type;
      return 0;
    }

    inline int32_t setLayerRequest(int32_t Request)
    {
      mLayerRequestFlag = ((mLayerRequest & CLEAR_CLIENT_TARGET)
                            == CLEAR_CLIENT_TARGET) ? false : true;
      mLayerRequest     = Request;
      return 0;
    }

    inline int32_t setDataSpace(int32_t dataspace)
    {
      mDataSpace = dataspace;
      return 0; 
    }

    inline int32_t setSidebandStream(native_handle_t *stream)
    {
      mSideBandStream = stream;
      return 0;
    }

    inline int32_t setSourceCrop(hwc_frect_t crop)
    {
      srcRect.x      = crop.left;
      srcRect.y      = crop.top;
      srcRect.w      = crop.right  - srcRect.x;
      srcRect.h      = crop.bottom - srcRect.y;
      srcRect.right  = crop.right;
      srcRect.bottom = crop.bottom;

      // float
      srcRectF.x      = crop.left;
      srcRectF.y      = crop.top;
      srcRectF.w      = crop.right  - srcRectF.x;
      srcRectF.h      = crop.bottom - srcRectF.y;
      srcRectF.right  = crop.right;
      srcRectF.bottom = crop.bottom;
      return 0;
    }

    int32_t setVisibleRegion(hwc_region_t visible);

    inline int32_t setZOrder(uint32_t z) 
    {
      mZOrder = z;
      return 0;
    }
};

typedef Vector<SprdHWLayer *> LIST;

#endif
