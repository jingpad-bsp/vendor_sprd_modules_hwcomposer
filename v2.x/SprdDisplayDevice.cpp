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
 ** File: SprdDisplayDevice.cpp         DESCRIPTION                           *
 **                                   Define some display device structures.  *
 ******************************************************************************
 ******************************************************************************
 ** Author:         zhongjun.chen@spreadtrum.com                              *
 *****************************************************************************/

#include "SprdDisplayDevice.h"
#include "SprdUtil.h"

#include "dump.h"


SprdDisplayClient::SprdDisplayClient(int32_t displayId, int32_t displayType)
  : mInitFlag(false),
    mDisplayId(displayId),
    mDisplayType(displayType),
    mDisplayAttributes(NULL),
    mData(NULL),
    mFBTargetLayer(NULL),
    mOutputLayer(NULL),
    mReleaseFence(-1),
#ifdef ENABLE_PENDING_RELEASE_FENCE_FEATURE
    mPreReleaseFence(-1),
#endif
    mReleaseFence2(-1),
    mColorTransformHit(0)
{
#ifdef ENABLE_PENDING_RELEASE_FENCE_FEATURE
  for (int i = 0; i < RETIRED_THRESHOLD; i++)
  {
    mReleaseFences[i] = -1;
  }
#endif

}

SprdDisplayClient::~SprdDisplayClient()
{
  if (mFBTargetLayer)
  {
    delete mFBTargetLayer;
    mFBTargetLayer = NULL;
  }

  if (mDisplayAttributes)
  {
    free(mDisplayAttributes);
    mDisplayAttributes = NULL;
  }
}

bool SprdDisplayClient::Init(const char *displayName)
{
  uint32_t size = 0;

  size = strlen(mDisplayName) + 1;

  size = (size > (strlen(displayName) + 1)) ?
         (strlen(displayName) + 1) : size;

  strncpy(mDisplayName, displayName, size);

  mDisplayAttributes = (DisplayAttributes *)malloc(sizeof(DisplayAttributes));
  if (mDisplayAttributes == NULL)
  {
    ALOGE("DisplayClient::Init name:%s malloc DisplayAttributes failed", displayName);
    mInitFlag = false;
    return false;
  }

  DisplayAttributes *dpyAttr = mDisplayAttributes;
  dpyAttr->configsIndex = 0;
  dpyAttr->connected = false;
  dpyAttr->AcceleratorMode = ACCELERATOR_NON;

  for (int j = 0; j < 5/* MAX_DISPLAYS */; j++)
  {
      dpyAttr->sets[j].fillFlag = false;
  }

  mInitFlag = true;

  return true;
}

int32_t /*hwc2_error_t*/SprdDisplayClient::GET_CLIENT_TARGET_SUPPORT(
           uint32_t width, uint32_t height, int32_t /*android_pixel_format_t*/ format,
           int32_t /*android_dataspace_t*/ dataspace)
{
  int32_t err = ERR_NONE;
  /* TODO: Implement this function lator, SurfaceFlinger seems do not use it */

  HWC_IGNORE(width);
  HWC_IGNORE(height);
  HWC_IGNORE(format);
  HWC_IGNORE(dataspace);
  return err;
}

int32_t /*hwc2_error_t*/SprdDisplayClient::GET_COLOR_MODES(
           uint32_t* outNumModes,
           int32_t* /*android_color_mode_t*/ outModes)
{
  HWC_IGNORE(outNumModes);
  HWC_IGNORE(outModes);
  *outNumModes = 1;
   if (outModes)
   {
     outModes[0] = HAL_COLOR_MODE_NATIVE;
   }
  return ERR_NONE;
}

int32_t /*hwc2_error_t*/SprdDisplayClient::GET_DISPLAY_NAME(
           uint32_t* outSize,
           char* outName)
{
  uint32_t size = 0;

  size = strlen(mDisplayName) + 1;
  if (outSize)
  {
    *outSize = size;
  }

  if (outName)
  {
    size = (*outSize < size) ? *outSize : size;
    strncpy(outName, mDisplayName, size);
  }

   return ERR_NONE;
}

int32_t /*hwc2_error_t*/SprdDisplayClient::GET_DISPLAY_TYPE(
           int32_t* /*hwc2_display_type_t*/ outType)
{
  if (outType == NULL)
  {
    ALOGE("DisplayClient::GET_DISPLAY_TYPE outType is NULL");
    return ERR_BAD_PARAMETER;
  }

  *outType = mDisplayType;

  return ERR_NONE;
}

