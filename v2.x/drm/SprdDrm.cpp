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

/*
 *  SprdDrm:: surpport drm.
 *  It pass display data from HWC to drm
*/

#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <signal.h>
#include <utils/Vector.h>
#include <utils/String8.h>
#include "AndroidFence.h"
#include "SprdDrm.h"
#include "dump.h"
#include <utils/Timers.h>
#include <drm/drm_fourcc.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include "drmresources.h"
#include <poll.h>
#include <pthread.h>
#include <sys/resource.h>
#include <drm/drm_mode.h>
#include <ui/GraphicBufferAllocator.h>
#ifdef SPRD_CABC
#include <vendor/sprd/hardware/enhance/1.0/IEnhance.h>
#include <vendor/sprd/hardware/enhance/1.0/types.h>
using android::sp;
using ::android::hidl::base::V1_0::IBase;
using ::vendor::sprd::hardware::enhance::V1_0::IEnhance;
using ::vendor::sprd::hardware::enhance::V1_0::Type;

void  enhance_vsync_update(void) {
        sp<IEnhance>  enhance_srv = IEnhance::getService();
        /* vsync update to enhance */
        enhance_srv->setMode(Type::CABC, CABC_VSYNC);
}

void enhance_flip_update(void) {
        sp<IEnhance>  enhance_srv = IEnhance::getService();
        /* vsync update to enhance */
        enhance_srv->setMode(Type::CABC, CABC_FLIP);
}

#endif
uint32_t ConvertHalFormatToDrm(uint32_t hal_format) {
  switch (hal_format) {
  case HAL_PIXEL_FORMAT_RGB_888:
    return DRM_FORMAT_BGR888;
  case HAL_PIXEL_FORMAT_BGRA_8888:
    return DRM_FORMAT_ARGB8888;
  case HAL_PIXEL_FORMAT_RGBX_8888:
    return DRM_FORMAT_XBGR8888;
  case HAL_PIXEL_FORMAT_RGBA_8888:
    return DRM_FORMAT_ABGR8888;
  case HAL_PIXEL_FORMAT_RGB_565:
    return DRM_FORMAT_RGB565;
  case HAL_PIXEL_FORMAT_YV12:
    return DRM_FORMAT_YVU420;
  case HAL_PIXEL_FORMAT_YCbCr_420_SP:
    return DRM_FORMAT_NV12;
  case HAL_PIXEL_FORMAT_YCrCb_420_SP:
  case HAL_PIXEL_FORMAT_YCbCr_420_888:
  case HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED:
    return DRM_FORMAT_NV21;
  default:
    ALOGE("Cannot convert hal format to drm format %u", hal_format);
    return DRM_FORMAT_ABGR8888;
  }
}

uint64_t ConvertRotationToDrm(int32_t angle) {
  uint64_t rot;

  switch (angle) {
  case 0:
    rot = DRM_MODE_ROTATE_0;
    break;
  case HAL_TRANSFORM_FLIP_H: // 1
    rot = DRM_MODE_REFLECT_Y;
    break;
  case HAL_TRANSFORM_FLIP_V: // 2
    rot = DRM_MODE_REFLECT_X;
    break;
  case HAL_TRANSFORM_ROT_180: // 3
    rot = DRM_MODE_ROTATE_180;
    break;
  case HAL_TRANSFORM_ROT_90: // 4
    rot = DRM_MODE_ROTATE_270;
    break;
  case (HAL_TRANSFORM_ROT_90 | HAL_TRANSFORM_FLIP_H): // 5
    rot = DRM_MODE_ROTATE_90 | DRM_MODE_REFLECT_X;
    break;
  case (HAL_TRANSFORM_ROT_90 | HAL_TRANSFORM_FLIP_V): // 6
    rot = DRM_MODE_ROTATE_90 | DRM_MODE_REFLECT_Y;
    break;
  case HAL_TRANSFORM_ROT_270: // 7
    rot = DRM_MODE_ROTATE_90;
    break;
  default:
    rot = DRM_MODE_ROTATE_0;
    ALOGE("ConvertRotationToDrm, unsupport angle=%d.", angle);
    break;
  }

  return rot;
}

uint64_t ConvertCSCMatrixToSprd(int32_t y2r_mod) {
  int32_t y2r_coef;
  switch (y2r_mod) {
  case USC_YUV_NO_INFO:
    y2r_coef = 3;
    break;
  case USC_YUV_BT601_NARROW:
    y2r_coef = 1;
    break;
  case USC_YUV_BT601_WIDE:
    y2r_coef = 0;
    break;
  case USC_YUV_BT709_NARROW:
    y2r_coef = 3;
    break;
  case USC_YUV_BT709_WIDE:
    y2r_coef = 2;
    break;
  case USC_YUV_BT2020_NARROW:
    y2r_coef = 5;
    break;
  case USC_YUV_BT2020_WIDE:
    y2r_coef = 4;
    break;
  default:
    y2r_coef = 0;
    ALOGE("ConvertCSCMatrixToSprd, unsupport yuv csc matrix=%d.", y2r_mod);
    break;
  }

  return y2r_coef;
}

SprdDrm::SprdDrm()
    : mNumInterfaces(0), mDebugFlag(0), event_thread(0), mLastLayerCount(0),
      bo_(NULL), vsync_enabled(false), mBufHandle(NULL) {
  memset(mFlushContext, 0x00, sizeof(FlushContext) * DEFAULT_DISPLAY_TYPE_NUM);
}

SprdDrm::~SprdDrm() {
  mNumInterfaces = 0;

  deInit();
}

