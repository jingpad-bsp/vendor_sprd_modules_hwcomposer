/*
 * Copyright (C) 2016 The Android Open Source Project
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
 ** File: SprdHWC2Error.h            DESCRIPTION                              *
 **                                   remap HWC2 error to local               *
 ******************************************************************************
 ******************************************************************************
 ** Author:         zhongjun.chen@spreadtrum.com                              *
 *****************************************************************************/


#ifndef _SPRD_HWC2_DATA_TYPE_H_
#define _SPRD_HWC2_DATA_TYPE_H_

#include <hardware/hwcomposer2.h>

enum {
    ERR_NONE          = HWC2_ERROR_NONE,
    ERR_BAD_CONFIG    = HWC2_ERROR_BAD_CONFIG,
    ERR_BAD_DISPLAY   = HWC2_ERROR_BAD_DISPLAY,
    ERR_BAD_LAYER     = HWC2_ERROR_BAD_LAYER,
    ERR_BAD_PARAMETER = HWC2_ERROR_BAD_PARAMETER,
    ERR_HAS_CHANGE    = HWC2_ERROR_HAS_CHANGES,
    ERR_NO_RESOURCES  = HWC2_ERROR_NO_RESOURCES,
    ERR_NOT_VALIDATED = HWC2_ERROR_NOT_VALIDATED,
    ERR_UNSUPPORTED   = HWC2_ERROR_UNSUPPORTED,
    ERR_NO_JOB        = 100,
};

enum {
  COMPOSITION_INVALID        = HWC2_COMPOSITION_INVALID,
  COMPOSITION_CLIENT         = HWC2_COMPOSITION_CLIENT,
  COMPOSITION_DEVICE         = HWC2_COMPOSITION_DEVICE,
  COMPOSITION_SOLID_COLOR    = HWC2_COMPOSITION_SOLID_COLOR,
  COMPOSITION_CURSOR         = HWC2_COMPOSITION_CURSOR,
  COMPOSITION_SIDEBAND       = HWC2_COMPOSITION_SIDEBAND,
};

enum {
  CONNECTION_INVALID         = HWC2_CONNECTION_INVALID,
  CONNECTION_CONNECTED       = HWC2_CONNECTION_CONNECTED,
  CONNECTION_DISCONNECTED    = HWC2_CONNECTION_DISCONNECTED,
};

enum {
  LAYER_REQUEST_NONE  = 0,
  CLEAR_CLIENT_TARGET = HWC2_LAYER_REQUEST_CLEAR_CLIENT_TARGET,
};

enum {
  BLEND_MODE_INVALID       = HWC2_BLEND_MODE_INVALID,
  BLEND_MODE_NONE          = HWC2_BLEND_MODE_NONE,
  BLEND_MODE_PREMULTIPLIED = HWC2_BLEND_MODE_PREMULTIPLIED,
  BLEND_MODE_COVERAGE      = HWC2_BLEND_MODE_COVERAGE,
};

enum {
  DISPLAY_TYPE_INVALID  = HWC2_DISPLAY_TYPE_INVALID,
  DISPLAY_TYPE_PHYSICAL = HWC2_DISPLAY_TYPE_PHYSICAL,
  DISPLAY_TYPE_VIRTUAL  = HWC2_DISPLAY_TYPE_VIRTUAL,
};

enum {
  CAPABILITY_INVALID          = HWC2_CAPABILITY_INVALID,
  CAPABILITY_SIDEBAND_STREAM  = HWC2_CAPABILITY_SIDEBAND_STREAM,
};

enum {
  VSYNC_INVALID  = HWC2_VSYNC_INVALID,
  VSYNC_ENABLE   = HWC2_VSYNC_ENABLE,
  VSYNC_DISABLE  = HWC2_VSYNC_DISABLE,
};

#endif