int32_t /*hwc2_error_t*/SprdDisplayClient::GET_DOZE_SUPPORT(
           int32_t* outSupport)
{
  if (outSupport == NULL)
  {
    ALOGE("DisplayClient::GET_DOZE_SUPPORT outSupport is NULL");
    return ERR_BAD_PARAMETER;
  }

  *outSupport = 0;

  return ERR_NONE;
}

int32_t /*hwc2_error_t*/SprdDisplayClient::GET_HDR_CAPABILITIES(
           uint32_t* outNumTypes,
           int32_t* /*android_hdr_t*/ outTypes, float* outMaxLuminance,
           float* outMaxAverageLuminance, float* outMinLuminance)
{
  if (outNumTypes == NULL)
  {
    ALOGE("DisplayClient::GET_HDR_CAPABILITIES outNumTypes is NULL");
    return ERR_BAD_PARAMETER;
  }

  *outNumTypes = 0;

  if (outTypes)
  {
    *outTypes = 0;
  }

  if (outMaxLuminance)
  {
    *outMaxLuminance = 0.0;
  }

  if (outMaxAverageLuminance)
  {
    *outMaxAverageLuminance = 0.0;;
  }

  if (outMinLuminance)
  {
    *outMinLuminance = 0.0;
  }

  return ERR_NONE;
}

int32_t /*hwc2_error_t*/SprdDisplayClient::SET_CLIENT_TARGET(
           buffer_handle_t target,
           int32_t acquireFence, int32_t /*android_dataspace_t*/ dataspace,
           hwc_region_t damage)
{
  native_handle_t *buf = (native_handle_t *)target;
  if (mFBTargetLayer)
  {
    delete mFBTargetLayer;
    mFBTargetLayer = NULL;
  }

  if (buf == NULL)
  {
    ALOGE("DisplayClient::SET_CLIENT_TARGET target is NULL");
    return ERR_NONE;
  }

  WrapFBTargetLayer(buf, acquireFence, dataspace, damage);

  return ERR_NONE; 
}

int32_t /*hwc2_error_t*/SprdDisplayClient::SET_COLOR_MODE(
           int32_t /*android_color_mode_t*/ mode)
{
  HWC_IGNORE(mode);
  if (mode == -1)
  {
    return ERR_BAD_PARAMETER;
  }
  return ERR_NONE;
}

int32_t /*hwc2_error_t*/SprdDisplayClient::SET_COLOR_TRANSFORM(
           const float* matrix,
           int32_t /*android_color_transform_t*/ hint)
{
  if ((matrix[0] == 1.0) && (matrix[1] == 0.0) && (matrix[2] == 0.0) &&
    (matrix[3] == 0.0) && (matrix[4] == 0.0) && (matrix[5] == 1.0) &&
    (matrix[6] == 0.0) && (matrix[7] == 0.0) && (matrix[8] == 0.0) &&
    (matrix[9] == 0.0) && (matrix[10] == 1.0) && (matrix[11] == 0.0) &&
    (matrix[12] == 0.0) && (matrix[13] == 0.0) && (matrix[14] == 0.0) &&
    (matrix[15] == 1.0)) {
    return ERR_NONE;
  } else {
    return ERR_BAD_PARAMETER;
  }

#if 0
  uint32_t size = 0;

  if (matrix)
  {
    ALOGE("SprdDisplayClient::SET_COLOR_TRANSFORM matrix is NULL");
    return ERR_BAD_PARAMETER;
  }

  size = (sizeof(matrix) > COLORMATRIX_NUM) ? COLORMATRIX_NUM : sizeof(matrix);
  memset(mColorMatrix, 0x00, COLORMATRIX_NUM);
  memcpy(mColorMatrix, matrix, size);
  mColorTransformHit = hint;

  return ERR_NONE;
#endif
}