bool SprdDrm::Init() {
  if (SprdDisplayCore::Init() == false) {
    ALOGE("SprdDrm:: Init SprdDisplayCore::Init failed");
    return false;
  }

  int ret = drm_.Init();
  if (ret) {
    ALOGE("Can't initialize drm object %d", ret);
    return false;
  }

  crtc_ = drm_.GetCrtcForDisplay(static_cast<int>(HWC_DISPLAY_PRIMARY));
  if (!crtc_) {
    ALOGE("Failed to get crtc for display");
    return false;
  }

  connector_ =
      drm_.GetConnectorForDisplay(static_cast<int>(HWC_DISPLAY_PRIMARY));
  if (!connector_) {
    ALOGE("Failed to get connector for display");
    return false;
  }

  size_t num_configs;
  int err =
      GetConfigs(static_cast<int>(HWC_DISPLAY_PRIMARY), NULL, &num_configs);
  if (err != 0 || !num_configs) {
    ALOGE("Failed to first GetConfigs for display, err:%d, num_configs:%zu",
          err, num_configs);
    return false;
  }

  pthread_attr_t attrs;
  pthread_attr_init(&attrs);
  pthread_attr_setdetachstate(&attrs, PTHREAD_CREATE_JOINABLE);
  ret = pthread_create(&event_thread, &attrs, &EventHandler, this);
  if (ret) {
    ALOGE("Failed to create event thread:%s", strerror(ret));
    return false;
  }

  // Grab the first mode, we'll choose this as the active mode
  // TODO: Should choose the preferred mode here
  hwc2_config_t default_config;
  num_configs = 1;
  err = GetConfigs(static_cast<int>(HWC_DISPLAY_PRIMARY), &default_config,
                   &num_configs);
  if (err != 0 || !num_configs) {
    ALOGE("Failed to second GetConfigs for display, err:%d, num_configs:%zu",
          err, num_configs);
    return false;
  }

  if (setActiveConfig(DISPLAY_PRIMARY, default_config)) {
    ALOGE("Failed to get setActiveConfig for display");
    return false;
  }

  drmModeAtomicReqPtr pset = drmModeAtomicAlloc();
  if (!pset) {
    ALOGE("Failed to allocate property set");
    return false;
  }

  if (mode_.needs_modeset) {
    ret =
        drmModeAtomicAddProperty(pset, crtc_->id(), crtc_->mode_property().id(),
                                 mode_.blob_id) < 0 ||
        drmModeAtomicAddProperty(pset, connector_->id(),
                                 connector_->crtc_id_property().id(),
                                 crtc_->id()) < 0;
    if (ret) {
      ALOGE("Failed to add blob %d to pset", mode_.blob_id);
      drmModeAtomicFree(pset);
      return false;
    }
  }

  if (!ret) {
    uint32_t flags = DRM_MODE_ATOMIC_ALLOW_MODESET;

    ret = drmModeAtomicCommit(drm_.fd(), pset, flags, &drm_);
    if (ret) {
      ALOGE("Failed to commit pset ret=%d\n", ret);
      drmModeAtomicFree(pset);
      return false;
    }
  }
  if (pset)
    drmModeAtomicFree(pset);

  if (mode_.needs_modeset) {
    ret = drm_.DestroyPropertyBlob(mode_.old_blob_id);
    if (ret) {
      ALOGE("Failed to destroy old mode property blob %d, %d",
            mode_.old_blob_id, ret);
      return false;
    }
    connector_->set_active_mode(mode_.mode);
    mode_.old_blob_id = mode_.blob_id;
    mode_.blob_id = 0;
    mode_.needs_modeset = false;
  }

  mInitFlag = true;

  ALOGI("SprdDrm:: Init success find interface num: %d", mNumInterfaces);
  return true;
}

void SprdDrm::deInit() {
  GraphicBufferAllocator::get().free((buffer_handle_t)mBufHandle);
  mBufHandle = NULL;

  mInitFlag = false;
}

int SprdDrm::QueryDisplayInfo(uint32_t *DisplayNum) {
  if (mInitFlag == false) {
    ALOGE("func: %s line: %d SprdADFWrapper Need Init first", __func__,
          __LINE__);
    return -1;
  }

  *DisplayNum = 1;

  return 0;
}

int SprdDrm::GetConfigs(int DisplayType, uint32_t *Configs,
                        size_t *NumConfigs) {
  if (mInitFlag == false) {
    ALOGE("func: %s line: %d SprdDrm Need Init first", __func__, __LINE__);
    return -1;
  }

  if (!Configs) {
    int ret = connector_->UpdateModes();
    if (ret) {
      ALOGE("Failed to update display modes %d", ret);
      return -1;
    }
  }

  auto num_modes = static_cast<uint32_t>(connector_->modes().size());
  if (!Configs) {
    *NumConfigs = num_modes;
    return 0;
  }

  uint32_t idx = 0;
  for (const DrmMode &mode : connector_->modes()) {
    if (idx >= *NumConfigs) {
      break;
    }
    Configs[idx++] = mode.id();
  }
  *NumConfigs = idx;

  ALOGD("SprdDrm:: GetConfigs , [%d]", Configs[0]);

  return 0;
}

uint32_t SprdDrm::CreateModeBlob(const DrmMode &mode) {
  struct drm_mode_modeinfo drm_mode;
  memset(&drm_mode, 0, sizeof(drm_mode));
  mode.ToDrmModeModeInfo(&drm_mode);

  uint32_t id = 0;
  int ret =
      drm_.CreatePropertyBlob(&drm_mode, sizeof(struct drm_mode_modeinfo), &id);
  if (ret) {
    ALOGE("Failed to create mode property blob: %d", ret);
    return 0;
  }
  ALOGI("Create blob_id: %d\n", id);
  return id;
}

int SprdDrm::setActiveConfig(int DisplayType, uint32_t Config) {
  if (mInitFlag == false) {
    ALOGE("func: %s line: %d SprdDrm Need Init first", __func__, __LINE__);
    return -1;
  }
  ALOGI("SprdDrm:: setActiveConfig: %d", Config);

  auto mode =
      std::find_if(connector_->modes().begin(), connector_->modes().end(),
                   [Config](DrmMode const &m) { return m.id() == Config; });
  if (mode == connector_->modes().end()) {
    ALOGE("SprdDrm:: Could not find active mode for %d", Config);
    return -1;
  }

  mode_.mode = *mode;
  if (mode_.blob_id)
    drm_.DestroyPropertyBlob(mode_.blob_id);
  mode_.blob_id = CreateModeBlob(mode_.mode);
  if (mode_.blob_id == 0) {
    ALOGE("Failed to create mode blob for display ");
    return -1;
  }
  mode_.needs_modeset = true;

  if (connector_->active_mode().id() == 0)
    connector_->set_active_mode(*mode);

  return 0;
}

