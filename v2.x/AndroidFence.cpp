
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
 ** 22/09/2013    Hardware Composer   Responsible for processing some         *
 **                                   Hardware layers. These layers comply    *
 **                                   with display controller specification,  *
 **                                   can be displayed directly, bypass       *
 **                                   SurfaceFligner composition. It will     *
 **                                   improve system performance.             *
 ******************************************************************************
 ** File: AndroidFence.h              DESCRIPTION                             *
 **                                   Handle Android Framework fence          *
 ******************************************************************************
 ******************************************************************************
 ** Author:         zhongjun.chen@spreadtrum.com                              *
 *****************************************************************************/

#include <sys/mman.h>
#include <sys/types.h>
#include <hardware/hardware.h>
#include "sprd_ion.h"

#include "AndroidFence.h"
#include <cutils/log.h>
#include "SprdDisplayDevice.h"
#include "SprdTrace.h"

using namespace android;

#define ION_DEVICE "/dev/ion"

#define ION_IOC_MAGIC 'I'
#define ION_IOC_CUSTOM _IOWR(ION_IOC_MAGIC, 6, struct ion_custom_data)

static int ion_device_fd = -1;
static int s_debug = 0;

enum { TIMEOUT_NEVER = -1 };

struct HWC_fence_data {
  int release_fence_fd;
  int retired_fence_fd;
};
#if 0
#ifndef MALI_DDK
struct ion_custom_data {
  unsigned int cmd;
  unsigned long arg;
};
#endif
int sprd_fence_build(enum SPRD_DEVICE_SYNC_TYPE type,
                     struct HWC_fence_data *HWCData) {
  HWC_TRACE_CALL;
  if (ion_device_fd < 0) {
    ALOGE("get ion device failed");
    return -1;
  }

  if (HWCData == NULL) {
    ALOGE("sprd_fence_build input para is NULL");
    return -1;
  }

  struct ion_custom_data custom_data;
  struct ion_fence_data data;

  data.device_type = type;
  data.life_value = 1;
  data.release_fence_fd = -1;
  data.retired_fence_fd = -1;

  custom_data.cmd = ION_SPRD_CUSTOM_FENCE_CREATE;
  custom_data.arg = (unsigned long)&data;

  int ret = ioctl(ion_device_fd, ION_IOC_CUSTOM, &custom_data);
  if (ret < 0) {
    ALOGE("sprd_fence_create failed");
    return -1;
  }

  if (data.release_fence_fd < 0 || data.retired_fence_fd < 0) {
    ALOGE("sprd_fence_build return data error");
    return -1;
  }

  HWCData->release_fence_fd = data.release_fence_fd;
  HWCData->retired_fence_fd = data.retired_fence_fd;

  return 0;
}

int sprd_fence_signal(enum SPRD_DEVICE_SYNC_TYPE type) {
  HWC_TRACE_CALL;
  if (ion_device_fd < 0) {
    ALOGE("get ion device failed");
    return -1;
  }

  struct ion_custom_data custom_data;
  struct ion_fence_data data;

  memset(&data, 0, sizeof(struct ion_fence_data));
  data.device_type = type;

  custom_data.cmd = ION_SPRD_CUSTOM_FENCE_SIGNAL;
  custom_data.arg = (unsigned long)&data;

  int ret = ioctl(ion_device_fd, ION_IOC_CUSTOM, &custom_data);
  if (ret < 0) {
    ALOGE("sprd_fence_signal failed");
    return -1;
  }

  return 0;
}
#else
int sprd_fence_build(enum SPRD_DEVICE_SYNC_TYPE type,
                     struct HWC_fence_data *HWCData) {
  (void)type;

  if (HWCData == NULL) {
    ALOGE("sprd_fence_build input para is NULL");
    return -1;
  }

  HWCData->release_fence_fd = -1;
  HWCData->retired_fence_fd = -1;

  if (HWCData->release_fence_fd < 0 || HWCData->retired_fence_fd < 0) {
    ALOGE("sprd_fence_build return data error");
    return -1;
  }

  return 0;
}

int sprd_fence_signal(enum SPRD_DEVICE_SYNC_TYPE type) {
  (void)type;

  return 0;
}
#endif

int openSprdFence() {
  ion_device_fd = open(ION_DEVICE, O_RDWR);

  if (ion_device_fd < 0) {
    ALOGE("open ION_DEVICE failed");
    return -1;
  }

  return 0;
}