int32_t /*hwc2_error_t*/ SprdDisplayClient::SET_OUTPUT_BUFFER(
           buffer_handle_t buffer,
           int32_t releaseFence)
{
  struct sprdRectF *src = NULL;
  struct sprdRect *fb = NULL;
  native_handle_t *privateH = (native_handle_t *)buffer;

  if (privateH == NULL)
  {
    ALOGE("SprdDisplayClient::SET_OUTPUT_BUFFER input buffer is NULL");
    return ERR_BAD_PARAMETER;
  }

  if (mOutputLayer)
  {
    delete mOutputLayer;
    mOutputLayer = NULL;
  }

  mOutputLayer = new SprdHWLayer(privateH, ADP_FORMAT(privateH), 1.0, BLEND_MODE_NONE,
                                 0, releaseFence, 0);
  if (mOutputLayer == NULL)
  {
    ALOGE("SprdDisplayClient::SET_OUTPUT_BUFFER new mOutputLayer failed");
    return ERR_NO_RESOURCES;
  }

  src = mOutputLayer->getSprdSRCRectF();
  fb  = mOutputLayer->getSprdFBRect();

  src->x = 0;
  src->y = 0;
  src->w = ADP_WIDTH(privateH);
  src->h = ADP_HEIGHT(privateH);
  src->right  = src->x + src->w;
  src->bottom = src->y + src->h;

  fb->x = 0;
  fb->y = 0;
  fb->w = ADP_WIDTH(privateH);
  fb->h = ADP_HEIGHT(privateH);
  fb->right  = fb->x + fb->w;
  fb->bottom = fb->y + fb->h;

  return ERR_NONE;
}

int SprdDisplayClient::WrapFBTargetLayer(
    native_handle_t *buf, int32_t acquireFence,
    int32_t dataspace, hwc_region_t damage) {
  struct sprdRectF *src = NULL;
  struct sprdRect *fb = NULL;

  if (buf == NULL) {
    ALOGE("WrapFBTargetLayer FBT handle is NULL");
    return -1;
  }

  /* TODO: 0 z order may has a risk if FBT layer take participate in the
     second composition
  */
  mFBTargetLayer = new SprdHWLayer(buf, ADP_FORMAT(buf), acquireFence,
                                  dataspace, damage, 0);
  if (mFBTargetLayer == NULL) {
    ALOGE("WrapFBTargetLayer new mFBTargetLayer failed");
    return -1;
  }

  src = mFBTargetLayer->getSprdSRCRectF();
  fb  = mFBTargetLayer->getSprdFBRect();

  if (true/*damage.numRects > 0*/)
  {
    src->x = 0;
    src->y = 0;
    src->w = ADP_WIDTH(buf);
    src->h = ADP_HEIGHT(buf);
    src->right  = src->x + src->w;
    src->bottom = src->y + src->h;

    fb->x = 0;
    fb->y = 0;
    fb->w = ADP_WIDTH(buf);
    fb->h = ADP_HEIGHT(buf);
    fb->right  = fb->x + fb->w;
    fb->bottom = fb->y + fb->h;
  }
  else
  {
    /*  TODO: */
  }

  return 0;
}

int SprdDisplayClient::processRetiredFence(int fd)
{
  return fd;
#if 0
  static unsigned int count = 0;
  int index = 0;
  int index2 = 0;
  int ret = -1;

  index = count % RETIRED_THRESHOLD;

  if (index >= RETIRED_THRESHOLD)
  {
    index = RETIRED_THRESHOLD - 1;
  }

  mRetiredFences[index] = fd;

  if (count < RETIRED_THRESHOLD)
  {
    ret = -1;
  }
  else
  {
    index2 = (index + RETIRED_THRESHOLD - 1) % RETIRED_THRESHOLD;
    ret = mRetiredFences[index2];
  }

  count++;

  // ALOGD("processRetiredFence fd: %d, i:%d, i2:%d", ret, index, index2);

  return ret;
#endif
}

#ifdef ENABLE_PENDING_RELEASE_FENCE_FEATURE
int SprdDisplayClient::processReleaseFence(int fd)
{
  static unsigned int release_count = 0;
  int index = 0;
  int index2 = 0;
  int ret = -1;

  index = release_count % RETIRED_THRESHOLD;
  //ALOGE("processReleaseFence infd = %d;index = %d;release_count = %d;",fd, index, release_count);
  mReleaseFences[index] = fd;

  if (release_count == 0)
  {
    ret = -1;
  }
  else
  {
    index2 = (index + RETIRED_THRESHOLD - 1) % RETIRED_THRESHOLD;
    ret = mReleaseFences[index2];
  }

  if (release_count > RETIRED_THRESHOLD && index == 0)
  {
	  release_count = RETIRED_THRESHOLD;
  }
  release_count++;

  //ALOGE("processReleaseFence outfd = %d;index2 = %d;release_count = %d;",ret, index2, release_count);

  return ret;
}
#endif