int SprdDrm::getActiveConfig(int DisplayType, uint32_t *pConfig) {
  if (mInitFlag == false) {
    ALOGE("func: %s line: %d SprdDrm Need Init first", __func__, __LINE__);
    return -1;
  }

  ALOGD("SPRD_SR SprdDrm:: getActiveConfig: %d", *pConfig);
  return 0;
}

int SprdDrm::GetConfigAttributes(int DisplayType, uint32_t Config,
                                 const uint32_t *attributes, int32_t *values) {
  if (mInitFlag == false) {
    ALOGE("func: %s line: %d SprdDrm Need Init first", __func__, __LINE__);
    return -1;
  }
  auto mode =
      std::find_if(connector_->modes().begin(), connector_->modes().end(),
                   [Config](DrmMode const &m) { return m.id() == Config; });
  if (mode == connector_->modes().end()) {
    ALOGE("Could not find active mode for %d", Config);
    return -1;
  }

  static const int32_t kUmPerInch = 25400;
  uint32_t mm_width = connector_->mm_width();
  uint32_t mm_height = connector_->mm_height();
  static const uint32_t DISPLAY_ATTRIBUTES[] = {
      HWC_DISPLAY_VSYNC_PERIOD, HWC_DISPLAY_WIDTH, HWC_DISPLAY_HEIGHT,
      HWC_DISPLAY_DPI_X,        HWC_DISPLAY_DPI_Y, HWC_DISPLAY_NO_ATTRIBUTE,
  };
  for (int i = 0; i < ((int)NUM_DISPLAY_ATTRIBUTES) - 1; i++) {
    switch (attributes[i]) {
    case HWC_DISPLAY_WIDTH:
      values[i] = mode->h_display();
      break;
    case HWC_DISPLAY_HEIGHT:
      values[i] = mode->v_display();
      break;
    case HWC_DISPLAY_VSYNC_PERIOD:
      values[i] = (int32_t)(1000 * 1000 * 1000 / mode->v_refresh());
      break;
    case HWC_DISPLAY_DPI_X:
      // Dots per 1000 inches
      values[i] = mm_width ? (mode->h_display() * kUmPerInch) / mm_width : -1;
      break;
    case HWC_DISPLAY_DPI_Y:
      // Dots per 1000 inches
      values[i] = mm_height ? (mode->v_display() * kUmPerInch) / mm_height : -1;
      break;
    default:
      values[i] = -1;
      ALOGE("GetConfigAttributes error :%d", attributes[i]);
      return -1;
    }
  }
  return 0;
}

int SprdDrm::EventControl(int DisplayType, int event, bool enabled) {
  if (mInitFlag == false) {
    ALOGE("func: %s line: %d SprdDrm Need Init first", __func__, __LINE__);
    return -1;
  }

  switch (event) {
  case HWC_EVENT_VSYNC: { // scope for lock
    Mutex::Autolock _l(mLock);
    vsync_enabled = enabled;
    mCondition.signal();
    return 0;
  }
  default:
    return -1;
  }

  return 0;
}

int SprdDrm::Blank(int DisplayType, bool enabled) {
  int ret = -1;
  if (mInitFlag == false) {
    ALOGE("func: %s line: %d SprdDrm Need Init first", __func__, __LINE__);
    return -1;
  }

  int64_t start = systemTime(SYSTEM_TIME_BOOTTIME);
  uint64_t dpms_value = 0;

  if (enabled)
    dpms_value = DRM_MODE_DPMS_OFF;
  else
    dpms_value = DRM_MODE_DPMS_ON;

  const DrmProperty &prop = connector_->dpms_property();
  ret = drmModeConnectorSetProperty(drm_.fd(), connector_->id(), prop.id(),
                                    dpms_value);
  if (ret) {
    ALOGE("Failed to set DPMS property for connector %d", connector_->id());
    return ret;
  }

  int durationMillis = (int)nanoseconds_to_milliseconds(
      systemTime(SYSTEM_TIME_BOOTTIME) - start);
  if (durationMillis > 100) {
    ALOGD("Excessive delay in drm_blank(): %dms", durationMillis);
  }

  return 0;
}

int SprdDrm::Dump(char *buffer) {
  if (mInitFlag == false) {
    ALOGE("func: %s line: %d SprdDrm Need Init first,buffer:%p", __func__,
          __LINE__, buffer);
    return -1;
  }

  return 0;
}

int SprdDrm::AddFlushData(int DisplayType, SprdHWLayer **list, int LayerCount) {
  FlushContext *ctx = NULL;
  queryDebugFlag(&mDebugFlag);

  if ((DisplayType < 0) || (DisplayType >= DEFAULT_DISPLAY_TYPE_NUM)) {
    ALOGE("SprdDrm:: AddFlushData DisplayType is invalidate");
    return -1;
  }

  if (list == NULL || LayerCount <= 0) {
    ALOGE("SprdDrm:: AddFlushData context para error");
    return -1;
  }

  // WORKAROUND for wechat video call has 1 invalid layer(640x480->1x1).
  // FIXME: If wechat fix this invalid layer,this patch will be reverted.

  int j = 0;
  for (int i = 0; i < LayerCount; i++) {
    if (!list[i]) {
      ALOGE("SprdDrm:: AddFlushData layer %d is null", i);
      return -1;
    }
    if ((list[i]->getSprdFBRect()->w == 1) &&
        (list[i]->getSprdFBRect()->h == 1) &&
        (list[i]->getSprdSRCRect()->w == 640) &&
        (list[i]->getSprdSRCRect()->h == 480)) {
      ALOGI_IF(mDebugFlag,
               "SprdADFWrapper:: AddFlushData drop 640X480->1x1 layer");
      for (int j = i; j < LayerCount - 1; j++)
        list[j] = list[j + 1];
      LayerCount--;
      i--;
    }
  }

  ctx = &(mFlushContext[DisplayType]);
  ctx->LayerCount = LayerCount;
  ctx->LayerList = list;
  ctx->DisplayType = DisplayType;
  ctx->user = NULL;
  ctx->Active = true;
  mLayerCount += ctx->LayerCount;
  mActiveContextCount++;

  ALOGI_IF(mDebugFlag, "SprdDrm:: AddFlushData disp: %d, LayerCount: %d",
           DisplayType, LayerCount);

  return 0;
}

