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

#ifndef _SPRD_ADF_WRAPPER_H
#define _SPRD_ADF_WRAPPER_H

#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <hardware/hwcomposer.h>

/* for our custom validate ioctl */

#include "SprdDisplayCore.h"
#include "SprdHWLayer.h"
#include "SprdDisplayDevice.h"
#include "drmresources.h"
#include <utils/threads.h>

using namespace android;

#ifdef SPRD_CABC
#define CABC_VSYNC 0XAABBCC
#define CABC_FLIP 0XCCAABB
#endif

#ifndef ALIGN
#define ALIGN(value, base) (((value) + ((base)-1)) & ~((base)-1))
#endif

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
#endif

class SprdDrm : public SprdDisplayCore {
public:
  SprdDrm();
  ~SprdDrm();

  /*
   *  Init func: open ADF device, and get some configs.
   * */
  virtual bool Init();

  virtual int AddFlushData(int DisplayType, SprdHWLayer **list, int LayerCount);

  virtual int PostDisplay(DisplayTrack *tracker);

  virtual int QueryDisplayInfo(uint32_t *DisplayNum);

  virtual int GetConfigs(int DisplayType, uint32_t *Configs,
                         size_t *NumConfigs);
  virtual int setActiveConfig(int DisplayType, uint32_t Config);
  virtual int getActiveConfig(int DisplayType, uint32_t *pConfig);
  virtual int GetConfigAttributes(int DisplayType, uint32_t Config,
                                  const uint32_t *attributes, int32_t *values);

  virtual int EventControl(int DisplayType, int event, bool enabled);

  virtual int Blank(int DisplayType, bool enabled);

  virtual int Dump(char *buffer);

private:
  typedef struct {
    int LayerCount;
    SprdHWLayer **LayerList;
    int32_t DisplayType;
    bool Active;
    void *user;
  } FlushContext;

  int32_t mNumInterfaces;
  FlushContext mFlushContext[DEFAULT_DISPLAY_TYPE_NUM];
  int mDebugFlag;
  DrmResources drm_;
  DrmCrtc *crtc_ = NULL;
  DrmConnector *connector_ = NULL;
  pthread_t event_thread;
  int32_t mLastLayerCount;

  typedef struct hwc_drm_bo {
    uint32_t width;
    uint32_t height;
    uint32_t format; /* DRM_FORMAT_* from drm_fourcc.h */
    uint32_t pitches[4];
    uint32_t offsets[4];
    uint32_t gem_handles[4];
    uint32_t fb_id;
    uint64_t modifier[4];
    int acquire_fence_fd;
    void *priv;
  } hwc_drm_bo_t;
  hwc_drm_bo *bo_;
  bool vsync_enabled;

  struct ModeState {
    bool needs_modeset = false;
    DrmMode mode;
    uint32_t blob_id = 0;
    uint32_t old_blob_id = 0;
  };
  ModeState mode_;

  mutable Mutex mLock;
  Condition mCondition;
  native_handle_t *mBufHandle;

#ifdef SPRD_SR
  bool checkBootSR(FlushContext *ctx, hwc_drm_bo_t *bo, sprdRectF *source_crop,
                   sprdRect *fb_rect);
#endif
  void deInit();

  int implementBufferObject(SprdHWLayer *l, hwc_drm_bo_t *bufferObject);

  void invalidateFlushContext();
  int ImportBuffer(buffer_handle_t handle, hwc_drm_bo_t *bo, int format);
  int ReleaseBuffer(hwc_drm_bo_t *bo);
  int CommitFrame(FlushContext *ctx, hwc_drm_bo_t *bo, int *releaseFencePtr,
                  bool test_only);
  int SendVblankRequest(int disp);
  void VblankHandler(int fd, unsigned int frame, unsigned int sec,
                     unsigned int usec, void *data);
  static void VblankHandlerRun(int fd, unsigned int frame, unsigned int sec,
                               unsigned int usec, void *data);
  static void *EventHandler(void *arg);
  void *EventHandlerRun();
  uint32_t CreateModeBlob(const DrmMode &mode);
  int CreateSolidColorBuf();

  inline FlushContext *getFlushContext(int displayType) {
    if (displayType >= 0) {
      return &(mFlushContext[displayType]);
    } else {
      return NULL;
    }
  }
};

#endif // #ifndef _SPRD_ADF_WRAPPER_H