void closeSprdFence() {
  if (ion_device_fd >= 0) {
    close(ion_device_fd);
  }
}

void closeAcquireFDs(LIST& list, int debug) {
  s_debug = debug;
  for (size_t i = 0; i < list.size(); i++) {
    SprdHWLayer *l = list[i];

    if (l == NULL)
    {
      ALOGE("closeAcquireFDs layer[%d]", (int)i);
      continue;
    }

    if (l->getAcquireFence() >= 0) {
      ALOGI_IF(debug, "<09> close src layers[%d].acq:%d", (int)i,
               l->getAcquireFence());
      closeFence(l->getAcquireFencePointer());
    }
  }
}

int FenceWaitForever(const String8 &name, int fenceFd) {
  HWC_TRACE_CALL;
  if (fenceFd < 0) {
    return 0;
  }

  unsigned int warningTimeout = 3000;

  int err = sync_wait(fenceFd, warningTimeout);
  if (err < 0) {
    ALOGE("Fence: %s FD: %d didn't signal in %u ms", name.string(), fenceFd,
          warningTimeout);

    err = sync_wait(fenceFd, 6000);
    if (err < 0) {
      ALOGE(
          "Fence: %s FD: %d didn't signal in 6000 ms, app do not finish the "
          "rendering work",
          name.string(), fenceFd);
    }
  }

  return err;
}

int AdfFenceWait(const String8 &name, int fenceFd) {
  HWC_TRACE_CALL;
  if (fenceFd < 0) {
    return 0;
  }

  unsigned int warningTimeout = 3000;

  int err = sync_wait(fenceFd, warningTimeout);
  if (err < 0) {
    ALOGE("Fence: %s FD: %d didn't signal in %u ms", name.string(), fenceFd,
          warningTimeout);
    }

  return err;
}

int FenceMerge(const char *name, int fd1, int fd2) {
  if (name == NULL || fd1 < 0 || fd2 < 0) {
    ALOGE("FenceMerge input para is invalid");
    return -1;
  }

  int fd = sync_merge("HWCRel", fd1, fd2);
  if (fd < 0) {
    ALOGE("HWC: FenceMerge fd1: %d, fd2: %d failed", fd1, fd2);
    return -1;
  }

  return fd;
}

void closeFence(int *fd) {
  if (fd == NULL || *fd < 0) {
    return;
  }
  close(*fd);
  *fd = -1;
}

int waitAcquireFence(LIST& list) {
  HWC_TRACE_CALL;

  int ret = -1;

  for (size_t i = 0; i < list.size(); i++) {
    SprdHWLayer *l = list[i];

    if (l == NULL)
    {
      continue;
    }

    if (l->InitCheck() == false) {
      continue;
    }

    if (l->getAcquireFence() >= 0) {
      String8 name;

      name.appendFormat("acquireFence%d", (int)i);

      ret = FenceWaitForever(name, l->getAcquireFence());
    }
  }

  return ret;
}

int GenerateSyncFenceForFBDevice(int display, int *relFd, int *retiredFd) {
  HWC_TRACE_CALL;

  static int releaseFenceFd = -1;
  enum SPRD_DEVICE_SYNC_TYPE device_type = SPRD_DEVICE_PRIMARY_SYNC;
  struct HWC_fence_data fenceData;

  if (display == DISPLAY_VIRTUAL) {
    device_type = SPRD_DEVICE_VIRTUAL_SYNC;
  }

  if (releaseFenceFd >= 0) {
    int ret = -1;

    /*
     *  Display do not need previous buffer any more.
     *  Just release the previous buffer release fence.
     * */
    ret = sprd_fence_signal(device_type);

    if (ret < 0) {
      ALOGE("sprd_fence_signal name");
      return -1;
    }
    ALOGI_IF(s_debug, "%s[%d]  close fd:%d.", __func__, __LINE__,
             releaseFenceFd);
    close(releaseFenceFd);
    releaseFenceFd = -1;
  }

  if (sprd_fence_build(device_type, &fenceData) < 0) {
    ALOGE("HWCBufferSyncBuild create fence fd failed");
    return -1;
  }

  releaseFenceFd = dup(fenceData.release_fence_fd);

  *relFd = fenceData.release_fence_fd;
  *retiredFd = fenceData.retired_fence_fd;

  return 0;
}