int SprdDrm::CreateSolidColorBuf() {
  std::string reqname = "SolidColor";
  uint32_t stride = 0;

  int usage = GRALLOC_USAGE_HW_RENDER;
  GraphicBufferAllocator::get().allocate(1, 1, HAL_PIXEL_FORMAT_RGBA_8888, 1,
                                         usage, (buffer_handle_t *)&mBufHandle,
                                         &stride, 0x689423, reqname);
  if (mBufHandle == NULL) {
    ALOGE("CreateSolidColorBuf cannot alloc buffer");
    return -1;
  }

  return 0;
}

int SprdDrm::ImportBuffer(buffer_handle_t handle, hwc_drm_bo_t *bo,
                          int format) {
  buffer_handle_t gr_handle = handle;
  if (!gr_handle)
    return -EINVAL;

  uint32_t gem_handle;
  int ret = drmPrimeFDToHandle(drm_.fd(), ADP_BUFFD(gr_handle), &gem_handle);
  if (ret) {
    ALOGE("failed to import prime fd %d ret=%d", ADP_BUFFD(gr_handle), ret);
    return ret;
  }

  int Bpf = ADP_STRIDE(gr_handle) * ADP_HEIGHT(gr_handle);

  bo->width = ADP_WIDTH(gr_handle);
  bo->height = ADP_HEIGHT(gr_handle);
  bo->format = ConvertHalFormatToDrm(format);
  bo->pitches[0] = ADP_STRIDE(gr_handle) * 4;
  bo->gem_handles[0] = gem_handle;
  bo->offsets[0] = 0;
  bo->modifier[0] = ADP_COMPRESSED(gr_handle) ? 1 : 0;

  switch (format) {
  case HAL_PIXEL_FORMAT_RGBA_8888:
  case HAL_PIXEL_FORMAT_BGRA_8888:
  case HAL_PIXEL_FORMAT_RGBX_8888:
    break;
  case HAL_PIXEL_FORMAT_RGB_888:
    bo->pitches[0] = ADP_STRIDE(gr_handle) * 3;
    break;
  case HAL_PIXEL_FORMAT_RGB_565:
    bo->pitches[0] = ADP_STRIDE(gr_handle) * 2;
    break;
  case HAL_PIXEL_FORMAT_YCbCr_420_SP:
    bo->gem_handles[1] = gem_handle;
    bo->offsets[1] = Bpf;
    bo->pitches[0] = ADP_STRIDE(gr_handle) * 1;
    bo->pitches[1] = bo->pitches[0];
    bo->modifier[1] = ADP_COMPRESSED(gr_handle) ? 1 : 0;
    break;
  case HAL_PIXEL_FORMAT_YCrCb_420_SP:
  case HAL_PIXEL_FORMAT_YCbCr_420_888:
  case HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED:
    bo->gem_handles[1] = gem_handle;
    bo->offsets[1] = Bpf;
    bo->pitches[0] = ADP_STRIDE(gr_handle) * 1;
    bo->pitches[1] = bo->pitches[0];
    bo->modifier[1] = ADP_COMPRESSED(gr_handle) ? 1 : 0;
    break;
  case HAL_PIXEL_FORMAT_YCbCr_422_SP:
    bo->gem_handles[1] = gem_handle;
    bo->offsets[1] = Bpf;
    bo->pitches[0] = ADP_STRIDE(gr_handle) * 1;
    bo->pitches[1] = bo->pitches[0];
    bo->modifier[1] = ADP_COMPRESSED(gr_handle) ? 1 : 0;
    break;
  case HAL_PIXEL_FORMAT_YCrCb_422_SP:
    bo->gem_handles[1] = gem_handle;
    bo->offsets[1] = Bpf;
    bo->pitches[0] = ADP_STRIDE(gr_handle) * 1;
    bo->pitches[1] = bo->pitches[0];
    bo->modifier[1] = ADP_COMPRESSED(gr_handle) ? 1 : 0;
    break;
  case HAL_PIXEL_FORMAT_YV12:
    bo->gem_handles[1] = gem_handle;
    bo->gem_handles[2] = gem_handle;
    bo->offsets[1] = Bpf;
    bo->offsets[2] =
        Bpf + ALIGN(ADP_STRIDE(gr_handle) / 2, 16) * ADP_HEIGHT(gr_handle) / 2;
    bo->pitches[0] = ADP_STRIDE(gr_handle) * 1;
    bo->pitches[1] = ALIGN(ADP_STRIDE(gr_handle) / 2, 16);
    bo->pitches[2] = bo->pitches[1];
    bo->modifier[1] = ADP_COMPRESSED(gr_handle) ? 1 : 0;
    bo->modifier[2] = ADP_COMPRESSED(gr_handle) ? 1 : 0;
    break;
  case HAL_PIXEL_FORMAT_YCbCr_422_I:
    bo->pitches[0] = ADP_STRIDE(gr_handle) * 2;
    break;
  default:
    ALOGE("don't support format:%x", format);
    break;
  }

  ret = drmModeAddFB2WithModifiers(drm_.fd(), bo->width, bo->height, bo->format,
                                   bo->gem_handles, bo->pitches, bo->offsets,
                                   bo->modifier, &bo->fb_id,
                                   DRM_MODE_FB_MODIFIERS);
  if (ret) {
    ALOGE("could not create drm fb %d", ret);
    return ret;
  }

  struct drm_gem_close gem_close;
  memset(&gem_close, 0, sizeof(gem_close));
  int num_gem_handles = sizeof(bo->gem_handles) / sizeof(bo->gem_handles[0]);
  for (int i = 0; i < num_gem_handles; i++) {
    if (!bo->gem_handles[i])
      continue;

    gem_close.handle = bo->gem_handles[i];
    int ret = drmIoctl(drm_.fd(), DRM_IOCTL_GEM_CLOSE, &gem_close);
    if (ret) {
      ALOGE("Failed to close gem handle %d %d", i, ret);
    } else {
      for (int j = i + 1; j < num_gem_handles; j++)
        if (bo->gem_handles[j] == bo->gem_handles[i])
          bo->gem_handles[j] = 0;
      bo->gem_handles[i] = 0;
    }
  }

  return ret;
}

