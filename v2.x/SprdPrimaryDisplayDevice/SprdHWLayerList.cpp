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
 ** File: SprdHWLayerList.cpp         DESCRIPTION                             *
 **                                   Mainly responsible for filtering HWLayer*
 **                                   list, find layers that meet OverlayPlane*
 **                                   and PrimaryPlane specifications and then*
 **                                   mark them as HWC_OVERLAY.               *
 ******************************************************************************
 ******************************************************************************
 ** Author:         zhongjun.chen@spreadtrum.com                              *
 *****************************************************************************/

#include "SprdFrameBufferHAL.h"
#include "SprdHWLayerList.h"
#include "dump.h"
#include "SprdUtil.h"

#include "SprdHWC2DataType.h"

using namespace android;


SprdHWLayerList::~SprdHWLayerList()
{
    if (!(mList.isEmpty()))
    {
      for (size_t i = 0; i < mList.size(); i++)
      {
        SprdHWLayer *l = mList[i];
        if (l)
        {
          delete l;
          l = NULL;;
          mList.removeAt(i);
        }
      }
      mList.clear();
    }

    if (mGXPLayerList)
    {
        delete [] mGXPLayerList;
        mGXPLayerList = NULL;
    }

    if (mOVCLayerList)
    {
      delete [] mOVCLayerList;
      mOVCLayerList = NULL;
    }

    if (mDispCLayerList)
    {
        delete [] mDispCLayerList;
        mDispCLayerList = NULL;
    }

    if (mLayerList)
    {
        delete [] mLayerList;
        mLayerList = NULL;
    }
}

void SprdHWLayerList::dump_yuv(uint8_t* pBuffer,uint32_t aInBufSize)
{
    FILE *fp = fopen("/data/video.data","ab");
    if (fp) {
        fwrite(pBuffer,1,aInBufSize,fp);
        fclose(fp);
    }
}

