/*
 * Copyright (C) 2015 The Android Open Source Project
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

#define LOG_TAG "hwc-drm-plane"

#include "drmplane.h"
#include "drmresources.h"

#include <cinttypes>
#include <errno.h>
#include <stdint.h>
#include <log/log.h>
#include <xf86drmMode.h>

namespace android {

DrmPlane::DrmPlane(DrmResources *drm, drmModePlanePtr p)
    : drm_(drm), id_(p->plane_id), possible_crtc_mask_(p->possible_crtcs),
      type_(0), index_(0) {}

int DrmPlane::Init() {
  DrmProperty p;

  int ret = drm_->GetPlaneProperty(*this, "type", &p);
  if (ret) {
    ALOGE("Could not get plane type property");
    return ret;
  }

  uint64_t type;
  ret = p.value(&type);
  if (ret) {
    ALOGE("Failed to get plane type property value");
    return ret;
  }
  switch (type) {
  case DRM_PLANE_TYPE_OVERLAY:
  case DRM_PLANE_TYPE_PRIMARY:
  case DRM_PLANE_TYPE_CURSOR:
    type_ = (uint32_t)type;
    break;
  default:
    ALOGE("Invalid plane type %" PRIu64, type);
    return -EINVAL;
  }

  ret = drm_->GetPlaneProperty(*this, "CRTC_ID", &crtc_property_);
  if (ret) {
    ALOGE("Could not get CRTC_ID property");
    return ret;
  }

  ret = drm_->GetPlaneProperty(*this, "FB_ID", &fb_property_);
  if (ret) {
    ALOGE("Could not get FB_ID property");
    return ret;
  }

  ret = drm_->GetPlaneProperty(*this, "CRTC_X", &crtc_x_property_);
  if (ret) {
    ALOGE("Could not get CRTC_X property");
    return ret;
  }

  ret = drm_->GetPlaneProperty(*this, "CRTC_Y", &crtc_y_property_);
  if (ret) {
    ALOGE("Could not get CRTC_Y property");
    return ret;
  }

  ret = drm_->GetPlaneProperty(*this, "CRTC_W", &crtc_w_property_);
  if (ret) {
    ALOGE("Could not get CRTC_W property");
    return ret;
  }

  ret = drm_->GetPlaneProperty(*this, "CRTC_H", &crtc_h_property_);
  if (ret) {
    ALOGE("Could not get CRTC_H property");
    return ret;
  }

  ret = drm_->GetPlaneProperty(*this, "SRC_X", &src_x_property_);
  if (ret) {
    ALOGE("Could not get SRC_X property");
    return ret;
  }

  ret = drm_->GetPlaneProperty(*this, "SRC_Y", &src_y_property_);
  if (ret) {
    ALOGE("Could not get SRC_Y property");
    return ret;
  }

  ret = drm_->GetPlaneProperty(*this, "SRC_W", &src_w_property_);
  if (ret) {
    ALOGE("Could not get SRC_W property");
    return ret;
  }

  ret = drm_->GetPlaneProperty(*this, "SRC_H", &src_h_property_);
  if (ret) {
    ALOGE("Could not get SRC_H property");
    return ret;
  }

  ret = drm_->GetPlaneProperty(*this, "rotation", &rotation_property_);
  if (ret)
    ALOGE("Could not get rotation property");

  ret = drm_->GetPlaneProperty(*this, "alpha", &alpha_property_);
  if (ret)
    ALOGI("Could not get alpha property");

  ret = drm_->GetPlaneProperty(*this, "IN_FENCE_FD", &in_fence_fd_property_);
  if (ret)
    ALOGI("Could not get IN_FENCE_FD property");

  ret = drm_->GetPlaneProperty(*this, "pixel blend mode", &blend_property_);
  if (ret)
    ALOGI("Could not get pixel blend mode property");

  ret = drm_->GetPlaneProperty(*this, "FBC header size RGB",
                               &fbc_hsize_r_property_);
  if (ret)
    ALOGI("Could not get fbc_hsize_r property");

  ret = drm_->GetPlaneProperty(*this, "FBC header size Y",
                               &fbc_hsize_y_property_);
  if (ret)
    ALOGI("Could not get fbc_hsize_r property");

  ret = drm_->GetPlaneProperty(*this, "FBC header size UV",
                               &fbc_hsize_uv_property_);
  if (ret)
    ALOGI("Could not get fbc_hsize_r property");

  ret = drm_->GetPlaneProperty(*this, "YUV2RGB coef", &y2r_coef_property_);
  if (ret)
    ALOGI("Could not get y2r_coef property");

  ret = drm_->GetPlaneProperty(*this, "pallete enable", &pallete_en_property_);
  if (ret)
    ALOGI("Could not get pallete_enable property");

  ret =
      drm_->GetPlaneProperty(*this, "pallete color", &pallete_color_property_);
  if (ret)
    ALOGI("Could not get pallete_color property");

  return 0;
}

uint32_t DrmPlane::id() const { return id_; }

uint32_t DrmPlane::index() const { return index_; }

void DrmPlane::SetIndex(int index) {
  index_ = index;
  return;
}

bool DrmPlane::GetCrtcSupported(const DrmCrtc &crtc) const {
  return !!((1 << crtc.pipe()) & possible_crtc_mask_);
}

uint32_t DrmPlane::type() const { return type_; }

const DrmProperty &DrmPlane::crtc_property() const { return crtc_property_; }

const DrmProperty &DrmPlane::fb_property() const { return fb_property_; }

const DrmProperty &DrmPlane::crtc_x_property() const {
  return crtc_x_property_;
}

const DrmProperty &DrmPlane::crtc_y_property() const {
  return crtc_y_property_;
}

const DrmProperty &DrmPlane::crtc_w_property() const {
  return crtc_w_property_;
}

const DrmProperty &DrmPlane::crtc_h_property() const {
  return crtc_h_property_;
}

const DrmProperty &DrmPlane::src_x_property() const { return src_x_property_; }

const DrmProperty &DrmPlane::src_y_property() const { return src_y_property_; }

const DrmProperty &DrmPlane::src_w_property() const { return src_w_property_; }

const DrmProperty &DrmPlane::src_h_property() const { return src_h_property_; }

const DrmProperty &DrmPlane::rotation_property() const {
  return rotation_property_;
}

const DrmProperty &DrmPlane::alpha_property() const { return alpha_property_; }

const DrmProperty &DrmPlane::in_fence_fd_property() const {
  return in_fence_fd_property_;
}

const DrmProperty &DrmPlane::blend_property() const { return blend_property_; }

const DrmProperty &DrmPlane::fbc_hsize_r_property() const {
  return fbc_hsize_r_property_;
}

const DrmProperty &DrmPlane::fbc_hsize_y_property() const {
  return fbc_hsize_y_property_;
}

const DrmProperty &DrmPlane::fbc_hsize_uv_property() const {
  return fbc_hsize_uv_property_;
}

const DrmProperty &DrmPlane::y2r_coef_property() const {
  return y2r_coef_property_;
}

const DrmProperty &DrmPlane::pallete_en_property() const {
  return pallete_en_property_;
}

const DrmProperty &DrmPlane::pallete_color_property() const {
  return pallete_color_property_;
}
}