int SprdDrm::ReleaseBuffer(hwc_drm_bo_t *bo) {

  if (bo->fb_id)
    if (drmModeRmFB(drm_.fd(), bo->fb_id))
      ALOGE("Failed to rm fb");
  return 0;
}

#ifdef SPRD_SR
bool SprdDrm::checkBootSR(FlushContext *ctx, hwc_drm_bo_t *bo,
                          sprdRectF *source_crop, sprdRect *fb_rect) {
  static bool boot_finished = false;

  if (boot_finished) {
    return false;
  }

  if (ctx->LayerCount == 1) {

    SprdHWLayer *l = ctx->LayerList[0];
    if (l == NULL) {
      ALOGE("SprdDrm:: checkBootSR layer is null");
      return false;
    }

    ALOGD_IF(mDebugFlag, "bootanimation on sr [%d, %d, %d, %d],bo[%d,%d]",
             fb_rect->left, fb_rect->top, fb_rect->right, fb_rect->bottom,
             bo[0].width, bo[0].height);

    if (l->getHasColorMatrix()) {
      ALOGI("do not modify the boot animation size with invert colors");
      boot_finished = true;
      return false;
    }

    if (fb_rect->w < bo[0].width && fb_rect->h < bo[0].height) {
      fb_rect->w = bo[0].width;
      fb_rect->h = bo[0].height;
      source_crop->w = bo[0].width;
      source_crop->h = bo[0].height;

      ALOGD_IF(mDebugFlag, "Changing displayFrame for bootanimation on sr");
      return true;
    }

  } else {
    ALOGD("bootanimation finished");
    boot_finished = true;
  }

  return false;
}
#endif