void SprdHWLayerList::dump_layer(SprdHWLayer *l) {
    if (l == NULL)
    {
      return;
    }

    ALOGI_IF(mDebugFlag , "Layer:%p z=%d, type=%d, format=0x%x, handle=%p, tr=%02x, blend=%04x,"
             " crop{%f,%f,%f,%f}, dst{%d,%d,%d,%d}, planeAlpha: %d",
             (void *)l,
             l->getZOrder(),
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

void SprdHWLayerList:: HWCLayerPreCheck()
{
    char value[PROPERTY_VALUE_MAX];

    property_get("debug.hwc.disable", value, "0");

    if (atoi(value) == 1)
    {
        mDisableHWCFlag = true;
    }
    else
    {
        mDisableHWCFlag = false;;
    }
}

bool SprdHWLayerList::IsHWCLayer(SprdHWLayer *layer)
{
  if (layer == NULL)
  {
    ALOGE("SprdHWLayerList::IsHWCLayer layer is NULL");
    return false;
  }

    if (layer->getCompositionType() == COMPOSITION_CLIENT)
    {
        ALOGI_IF(mDebugFlag, "SF request COMPOSITION_CLIENT, Skip layer");
        return false;
    }
    else if (layer->getCompositionType() == COMPOSITION_SOLID_COLOR)
    {
        ALOGI_IF(mDebugFlag, "SF request COMPOSITION_SOLID_COLOR, Skip layer");
        return false;
    }

    /*
     *  Here should check buffer usage
     * */

    return true;
}


/* public func for HWC2 */
int32_t SprdHWLayerList:: acceptGeometryChanged()
{
  if (mValidateDisplayed == false)
  {
    ALOGE("SprdHWLayerList:: acceptGeometryChanged validateDisplay should be called first");
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
        ALOGI_IF(mDebugFlag, "SprdHWLayerList:: acceptGeometryChanged resetOverlayFlag layer: %d", (int)i);
    }
  }

  mValidateDisplayed = false;

  return ERR_NONE;
}

int32_t SprdHWLayerList:: createSprdLayer(hwc2_layer_t* outLayer)
{
  SprdHWLayer *sprdLayer = NULL;

  sprdLayer = new SprdHWLayer();
  if (sprdLayer == NULL)
  {
    ALOGE("SprdHWLayerList:: createSprdLayer failed");
    return ERR_NO_RESOURCES;
  }

  mList.add(sprdLayer);
  *outLayer = SprdHWLayer::remapToAndroidLayer(sprdLayer);

  ALOGI_IF(mDebugFlag, "SprdHWLayerList:: createSprdLayer Id:0x%lx", (unsigned long)(*outLayer));

  return ERR_NONE;
}

int32_t SprdHWLayerList:: destroySprdLayer(hwc2_layer_t layer)
{
  bool find = false;
  size_t i;
  SprdHWLayer *sprdLayer = NULL;

  sprdLayer = SprdHWLayer::remapFromAndroidLayer(layer);
  if (sprdLayer == NULL)
  {
    ALOGE("SprdHWLayerList:: destroySprdLayer BAD hwc2 layer");
    return ERR_BAD_LAYER;
  }

  for (i = 0; i < mList.size(); i++)
  {
    if (sprdLayer == mList[i])
    {
      ALOGI_IF(mDebugFlag, "SprdHWLayerList:: destroySprdLayer Id:0x%lx", (unsigned long)layer);
      delete mList[i];
      mList.removeAt(i);
      find = true;
      break;
    }
  }

  if (find == false)
  {
    ALOGI_IF(mDebugFlag, "SprdHWLayerList:: destroySprdLayer Illegal Id:0x%lx", (unsigned long)layer);
  }

  sprdLayer = NULL;

  return ERR_NONE;  
}

int32_t SprdHWLayerList:: getChangedCompositionTypes(
                          uint32_t* outNumElements, hwc2_layer_t* outLayers, int32_t* outTypes)
{
  uint32_t index = 0;

  if (outNumElements == NULL)
  {
    ALOGE("SprdHWLayerList:: getChangedCompositionTypes input is NULL");
    return ERR_BAD_PARAMETER;
  }

  if (mValidateDisplayed == false)
  {
    ALOGE("SprdHWLayerList:: getChangedCompositionTypes validateDisplay should be called first");
    return ERR_NOT_VALIDATED;
  }

  *outNumElements = mCompositionChangedNum;

  if (outLayers && outTypes)
  {
    uint32_t ChangedNum = 0;
    for (unsigned int i = 0; i < mLayerCount; i++)
    {
      SprdHWLayer *l = mList[i];
      if (l && (l->getCompositionChangedFlag()) &&
          (index < mCompositionChangedNum))
      {
        outLayers[index] = SprdHWLayer::remapToAndroidLayer(l);
        outTypes[index]  = l->getCompositionType();
        index++;
        ChangedNum++;

        ALOGI_IF(mDebugFlag, "SprdHWLayerList:: getChangedCompositionTypes layer[%d], Id: 0x%lx",
                 i, (unsigned long)(outLayers[i]));
      }
    }

    if (ChangedNum != mCompositionChangedNum)
    {
      ALOGE("SprdHWLayerList:: getChangedCompositionTypes mCompositionChangedNum error");
    }
  }
  else
  {
    ALOGI_IF(mDebugFlag, "SprdHWLayerList:: getChangedCompositionTypes outLayers:0x%p, outTypes: 0x%p",
          (void *)outLayers, (void *)outTypes);
  }

  return ERR_NONE;
}

int32_t SprdHWLayerList:: getDisplayRequests(int32_t *outDisplayRequests, uint32_t* outNumElements,
                                             hwc2_layer_t* outLayers, int32_t* outLayerRequests)
{
  uint32_t index = 0;

  if (mValidateDisplayed == false)
  {
    ALOGE("SprdHWLayerList:: getDisplayRequests validateDisplay should be called first");
    return ERR_NOT_VALIDATED;
  }

  if (outDisplayRequests)
  {
    *outDisplayRequests = 0;
  }

  if (outNumElements == NULL)
  {
    ALOGE("SprdHWLayerList:: getDisplayRequests outNumElements is NULL");
    return ERR_BAD_PARAMETER;
  }

  *outNumElements = mCompositionChangedNum;
  ALOGI_IF(mDebugFlag, "SprdHWLayerList:: getDisplayRequests mCompositionChangedNum: %d", mCompositionChangedNum);

  if (outLayers && outLayerRequests)
  {
    uint32_t ChangedNum = 0;
    for (unsigned int i = 0; i < mLayerCount; i++)
    {
      SprdHWLayer *l = mList[i];
      if (l && (l->getCompositionChangedFlag()) &&
          (index < mCompositionChangedNum))
      {
        outLayers[index]        = SprdHWLayer::remapToAndroidLayer(l);
        outLayerRequests[index] = l->getLayerRequest();
        index++;
        ChangedNum++;

        ALOGI_IF(mDebugFlag, "SprdHWLayerList:: getDisplayRequests layer[%d], Id: 0x%lx",
                 i, (unsigned long)(outLayers[i]));
      }
    }

    if (ChangedNum > mCompositionChangedNum)
    {
      ALOGE("SprdHWLayerList:: getDisplayRequests mCompositionChangedNum error");
    }
  }
  else
  {
    ALOGI_IF(mDebugFlag, "SprdHWLayerList:: getDisplayRequests outLayers:0x%p, outLayerRequests: 0x%p",
          (void *)outLayers, (void *)outLayerRequests);
  }

  return ERR_NONE;
}

int32_t SprdHWLayerList:: validateDisplay(uint32_t* outNumTypes, uint32_t* outNumRequests,
                                           int accelerator, int& DisplayFlag,
                                           SprdPrimaryDisplayDevice *mPrimary)
{
  int32_t err = ERR_NONE;

  if (outNumTypes == NULL || outNumRequests == NULL)
  {
    ALOGE("SprdHWLayerList:: validate_display outNumTypes/outNumRequests is NULL");
    return ERR_BAD_PARAMETER;
  }

  if (updateGeometry(accelerator) !=0)
  {
    ALOGE("SprdHWLayerList:: validate_display updateGeometry failed");
    return ERR_NO_RESOURCES;
  }

  if (revisitGeometry(DisplayFlag, mPrimary) != 0)
  {
    ALOGE("SprdHWLayerList:: validate_display revisitGeometry failed");
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
/* public func done */

/*
 *  function:updateGeometry
 *	check the list whether can be process by these accelerator.
 *	this function will init local layer object mLayerList from hwc_display_contents_1_t.
 *	it focus mainly on single layer check.
 *  accelerator:available accelerator for this display.
 *  list:the app layer list that will be composited and show out.
 * */
int SprdHWLayerList:: updateGeometry(int accelerator)
{
    int ret = -1;
    mLayerCount = 0;
    mOSDLayerCount = 0;
    mVideoLayerCount = 0;
    mDispCLayerCount = 0;
    mGXPLayerCount = 0;
    mOVCLayerCount = 0;
    mYUVLayerCount = 0;
    mCompositionChangedNum = 0;
    mRequestLayerNum = 0;
    mSkipLayerFlag = false;
    mAcceleratorMode = accelerator;
#ifdef FORCE_OVC
    mAcceleratorMode = ACCELERATOR_OVERLAYCOMPOSER;
#endif
    mGXPSupport = false;
    mGlobalProtectedFlag = false;
    mSprdLayerCount = 0;
    bool Acc2D = true;

    if (mGXPLayerList)
    {
        delete [] mGXPLayerList;
        mGXPLayerList = NULL;
    }

    if (mOVCLayerList)
    {
      delete [] mOVCLayerList;
      mOVCLayerList = NULL;
    }

    if (mDispCLayerList)
    {
        delete [] mDispCLayerList;
        mDispCLayerList = NULL;
    }

    if (mLayerList)
    {
        delete [] mLayerList;
        mLayerList = NULL;
    }

    queryDebugFlag(&mDebugFlag);
    queryDumpFlag(&mDumpFlag);
    if (HWCOMPOSER_DUMP_ORIGINAL_LAYERS & mDumpFlag)
    {
        LIST& list = mList;
        dumpImage(list);
    }

    HWCLayerPreCheck();

    if (mDisableHWCFlag)
    {
        ALOGI_IF(mDebugFlag, "HWComposer is disabled now ...");
        return 0;
    }

    /*
     *  list->numHwLayers should not includes the FramebufferTarget layer.
     * */
    mLayerCount = mList.size();

    mLayerList = new SprdHWLayer*[mLayerCount];
    if (mLayerList == NULL)
    {
      ALOGE("Cannot create mLayerList");
      return -1;
    }
    memset(mLayerList, 0x0, mLayerCount * sizeof(long));


    mGXPLayerList = new SprdHWLayer*[mLayerCount];
    if (mGXPLayerList == NULL)
    {
        ALOGE("Cannot create GXPLayerList");
        return -1;
    }
    memset(mGXPLayerList, 0x0, mLayerCount * sizeof(long));

    mOVCLayerList = new SprdHWLayer*[mLayerCount];
    if (mOVCLayerList == NULL)
    {
      ALOGE("Cannot create OVCLayerList");
      return -1;
    }
    memset(mOVCLayerList, 0x0, mLayerCount * sizeof(long));

    mDispCLayerList = new SprdHWLayer*[mLayerCount];
    if (mDispCLayerList == NULL)
    {
        ALOGE("Cannot create DispC Layer list");
        return -1;
    }
    memset(mDispCLayerList, 0x0, mLayerCount * sizeof(long));

    mFBLayerCount = mLayerCount;

    for (unsigned int i = 0; i < mLayerCount; i++)
    {
        unsigned int index = 0;
        SprdHWLayer *layer = mList[i];

        ALOGI_IF(mDebugFlag,"process LayerList[%d/%d]", i, mLayerCount);
        dump_layer(layer);


        if (layer == NULL || layer->getCompositionType() == COMPOSITION_CLIENT ||
            layer->getCompositionType() == COMPOSITION_CURSOR)
        {
            ALOGI_IF(mDebugFlag, "NOT HWC layer");
            mSkipLayerFlag = true;
            mOVCLayerCount++;
            continue;
        }

        prepareOSDLayer(layer);

        prepareVideoLayer(layer);

        if (layer->getZOrder() == i)
        {
            index = i;
        }
        else if (layer->getZOrder() < mLayerCount)
        {
            index = layer->getZOrder();
        }
        else
        {
            ALOGE("updateGeometry L:%p zorder: %d is out of control",
                  (void *)layer, layer->getZOrder());
            index = i;
        }

        if (layer->getCompositionType() != COMPOSITION_SOLID_COLOR)
            mOVCLayerList[index] = layer;
        mOVCLayerCount++;

        mLayerList[index] = layer;
    }

    /*
     *  Prepare Layer geometry for Sprd Own accerlator: GXP/DPU
     * */
    if (mAcceleratorMode & (ACCELERATOR_GSP | ACCELERATOR_DISPC))
    {
        if ((mLayerCount > 0) && (mSkipLayerFlag == false))
        {
            SprdHWLayer *l = NULL;
            ret = mAccerlator->Prepare(mLayerList, mLayerCount, mGXPSupport);

            int dis_gsp = 0;
            queryIntFlag("debug.hwc.gsp.disable",&dis_gsp);
            ALOGI_IF(dis_gsp,"updateGeometry() force mGXPSupport from true to false.");
            mGXPSupport = (dis_gsp>0)?false:mGXPSupport;

            for (unsigned int i = 0; i < mLayerCount; i++)
            {
              l = mLayerList[i];

              if (l == NULL)
              {
                ALOGI_IF(mDebugFlag, "layer is null");
                return -1;
              }

              if ((l->getAccelerator() == ACCELERATOR_GSP) && mGXPSupport)
              {
                unsigned int count   = mGXPLayerCount;
                mGXPLayerList[count] = mLayerList[i];
                mGXPLayerCount++;
                ALOGI_IF(mDebugFlag,"updateGeometry() GXP Process L[%d], mGXPSupport:%d", i, mGXPSupport);
              }
              else if (l->getAccelerator() == ACCELERATOR_DISPC)
              {
                unsigned int count    = mDispCLayerCount;
                mDispCLayerList[count] = mLayerList[i];
                mDispCLayerCount++;
                ALOGI_IF(mDebugFlag, "updateGeometry Dispc process L[%d]", i);
              }
              else
              {
                ALOGI_IF(mDebugFlag, "updateGeometry L:%d, NO 2D ACC, type:%d", i, l->getAccelerator());
                Acc2D = false;
              }
            }

            if (ret != 0)
            {
                ALOGI_IF(mDebugFlag,"SprdHWLayerList:: updateGeometry SprdUtil Prepare failed ret: %d", ret);
                mGXPSupport = false;
            }
        }
        else
        {
            ALOGI_IF(mDebugFlag, "updateGeometry 2D disabled, LayerCount:%d, SkipLayerFlag:%d",
                     mLayerCount, mSkipLayerFlag);
        }

       if (!Acc2D && !mSkipLayerFlag)
       {
           mFBLayerCount += mDispCLayerCount;
           mFBLayerCount += mGXPLayerCount;
           mDispCLayerCount = 0;
           mGXPLayerCount   = 0;
           SprdHWLayer *l = NULL;

           for (unsigned int i = 0; i < mLayerCount; i++)
           {
             l = mLayerList[i];

             if (l == NULL)
             {
               ALOGI_IF(mDebugFlag, "layer is null");
               continue;
             }
             l->setLayerAccelerator(ACCELERATOR_NON);
           }

       }
    }

    return 0;
}

/*
 *  function:revisitGeometry
 *	check the list whether can be process by these accelerator.
 *	it checks at a global view on all layers of this frame.
 * */
int SprdHWLayerList:: revisitGeometry(int& DisplayFlag, SprdPrimaryDisplayDevice *mPrimary)
{
    uint32_t i = 0;
    uint32_t CompositionTypeChangedLocal = 0;
    int LayerCount = mLayerCount;
    bool accelerateByDPC = false; // DPC: Display Controller
    bool accelerateByGXP = false; // GXP: GSP/GPP
    bool accelerateByOVC = false; // OVC: OverlayComposer
    bool OVCSkipLayerFlag = mSkipLayerFlag;

    if (mDisableHWCFlag)
    {
        return 0;
    }

    if (mPrimary == NULL)
    {
        ALOGE("prdHWLayerList:: revisitGeometry input parameters error");
        return -1;
    }

    /*
     *  revisit Overlay layer geometry.
     *  TODO: if Display Controller support composition job,
     *  We should change some condition.
     * */
    if (mDispCLayerCount > 0)
    {
        accelerateByDPC = true;
    }

    if ((mGXPLayerCount > 0) && mGXPSupport)
    {
        accelerateByGXP = true;
    }

    if ((mDispCLayerCount + mGXPLayerCount < (mLayerCount -1)) ||
        (mPrimary->getHasColorMatrix()))
    {
     //ALOGI_IF(mDebugFlag, "(FILE:%s, line:%d, func:%s) revisitGeometry accelerateByGXP :%d, mGXPLayerCount = %d, mDispCLayerCount = %d, mLayerCount = %d",
     //         __FILE__, __LINE__, __func__, accelerateByGXP, mGXPLayerCount, mDispCLayerCount, mLayerCount);
        /*
         *  DispC and GXP can not handle part of layers.
         *  Should Disable accelerateByDPC and accelerateByGXP
         * */
         //hl changed 0417
       accelerateByDPC = false;
       accelerateByGXP = false;
       mGXPLayerCount = 0;
       mDispCLayerCount = 0;
    }

    if ((accelerateByDPC == false) && (accelerateByGXP == false))
    {
        accelerateByOVC = true;
    }

    if (accelerateByOVC)
    {
        revisitOVCLayers(DisplayFlag);
    }

#ifdef DYNAMIC_RELEASE_PLANEBUFFER
    int ret = -1;
    bool holdCond = false;

    if (accelerateByDPC || accelerateByGXP || accelerateByOVC)
    {
        holdCond = true;
    }

    ret = mPrimary->reclaimPlaneBuffer(holdCond);
    if (ret == 1)
    {
        mSkipLayerFlag = true;
        ALOGI_IF(mDebugFlag, "alloc plane buffer failed, goto FB");
    }
#endif

    int FinalSFLayerIndex = 0;
    if (!mSkipLayerFlag && OVCSkipLayerFlag)
    {
        for (size_t i = 0; i < mList.size(); i++)
        {
            SprdHWLayer *SprdLayer = mList[i];
            if (SprdLayer == NULL)
                continue;
            if (!IsHWCLayer(SprdLayer))
            {
                if(SprdLayer->getLayerIndex() > FinalSFLayerIndex)
                    FinalSFLayerIndex = SprdLayer->getLayerIndex();
            }
        }
    }

    for (size_t i = 0; i < mList.size(); i++)
    {
        SprdHWLayer *SprdLayer = mList[i];
        if (SprdLayer == NULL)
        {
            continue;
        }

        if (mSkipLayerFlag || (OVCSkipLayerFlag &&
			SprdLayer->getLayerIndex() <= FinalSFLayerIndex))
        {
            SprdLayer->setLayerType(LAYER_SURFACEFLINGER);
            if (SprdLayer->InitCheck())
            {
              mFBLayerCount++;
            }
            ALOGI_IF(mDebugFlag, "revisitGeometry Skip layer found, switch to SF, i: %d", (int)i);
        }

        setOverlayFlag(SprdLayer, i);

        if (SprdLayer->getCompositionChangedFlag())
        {
          CompositionTypeChangedLocal++;
        }
    }

    if (CompositionTypeChangedLocal > mCompositionChangedNum)
    {
      ALOGI_IF(mDebugFlag, "HWC status error, actually the Composition Changed Num: %d, final get num: %d",
             mCompositionChangedNum, CompositionTypeChangedLocal);
    }

    ALOGI_IF(mDebugFlag, "Total layer: %d, FB layer: %d, OSD layer: %d, video layer: %d, mCompositionChangedNum: %d",
            mLayerCount, mFBLayerCount, mOSDLayerCount, mVideoLayerCount, mCompositionChangedNum);

    return 0;
}

void SprdHWLayerList:: ClearFrameBuffer(SprdHWLayer *l, unsigned int index)
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

void SprdHWLayerList:: setOverlayFlag(SprdHWLayer *l, unsigned int index)
{
    if (l == NULL)
    {
        ALOGE("Input parameters SprdHWLayer is NULL");
        return;
    }

    switch (l->getLayerType())
    {
        case LAYER_OSD:
            l->setSprdLayerIndex(mOSDLayerCount);
            mOSDLayerCount++;
            /*
            * HWComposer::validateChange will report an error,
            * if composition type is changed from SolidColor to Device.
            **/
            if(l->getCompositionType() != COMPOSITION_SOLID_COLOR)
              forceOverlay(l, COMPOSITION_DEVICE);
            ClearFrameBuffer(l, index);
            break;
        case LAYER_OVERLAY:
            l->setSprdLayerIndex(mVideoLayerCount);
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

    l->setLayerIndex(l->getZOrder());
}

void SprdHWLayerList:: forceOverlay(SprdHWLayer *l, int32_t compositionType)
{
    if (l == NULL)
    {
      ALOGE("Input parameters SprdHWLayer is NULL");
      return;
    }

    ALOGI_IF(mDebugFlag, "SprdHWLayerList:: forceOverlay Layer orig composition type:%d, cur type: %d",
          l->getCompositionType(), compositionType);

    /* TODO: should check the composition type first */
    if ((l->getCompositionType() != compositionType) &&
        (l->getCompositionType() != COMPOSITION_INVALID))
    {
      mCompositionChangedNum++;
    }

    l->setCompositionType(compositionType);
}

/*
 *  resetOverlayFlag
 *  set hwc_layer_1_t::compositionType to HWC_FRAMEBUFFER,
 *  means the default composition should execute in SF.
 * */
void SprdHWLayerList:: resetOverlayFlag(SprdHWLayer *l)
{
    if (l == NULL)
    {
        ALOGI_IF(mDebugFlag, "SprdHWLayer is NULL");
        return;
    }

#if 0
    if (l->getCompositionType() != COMPOSITION_CLIENT)
    {
      mCompositionChangedNum++;
    }
#endif

    l->mCompositionType = COMPOSITION_CLIENT;

    int index = l->getSprdLayerIndex();

    if (index < 0)
    {
        return;
    }

    switch (l->getLayerType())
    {
        case LAYER_OSD:
            mOSDLayerCount--;
            break;
        case LAYER_OVERLAY:
            mVideoLayerCount--;
            break;
        default:
            return;
    }
}

/*
 *  prepareOSDLayer
 *  if it's rgb format,init SprdHWLayer obj from hwc_layer_1_t
 *  and check whether these accelerator can process or not.
 * */
int SprdHWLayerList:: prepareOSDLayer(SprdHWLayer *l)
{
    if (l == NULL)
    {
      ALOGE("SprdHWLayerList:: prepareOSDLayer intput SprdHWLayer is NULL");
      return -1;
    }

    native_handle_t *privateH = l->getBufferHandle();
    struct sprdRectF *srcRect = l->getSprdSRCRectF();
    struct sprdRect *FBRect  = l->getSprdFBRect();

    unsigned int mFBWidth  = mFBInfo->fb_width;
    unsigned int mFBHeight = mFBInfo->fb_height;

    float sourceLeft   = srcRect->left;
    float sourceTop    = srcRect->top;
    float sourceRight  = srcRect->right;
    float sourceBottom = srcRect->bottom;

    if (l->getCompositionType() == COMPOSITION_SOLID_COLOR) {
        ALOGI_IF(mDebugFlag, "prepareOSDLayer dim layer");
        l->setLayerType(LAYER_OSD);
        return 0;
    }

    if (privateH == NULL)
    {
        ALOGI_IF(mDebugFlag, "prepareOSDLayer layer handle is NULL");
        return -1;
    }

    /*
     *  if it's not rgb format,leave it to prepareVideoLayer().
     * */
    if (!(l->checkRGBLayerFormat()))
    {
        ALOGI_IF(mDebugFlag, "prepareOSDLayer NOT RGB format layer Line:%d", __LINE__);
        return 0;
    }

    if ((ADP_USAGE(privateH) & GRALLOC_USAGE_PROTECTED) == GRALLOC_USAGE_PROTECTED)
    {
        ALOGI_IF(mDebugFlag, "prepareOSDLayer do not process RGB DRM Line:%d", __LINE__);
        return 0;
    }

    if (ADP_USAGE(privateH) & GRALLOC_USAGE_HW_TILE_ALIGN)
    {
        ALOGI_IF(mDebugFlag, "prepareOSDLayer do not support Tile align layer Line:%d", __LINE__);
        return 0;
    }

    l->setLayerFormat(ADP_FORMAT(privateH));
    l->resetAccelerator();
    /*
     *  first we set SprdHWLayer::mAccelerator to overlay-GPU,
     *  then we will check DISPC&GSP capability.
     * */
    if (mAcceleratorMode & ACCELERATOR_OVERLAYCOMPOSER)
    {
        l->setLayerAccelerator(ACCELERATOR_OVERLAYCOMPOSER);
    }

#if 0
    srcRect->x = MAX(sourceLeft, 0);
    srcRect->y = MAX(sourceTop, 0);
    srcRect->w = MIN(sourceRight - sourceLeft, ADP_WIDTH(privateH));
    srcRect->h = MIN(sourceBottom - sourceTop, ADP_HEIGHT(privateH));

    FBRect->x = MAX(l->getSprdFBRect().left, 0);
    FBRect->y = MAX(l->getSprdFBRect().top, 0);
    FBRect->w = MIN(l->getSprdFBRect().right  - l->getSprdFBRect().left, mFBWidth);
    FBRect->h = MIN(l->getSprdFBRect().bottom - l->dgetSprdFBRect().top, mFBHeight);
#endif

    ALOGI_IF(mDebugFlag, "displayFrame[l%d,t%d,r%d,b%d] mFBWidth:%d mFBHeight:%d",
        l->getSprdFBRect()->left, l->getSprdFBRect()->top,
        l->getSprdFBRect()->right,l->getSprdFBRect()->bottom,
        mFBWidth, mFBHeight);

    /*
     *  if DISPC can't accelerate , check GPU limit.
     * */
    if (l->getAccelerator() == ACCELERATOR_OVERLAYCOMPOSER)
    {
        int ret = prepareOVCLayer(l);
        if (ret != 0)
        {
            ALOGI_IF(mDebugFlag, "prepareOverlayComposerLayer find irregular layer, give up OverlayComposerGPU,ret 0, L%d", __LINE__);
            l->resetAccelerator();
            return 0;
        }
    }

    /*
     *  if it's rgb format and can be accelerated by DISPC/GSP/GPU,
     *  set it to LAYER_OSD,means it can be process in hwc and is rgb.
     * */
    l->setLayerType(LAYER_OSD);
    ALOGI_IF(mDebugFlag, "prepareOSDLayer[L%d],set type OSD, accelerator: 0x%x",
              __LINE__, l->getAccelerator());

    mFBLayerCount--;

    return 0;
}

/*
 *  prepareVideoLayer
 *  if it's yuv format,init SprdHWLayer obj from hwc_layer_1_t
 *  and check whether these accelerator can process or not.
 * */
int SprdHWLayerList:: prepareVideoLayer(SprdHWLayer *l)
{
    if (l == NULL)
    {
      ALOGE("SprdHWLayerList:: prepareVideoLayer intput SprdHWLayer is NULL");
      return -1;
    }

    native_handle_t *privateH = l->getBufferHandle();
    struct sprdRectF *srcRect = l->getSprdSRCRectF();
    struct sprdRect *FBRect  = l->getSprdFBRect();

    unsigned int mFBWidth  = mFBInfo->fb_width;
    unsigned int mFBHeight = mFBInfo->fb_height;

    float sourceLeft   = srcRect->left;
    float sourceTop    = srcRect->top;
    float sourceRight  = srcRect->right;
    float sourceBottom = srcRect->bottom;

    if (privateH == NULL)
    {
        ALOGI_IF(mDebugFlag, "prepareVideoLayer layer handle is NULL");
        return -1;
    }

    if ((ADP_USAGE(privateH) & GRALLOC_USAGE_PROTECTED) == GRALLOC_USAGE_PROTECTED)
    {
        l->setProtectedFlag(true);
        mGlobalProtectedFlag = true;
        ALOGI_IF(mDebugFlag, "prepareVideoLayer L: %d, find protected video",
                 __LINE__);
    }
    else
    {
        l->setProtectedFlag(false);
    }

    /*
     *  Some RGB DRM video should also be considered as video layer
     *  which must be processed by HWC.
     * */
    if ((!(l->checkYUVLayerFormat()))
        && (l->getProtectedFlag() == false))
    {
        ALOGI_IF(mDebugFlag, "prepareVideoLayer L%d,color format:0x%08x,ret 0", __LINE__, ADP_FORMAT(privateH));
        return 0;
    }

    l->setLayerFormat(ADP_FORMAT(privateH));

    mYUVLayerCount++;

    l->resetAccelerator();

    if (mAcceleratorMode & ACCELERATOR_OVERLAYCOMPOSER)
    {
        l->setLayerAccelerator(ACCELERATOR_OVERLAYCOMPOSER);
    }

#if 0
    srcRect->x = MAX(sourceLeft, 0);
    srcRect->y = MAX(sourceTop, 0);
    srcRect->w = MIN(sourceRight - sourceLeft, ADP_WIDTH(privateH));
    srcRect->h = MIN(sourceBottom - sourceTop, ADP_HEIGHT(privateH));

    FBRect->x = MAX(l->getSprdFBRect().left, 0);
    FBRect->y = MAX(l->getSprdFBRect().top, 0);
    FBRect->w = MIN(l->getSprdFBRect().right  - l->getSprdFBRect().left, mFBWidth);
    FBRect->h = MIN(l->getSprdFBRect().bottom - l->getSprdFBRect().top, mFBHeight);
#endif

#ifdef TRANSFORM_USE_DCAM
    int ret = DCAMTransformPrepare(layer, srcRect, FBRect);
    if (ret != 0)
    {
        return 0;
    }
#endif

    ALOGV("rects {%f,%f,%f,%f}, {%d,%d,%d,%d}", srcRect->x, srcRect->y, srcRect->w, srcRect->h,
          FBRect->x, FBRect->y, FBRect->w, FBRect->h);

if (mAcceleratorMode & ACCELERATOR_OVERLAYCOMPOSER)
    {
        l->setLayerAccelerator(ACCELERATOR_OVERLAYCOMPOSER);
        ALOGI_IF(mDebugFlag, "prepareOverlayLayer L%d, Use OVC to accelerate", __LINE__);
    }
    else if (mAcceleratorMode & ACCELERATOR_NON)
    {
        if(l->getTransform() == HAL_TRANSFORM_FLIP_V)
        {
           ALOGI_IF(mDebugFlag, "prepareVideoLayer L%d,transform:0x%08x,ret 0", __LINE__, l->getTransform());
            l->resetAccelerator();
            return 0;
        }

        if((l->getTransform() == (HAL_TRANSFORM_ROT_90 | HAL_TRANSFORM_FLIP_H))
            || (l->getTransform() == (HAL_TRANSFORM_ROT_90 | HAL_TRANSFORM_FLIP_V)))
        {
            ALOGI_IF(mDebugFlag, "prepareVideoLayer L%d,transform:0x%08x,ret 0", __LINE__, l->getTransform());
            l->resetAccelerator();
            return 0;
        }
    }

    /*
     *  if it's yuv format and can be accelerated by DISPC/GSP/GPU,
     *  set it to LAYER_OSD,means it can be process in hwc and is yuv.
     * */
    l->setLayerType(LAYER_OVERLAY);
    ALOGI_IF(mDebugFlag, "prepareVideoLayer[L%d],set type Video, accelerator: 0x%x",
             __LINE__, l->getAccelerator());

    mFBLayerCount--;

    return 0;
}

int SprdHWLayerList::prepareForDispC(SprdHWLayer *l)
{
    uint32_t srcWidth;
    uint32_t srcHeight;
    uint32_t destWidth;
    uint32_t destHeight;

    if (l == NULL)
    {
        ALOGI_IF(mDebugFlag, "prepareForDispC input SprdHWLayer is NULL L:%d", __LINE__);
        return -1;
    }

    native_handle_t *privateH = l->getBufferHandle();
    if (privateH == NULL)
    {
        ALOGI_IF(mDebugFlag, "prepareForDispC input handle is NULL L:%d", __LINE__);
        return -1;
    }

    return 0;
}

/*
 *  prepareOverlayComposerLayer
 *  check GPU overlay limited.
 * */
int SprdHWLayerList::prepareOVCLayer(SprdHWLayer *l)
{
#ifndef OVERLAY_COMPOSER_GPU
        mSkipLayerFlag = true;
        return -1;
#endif
    if (l == NULL)
    {
        ALOGE("prepareOverlayComposerLayer input layer is NULL");
        return -1;
    }

    float sourceLeft   = l->getSprdSRCRectF()->left;
    float sourceTop    = l->getSprdSRCRectF()->top;
    float sourceRight  = l->getSprdSRCRectF()->right;
    float sourceBottom = l->getSprdSRCRectF()->bottom;

    if ((l->getSprdFBRect()->right - l->getSprdFBRect()->left >
         (unsigned int)(mFBInfo->fb_width)) ||
        (l->getSprdFBRect()->bottom - l->getSprdFBRect()->top >
         (unsigned int)(mFBInfo->fb_height)))
    {
        mSkipLayerFlag = true;
        return -1;
    }

    if (sourceLeft < 0 ||
        sourceTop < 0 ||
        sourceBottom < 0 ||
        sourceRight < 0)
    {
        mSkipLayerFlag = true;
        return -1;
    }

    return 0;
}

int SprdHWLayerList:: revisitOVCLayers(int& DisplayFlag)
{
    int LayerCount = mLayerCount;
    int displayType = HWC_DISPLAY_MASK;

#ifndef OVERLAY_COMPOSER_GPU
    mSkipLayerFlag = true;
#endif

    /*
     *  At present, OverlayComposer cannot handle 2 or more than 2 YUV layers.
     *  And OverlayComposer do not handle cropped RGB layer except DRM video.
     *  DRM video must go into Overlay.
     * */
        if (mYUVLayerCount > 0)
        {
            if (mGlobalProtectedFlag)
            {
                ALOGI_IF(mDebugFlag, "Find Protected Video layer, force Overlay");
                if(mSkipLayerFlag)
                {
                    int FinalSFLayerIndex = 0;
                    int FirstDRMLayerIndex = 100;
                    for (size_t i = 0; i < mList.size(); i++)
                    {
                        SprdHWLayer *SprdLayer = mList[i];
                        if (SprdLayer == NULL)
                            continue;
                        if (!IsHWCLayer(SprdLayer))
                        {
                            if(SprdLayer->getLayerIndex() > FinalSFLayerIndex)
                                FinalSFLayerIndex = SprdLayer->getLayerIndex();
                        }
                        native_handle_t *privateH = SprdLayer->getBufferHandle();
                        if (privateH == NULL)
                            continue;
                        if (((ADP_USAGE(privateH) & GRALLOC_USAGE_PROTECTED) == GRALLOC_USAGE_PROTECTED))
                        {
                            if(SprdLayer->getLayerIndex() < FirstDRMLayerIndex)
                                FirstDRMLayerIndex = SprdLayer->getLayerIndex();
                        }
                     }
                     if (FinalSFLayerIndex < FirstDRMLayerIndex)
                         mSkipLayerFlag = false;
                }
            }
            else
            {
                mSkipLayerFlag = true;
                ALOGI_IF(mDebugFlag, "Not find protected video, mSkipLayerFlag is true");
            }
#ifdef FORCE_OVC
        mSkipLayerFlag = false;
#endif
        }
        else
        {
            ALOGI_IF(mDebugFlag, "Not find protected video, switch to SF");
            mSkipLayerFlag = true;
        }

    if (!mSkipLayerFlag) {
        for (uint32_t j = 0; j < mOVCLayerCount; j++)
        {
            SprdHWLayer *SprdLayer = mOVCLayerList[j];
            if (SprdLayer == NULL)
            {
                continue;
            }

            if (!IsHWCLayer(SprdLayer))
            {
                continue;
            }

            int format = SprdLayer->getLayerFormat();
            if (SprdLayer->checkRGBLayerFormat())
            {
                SprdLayer->setLayerType(LAYER_OSD);
            }
            else if (SprdLayer->checkYUVLayerFormat())
            {
                SprdLayer->setLayerType(LAYER_OVERLAY);
            }
            ALOGI_IF(mDebugFlag, "Force layer format:%d go into OVC", format);
        }
        displayType |= HWC_DISPLAY_OVERLAY_COMPOSER_GPU;
    }


     /*
      *  When Skip layer is found, SurfaceFlinger maybe want to do the Animation,
      *  or other thing, here just disable OverlayComposer.
      *  Switch back to SurfaceFlinger for composition.
      *  At present, it is just a workaround method.
      * */
     if (mSkipLayerFlag)
     {
         displayType &= HWC_DISPLAY_MASK;
         displayType |= HWC_DISPLAY_FRAMEBUFFER_TARGET;
     }

     DisplayFlag |= displayType;

     return 0;
}

#ifdef TRANSFORM_USE_DCAM
int SprdHWLayerList:: DCAMTransformPrepare(SprdHWLayer *l, struct sprdRectF *srcRect, struct sprdRect *FBRect)
{
    if (l == NULL)
    {
      ALOGE("DCAMTransformPrepare input SprdHWLayer is NULL");
      return -1;
    }

    native_handle_t *privateH = l->getBufferHandle();
    struct sprdYUV srcImg;
    unsigned int mFBWidth  = mFBInfo->fb_width;
    unsigned int mFBHeight = mFBInfo->fb_height;

    srcImg.format = ADP_FORMAT(privateH);
    srcImg.w = ADP_WIDTH(privateH);
    srcImg.h = ADP_HEIGHT(privateH);


    int rot_90_270 = (l->getTransform() & HAL_TRANSFORM_ROT_90) == HAL_TRANSFORM_ROT_90;

    srcRect->x = MAX(l->getSprdSRCRectF().left, 0);
    srcRect->x = (srcRect->x + SRCRECT_X_ALLIGNED) & (~SRCRECT_X_ALLIGNED);//dcam 8 pixel crop
    srcRect->x = MIN(srcRect->x, srcImg.w);
    srcRect->y = MAX(l->getSprdSRCRectF().top, 0);
    srcRect->y = (srcRect->y + SRCRECT_Y_ALLIGNED) & (~SRCRECT_Y_ALLIGNED);//dcam 8 pixel crop
    srcRect->y = MIN(srcRect->y, srcImg.h);

    srcRect->w = MIN(l->getSprdSRCRectF().right  - l->getSprdSRCRectF().left, srcImg.w - srcRect->x);
    srcRect->h = MIN(l->getSprdSRCRectF().bottom - l->getSprdSRCRectF().top, srcImg.h - srcRect->y);

    if((srcRect->w - (srcRect->w & (~SRCRECT_WIDTH_ALLIGNED)))> ((SRCRECT_WIDTH_ALLIGNED+1)>>1))
    {
        srcRect->w = (srcRect->w + SRCRECT_WIDTH_ALLIGNED) & (~SRCRECT_WIDTH_ALLIGNED);//dcam 8 pixel crop
    } else
    {
        srcRect->w = (srcRect->w) & (~SRCRECT_WIDTH_ALLIGNED);//dcam 8 pixel crop
    }

    if((srcRect->h - (srcRect->h & (~SRCRECT_HEIGHT_ALLIGNED)))> ((SRCRECT_HEIGHT_ALLIGNED+1)>>1))
    {
        srcRect->h = (srcRect->h + SRCRECT_HEIGHT_ALLIGNED) & (~SRCRECT_HEIGHT_ALLIGNED);//dcam 8 pixel crop
    }
    else
    {
        srcRect->h = (srcRect->h) & (~SRCRECT_HEIGHT_ALLIGNED);//dcam 8 pixel crop
    }

    srcRect->w = MIN(srcRect->w, srcImg.w - srcRect->x);
    srcRect->h = MIN(srcRect->h, srcImg.h - srcRect->y);
    //--------------------------------------------------
    FBRect->x = MAX(l->getSprdFBRect().left, 0);
    FBRect->y = MAX(l->getSprdFBRect().top, 0);
    FBRect->x = MIN(FBRect->x, mFBWidth);
    FBRect->y = MIN(FBRect->y, mFBHeight);

    FBRect->w = MIN(l->getSprdFBRect().right - l->getSprdFBRect().left, mFBWidth - FBRect->x);
    FBRect->h = MIN(l->getSprdFBRect().bottom - l->getSprdFBRect().top, mFBHeight - FBRect->y);
    if((FBRect->w - (FBRect->w & (~FB_WIDTH_ALLIGNED)))> ((FB_WIDTH_ALLIGNED+1)>>1))
    {
        FBRect->w = (FBRect->w + FB_WIDTH_ALLIGNED) & (~FB_WIDTH_ALLIGNED);//dcam 8 pixel and lcdc must 4 pixel for yuv420
    }
    else
    {
        FBRect->w = (FBRect->w) & (~FB_WIDTH_ALLIGNED);//dcam 8 pixel and lcdc must 4 pixel for yuv420
    }

    if((FBRect->h - (FBRect->h & (~FB_HEIGHT_ALLIGNED)))> ((FB_HEIGHT_ALLIGNED+1)>>1))
    {
        FBRect->h = (FBRect->h + FB_HEIGHT_ALLIGNED) & (~FB_HEIGHT_ALLIGNED);//dcam 8 pixel and lcdc must 4 pixel for yuv420
    }
    else
    {
        FBRect->h = (FBRect->h) & (~FB_HEIGHT_ALLIGNED);//dcam 8 pixel and lcdc must 4 pixel for yuv420
    }


    FBRect->w = MIN(FBRect->w, mFBWidth - ((FBRect->x + FB_WIDTH_ALLIGNED) & (~FB_WIDTH_ALLIGNED)));
    FBRect->h = MIN(FBRect->h, mFBHeight - ((FBRect->y + FB_HEIGHT_ALLIGNED) & (~FB_HEIGHT_ALLIGNED)));

    if(srcRect->w < 4 || srcRect->h < 4 ||
       FBRect->w < 4 || FBRect->h < 4 ||
       FBRect->w > 960 || FBRect->h > 960)
    { //dcam scaling > 960 should use slice mode
        ALOGI_IF(mDebugFlag,"prepareVideoLayer, dcam scaling > 960 should use slice mode! L%d",__LINE__);
        return -1;
    }

    if(4 * srcWidth < destWidth || srcWidth > 4 * destWidth ||
       4 * srcHeight < destHeight || srcHeight > 4 * destHeight)
    { //dcam support 1/4-4 scaling
        ALOGI_IF(mDebugFlag,"prepareVideoLayer, dcam support 1/4-4 scaling! L%d",__LINE__);
        return -1;
    }

    return 0;
}
#endif