int SprdDrm::CommitFrame(FlushContext *ctx, hwc_drm_bo_t *bo,
                         int *releaseFencePtr, bool test_only) {

  int ret = 0;

  DrmConnector *connector =
      drm_.GetConnectorForDisplay(static_cast<int>(HWC_DISPLAY_PRIMARY));
  if (!connector) {
    ALOGE("Could not locate connector for display");
    return -ENODEV;
  }

  DrmCrtc *crtc = drm_.GetCrtcForDisplay(static_cast<int>(HWC_DISPLAY_PRIMARY));
  if (!crtc) {
    ALOGE("Could not locate crtc for display ");
    return -ENODEV;
  }

  drmModeAtomicReqPtr pset = drmModeAtomicAlloc();
  if (!pset) {
    ALOGE("Failed to allocate property set");
    return -ENOMEM;
  }

  if (mode_.needs_modeset) {
    ret = drmModeAtomicAddProperty(pset, crtc->id(), crtc->mode_property().id(),
                                   mode_.blob_id) < 0 ||
          drmModeAtomicAddProperty(pset, connector->id(),
                                   connector->crtc_id_property().id(),
                                   crtc->id()) < 0;
    if (ret) {
      ALOGE("Failed to add blob %d to pset", mode_.blob_id);
      drmModeAtomicFree(pset);
      return ret;
    }
  }

  if (!test_only) {
    ret = drmModeAtomicAddProperty(pset, crtc->id(),
                                   crtc->out_fence_ptr_property().id(),
                                   (uint64_t)releaseFencePtr) < 0;
    if (ret) {
      ALOGE("Failed to get out_fence_ptr_property");
    }
  }
  for (int i = 0; i < ctx->LayerCount; i++) {
    DrmPlane *plane = drm_.GetPlane(i);
    if (plane == NULL) {
      ALOGE("drm null plane");
      return 0;
    }

    int fb_id = -1;
    uint64_t rotation = 0;
    uint64_t alpha = 0xFF;
    uint64_t blend;
    uint32_t fbc_hsize_r;
    uint32_t fbc_hsize_y;
    uint32_t fbc_hsize_uv;
    native_handle_t *privateH = NULL;
    int y2r_coef;
    uint32_t pallete_en = 0;
    uint32_t pallete_color = 0;

    SprdHWLayer *l = ctx->LayerList[i];
    if (l == NULL) {
      ALOGE("SprdDrm:: layer is null");
      return -1;
    }

    privateH = l->getBufferHandle();
    if (privateH == NULL &&
        l->getCompositionType() != COMPOSITION_SOLID_COLOR) {
      ALOGE("SprdDrm:: CommitFrame buffer handle error");
      return -1;
    }

    fb_id = bo[i].fb_id;
    struct sprdRectF src;
    struct sprdRect fb;
    src.x = l->getSprdSRCRectF()->x;
    src.y = l->getSprdSRCRectF()->y;
    src.w = l->getSprdSRCRectF()->right - src.x;
    src.h = l->getSprdSRCRectF()->bottom - src.y;
    src.right = l->getSprdSRCRectF()->right;
    src.bottom = l->getSprdSRCRectF()->bottom;

    fb.x = l->getSprdFBRect()->left;
    fb.y = l->getSprdFBRect()->top;
    fb.w = l->getSprdFBRect()->right - fb.x;
    fb.h = l->getSprdFBRect()->bottom - fb.y;
    fb.right = l->getSprdFBRect()->right;
    fb.bottom = l->getSprdFBRect()->bottom;

#ifdef SPRD_SR
    /*
     * WORKAROUND MUST use dpu to handle boot animation.
     * Crop and displayFrame size are changed for SR,
     * but boot animation buffer size isn't changed.
     */
    checkBootSR(ctx, bo, &src, &fb);
#endif

    int acquire_fence_fd = l->getAcquireFence();
    rotation = ConvertRotationToDrm(l->getTransform());
    alpha = l->getPlaneAlpha();

    fbc_hsize_r = ADP_HEADERSIZER(privateH);
    fbc_hsize_y = ADP_HEADERSIZEY(privateH);
    fbc_hsize_uv = ADP_HEADERSIZEUV(privateH);

    y2r_coef = ConvertCSCMatrixToSprd(ADP_YINFO(privateH));

    ALOGI_IF(mDebugFlag, "SprdDrm:: CommitFrame ADP_YINFO:%d",
             ADP_YINFO(privateH));

    if (l->getCompositionType() == COMPOSITION_SOLID_COLOR) {
      pallete_en = 1;
      pallete_color = (l->getColor()->a << 24) | (l->getColor()->r << 16) |
                      (l->getColor()->g << 8) | l->getColor()->b;
      src.x = 0;
      src.y = 0;
      src.w = 1;
      src.h = 1;

      ALOGI_IF(mDebugFlag,
               "SprdDrm:: CommitFrame pallete_color:%u,a:%u,r:%u,g:%u,b:%u",
               pallete_color, l->getColor()->a, l->getColor()->r,
               l->getColor()->g, l->getColor()->b);
    }

    ret = drmModeAtomicAddProperty(pset, plane->id(),
                                   plane->crtc_property().id(), crtc->id()) < 0;
    ret |= drmModeAtomicAddProperty(pset, plane->id(),
                                    plane->fb_property().id(), fb_id) < 0;
    ret |= drmModeAtomicAddProperty(pset, plane->id(),
                                    plane->crtc_x_property().id(), fb.left) < 0;
    ret |= drmModeAtomicAddProperty(pset, plane->id(),
                                    plane->crtc_y_property().id(), fb.top) < 0;
    ret |= drmModeAtomicAddProperty(pset, plane->id(),
                                    plane->crtc_w_property().id(), fb.w) < 0;
    ret |= drmModeAtomicAddProperty(pset, plane->id(),
                                    plane->crtc_h_property().id(), fb.h) < 0;
    ret |= drmModeAtomicAddProperty(pset, plane->id(),
                                    plane->src_x_property().id(),
                                    (int)(src.x) << 16) < 0;
    ret |= drmModeAtomicAddProperty(pset, plane->id(),
                                    plane->src_y_property().id(),
                                    (int)(src.y) << 16) < 0;
    ret |= drmModeAtomicAddProperty(pset, plane->id(),
                                    plane->src_w_property().id(),
                                    (int)(src.w) << 16) < 0;
    ret |= drmModeAtomicAddProperty(pset, plane->id(),
                                    plane->src_h_property().id(),
                                    (int)(src.h) << 16) < 0;
    ret |= drmModeAtomicAddProperty(pset, plane->id(),
                                    plane->in_fence_fd_property().id(),
                                    acquire_fence_fd) < 0;
    if (ret) {
      ALOGE("Failed to add plane %d to set", plane->id());
      break;
    }

    if (plane->rotation_property().id()) {
      ret = drmModeAtomicAddProperty(pset, plane->id(),
                                     plane->rotation_property().id(),
                                     rotation) < 0;
      if (ret) {
        ALOGE("Failed to add rotation property %d to plane %d",
              plane->rotation_property().id(), plane->id());
        break;
      }
    }

    if (plane->alpha_property().id()) {
      ret = drmModeAtomicAddProperty(pset, plane->id(),
                                     plane->alpha_property().id(), alpha) < 0;
      if (ret) {
        ALOGE("Failed to add alpha property %d to plane %d",
              plane->alpha_property().id(), plane->id());
        break;
      }
    }

    if (plane->blend_property().id()) {
      switch (l->getBlendMode()) {
      case SPRD_HWC_BLENDING_PREMULT:
        std::tie(blend, ret) =
            plane->blend_property().GetEnumValueWithName("Pre-multiplied");
        break;
      case SPRD_HWC_BLENDING_COVERAGE:
        std::tie(blend, ret) =
            plane->blend_property().GetEnumValueWithName("Coverage");
        break;
      case SPRD_HWC_BLENDING_NONE:
      default:
        std::tie(blend, ret) =
            plane->blend_property().GetEnumValueWithName("None");
        break;
      }
      ret = drmModeAtomicAddProperty(pset, plane->id(),
                                     plane->blend_property().id(), blend) < 0;
      if (ret) {
        ALOGE("Failed to add pixel blend mode property %d to plane %d",
              plane->blend_property().id(), plane->id());
        break;
      }
    }

    if (plane->y2r_coef_property().id()) {
      ret = drmModeAtomicAddProperty(pset, plane->id(),
                                     plane->y2r_coef_property().id(),
                                     y2r_coef) < 0;
      if (ret) {
        ALOGE("Failed to add y2r_coef property %d to plane %d",
              plane->y2r_coef_property().id(), plane->id());
        break;
      }
    }

    if (plane->fbc_hsize_r_property().id()) {
      ret = drmModeAtomicAddProperty(pset, plane->id(),
                                     plane->fbc_hsize_r_property().id(),
                                     fbc_hsize_r) < 0;
      if (ret) {
        ALOGE("Failed to add fbc hsize_r %d to plane %d",
              plane->fbc_hsize_r_property().id(), plane->id());
        break;
      }
    }

    if (plane->fbc_hsize_y_property().id()) {
      ret = drmModeAtomicAddProperty(pset, plane->id(),
                                     plane->fbc_hsize_y_property().id(),
                                     fbc_hsize_y) < 0;
      if (ret) {
        ALOGE("Failed to add fbc hsize_y %d to plane %d",
              plane->fbc_hsize_y_property().id(), plane->id());
        break;
      }
    }

    if (plane->fbc_hsize_uv_property().id()) {
      ret = drmModeAtomicAddProperty(pset, plane->id(),
                                     plane->fbc_hsize_uv_property().id(),
                                     fbc_hsize_uv) < 0;
      if (ret) {
        ALOGE("Failed to add fbc hsize_uv %d to plane %d",
              plane->fbc_hsize_uv_property().id(), plane->id());
        break;
      }
    }

    if (plane->pallete_en_property().id()) {
      ret = drmModeAtomicAddProperty(pset, plane->id(),
                                     plane->pallete_en_property().id(),
                                     pallete_en) < 0;
      if (ret) {
        ALOGE("Failed to add alpha property %d to plane %d",
              plane->pallete_en_property().id(), plane->id());
        break;
      }
    }

    if (plane->pallete_color_property().id()) {
      ret = drmModeAtomicAddProperty(pset, plane->id(),
                                     plane->pallete_color_property().id(),
                                     pallete_color) < 0;
      if (ret) {
        ALOGE("Failed to add alpha property %d to plane %d",
              plane->pallete_color_property().id(), plane->id());
        break;
      }
    }
  }

  if (!ret) {
    uint32_t flags = DRM_MODE_ATOMIC_ALLOW_MODESET | DRM_MODE_ATOMIC_NONBLOCK;
    if (test_only)
      flags |= DRM_MODE_ATOMIC_TEST_ONLY;

    ret = drmModeAtomicCommit(drm_.fd(), pset, flags, &drm_);
    if (ret) {
      if (test_only)
        ALOGI("Commit test pset failed ret=%d\n", ret);
      else
        ALOGE("Failed to commit pset ret=%d\n", ret);
      drmModeAtomicFree(pset);
      return ret;
    }
    ALOGI_IF(mDebugFlag, "SprdDrm:: CommitFrame success release_fd: %d",
             *releaseFencePtr);
  }
  if (pset)
    drmModeAtomicFree(pset);
  if (!test_only && mode_.needs_modeset) {
    ret = drm_.DestroyPropertyBlob(mode_.old_blob_id);
    if (ret) {
      ALOGE("Failed to destroy old mode property blob %d, %d",
            mode_.old_blob_id, ret);
      return ret;
    }
    connector->set_active_mode(mode_.mode);
    mode_.old_blob_id = mode_.blob_id;
    mode_.blob_id = 0;
    mode_.needs_modeset = false;
  }

#ifdef SPRD_CABC
  enhance_flip_update();
#endif

  return ret;
}

int SprdDrm::PostDisplay(DisplayTrack *tracker) {
  int ret = -1;
  int i = 0;
  int j = 0;
  uint32_t interfaceNum = 0;
  int32_t currentIndex = 0;
  static Vector<int> fllow_control_rel_fds;
  struct hwc_drm_bo *BufferObject;
  struct hwc_drm_bo *temp_bo_;
  bool need_copy_last_bo = false;

  if (tracker == NULL) {
    ALOGE("SprdDrm:: PostDisplay input para error");
    ret = -1;
    goto EXT0;
  }

  queryDebugFlag(&mDebugFlag);

  if (mLayerCount <= 0) {
    ALOGI_IF(mDebugFlag, "SprdDrm:: PostDisplay No Layer should be displayed");
    ret = -1;
    goto EXT0;
  }

  BufferObject =
      (struct hwc_drm_bo *)calloc(mLayerCount, sizeof(struct hwc_drm_bo));
  if (BufferObject == NULL) {
    ALOGE("SprdDrm:: PostDisplay calloc hwc_drm_bo failed");
    ret = -1;
    goto EXT0;
  }

  if (mLayerCount > mLastLayerCount) {
    temp_bo_ = (struct hwc_drm_bo *)realloc(bo_,
                                       mLayerCount * sizeof(struct hwc_drm_bo));
    if (temp_bo_ == NULL) {
      ALOGE("SprdDrm:: PostDisplay realloc hwc_drm_bo failed");
      ret = -1;
      goto EXT1;
    }
    else {
      bo_ = temp_bo_;
    }
  }

  mActiveContextCount = (mActiveContextCount > DEFAULT_DISPLAY_TYPE_NUM)
                            ? DEFAULT_DISPLAY_TYPE_NUM
                            : mActiveContextCount;

  if (fllow_control_rel_fds.size() > NUM_FB_BUFFERS + 1) {
    Vector<int>::iterator front(fllow_control_rel_fds.begin());
    String8 name("SprdDrm:fllow_control");
    int ret = AdfFenceWait(name, *front);
    if (ret == 0) {
      ALOGI_IF(mDebugFlag, "wait fence fd:%d success.", *front);
    } else {
      ALOGI_IF(mDebugFlag, "wait fence fd:%d failed!", *front);
    }

    if (*front >= 0) {
      close(*front);
      *front = -1;
    }
    fllow_control_rel_fds.erase(front);
  }

  for (i = 0; i < mActiveContextCount; i++) {
    FlushContext *ctx = getFlushContext(i);

    if (ctx->Active == false) {
      continue;
    }

    if (ctx->LayerCount <= 0) {
      continue;
    }

    for (j = 0; j < ctx->LayerCount; j++) {
      int32_t index = currentIndex + j;
      SprdHWLayer *l = ctx->LayerList[j];

      if (implementBufferObject(l, &(BufferObject[index]))) {
        ALOGE("SprdDrm:: PostDisplay implementBufferObject failed");
        ret = -1;
        goto EXT1;
      }

      ALOGI_IF(mDebugFlag, "SprdDrm:: PostDisplay config %dth layer", index);
    }
    currentIndex += ctx->LayerCount;
    interfaceNum++;

    ret = CommitFrame(ctx, BufferObject, &tracker->releaseFenceFd, false);
    if (ret == 0) {
      for (j = 0; j < mLastLayerCount; j++)
        ReleaseBuffer(&bo_[j]);
      need_copy_last_bo = true;
    } else {
      for (j = 0; j < mLayerCount; j++)
        ReleaseBuffer(&BufferObject[j]);
    }
  }

  if (need_copy_last_bo) {
    memcpy(bo_, BufferObject, mLayerCount * sizeof(struct hwc_drm_bo));
    mLastLayerCount = mLayerCount;
  }

  tracker->retiredFenceFd =
      dup(tracker->releaseFenceFd); // custom->retire_fence; // ? fill later;

  fllow_control_rel_fds.push_back(dup(tracker->releaseFenceFd));
  ret = 0;

EXT1:
  free(BufferObject);
  BufferObject = NULL;
EXT0:
  mLayerCount = 0;
  mActiveContextCount = 0;
  invalidateFlushContext();

  return ret;
}

int SprdDrm::implementBufferObject(SprdHWLayer *l, hwc_drm_bo_t *bufferObject) {
  int format = -1;
  int ret = -1;
  native_handle_t *privateH = NULL;
  if (l == NULL) {
    ALOGE("SprdDrm:: implementBufferObject input para invalid");
    return -1;
  }

  memset(bufferObject, 0, sizeof(hwc_drm_bo_t));

  /*
  * FIXME:  The drm kernel native architecture does not support SOLID_COLOR.
  * SOLID_COLOR layer has no buffer, drm atomic check will return false.
  * SOLID_COLOR layer uses a 1x1 buffer to workaroud drm atomic check.
  */
  if (l->getCompositionType() != COMPOSITION_SOLID_COLOR)
    privateH = l->getBufferHandle();
  else {
    if (mBufHandle == NULL) {
      ret = CreateSolidColorBuf();
      if (ret) {
        ALOGE("Failed to create solid color buffer: %d", ret);
        return ret;
      }
    }
    privateH = mBufHandle;
  }
  if (privateH == NULL) {
    ALOGE("SprdDrm:: implementBufferObject buffer handle error");
    return -1;
  }

  if (l->getCompositionType() == COMPOSITION_SOLID_COLOR)
    format = ADP_FORMAT(privateH);
  else
    format = l->getLayerFormat();

  ret = ImportBuffer(privateH, bufferObject, format);
  if (ret) {
    ALOGE("SprdDrm:: implementBufferObject ImportBuffer failed ret:%d", ret);
    return -1;
  }

  return 0;
}

void SprdDrm::invalidateFlushContext() {
  int i = 0;

  for (i = 0; i < mActiveContextCount; i++) {
    FlushContext *ctx = getFlushContext(i);

    ctx->LayerCount = 0;
    ctx->LayerList = NULL;
    ctx->DisplayType = -1;
    ctx->Active = false;
  }
}

int SprdDrm::SendVblankRequest(int disp) {
  int ret;
  drmVBlank vbl;
  int drm_fd = drm_.fd();

  vbl.request.type = (drmVBlankSeqType)(DRM_VBLANK_RELATIVE | DRM_VBLANK_EVENT);
  vbl.request.sequence = 1;
  vbl.request.signal = reinterpret_cast<unsigned long>(this);

  ret = drmWaitVBlank(drm_fd, &vbl);
  if (ret < 0)
    ALOGE("Failed to request vblank %d", errno);

  return ret;
}

void SprdDrm::VblankHandler(int fd, unsigned int frame, unsigned int sec,
                            unsigned int usec, void *data) {
  if (vsync_enabled) {
    int64_t timestamp = (int64_t)sec * 1000000000 + (int64_t)usec * 1000;
    SprdEventHandle::SprdHandleVsyncReport(data, DISPLAY_PRIMARY, timestamp);
  }
}

void SprdDrm::VblankHandlerRun(int fd, unsigned int frame, unsigned int sec,
                               unsigned int usec, void *data) {
  if (data)
    return reinterpret_cast<SprdDrm *>(data)
        ->VblankHandler(fd, frame, sec, usec, data);
  return;
}

void *SprdDrm::EventHandler(void *arg) {
  if (arg) {
    return reinterpret_cast<SprdDrm *>(arg)->EventHandlerRun();
  }
  return NULL;
}

void *SprdDrm::EventHandlerRun() {
  int drm_fd = drm_.fd();
  drmEventContext evctx = {
      .version = DRM_EVENT_CONTEXT_VERSION,
      .vblank_handler = &SprdDrm::VblankHandlerRun,
      .page_flip_handler = NULL,
  };
  struct pollfd pfds[1] = {
      {.fd = drm_fd, .events = POLLIN, .revents = POLLERR}};

  setpriority(PRIO_PROCESS, 0, HAL_PRIORITY_URGENT_DISPLAY);

  while (1) {
    { // scope for lock
      Mutex::Autolock _l(mLock);
      while (!vsync_enabled) {
        mCondition.wait(mLock);
      }
    }
    SendVblankRequest(HWC_DISPLAY_PRIMARY);
    int ret = poll(pfds, ARRAY_SIZE(pfds), 60000);
    if (ret < 0) {
      ALOGE("Event handler error %d", errno);
      break;
    } else if (ret == 0) {
      ALOGI("Event handler timeout");
      continue;
    }
    for (int i = 0; i < ret; i++) {
      if (pfds[i].fd == drm_fd) {
        int err = drmHandleEvent(drm_fd, &evctx);
        if (err != 0)
          ALOGE("drmHandleEvent failed err %d\n", err);
      }
    }
  }
  return NULL;
}
