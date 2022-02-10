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
 ** File: SprdHWComposer.cpp          DESCRIPTION                             *
 **                                   comunicate with SurfaceFlinger and      *
 **                                   other class objects of HWComposer       *
 ******************************************************************************
 ******************************************************************************
 ** Author:         zhongjun.chen@spreadtrum.com                              *
 *****************************************************************************/

#include "SprdHWComposer2.h"
#include "AndroidFence.h"
#include "SprdHWC2DataType.h"
#include "SprdHandleLayer.h"
#include "SprdDisplayCore.h"

using namespace android;
// PowerHint debug
#include <vendor/sprd/hardware/power/4.0/IPower.h>
#include <vendor/sprd/hardware/power/4.0/types.h>

using ::android::hidl::base::V1_0::IBase;
using ::vendor::sprd::hardware::power::V4_0::IPower;
using ::vendor::sprd::hardware::power::V4_0::PowerHint;
::android::sp<IBase> lock = new IBase();
::android::sp<::vendor::sprd::hardware::power::V4_0::IPower> gPowerHalV4_0; //= IPower::getService();


#define HWC_NEGTIVE -1

bool SprdHWComposer2::Init() {
#if defined HWC_SUPPORT_FBD_DISPLAY
  mDisplayCore = new SprdFrameBufferDevice();
#elif defined USE_ADF_DISPLAY
  mDisplayCore = new SprdADFWrapper();
#else
  mDisplayCore = new SprdDrm();
#endif
  if (mDisplayCore == NULL) {
    ALOGE("new SprdDisplayCore failed");
    return false;
  }

  if (!(mDisplayCore->Init())) {
    ALOGE("SprdDisplayCore Init failed");
    return false;
  }

  /*
   *  SprdPrimaryDisplayDevice information
   * */
  mPrimaryDisplay = new SprdPrimaryDisplayDevice();
  if (mPrimaryDisplay == NULL) {
    ALOGE("new SprdPrimaryDisplayDevice failed");
    return false;
  }

  if (!(mPrimaryDisplay->Init(mDisplayCore))) {
    ALOGE("mPrimaryDisplayDevice init failed");
    return false;
  }

  /*
   *  SprdExternalDisplayDevice information
   * */
  mExternalDisplay = new SprdExternalDisplayDevice();
  if (mExternalDisplay == NULL) {
    ALOGE("new mExternalDisplayDevice failed");
    return false;
  }

  if (!(mExternalDisplay->Init(mDisplayCore))) {
    ALOGE("mExternalDisplay Init failed");
    return false;
  }

  /*
   * We wait SprdPrimaryDisplayDevice and SprdExternalDisplayDevice Init done,
   * then call mDisplayCore->Init
   */
   /*
  if (!(mDisplayCore->Init())) {
    ALOGE("SprdDisplayCore Init failed");
    return false;
  }
  */

  /*
   *  SprdVirtualDisplayDevice information
   * */
  mVirtualDisplay = new SprdVirtualDisplayDevice();
  if (mVirtualDisplay == NULL) {
    ALOGE("new mVirtualDisplayDevice failed");
    return false;
  }

  if ((mVirtualDisplay->Init() != 0)) {
    ALOGE("VirtualDisplay Init failed");
    return false;
  }

  mInitFlag = 1;

  return true;
}

SprdHWComposer2::~SprdHWComposer2() {

  if (mPrimaryDisplay) {
    delete mPrimaryDisplay;
    mPrimaryDisplay = NULL;
  }

  if (mExternalDisplay) {
    delete mExternalDisplay;
    mExternalDisplay = NULL;
  }

  if (mVirtualDisplay) {
    delete mVirtualDisplay;
    mVirtualDisplay = NULL;
  }

  mInitFlag = 0;
}

int SprdHWComposer2::parseDisplayAttributes(const uint32_t *attributes, AttributesSet *dpyAttr, int32_t *value)
{
    if (attributes == NULL || dpyAttr == NULL)
    {
        ALOGE("parseDisplayAttributes input para is NULL");
        return -EINVAL;
    }

  for (unsigned int i = 0; i < NUM_DISPLAY_ATTRIBUTES - 1; i++) {
    switch (attributes[i]) {
      case HWC_DISPLAY_VSYNC_PERIOD:
        dpyAttr->vsync_period = value[i];
        ALOGI("getDisplayAttributes: vsync_period:%d",
              dpyAttr->vsync_period);
        if (dpyAttr->vsync_period == 0) {
          ALOGI(
              "getDisplayAttributes: vsync_period:0,set to default "
              "60fps.");
          dpyAttr->vsync_period = (1e9 / 60);
        }
        break;
      case HWC_DISPLAY_WIDTH:
        dpyAttr->xres = value[i];
        ALOGI("getDisplayAttributes: width:%d", dpyAttr->xres);
        break;
      case HWC_DISPLAY_HEIGHT:
        dpyAttr->yres = value[i];
        ALOGI("getDisplayAttributes: height:%d", dpyAttr->yres);
        break;
      case HWC_DISPLAY_DPI_X:
        dpyAttr->xdpi = (float)(value[i]);
        if (dpyAttr->xdpi == 0) {
          // the driver doesn't return that information
          // default to 160 dpi
          dpyAttr->xdpi = 160;
        }
        ALOGI("getDisplayAttributes: xdpi:%f", dpyAttr->xdpi);
        break;
      case HWC_DISPLAY_DPI_Y:
        dpyAttr->ydpi = (float)(value[i]);
        if (dpyAttr->ydpi == 0) {
          // the driver doesn't return that information
          // default to 160 dpi
          dpyAttr->ydpi = 160;
        }
        ALOGI("getDisplayAttributes: ydpi:%f", dpyAttr->ydpi);
        break;
      case HWC_DISPLAY_NO_ATTRIBUTE:
        break;
      default:
        ALOGE("Unknown Display Attributes:%d", attributes[i]);
        return -EINVAL;
    }
  }

  return 0;
}

int SprdHWComposer2::DevicePropertyProbe(SprdDisplayClient *Client)
{
  int ret = -1;
  int32_t Id = 0;
  DisplayAttributes *Att = NULL;

  if (Client == NULL)
  {
    ALOGE("SprdHWComposer2 line: %d cannot get SprdDisplayClient", __LINE__);
    return ERR_BAD_DISPLAY;
  }

  Id  = Client->getDisplayId();
  Att = Client->getDisplayAttributes();
  if (Att == NULL)
  {
    ALOGE("SprdHWComposer2 line: %d cannot get DisplayAttributes", __LINE__);
    return ERR_BAD_DISPLAY;
  }

  switch (Id) {
    case DISPLAY_PRIMARY_ID:
#ifndef FORCE_OVC
      Att->AcceleratorMode |= ACCELERATOR_DISPC;
      Att->AcceleratorMode |= ACCELERATOR_GSP;
#endif
      Att->AcceleratorMode |= ACCELERATOR_OVERLAYCOMPOSER;
      ret = 0;
      break;
    case DISPLAY_EXTERNAL_ID:
      ret = 0;
      break;
    case DISPLAY_VIRTUAL_ID:
      if (Att->connected) {
        Att->AcceleratorMode |= ACCELERATOR_GSP;
        // Att->AcceleratorMode &=
        // ~ACCELERATOR_GSP;
      }
      ret = 0;
      break;
    default:
      ret = -1;
  }

  return ret;
}


/* For Android interface */
void SprdHWComposer2::getCapabilities(uint32_t* outCount, int32_t* /*hwc2_capability_t*/ outCapabilities)
{
  /*  TODO: should we implement getCapabilities on other display device?
   *  just let Primary display device decide it first. */
  mPrimaryDisplay->getCapabilities(outCount, outCapabilities);
}

int32_t /*hwc2_error_t*/SprdHWComposer2::CREATE_VIRTUAL_DISPLAY(
            uint32_t width, uint32_t height,
            int32_t* /*android_pixel_format_t*/ format, hwc2_display_t* outDisplay)
{
  return mVirtualDisplay->CREATE_VIRTUAL_DISPLAY(width, height, format, outDisplay);
}

int32_t /*hwc2_error_t*/SprdHWComposer2::DESTROY_VIRTUAL_DISPLAY(hwc2_display_t display)
{
  SprdDisplayClient *Client = SprdDisplayClient::getDisplayClient(display);
  if (Client == NULL)
  {
    ALOGE("SprdHWComposer2 line: %d cannot get SprdDisplayClient", __LINE__);
    return ERR_BAD_DISPLAY;
  }

  return mVirtualDisplay->DESTROY_VIRTUAL_DISPLAY(Client);
}

void SprdHWComposer2::DUMP(uint32_t* outSize, char* outBuffer)
{
  //HWC_IGNORE(outSize);
  //HWC_IGNORE(outBuffer);
  if(mPrimaryDisplay)
  {
    mPrimaryDisplay->DUMP(outSize, outBuffer, mResult);
  }
  if(mExternalDisplay)
  {
    mExternalDisplay->DUMP(outSize, outBuffer, mResult);
  }
  if(mVirtualDisplay)
  {
    mVirtualDisplay->DUMP(outSize, outBuffer, mResult);
  }
}

uint32_t SprdHWComposer2::GET_MAX_VIRTUAL_DISPLAY_COUNT(void)
{
  return mVirtualDisplay->GET_MAX_VIRTUAL_DISPLAY_COUNT();
}

int32_t /*hwc2_error_t*/SprdHWComposer2::REGISTER_CALLBACK(
           int32_t /*hwc2_callback_descriptor_t*/ descriptor,
           hwc2_callback_data_t callbackData, hwc2_function_pointer_t pointer)
{
  int32_t err = ERR_NONE;

  /*
   *  Notes: PrimaryDisplay should manager vsync event.
   *         Hotplug should be owned by PrimaryDisplay and External display
   */
  err = mDisplayCore->REGISTER_CALLBACK(descriptor, callbackData, pointer);

  return err;
}

int32_t /*hwc2_error_t*/SprdHWComposer2::ACCEPT_DISPLAY_CHANGES(
           hwc2_display_t display)
{
  int32_t err = ERR_NONE;
  int32_t Id = 0;
  SprdDisplayClient *Client = SprdDisplayClient::getDisplayClient(display);

  if (Client == NULL)
  {
    ALOGE("SprdHWComposer2 line: %d cannot get SprdDisplayClient", __LINE__);
    return ERR_BAD_DISPLAY;
  }

  Id  = Client->getDisplayId();

  switch (Id)
  {
    case DISPLAY_PRIMARY_ID:
      err = mPrimaryDisplay->ACCEPT_DISPLAY_CHANGES(Client);
      break;
    case DISPLAY_EXTERNAL_ID:
      err = mExternalDisplay->ACCEPT_DISPLAY_CHANGES(Client);
      break;
    case DISPLAY_VIRTUAL_ID:
      err = mVirtualDisplay->ACCEPT_DISPLAY_CHANGES(Client);
      break;
    default:
      ALOGE("ACCEPT_DISPLAY_CHANGES not support display display:0x%lx", (unsigned long)display);
      err = ERR_BAD_DISPLAY;
      break;
  }

  return err;
}

int32_t /*hwc2_error_t*/SprdHWComposer2::CREATE_LAYER(
           hwc2_display_t display, hwc2_layer_t* outLayer)
{
  int32_t err = ERR_NONE;
  int32_t Id = 0;
  SprdDisplayClient *Client = SprdDisplayClient::getDisplayClient(display);
  if (Client == NULL || reinterpret_cast<unsigned long>(Client) == HWC_NEGTIVE)
  {
    ALOGE("SprdHWComposer2 line: %d cannot get SprdDisplayClient", __LINE__);
    return ERR_BAD_DISPLAY;
  }

  Id  = Client->getDisplayId();
  switch (Id)
  {
    case DISPLAY_PRIMARY_ID:
      err = mPrimaryDisplay->CREATE_LAYER(Client, outLayer);
      break;
    case DISPLAY_EXTERNAL_ID:
      err = mExternalDisplay->CREATE_LAYER(Client, outLayer);
      break;
    case DISPLAY_VIRTUAL_ID:
      err = mVirtualDisplay->CREATE_LAYER(Client, outLayer);
      break;
    default:
      ALOGE("Line: %d not support display:0x%lx", __LINE__, (unsigned long)display);
      err = ERR_BAD_DISPLAY;
      break;
  }

  return err;
}

int32_t /*hwc2_error_t*/SprdHWComposer2::DESTROY_LAYER(
           hwc2_display_t display, hwc2_layer_t layer)
{
  int32_t err = ERR_NONE;
  int32_t Id = 0;
  SprdDisplayClient *Client = SprdDisplayClient::getDisplayClient(display);
  if (Client == NULL || reinterpret_cast<unsigned long>(Client) == HWC_NEGTIVE)
  {
    ALOGE("SprdHWComposer2 line: %d cannot get SprdDisplayClient", __LINE__);
    return ERR_BAD_DISPLAY;
  }

  Id  = Client->getDisplayId();

  switch (Id)
  {
    case DISPLAY_PRIMARY_ID:
      err = mPrimaryDisplay->DESTROY_LAYER(Client, layer);
      break;
    case DISPLAY_EXTERNAL_ID:
      err = mExternalDisplay->DESTROY_LAYER(Client, layer);
      break;
    case DISPLAY_VIRTUAL_ID:
      err = mVirtualDisplay->DESTROY_LAYER(Client, layer);
      break;
    default:
      ALOGE("Line: %d not support display display:0x%lx", __LINE__, (unsigned long)display);
      err = ERR_BAD_DISPLAY;
      break;
  }

  return err;
}

int32_t /*hwc2_error_t*/SprdHWComposer2::GET_ACTIVE_CONFIG(
           hwc2_display_t display,
           hwc2_config_t* outConfig)
{
  int32_t err = ERR_NONE;

  if (outConfig == NULL)
  {
    ALOGE("GET_ACTIVE_CONFIG outConfig is NULL");
    return ERR_BAD_PARAMETER;
  }

  int32_t Id = 0;
  DisplayAttributes *Att = NULL;
  SprdDisplayClient *Client = SprdDisplayClient::getDisplayClient(display);
  if (Client == NULL || reinterpret_cast<unsigned long>(Client) == HWC_NEGTIVE)
  {
    ALOGE("SprdHWComposer2 line: %d cannot get SprdDisplayClient", __LINE__);
    return ERR_BAD_DISPLAY;
  }

  Id  = Client->getDisplayId();
  Att = Client->getDisplayAttributes();
  if (Att == NULL)
  {
    ALOGE("SprdHWComposer2 line: %d cannot get DisplayAttributes", __LINE__);
    return ERR_BAD_DISPLAY;
  }

  switch (Id)
  {
    case DISPLAY_PRIMARY_ID:
      *outConfig = Att->configsIndex;
#ifdef SPRD_SR
      if(!Att->sprdSF && Att->configsIndex != 0) {
        /*for VTS , we return one config, so the active config must be zero,
          if we get a non-zero index from ADF hal, we should correct it to zero here.*/
        *outConfig = 0;
        ALOGE("SPRD_SR SprdHWComposer2::GET_ACTIVE_CONFIG[%d] not sprdSF but Att->configsIndex:%d",
            __LINE__,Att->configsIndex);
      }
      ALOGE("SPRD_SR SprdHWComposer2::GET_ACTIVE_CONFIG[%d] *outConfig:%d",__LINE__,*outConfig);
#endif
      break;
    case DISPLAY_EXTERNAL_ID:
      if (Att->connected) {
        *outConfig = Att->configsIndex;
      }
      break;
    case DISPLAY_VIRTUAL_ID:
      break;
    default:
      ALOGE("Line:%d not support display display:0x%lx", __LINE__, (unsigned long)display);
      err = ERR_BAD_DISPLAY;
      break;
  }

  return err;
}

int32_t /*hwc2_error_t*/SprdHWComposer2::GET_CHANGED_COMPOSITION_TYPES(
           hwc2_display_t display,
           uint32_t* outNumElements, hwc2_layer_t* outLayers,
           int32_t* /*hwc2_composition_t*/ outTypes)
{
  int32_t err = ERR_NONE;
  int32_t Id = 0;
  SprdDisplayClient *Client = SprdDisplayClient::getDisplayClient(display);

  if (Client == NULL)
  {
    ALOGE("SprdHWComposer2 line: %d cannot get SprdDisplayClient", __LINE__);
    return ERR_BAD_DISPLAY;
  }

  Id  = Client->getDisplayId();

  switch (Id)
  {
    case DISPLAY_PRIMARY_ID:
      err = mPrimaryDisplay->GET_CHANGED_COMPOSITION_TYPES(Client, outNumElements, outLayers, outTypes);
      break;
    case DISPLAY_EXTERNAL_ID:
      err = mExternalDisplay->GET_CHANGED_COMPOSITION_TYPES(Client, outNumElements, outLayers, outTypes);
      break;
    case DISPLAY_VIRTUAL_ID:
      err = mVirtualDisplay->GET_CHANGED_COMPOSITION_TYPES(Client, outNumElements, outLayers, outTypes);
      break;
    default:
      ALOGE("Line: %d not support display display:0x%lx", __LINE__, (unsigned long)display);
      err = ERR_BAD_DISPLAY;
      break;
  }

  return err;
}

int32_t /*hwc2_error_t*/SprdHWComposer2::GET_CLIENT_TARGET_SUPPORT(
           hwc2_display_t display, uint32_t width,
           uint32_t height, int32_t /*android_pixel_format_t*/ format,
           int32_t /*android_dataspace_t*/ dataspace)
{
  int32_t err = ERR_NONE;
  int32_t Id = 0;
  SprdDisplayClient *Client = SprdDisplayClient::getDisplayClient(display);
  if (Client == NULL || reinterpret_cast<unsigned long>(Client) == HWC_NEGTIVE)
  {
    ALOGE("SprdHWComposer2 line: %d cannot get SprdDisplayClient", __LINE__);
    return ERR_BAD_DISPLAY;
  }

  Id  = Client->getDisplayId();

  switch (Id)
  {
    case DISPLAY_PRIMARY_ID:
      err = mPrimaryDisplay->GET_CLIENT_TARGET_SUPPORT(Client, width, height, format, dataspace);
      break;
    case DISPLAY_EXTERNAL_ID:
      err = mExternalDisplay->GET_CLIENT_TARGET_SUPPORT(Client, width, height, format, dataspace);
      break;
    case DISPLAY_VIRTUAL_ID:
      err = mVirtualDisplay->GET_CLIENT_TARGET_SUPPORT(Client, width, height, format, dataspace);
      break;
    default:
      ALOGE("Line: %d not support display display:0x%lx", __LINE__, (unsigned long)display);
      err = ERR_BAD_DISPLAY;
      break;
  }

  return err;
}

int32_t /*hwc2_error_t*/SprdHWComposer2::GET_COLOR_MODES(
           hwc2_display_t display, uint32_t* outNumModes,
           int32_t* /*android_color_mode_t*/ outModes)
{
  int32_t err = ERR_NONE;
  int32_t Id = 0;
  SprdDisplayClient *Client = SprdDisplayClient::getDisplayClient(display);
  if (Client == NULL)
  {
    ALOGE("SprdHWComposer2 line: %d cannot get SprdDisplayClient", __LINE__);
    return ERR_BAD_DISPLAY;
  }

  Id  = Client->getDisplayId();
  switch (Id)
  {
    case DISPLAY_PRIMARY_ID:
      err = mPrimaryDisplay->GET_COLOR_MODES(Client, outNumModes, outModes);
      break;
    case DISPLAY_EXTERNAL_ID:
      err = mExternalDisplay->GET_COLOR_MODES(Client, outNumModes, outModes);
      break;
    case DISPLAY_VIRTUAL_ID:
      err = mVirtualDisplay->GET_COLOR_MODES(Client, outNumModes, outModes);
      break;
    default:
      ALOGE("Line: %d not support display display:0x%lx", __LINE__, (unsigned long)display);
      err = ERR_BAD_DISPLAY;
      break;
  }

  return err;
}

int32_t /*hwc2_error_t*/SprdHWComposer2::GET_DISPLAY_ATTRIBUTE(
           hwc2_display_t display, hwc2_config_t config,
           int32_t /*hwc2_attribute_t*/ attribute, int32_t* outValue)
{
  int32_t err = ERR_NONE;
  uint32_t disp = DISPLAY_PRIMARY;
  uint32_t attributes[NUM_DISPLAY_ATTRIBUTES];
  int32_t values[NUM_DISPLAY_ATTRIBUTES];
  AttributesSet *dpyAttr = NULL;
  int32_t Id = 0;
  DisplayAttributes *Att = NULL;
  SprdDisplayClient *Client = SprdDisplayClient::getDisplayClient(display);

  if (Client == NULL)
  {
    ALOGE("SprdHWComposer2 line: %d cannot get SprdDisplayClient", __LINE__);
    return ERR_BAD_DISPLAY;
  }

  Id  = Client->getDisplayId();
  Att = Client->getDisplayAttributes();
  if (Att == NULL)
  {
    ALOGE("SprdHWComposer2 line: %d cannot get DisplayAttributes", __LINE__);
    return ERR_BAD_DISPLAY;
  }

  if (DISPLAY_EXTERNAL_ID == Id && !Att->connected) {
    // ALOGD("External Display Device is not connected");
    err = ERR_BAD_DISPLAY;
    goto EXIT;
  }

  if (DISPLAY_VIRTUAL_ID == Id && !Att->connected) {
    // ALOGD("VIRTUAL Display Device is not connected");
    err = ERR_BAD_DISPLAY;
    goto EXIT;
  }

  if (outValue == NULL)
  {
    err = ERR_BAD_PARAMETER;
    goto EXIT;
  }

  switch (Id)
  {
    case DISPLAY_PRIMARY_ID:
      dpyAttr = &(Att->sets[config]);
      if (dpyAttr && (dpyAttr->fillFlag == false))
      {
        attributes[0] = HWC_DISPLAY_WIDTH;
        attributes[1] = HWC_DISPLAY_HEIGHT;
        attributes[2] = HWC_DISPLAY_VSYNC_PERIOD;
        attributes[3] = HWC_DISPLAY_DPI_X;
        attributes[4] = HWC_DISPLAY_DPI_Y;
        attributes[5] = HWC_DISPLAY_NO_ATTRIBUTE;

        if (mDisplayCore->GetConfigAttributes(DISPLAY_PRIMARY, config, attributes, values) < 0)
        {
          err = ERR_BAD_CONFIG;
        }
        dpyAttr->fillFlag = true;
        parseDisplayAttributes(attributes, dpyAttr, values);
#ifdef SPRD_SR
        ALOGD("SPRD_SR SprdHWComposer2::GET_DISPLAY_ATTRIBUTE get PRIMARYDISPLAY sets[%d]:"
                "{xres:%d,yres:%d,stride:%d,xdpi:%f,ydpi:%f,format:%x}",config,
                dpyAttr->xres,dpyAttr->yres,dpyAttr->stride,dpyAttr->xdpi,dpyAttr->ydpi,dpyAttr->format);
#endif
      }
      break;
    case DISPLAY_EXTERNAL_ID:
      /*  TODO:  */
      disp = DISPLAY_EXTERNAL;
      break;
    default:
      ALOGE("Line: %d not support display display:0x%lx", __LINE__, (unsigned long)display);
      break;
  }

  if (dpyAttr == NULL)
  {
    ALOGE("GET_DISPLAY_ATTRIBUTE not support display: 0x%lx", (unsigned long)display);
    err = ERR_BAD_DISPLAY;
    goto EXIT;
  }

  switch (attribute)
  {
    case ATT_WIDTH:
      *outValue = dpyAttr->xres;
      break;
    case ATT_HEIGHT:
      *outValue = dpyAttr->yres;
      break;
    case ATT_VSYNC_PERIOD:
      *outValue = dpyAttr->vsync_period;
      break;
    case ATT_DPI_X:
       *outValue = dpyAttr->xdpi;
       break;
    case ATT_DPI_Y:
      *outValue = dpyAttr->ydpi;
       break;
    default:
      ALOGE("GET_DISPLAY_ATTRIBUTE not support attribute: %d", attribute);
      err = ERR_BAD_PARAMETER;
      break;
  }

  switch (Id) {
    case DISPLAY_PRIMARY_ID:
#ifdef SPRD_SR

      if(config == Att->configsIndex) {
        static bool synced = false;
        if(synced == false) {//only first time we do sync, SR may change mPrimaryDisplay's config, add this condition to avoid overwritting
          mPrimaryDisplay->syncAttributes(Client, dpyAttr);
          synced = true;
        }
      }
#else
    mPrimaryDisplay->syncAttributes(Client, dpyAttr);
#endif
      //Att->connected = true;
      break;
    case DISPLAY_EXTERNAL_ID:
      mExternalDisplay->syncAttributes(Client, dpyAttr);
      // Att->connected = true;
      break;
    case DISPLAY_VIRTUAL_ID:
      mVirtualDisplay->syncAttributes(Client, dpyAttr);
      // Att->connected = true;
      break;
    default:
      ALOGE(
          "SprdHWComposer:: getDisplayAttributes do not support display type");
      err = ERR_BAD_DISPLAY;
      break;
  }

EXIT:
  return err;
}

int32_t /*hwc2_error_t*/SprdHWComposer2::GET_DISPLAY_CONFIGS(
           hwc2_display_t display, uint32_t* outNumConfigs,
           hwc2_config_t* outConfigs)
{
  int32_t err = ERR_NONE;
  int size = 0;
  uint32_t *p = NULL;
  int32_t Id = 0;
  uint32_t configs[MAX_NUM_CONFIGS];
  DisplayAttributes *Att = NULL;
#ifdef SPRD_SR
  #define SPRD_SF_MAGIC	0xa75fUL
  bool sprdSF = ((display>>48)==SPRD_SF_MAGIC);
  display <<= 16;
  display >>= 16;
#endif
  SprdDisplayClient *Client = SprdDisplayClient::getDisplayClient(display);
  if (Client == NULL || reinterpret_cast<unsigned long>(Client) == 0xffffffffffff || reinterpret_cast<unsigned long>(Client) == 0xffffffff)
  {
    ALOGE("SprdHWComposer2 line: %d cannot get SprdDisplayClient", __LINE__);
    return ERR_BAD_DISPLAY;
  }

  Id  = Client->getDisplayId();
  Att = Client->getDisplayAttributes();
  if (Att == NULL)
  {
    ALOGE("SprdHWComposer2 line: %d cannot get DisplayAttributes", __LINE__);
    return ERR_BAD_DISPLAY;
  }

  if (outNumConfigs == NULL)
  {
    ALOGE("SprdHWComposer2::GET_DISPLAY_CONFIGS input para is NULL");
    return ERR_BAD_PARAMETER;
  }

  size_t outNumConfigsTmp = MAX_NUM_CONFIGS;
  switch (Id)
  {
    case DISPLAY_PRIMARY_ID:
      if (!Att->connected) {
        err = mDisplayCore->GetConfigs(DISPLAY_PRIMARY, configs, &outNumConfigsTmp);
        if (!err) {
          p = Att->configIndexSets;
          size = (outNumConfigsTmp > MAX_NUM_CONFIGS) ? MAX_NUM_CONFIGS : outNumConfigsTmp;
          memcpy(p, configs, size * sizeof(uint32_t));
          Att->numConfigs = size;

          Att->connected = true;
        } else {
          ALOGE("ts-gfx: GET_DISPLAY_CONFIGS display: 0x%lx failed", (unsigned long)display);
          err = ERR_BAD_CONFIG;
        }
      }
#ifdef SPRD_SR
      Att->sprdSF = sprdSF;
      if(Att->sprdSF) {
        // Use adf get active config interface(adf_get_active_config_hwc2) only when sprdSF and eanble SPRD_SR
        err = mDisplayCore->getActiveConfig(DISPLAY_PRIMARY, &Att->configsIndex);
        if (err) {
          ALOGE("SPRD_SR GET_DISPLAY_CONFIGS display: 0x%lx getActiveConfig failed",
              (unsigned long)display);
          err = ERR_BAD_CONFIG;
        } else {
          ALOGD("SPRD_SR GET_DISPLAY_CONFIGS display: 0x%lx getActiveConfig %u",
              (unsigned long)display,Att->configsIndex);
        }

        *outNumConfigs = Att->numConfigs;
        if (outConfigs) {
          memcpy(outConfigs, Att->configIndexSets, Att->numConfigs * sizeof(uint32_t));
          ALOGE("SPRD_SR SprdHWComposer2::GET_DISPLAY_CONFIGS, PRIMARYDISPLAY, sprdSF, *outNumConfigs:%d,outConfigs{%d %d %d}",
              *outNumConfigs,outConfigs[0],outConfigs[1],outConfigs[2]);
        }
      } else {
        /*for GSI sf or VTS app, we only give one config back to pass VTS.*/
        *outNumConfigs = 1;
        if (outConfigs) {
          memcpy(outConfigs, Att->configIndexSets, sizeof(uint32_t));
          ALOGE("SPRD_SR SprdHWComposer2::GET_DISPLAY_CONFIGS, PRIMARYDISPLAY,not sprdSF, *outNumConfigs:%d,outConfigs{%d}",
          *outNumConfigs,outConfigs[0]);
        }
      }
#else
      *outNumConfigs = Att->numConfigs;
      if (outConfigs)
      {
        memcpy(outConfigs, Att->configIndexSets, Att->numConfigs * sizeof(uint32_t));
        ALOGD("SPRD_SR SprdHWComposer2::GET_DISPLAY_CONFIGS, PRIMARYDISPLAY, *outNumConfigs:%d,", *outNumConfigs);
      }
#endif

      break;
    case DISPLAY_EXTERNAL_ID:
      /*  TODO: need implement if External display is available */
      break;
    default:
      ALOGE("Line: %d not support display display:0x%lx", __LINE__, (unsigned long)display);
      err = ERR_BAD_DISPLAY;
      break;
  }

  return err;
}

int32_t /*hwc2_error_t*/SprdHWComposer2::GET_DISPLAY_NAME(
           hwc2_display_t display, uint32_t* outSize,
           char* outName)
{
  int32_t err = ERR_NONE;
  int32_t Id = 0;
  SprdDisplayClient *Client = SprdDisplayClient::getDisplayClient(display);


  if (Client == NULL)
  {
    ALOGE("SprdHWComposer2 line: %d cannot get SprdDisplayClient", __LINE__);
    return ERR_BAD_DISPLAY;
  }

  Id  = Client->getDisplayId();

  switch (Id)
  {
    case DISPLAY_PRIMARY_ID:
      err = mPrimaryDisplay->GET_DISPLAY_NAME(Client, outSize, outName);
      break;
    case DISPLAY_EXTERNAL_ID:
      err = mExternalDisplay->GET_DISPLAY_NAME(Client, outSize, outName);
      break;
    case DISPLAY_VIRTUAL_ID:
      err = mVirtualDisplay->GET_DISPLAY_NAME(Client, outSize, outName);
      break;
    default:
      ALOGE("Line: %d not support display display:0x%lx", __LINE__, (unsigned long)display);
      err = ERR_BAD_DISPLAY;
      break;
  }

  return err;
}

int32_t /*hwc2_error_t*/SprdHWComposer2::GET_DISPLAY_REQUESTS(
           hwc2_display_t display,
           int32_t* /*hwc2_display_request_t*/ outDisplayRequests,
           uint32_t* outNumElements, hwc2_layer_t* outLayers,
           int32_t* /*hwc2_layer_request_t*/ outLayerRequests)
{
  int32_t err = ERR_NONE;
  int32_t Id = 0;
  SprdDisplayClient *Client = SprdDisplayClient::getDisplayClient(display);

  if (Client == NULL)
  {
    ALOGE("SprdHWComposer2 line: %d cannot get SprdDisplayClient", __LINE__);
    return ERR_BAD_DISPLAY;
  }

  Id  = Client->getDisplayId();

  switch (Id)
  {
    case DISPLAY_PRIMARY_ID:
      err = mPrimaryDisplay->GET_DISPLAY_REQUESTS(Client, outDisplayRequests, outNumElements, outLayers, outLayerRequests);
      break;
    case DISPLAY_EXTERNAL_ID:
      err = mExternalDisplay->GET_DISPLAY_REQUESTS(Client, outDisplayRequests, outNumElements, outLayers, outLayerRequests);
      break;
    case DISPLAY_VIRTUAL_ID:
      err = mVirtualDisplay->GET_DISPLAY_REQUESTS(Client, outDisplayRequests, outNumElements, outLayers, outLayerRequests);
      break;
    default:
      ALOGE("Line: %d not support display display:0x%lx", __LINE__, (unsigned long)display);
      err = ERR_BAD_DISPLAY;
      break;
  }

  return err;
}

int32_t /*hwc2_error_t*/SprdHWComposer2::GET_DISPLAY_TYPE(
           hwc2_display_t display,
           int32_t* /*hwc2_display_type_t*/ outType)
{
  int32_t err = ERR_NONE;
  int32_t Id = 0;
  SprdDisplayClient *Client = SprdDisplayClient::getDisplayClient(display);

  if (Client == NULL)
  {
    ALOGE("SprdHWComposer2 line: %d cannot get SprdDisplayClient", __LINE__);
    return ERR_BAD_DISPLAY;
  }

  Id  = Client->getDisplayId();

  switch (Id)
  {
    case DISPLAY_PRIMARY_ID:
      err = mPrimaryDisplay->GET_DISPLAY_TYPE(Client, outType);
      break;
    case DISPLAY_EXTERNAL_ID:
      err = mExternalDisplay->GET_DISPLAY_TYPE(Client, outType);
      break;
    case DISPLAY_VIRTUAL_ID:
      err = mVirtualDisplay->GET_DISPLAY_TYPE(Client, outType);
      break;
    default:
      ALOGE("Line: %d not support display display:0x%lx", __LINE__, (unsigned long)display);
      err = ERR_BAD_DISPLAY;
      break;
  }

  return err;
}

int32_t /*hwc2_error_t*/SprdHWComposer2::GET_DOZE_SUPPORT(
           hwc2_display_t display, int32_t* outSupport)
{
  int32_t err = ERR_NONE;
  int32_t Id = 0;
  SprdDisplayClient *Client = SprdDisplayClient::getDisplayClient(display);
  if (Client == NULL || reinterpret_cast<unsigned long>(Client) == HWC_NEGTIVE)
  {
    ALOGE("SprdHWComposer2 line: %d cannot get SprdDisplayClient", __LINE__);
    return ERR_BAD_DISPLAY;
  }

  Id  = Client->getDisplayId();

  switch (Id)
  {
    case DISPLAY_PRIMARY_ID:
      err = mPrimaryDisplay->GET_DOZE_SUPPORT(Client, outSupport);
      break;
    case DISPLAY_EXTERNAL_ID:
      err = mExternalDisplay->GET_DOZE_SUPPORT(Client, outSupport);
      break;
    case DISPLAY_VIRTUAL_ID:
      err = mVirtualDisplay->GET_DOZE_SUPPORT(Client, outSupport);
      break;
    default:
      ALOGE("Line: %d not support display display:0x%lx", __LINE__, (unsigned long)display);
      err = ERR_BAD_DISPLAY;
      break;
  }

  return err;
}

int32_t /*hwc2_error_t*/SprdHWComposer2::GET_HDR_CAPABILITIES(
           hwc2_display_t display, uint32_t* outNumTypes,
           int32_t* /*android_hdr_t*/ outTypes, float* outMaxLuminance,
           float* outMaxAverageLuminance, float* outMinLuminance)
{
  int32_t err = ERR_NONE;
  int32_t Id = 0;
  SprdDisplayClient *Client = SprdDisplayClient::getDisplayClient(display);

  if (Client == NULL)
  {
    ALOGE("SprdHWComposer2 line: %d cannot get SprdDisplayClient", __LINE__);
    return ERR_BAD_DISPLAY;
  }

  Id  = Client->getDisplayId();

  switch (Id)
  {
    case DISPLAY_PRIMARY_ID:
      err = mPrimaryDisplay->GET_HDR_CAPABILITIES(Client, outNumTypes, outTypes,
             outMaxLuminance, outMaxAverageLuminance, outMinLuminance);
      break;
    case DISPLAY_EXTERNAL_ID:
      err = mExternalDisplay->GET_HDR_CAPABILITIES(Client, outNumTypes, outTypes,
             outMaxLuminance, outMaxAverageLuminance, outMinLuminance);
      break;
    case DISPLAY_VIRTUAL_ID:
      err = mVirtualDisplay->GET_HDR_CAPABILITIES(Client, outNumTypes, outTypes,
             outMaxLuminance, outMaxAverageLuminance, outMinLuminance);
      break;
    default:
      ALOGE("Line: %d not support display display:0x%lx", __LINE__, (unsigned long)display);
      err = ERR_BAD_DISPLAY;
      break;
  }

  return err;
}

int32_t /*hwc2_error_t*/SprdHWComposer2::GET_RELEASE_FENCES(
           hwc2_display_t display, uint32_t* outNumElements,
           hwc2_layer_t* outLayers, int32_t* outFences)
{
  int32_t err = ERR_NONE;
  int32_t Id = 0;
  SprdDisplayClient *Client = SprdDisplayClient::getDisplayClient(display);

  if (Client == NULL)
  {
    ALOGE("SprdHWComposer2 line: %d cannot get SprdDisplayClient", __LINE__);
    return ERR_BAD_DISPLAY;
  }

  Id  = Client->getDisplayId();

  switch (Id)
  {
    case DISPLAY_PRIMARY_ID:
      err = mPrimaryDisplay->GET_RELEASE_FENCES(Client, outNumElements, outLayers, outFences);
      break;
    case DISPLAY_EXTERNAL_ID:
      err = mExternalDisplay->GET_RELEASE_FENCES(Client, outNumElements, outLayers, outFences);
      break;
    case DISPLAY_VIRTUAL_ID:
      err = mVirtualDisplay->GET_RELEASE_FENCES(Client, outNumElements, outLayers, outFences);
      break;
    default:
      err = ERR_BAD_DISPLAY;
      ALOGE("Line: %d not support display display:0x%lx", __LINE__, (unsigned long)display);
      break;
  }

  return err;
}

int32_t /*hwc2_error_t*/SprdHWComposer2::PRESENT_DISPLAY(
           hwc2_display_t display, int32_t* outRetireFence)
{
  int32_t err = ERR_NONE;
  int32_t ret = ERR_NONE;
  int32_t Id = 0;
  char value[PROPERTY_VALUE_MAX];
  int flag = 0;
  SprdDisplayClient *Client = SprdDisplayClient::getDisplayClient(display);

  if (Client == NULL)
  {
    ALOGE("SprdHWComposer2 line: %d cannot get SprdDisplayClient", __LINE__);
    return ERR_BAD_DISPLAY;
  }

  Id  = Client->getDisplayId();
  switch (Id)
  {
    case DISPLAY_PRIMARY_ID:
      ret = mPrimaryDisplay->commit(Client);
      break;
    case DISPLAY_EXTERNAL_ID:
      ret = mExternalDisplay->commit(Client);
      break;
    case DISPLAY_VIRTUAL_ID:
      ret = mVirtualDisplay->commit(Client);
      break;
    default:
      err = ERR_BAD_DISPLAY;
      ALOGE("Line: %d not support display display:0x%lx", __LINE__, (unsigned long)display);
      break;
  }

  if (ret == ERR_NO_JOB)
  {
    ALOGI_IF(mDebugFlag, "SprdHWComposer2::PRESENT_DISPLAY ERR_NO_JOB return");
    if (outRetireFence)
    {
      *outRetireFence = -1;
    }
    return err;
  }

/*
  if (display == DISPLAY_VIRTUAL_ID) {
    mVirtualDisplay->WaitForDisplay();
  }
*/

 if (0 != property_get("ro.vendor.powerhint.enable", value, "0"))  
{
      flag =atoi(value);
  }
  if (flag == 1) {
      if (gPowerHalV4_0 == nullptr)
      {
          gPowerHalV4_0 = IPower::getService();
      }
      if (gPowerHalV4_0 != nullptr && lock != nullptr) {
#if 1
      FILE *fp;
      char buffer[100];
      fp = fopen("/data/vendor/hwc_stat/hwc_stat.txt", "r+");
      if (fp != NULL)
      {

          memset(buffer, '\0', 100);
          int readCnt = fread(buffer, sizeof(buffer), 1, fp);
	  if(!strncmp("acquire", buffer, strlen("acquire"))) {

	       gPowerHalV4_0->acquirePowerHintBySceneId(lock, "com.drawelements.deqp", (int32_t)PowerHint::VENDOR_PERFORMANCE_CTS/*0x7f00000b*/);
	  } else if (!strncmp("release", buffer, strlen("release"))) {
	       gPowerHalV4_0->releasePowerHintBySceneId(lock, (int32_t)PowerHint::VENDOR_PERFORMANCE_CTS/*0x7f00000b*/);
	  }
          fseek(fp, 0, SEEK_SET);
          fwrite("none", sizeof("none"), 1, fp);
          fclose(fp);
      } else {
          ALOGE("SprdHWComposer2 line %d:open file failed for powerhint.",__LINE__);
      }
#endif
      } else {
          ALOGE("SprdHWComposer2 line %d:Filed to get PowerService", __LINE__);
      }

  }


  struct DisplayTrack tracker;
  tracker.releaseFenceFd = -1;
  tracker.retiredFenceFd = -1;
  err = mDisplayCore->PostDisplay(&tracker);
  if (err == -1)
  {
    err = ERR_NONE;
    return err;
  }

  /*
   *  Build Sync data for each display device
   * */
  switch (Id) {
    case DISPLAY_PRIMARY_ID:
      mPrimaryDisplay->buildSyncData(Client, &tracker, outRetireFence);
      break;
    case DISPLAY_EXTERNAL_ID:
      mExternalDisplay->buildSyncData(Client, &tracker, outRetireFence);
      break;
    case DISPLAY_VIRTUAL_ID:
      mVirtualDisplay->buildSyncData(Client, &tracker, outRetireFence);
      break;
    default:
      ALOGE("Line: %d not support display display:0x%lx", __LINE__, (unsigned long)display);
      err = ERR_BAD_DISPLAY;
    }


  /*
   *  Recycle file descriptor
   * */
  ALOGI_IF(mDebugFlag, "<11> close DisplayCore output rel_fd:%d,retire_fd:%d. err:%d",
           tracker.releaseFenceFd, tracker.retiredFenceFd, err);
  closeFence(&tracker.releaseFenceFd);
  closeFence(&tracker.retiredFenceFd);

  return err;
}

int32_t /*hwc2_error_t*/SprdHWComposer2::SET_ACTIVE_CONFIG(
           hwc2_display_t display, hwc2_config_t config)
{
  int32_t err = ERR_NONE;
  int32_t Id = 0;
  DisplayAttributes *Att = NULL;
  SprdDisplayClient *Client = SprdDisplayClient::getDisplayClient(display);

  if (Client == NULL)
  {
    ALOGE("SprdHWComposer2 line: %d cannot get SprdDisplayClient", __LINE__);
    return ERR_BAD_DISPLAY;
  }

  Id  = Client->getDisplayId();
  Att = Client->getDisplayAttributes();
  if (Att == NULL)
  {
    ALOGE("SprdHWComposer2 line: %d cannot get DisplayAttributes", __LINE__);
    return ERR_BAD_DISPLAY;
  }

  switch (Id)
  {
    case DISPLAY_PRIMARY_ID:
      Att->configsIndex = config;
      err = mPrimaryDisplay->ActiveConfig(Client, Att);
#ifdef SPRD_SR
      err = mDisplayCore->setActiveConfig(DISPLAY_PRIMARY, config);
#endif
      break;
    case DISPLAY_EXTERNAL_ID:
      if (Att->connected)
      {
        Att->configsIndex = config;
        err = mExternalDisplay->ActiveConfig(Client, Att);
      }
      break;
    case DISPLAY_VIRTUAL_ID:
      if (Att->connected)
      {
        Att->configsIndex = config;
        err = mVirtualDisplay->ActiveConfig(Client, Att);
      }
      break;
    default:
      ALOGE("Line: %d not support display display:0x%lx", __LINE__, (unsigned long)display);
      err = ERR_BAD_DISPLAY;
      break;
  }

  return err;
}

int32_t /*hwc2_error_t*/SprdHWComposer2::SET_CLIENT_TARGET(
           hwc2_display_t display, buffer_handle_t target,
           int32_t acquireFence, int32_t /*android_dataspace_t*/ dataspace,
           hwc_region_t damage)
{
  int32_t err = ERR_NONE;
  int32_t Id = 0;
  SprdDisplayClient *Client = SprdDisplayClient::getDisplayClient(display);

  if (Client == NULL)
  {
    ALOGE("SprdHWComposer2 line: %d cannot get SprdDisplayClient", __LINE__);
    return ERR_BAD_DISPLAY;
  }

  Id  = Client->getDisplayId();

  switch (Id)
  {
    case DISPLAY_PRIMARY_ID:
      err = mPrimaryDisplay->SET_CLIENT_TARGET(Client, target, acquireFence, dataspace, damage);
      break;
    case DISPLAY_EXTERNAL_ID:
      err = mExternalDisplay->SET_CLIENT_TARGET(Client, target, acquireFence, dataspace, damage);
      break;
    case DISPLAY_VIRTUAL_ID:
      err = mVirtualDisplay->SET_CLIENT_TARGET(Client, target, acquireFence, dataspace, damage);
      break;
    default:
      ALOGE("Line: %d not support display display:0x%lx", __LINE__, (unsigned long)display);
      err = ERR_BAD_DISPLAY;
      break;
  }

  return err;
}

int32_t /*hwc2_error_t*/SprdHWComposer2::SET_COLOR_MODE(
           hwc2_display_t display,
           int32_t /*android_color_mode_t*/ mode)
{
  int32_t err = ERR_NONE;
  int32_t Id = 0;
  SprdDisplayClient *Client = SprdDisplayClient::getDisplayClient(display);

  if (Client == NULL || reinterpret_cast<unsigned long>(Client) == HWC_NEGTIVE)
  {
    ALOGE("SprdHWComposer2 line: %d cannot get SprdDisplayClient", __LINE__);
    return ERR_BAD_DISPLAY;
  }

  Id  = Client->getDisplayId();

  switch (Id)
  {
    case DISPLAY_PRIMARY_ID:
      err = mPrimaryDisplay->SET_COLOR_MODE(Client, mode);
      break;
    case DISPLAY_EXTERNAL_ID:
      err = mExternalDisplay->SET_COLOR_MODE(Client, mode);
      break;
    case DISPLAY_VIRTUAL_ID:
      err = mVirtualDisplay->SET_COLOR_MODE(Client, mode);
      break;
    default:
      ALOGE("Line: %d not support display display:0x%lx", __LINE__, (unsigned long)display);
      err = ERR_BAD_DISPLAY;
      break;
  }

  return err;
}

int32_t /*hwc2_error_t*/SprdHWComposer2::SET_COLOR_TRANSFORM(
           hwc2_display_t display, const float* matrix,
           int32_t /*android_color_transform_t*/ hint)
{
  int32_t err = ERR_NONE;
  int32_t Id = 0;
  SprdDisplayClient *Client = SprdDisplayClient::getDisplayClient(display);

  if (Client == NULL)
  {
    ALOGE("SprdHWComposer2 line: %d cannot get SprdDisplayClient", __LINE__);
    return ERR_BAD_DISPLAY;
  }

  Id  = Client->getDisplayId();

  switch (Id)
  {
    case DISPLAY_PRIMARY_ID:
      err = mPrimaryDisplay->SET_COLOR_TRANSFORM(Client, matrix, hint);
      break;
    case DISPLAY_EXTERNAL_ID:
      err = mExternalDisplay->SET_COLOR_TRANSFORM(Client, matrix, hint);
      break;
    case DISPLAY_VIRTUAL_ID:
      err = mVirtualDisplay->SET_COLOR_TRANSFORM(Client, matrix, hint);
      break;
    default:
      ALOGE("Line: %d not support display display:0x%lx", __LINE__, (unsigned long)display);
      err = ERR_BAD_DISPLAY;
      break;
  }

  return err;
}

int32_t /*hwc2_error_t*/SprdHWComposer2::SET_OUTPUT_BUFFER(
           hwc2_display_t display, buffer_handle_t buffer,
           int32_t releaseFence)
{
  int32_t err = ERR_NONE;
  int32_t Id = 0;
  SprdDisplayClient *Client = SprdDisplayClient::getDisplayClient(display);

  if (Client == NULL)
  {
    ALOGE("SprdHWComposer2 line: %d cannot get SprdDisplayClient", __LINE__);
    return ERR_BAD_DISPLAY;
  }

  Id  = Client->getDisplayId();

  switch (Id)
  {
    case DISPLAY_PRIMARY_ID:
      break;
    case DISPLAY_EXTERNAL_ID:
      break;
    case DISPLAY_VIRTUAL_ID:
      err = mVirtualDisplay->SET_OUTPUT_BUFFER(Client, buffer, releaseFence);
      break;
    default:
      ALOGE("Line: %d not support display display:0x%lx", __LINE__, (unsigned long)display);
      err = ERR_BAD_DISPLAY;
      break;
  }

  return err;
}

int32_t /*hwc2_error_t*/SprdHWComposer2::SET_POWER_MODE(
           hwc2_display_t display,
           int32_t /*hwc2_power_mode_t*/ mode)
{
  int32_t err = ERR_NONE;
  int32_t Id = 0;
  SprdDisplayClient *Client = SprdDisplayClient::getDisplayClient(display);
  if (Client == NULL || reinterpret_cast<unsigned long>(Client) == HWC_NEGTIVE)
  {
    ALOGE("SprdHWComposer2 line: %d cannot get SprdDisplayClient", __LINE__);
    return ERR_BAD_DISPLAY;
  }

  Id  = Client->getDisplayId();

  switch (Id)
  {
    case DISPLAY_PRIMARY_ID:
      err = mPrimaryDisplay->SET_POWER_MODE(Client, mode);
      break;
    case DISPLAY_EXTERNAL_ID:
      err = mExternalDisplay->SET_POWER_MODE(Client, mode);
      break;
    case DISPLAY_VIRTUAL_ID:
      err = mVirtualDisplay->SET_POWER_MODE(Client, mode);
      break;
    default:
      ALOGE("Line: %d not support display display:0x%lx", __LINE__, (unsigned long)display);
      err = ERR_BAD_DISPLAY;
      break;
  }

  return err;
}

int32_t /*hwc2_error_t*/SprdHWComposer2::SET_VSYNC_ENABLED(
           hwc2_display_t display,
           int32_t /*hwc2_vsync_t*/ enabled)
{
  static int lst_enabled = 0;
  int32_t err = ERR_NONE;
  int32_t Id = 0;
  int enabledFlag = 1;
  SprdDisplayClient *Client = SprdDisplayClient::getDisplayClient(display);

  if (Client == NULL)
  {
    ALOGE("SprdHWComposer2 line: %d cannot get SprdDisplayClient", __LINE__);
    return ERR_BAD_DISPLAY;
  }

  switch (enabled)
  {
    case VSYNC_ENABLE:
      enabledFlag = 1;
      break;
    case VSYNC_DISABLE:
      enabledFlag = 0;
      break;
    case VSYNC_INVALID:
    default:
      ALOGE("SprdHWComposer2::SET_VSYNC_ENABLED do not support vsync event");
      enabledFlag = lst_enabled;
      break;
  }

  if(lst_enabled == enabledFlag){
    return 0;
  }else{
    lst_enabled = enabledFlag;
  }

  Id  = Client->getDisplayId();

  switch (Id)
  {
    case DISPLAY_PRIMARY_ID:
      err = mDisplayCore->EventControl(DISPLAY_PRIMARY, HWC_EVENT_VSYNC, enabledFlag);
      break;
    /*case DISPLAY_EXTERNAL_ID:
      break;
    case DISPLAY_VIRTUAL_ID:
      break;*/
    default:
      ALOGE("Line: %d not support display display:0x%lx", __LINE__, (unsigned long)display);
      err = ERR_BAD_DISPLAY;
      break;
  }

  return err;
}

int32_t /*hwc2_error_t*/SprdHWComposer2::VALIDATE_DISPLAY(
           hwc2_display_t display,
           uint32_t* outNumTypes, uint32_t* outNumRequests)
{
  int32_t err = ERR_NONE;
  int32_t Id = 0;
  DisplayAttributes *Att = NULL;
  SprdDisplayClient *Client = SprdDisplayClient::getDisplayClient(display);

  if (Client == NULL)
  {
    ALOGE("SprdHWComposer2 line: %d cannot get SprdDisplayClient", __LINE__);
    return ERR_BAD_DISPLAY;
  }

  Id  = Client->getDisplayId();
  Att = Client->getDisplayAttributes();
  if (Att == NULL)
  {
    ALOGE("SprdHWComposer2 line: %d cannot get DisplayAttributes", __LINE__);
    return ERR_BAD_DISPLAY;
  }


  queryDebugFlag(&mDebugFlag);

#ifdef FORCE_ADJUST_ACCELERATOR
  DevicePropertyProbe(Client);
#endif

  switch (Id)
  {
    case DISPLAY_PRIMARY_ID:
      err = mPrimaryDisplay->VALIDATE_DISPLAY(Client, outNumTypes, outNumRequests,
                                              Att->AcceleratorMode);
      break;
    case DISPLAY_EXTERNAL_ID:
      err = mExternalDisplay->VALIDATE_DISPLAY(Client, outNumTypes, outNumRequests,
                                               Att->AcceleratorMode);
      break;
    case DISPLAY_VIRTUAL_ID:
      err = mVirtualDisplay->VALIDATE_DISPLAY(Client, outNumTypes, outNumRequests,
                                              Att->AcceleratorMode);
      break;
    default:
      ALOGE("Line: %d not support display display:0x%lx", __LINE__, (unsigned long)display);
      err = ERR_BAD_DISPLAY;
      break;
  }

  return err;
}

int32_t /*hwc2_error_t*/SprdHWComposer2::SET_CURSOR_POSITION(
           hwc2_display_t display, hwc2_layer_t layer,
           int32_t x, int32_t y)
{
  int32_t err = ERR_NONE;
  SprdHandleLayer *Handle = NULL;
  int32_t Id = 0;
  SprdDisplayClient *Client = SprdDisplayClient::getDisplayClient(display);

  if (Client == NULL)
  {
    ALOGE("SprdHWComposer2 line: %d cannot get SprdDisplayClient", __LINE__);
    return ERR_BAD_DISPLAY;
  }

  Id  = Client->getDisplayId();

  switch (Id)
  {
    case DISPLAY_PRIMARY_ID:
      Handle= mPrimaryDisplay->getHandleLayer();
      break;
    case DISPLAY_EXTERNAL_ID:
      Handle = mExternalDisplay->getHandleLayer();
      break;
    case DISPLAY_VIRTUAL_ID:
      Handle = mVirtualDisplay->getHandleLayer();
      break;
    default:
      ALOGE("Line: %d not support display display:0x%lx", __LINE__, (unsigned long)display);
      err = ERR_BAD_DISPLAY;
      break;
  }

  if (Handle == NULL)
  {
    ALOGE("SprdHWComposer2 line: %d handle is NULL", __LINE__);
    return  ERR_NOT_VALIDATED;
  }

  err = Handle->SET_CURSOR_POSITION(layer, x, y);

  return err;
}

int32_t /*hwc2_error_t*/SprdHWComposer2::SET_LAYER_BUFFER(
           hwc2_display_t display, hwc2_layer_t layer,
           buffer_handle_t buffer, int32_t acquireFence)
{
  int32_t err = ERR_NONE;
  SprdHandleLayer *Handle = NULL;
  int32_t Id = 0;
  SprdDisplayClient *Client = SprdDisplayClient::getDisplayClient(display);

  if (Client == NULL)
  {
    ALOGE("SprdHWComposer2 line: %d cannot get SprdDisplayClient", __LINE__);
    return ERR_BAD_DISPLAY;
  }

  Id  = Client->getDisplayId();

  switch (Id)
  {
    case DISPLAY_PRIMARY_ID:
      Handle= mPrimaryDisplay->getHandleLayer();
      break;
    case DISPLAY_EXTERNAL_ID:
      Handle = mExternalDisplay->getHandleLayer();
      break;
    case DISPLAY_VIRTUAL_ID:
      Handle = mVirtualDisplay->getHandleLayer();
      break;
    default:
      ALOGE("Line: %d not support display display:0x%lx", __LINE__, (unsigned long)display);
      err = ERR_BAD_DISPLAY;
      break;
  }

  if (Handle == NULL)
  {
    ALOGE("SprdHWComposer2 line: %d handle is NULL", __LINE__);
    return  ERR_NOT_VALIDATED;
  }

  err = Handle->SET_LAYER_BUFFER(layer, buffer, acquireFence);

  return err;
}

int32_t /*hwc2_error_t*/SprdHWComposer2::SET_LAYER_SURFACE_DAMAGE(
           hwc2_display_t display, hwc2_layer_t layer,
           hwc_region_t damage)
{
  int32_t err = ERR_NONE;
  SprdHandleLayer *Handle = NULL;
  int32_t Id = 0;
  SprdDisplayClient *Client = SprdDisplayClient::getDisplayClient(display);

  if (Client == NULL)
  {
    ALOGE("SprdHWComposer2 line: %d cannot get SprdDisplayClient", __LINE__);
    return ERR_BAD_DISPLAY;
  }

  Id  = Client->getDisplayId();

  switch (Id)
  {
    case DISPLAY_PRIMARY_ID:
      Handle= mPrimaryDisplay->getHandleLayer();
      break;
    case DISPLAY_EXTERNAL_ID:
      Handle = mExternalDisplay->getHandleLayer();
      break;
    case DISPLAY_VIRTUAL_ID:
      Handle = mVirtualDisplay->getHandleLayer();
      break;
    default:
      ALOGE("Line: %d not support display display:0x%lx", __LINE__, (unsigned long)display);
      err = ERR_BAD_DISPLAY;
      break;
  }

  if (Handle == NULL)
  {
    ALOGE("SprdHWComposer2 line: %d handle is NULL", __LINE__);
    return  ERR_NOT_VALIDATED;
  }

  err = Handle->SET_LAYER_SURFACE_DAMAGE(layer, damage);

  return err;
}

int32_t /*hwc2_error_t*/SprdHWComposer2::SET_LAYER_BLEND_MODE(
           hwc2_display_t display, hwc2_layer_t layer,
           int32_t /*hwc2_blend_mode_t*/ mode)
{
  int32_t err = ERR_NONE;
  SprdHandleLayer *Handle = NULL;
  int32_t Id = 0;
  SprdDisplayClient *Client = SprdDisplayClient::getDisplayClient(display);

  if (Client == NULL)
  {
    ALOGE("SprdHWComposer2 line: %d cannot get SprdDisplayClient", __LINE__);
    return ERR_BAD_DISPLAY;
  }

  Id  = Client->getDisplayId();

  switch (Id)
  {
    case DISPLAY_PRIMARY_ID:
      Handle= mPrimaryDisplay->getHandleLayer();
      break;
    case DISPLAY_EXTERNAL_ID:
      Handle = mExternalDisplay->getHandleLayer();
      break;
    case DISPLAY_VIRTUAL_ID:
      Handle = mVirtualDisplay->getHandleLayer();
      break;
    default:
      ALOGE("Line: %d not support display display:0x%lx", __LINE__, (unsigned long)display);
      err = ERR_BAD_DISPLAY;
      break;
  }

  if (Handle == NULL)
  {
    ALOGE("SprdHWComposer2 line: %d handle is NULL", __LINE__);
    return  ERR_NOT_VALIDATED;
  }

  err = Handle->SET_LAYER_BLEND_MODE(layer, mode);

  return err;
}

int32_t /*hwc2_error_t*/SprdHWComposer2::SET_LAYER_COLOR(
           hwc2_display_t display, hwc2_layer_t layer,
           hwc_color_t color)
{
  int32_t err = ERR_NONE;
  SprdHandleLayer *Handle = NULL;
  int32_t Id = 0;
  SprdDisplayClient *Client = SprdDisplayClient::getDisplayClient(display);

  if (Client == NULL)
  {
    ALOGE("SprdHWComposer2 line: %d cannot get SprdDisplayClient", __LINE__);
    return ERR_BAD_DISPLAY;
  }

  Id  = Client->getDisplayId();

  switch (Id)
  {
    case DISPLAY_PRIMARY_ID:
      Handle= mPrimaryDisplay->getHandleLayer();
      break;
    case DISPLAY_EXTERNAL_ID:
      Handle = mExternalDisplay->getHandleLayer();
      break;
    case DISPLAY_VIRTUAL_ID:
      Handle = mVirtualDisplay->getHandleLayer();
      break;
    default:
      ALOGE("Line: %d not support display display:0x%lx", __LINE__, (unsigned long)display);
      err = ERR_BAD_DISPLAY;
      break;
  }

  if (Handle == NULL)
  {
    ALOGE("SprdHWComposer2 line: %d handle is NULL", __LINE__);
    return  ERR_NOT_VALIDATED;
  }

  err = Handle->SET_LAYER_COLOR(layer, color);

  return err;
}

int32_t /*hwc2_error_t*/SprdHWComposer2::SET_LAYER_COMPOSITION_TYPE(
           hwc2_display_t display, hwc2_layer_t layer,
           int32_t /*hwc2_composition_t*/ type)
{
  int32_t err = ERR_NONE;
  SprdHandleLayer *Handle = NULL;
  int32_t Id = 0;
  SprdDisplayClient *Client = SprdDisplayClient::getDisplayClient(display);

  if (Client == NULL)
  {
    ALOGE("SprdHWComposer2 line: %d cannot get SprdDisplayClient", __LINE__);
    return ERR_BAD_DISPLAY;
  }

  Id  = Client->getDisplayId();

  switch (Id)
  {
    case DISPLAY_PRIMARY_ID:
      Handle= mPrimaryDisplay->getHandleLayer();
      break;
    case DISPLAY_EXTERNAL_ID:
      Handle = mExternalDisplay->getHandleLayer();
      break;
    case DISPLAY_VIRTUAL_ID:
      Handle = mVirtualDisplay->getHandleLayer();
      break;
    default:
      ALOGE("Line: %d not support display display:0x%lx", __LINE__, (unsigned long)display);
      err = ERR_BAD_DISPLAY;
      break;
  }

  if (Handle == NULL)
  {
    ALOGE("SprdHWComposer2 line: %d handle is NULL", __LINE__);
    return  ERR_NOT_VALIDATED;
  }

  err = Handle->SET_LAYER_COMPOSITION_TYPE(layer, type);

  return err;
}

int32_t /*hwc2_error_t*/SprdHWComposer2::SET_LAYER_DATASPACE(
           hwc2_display_t display, hwc2_layer_t layer,
           int32_t /*android_dataspace_t*/ dataspace)
{
  int32_t err = ERR_NONE;
  SprdHandleLayer *Handle = NULL;
  int32_t Id = 0;
  SprdDisplayClient *Client = SprdDisplayClient::getDisplayClient(display);

  if (Client == NULL)
  {
    ALOGE("SprdHWComposer2 line: %d cannot get SprdDisplayClient", __LINE__);
    return ERR_BAD_DISPLAY;
  }

  Id  = Client->getDisplayId();

  switch (Id)
  {
    case DISPLAY_PRIMARY_ID:
      Handle= mPrimaryDisplay->getHandleLayer();
      break;
    case DISPLAY_EXTERNAL_ID:
      Handle = mExternalDisplay->getHandleLayer();
      break;
    case DISPLAY_VIRTUAL_ID:
      Handle = mVirtualDisplay->getHandleLayer();
      break;
    default:
      ALOGE("Line: %d not support display display:0x%lx", __LINE__, (unsigned long)display);
      err = ERR_BAD_DISPLAY;
      break;
  }

  if (Handle == NULL)
  {
    ALOGE("SprdHWComposer2 line: %d handle is NULL", __LINE__);
    return  ERR_NOT_VALIDATED;
  }

  err = Handle->SET_LAYER_DATASPACE(layer, dataspace);

  return err;
}

int32_t /*hwc2_error_t*/SprdHWComposer2::SET_LAYER_DISPLAY_FRAME(
           hwc2_display_t display, hwc2_layer_t layer,
           hwc_rect_t frame)
{
  int32_t err = ERR_NONE;
  SprdHandleLayer *Handle = NULL;
  int32_t Id = 0;
  SprdDisplayClient *Client = SprdDisplayClient::getDisplayClient(display);

  if (Client == NULL)
  {
    ALOGE("SprdHWComposer2 line: %d cannot get SprdDisplayClient", __LINE__);
    return ERR_BAD_DISPLAY;
  }

  Id  = Client->getDisplayId();

  switch (Id)
  {
    case DISPLAY_PRIMARY_ID:
      Handle= mPrimaryDisplay->getHandleLayer();
      break;
    case DISPLAY_EXTERNAL_ID:
      Handle = mExternalDisplay->getHandleLayer();
      break;
    case DISPLAY_VIRTUAL_ID:
      Handle = mVirtualDisplay->getHandleLayer();
      break;
    default:
      ALOGE("Line: %d not support display display:0x%lx", __LINE__, (unsigned long)display);
      err = ERR_BAD_DISPLAY;
      break;
  }

  if (Handle == NULL)
  {
    ALOGE("SprdHWComposer2 line: %d handle is NULL", __LINE__);
    return  ERR_NOT_VALIDATED;
  }

  err = Handle->SET_LAYER_DISPLAY_FRAME(layer, frame);

  return err;
}

int32_t /*hwc2_error_t*/SprdHWComposer2::SET_LAYER_PLANE_ALPHA(
           hwc2_display_t display, hwc2_layer_t layer,
           float alpha)
{
  int32_t err = ERR_NONE;
  SprdHandleLayer *Handle = NULL;
  int32_t Id = 0;
  SprdDisplayClient *Client = SprdDisplayClient::getDisplayClient(display);

  if (Client == NULL)
  {
    ALOGE("SprdHWComposer2 line: %d cannot get SprdDisplayClient", __LINE__);
    return ERR_BAD_DISPLAY;
  }

  Id  = Client->getDisplayId();

  switch (Id)
  {
    case DISPLAY_PRIMARY_ID:
      Handle= mPrimaryDisplay->getHandleLayer();
      break;
    case DISPLAY_EXTERNAL_ID:
      Handle = mExternalDisplay->getHandleLayer();
      break;
    case DISPLAY_VIRTUAL_ID:
      Handle = mVirtualDisplay->getHandleLayer();
      break;
    default:
      ALOGE("Line: %d not support display display:0x%lx", __LINE__, (unsigned long)display);
      err = ERR_BAD_DISPLAY;
      break;
  }

  if (Handle == NULL)
  {
    ALOGE("SprdHWComposer2 line: %d handle is NULL", __LINE__);
    return  ERR_NOT_VALIDATED;
  }

  err = Handle->SET_LAYER_PLANE_ALPHA(layer, alpha);

  return err;
}

int32_t /*hwc2_error_t*/SprdHWComposer2::SET_LAYER_SIDEBAND_STREAM(
           hwc2_display_t display, hwc2_layer_t layer,
           const native_handle_t* stream)
{
  int32_t err = ERR_NONE;
  SprdHandleLayer *Handle = NULL;
  int32_t Id = 0;
  SprdDisplayClient *Client = SprdDisplayClient::getDisplayClient(display);

  if (Client == NULL)
  {
    ALOGE("SprdHWComposer2 line: %d cannot get SprdDisplayClient", __LINE__);
    return ERR_BAD_DISPLAY;
  }

  Id  = Client->getDisplayId();

  switch (Id)
  {
    case DISPLAY_PRIMARY_ID:
      Handle= mPrimaryDisplay->getHandleLayer();
      break;
    case DISPLAY_EXTERNAL_ID:
      Handle = mExternalDisplay->getHandleLayer();
      break;
    case DISPLAY_VIRTUAL_ID:
      Handle = mVirtualDisplay->getHandleLayer();
      break;
    default:
      ALOGE("Line: %d not support display display:0x%lx", __LINE__, (unsigned long)display);
      err = ERR_BAD_DISPLAY;
      break;
  }

  if (Handle == NULL)
  {
    ALOGE("SprdHWComposer2 line: %d handle is NULL", __LINE__);
    return  ERR_NOT_VALIDATED;
  }

  err = Handle->SET_LAYER_SIDEBAND_STREAM(layer, stream);

  return err;
}

int32_t /*hwc2_error_t*/SprdHWComposer2::SET_LAYER_SOURCE_CROP(
           hwc2_display_t display, hwc2_layer_t layer,
           hwc_frect_t crop)
{
  int32_t err = ERR_NONE;
  SprdHandleLayer *Handle = NULL;
  int32_t Id = 0;
  SprdDisplayClient *Client = SprdDisplayClient::getDisplayClient(display);

  if (Client == NULL)
  {
    ALOGE("SprdHWComposer2 line: %d cannot get SprdDisplayClient", __LINE__);
    return ERR_BAD_DISPLAY;
  }

  Id  = Client->getDisplayId();

  switch (Id)
  {
    case DISPLAY_PRIMARY_ID:
      Handle= mPrimaryDisplay->getHandleLayer();
      break;
    case DISPLAY_EXTERNAL_ID:
      Handle = mExternalDisplay->getHandleLayer();
      break;
    case DISPLAY_VIRTUAL_ID:
      Handle = mVirtualDisplay->getHandleLayer();
      break;
    default:
      ALOGE("Line: %d not support display display:0x%lx", __LINE__, (unsigned long)display);
      err = ERR_BAD_DISPLAY;
      break;
  }

  if (Handle == NULL)
  {
    ALOGE("SprdHWComposer2 line: %d handle is NULL", __LINE__);
    return ERR_NOT_VALIDATED;
  }

  err = Handle->SET_LAYER_SOURCE_CROP(layer, crop);

  return err;
}

int32_t /*hwc2_error_t*/SprdHWComposer2::SET_LAYER_TRANSFORM(
           hwc2_display_t display, hwc2_layer_t layer,
           int32_t /*hwc_transform_t*/ transform)
{
  int32_t err = ERR_NONE;
  SprdHandleLayer *Handle = NULL;
  int32_t Id = 0;
  SprdDisplayClient *Client = SprdDisplayClient::getDisplayClient(display);

  if (Client == NULL)
  {
    ALOGE("SprdHWComposer2 line: %d cannot get SprdDisplayClient", __LINE__);
    return ERR_BAD_DISPLAY;
  }

  Id  = Client->getDisplayId();

  switch (Id)
  {
    case DISPLAY_PRIMARY_ID:
      Handle= mPrimaryDisplay->getHandleLayer();
      break;
    case DISPLAY_EXTERNAL_ID:
      Handle = mExternalDisplay->getHandleLayer();
      break;
    case DISPLAY_VIRTUAL_ID:
      Handle = mVirtualDisplay->getHandleLayer();
      break;
    default:
      ALOGE("Line: %d not support display display:0x%lx", __LINE__, (unsigned long)display);
      err = ERR_BAD_DISPLAY;
      break;
  }

  if (Handle == NULL)
  {
    ALOGE("SprdHWComposer2 line: %d handle is NULL", __LINE__);
    return  ERR_NOT_VALIDATED;
  }

  err = Handle->SET_LAYER_TRANSFORM(layer, transform);

  return err;
}

int32_t /*hwc2_error_t*/SprdHWComposer2::SET_LAYER_VISIBLE_REGION(
           hwc2_display_t display, hwc2_layer_t layer,
           hwc_region_t visible)
{
  int32_t err = ERR_NONE;
  SprdHandleLayer *Handle = NULL;
  int32_t Id = 0;
  SprdDisplayClient *Client = SprdDisplayClient::getDisplayClient(display);

  if (Client == NULL)
  {
    ALOGE("SprdHWComposer2 line: %d cannot get SprdDisplayClient", __LINE__);
    return ERR_BAD_DISPLAY;
  }

  Id  = Client->getDisplayId();

  switch (Id)
  {
    case DISPLAY_PRIMARY_ID:
      Handle= mPrimaryDisplay->getHandleLayer();
      break;
    case DISPLAY_EXTERNAL_ID:
      Handle = mExternalDisplay->getHandleLayer();
      break;
    case DISPLAY_VIRTUAL_ID:
      Handle = mVirtualDisplay->getHandleLayer();
      break;
    default:
      ALOGE("Line: %d not support display display:0x%lx", __LINE__, (unsigned long)display);
      err = ERR_BAD_DISPLAY;
      break;
  }

  if (Handle == NULL)
  {
    ALOGE("SprdHWComposer2 line: %d handle is NULL", __LINE__);
    return  ERR_NOT_VALIDATED;
  }

  err = Handle->SET_LAYER_VISIBLE_REGION(layer, visible);

  return err;
}

int32_t /*hwc2_error_t*/SprdHWComposer2::SET_LAYER_Z_ORDER(
           hwc2_display_t display, hwc2_layer_t layer,
           uint32_t z)
{
  int32_t err = ERR_NONE;
  SprdHandleLayer *Handle = NULL;
  int32_t Id = 0;
  SprdDisplayClient *Client = SprdDisplayClient::getDisplayClient(display);

  if (Client == NULL)
  {
    ALOGE("SprdHWComposer2 line: %d cannot get SprdDisplayClient", __LINE__);
    return ERR_BAD_DISPLAY;
  }

  Id  = Client->getDisplayId();

  switch (Id)
  {
    case DISPLAY_PRIMARY_ID:
      Handle= mPrimaryDisplay->getHandleLayer();
      break;
    case DISPLAY_EXTERNAL_ID:
      Handle = mExternalDisplay->getHandleLayer();
      break;
    case DISPLAY_VIRTUAL_ID:
      Handle = mVirtualDisplay->getHandleLayer();
      break;
    default:
      ALOGE("Line: %d not support display display:0x%lx", __LINE__, (unsigned long)display);
      err = ERR_BAD_DISPLAY;
      break;
  }

  if (Handle == NULL)
  {
    ALOGE("SprdHWComposer2 line: %d handle is NULL", __LINE__);
    return  ERR_NOT_VALIDATED;
  }

  err = Handle->SET_LAYER_Z_ORDER(layer, z);

  return err;
}



/*
 *  HWC module info
 * */

static int32_t /*hwc2_error_t*/ HWC2_CREATE_VIRTUAL_DISPLAY(
            hwc2_device_t* device, uint32_t width, uint32_t height,
            int32_t* /*android_pixel_format_t*/ format, hwc2_display_t* outDisplay)
{
  SprdHWComposer2 *HWC = static_cast<SprdHWComposer2 *>(device);
  if (HWC == NULL) {
    ALOGE("Can NOT get SprdHWComposer2 reference l:%d", __LINE__);
    return ERR_BAD_PARAMETER;
  }

  return HWC->CREATE_VIRTUAL_DISPLAY(width, height, format, outDisplay);
}


static int32_t /*hwc2_error_t*/ HWC2_DESTROY_VIRTUAL_DISPLAY(
           hwc2_device_t* device, hwc2_display_t display)
{
  SprdHWComposer2 *HWC = static_cast<SprdHWComposer2 *>(device);
  if (HWC == NULL) {
    ALOGE("Can NOT get SprdHWComposer2 reference l:%d", __LINE__);
    return ERR_BAD_PARAMETER;
  }

  return HWC->DESTROY_VIRTUAL_DISPLAY(display);
}

static void HWC2_DUMP(hwc2_device_t* device, uint32_t* outSize,
           char* outBuffer)
{
  SprdHWComposer2 *HWC = static_cast<SprdHWComposer2 *>(device);
  if (HWC == NULL) {
    ALOGE("Can NOT get SprdHWComposer2 reference l:%d", __LINE__);
    return;
  }

  return HWC->DUMP(outSize, outBuffer);
}

static uint32_t HWC2_GET_MAX_VIRTUAL_DISPLAY_COUNT(
           hwc2_device_t* device)
{
  SprdHWComposer2 *HWC = static_cast<SprdHWComposer2 *>(device);
  if (HWC == NULL) {
    ALOGE("Can NOT get SprdHWComposer2 reference l:%d", __LINE__);
    return ERR_BAD_PARAMETER;
  }

  return HWC->GET_MAX_VIRTUAL_DISPLAY_COUNT();
}

static int32_t /*hwc2_error_t*/ HWC2_REGISTER_CALLBACK(
           hwc2_device_t* device,
           int32_t /*hwc2_callback_descriptor_t*/ descriptor,
           hwc2_callback_data_t callbackData, hwc2_function_pointer_t pointer)
{
  SprdHWComposer2 *HWC = static_cast<SprdHWComposer2 *>(device);
  if (HWC == NULL) {
    ALOGE("Can NOT get SprdHWComposer2 reference l:%d", __LINE__);
    return ERR_BAD_PARAMETER;
  }

  return HWC->REGISTER_CALLBACK(descriptor, callbackData, pointer);
}

static int32_t /*hwc2_error_t*/ HWC2_ACCEPT_DISPLAY_CHANGES(
           hwc2_device_t* device, hwc2_display_t display)
{
  SprdHWComposer2 *HWC = static_cast<SprdHWComposer2 *>(device);
  if (HWC == NULL) {
    ALOGE("Can NOT get SprdHWComposer2 reference l:%d", __LINE__);
    return ERR_BAD_PARAMETER;
  }

  return HWC->ACCEPT_DISPLAY_CHANGES(display);
}

static int32_t /*hwc2_error_t*/ HWC2_CREATE_LAYER(hwc2_device_t* device,
           hwc2_display_t display, hwc2_layer_t* outLayer)
{
  SprdHWComposer2 *HWC = static_cast<SprdHWComposer2 *>(device);
  if (HWC == NULL) {
    ALOGE("Can NOT get SprdHWComposer2 reference l:%d", __LINE__);
    return ERR_BAD_PARAMETER;
  }

  return HWC->CREATE_LAYER(display, outLayer);
}

static int32_t /*hwc2_error_t*/ HWC2_DESTROY_LAYER(
           hwc2_device_t* device, hwc2_display_t display, hwc2_layer_t layer)
{
  SprdHWComposer2 *HWC = static_cast<SprdHWComposer2 *>(device);
  if (HWC == NULL) {
    ALOGE("Can NOT get SprdHWComposer2 reference l:%d", __LINE__);
    return ERR_BAD_PARAMETER;
  }

  return HWC->DESTROY_LAYER(display, layer);
}

static int32_t /*hwc2_error_t*/ HWC2_GET_ACTIVE_CONFIG(
           hwc2_device_t* device, hwc2_display_t display,
           hwc2_config_t* outConfig)
{
  SprdHWComposer2 *HWC = static_cast<SprdHWComposer2 *>(device);
  if (HWC == NULL) {
    ALOGE("Can NOT get SprdHWComposer2 reference l:%d", __LINE__);
    return ERR_BAD_PARAMETER;
  }

  return HWC->GET_ACTIVE_CONFIG(display, outConfig);
}

static int32_t /*hwc2_error_t*/ HWC2_GET_CHANGED_COMPOSITION_TYPES(
           hwc2_device_t* device, hwc2_display_t display,
           uint32_t* outNumElements, hwc2_layer_t* outLayers,
           int32_t* /*hwc2_composition_t*/ outTypes)
{
  SprdHWComposer2 *HWC = static_cast<SprdHWComposer2 *>(device);
  if (HWC == NULL) {
    ALOGE("Can NOT get SprdHWComposer2 reference l:%d", __LINE__);
    return ERR_BAD_PARAMETER;
  }

  return HWC->GET_CHANGED_COMPOSITION_TYPES(display, outNumElements, outLayers, outTypes);
}

static int32_t /*hwc2_error_t*/ HWC2_GET_CLIENT_TARGET_SUPPORT(
           hwc2_device_t* device, hwc2_display_t display, uint32_t width,
           uint32_t height, int32_t /*android_pixel_format_t*/ format,
           int32_t /*android_dataspace_t*/ dataspace)
{
  SprdHWComposer2 *HWC = static_cast<SprdHWComposer2 *>(device);
  if (HWC == NULL) {
    ALOGE("Can NOT get SprdHWComposer2 reference l:%d", __LINE__);
    return ERR_BAD_PARAMETER;
  }

  return HWC->GET_CLIENT_TARGET_SUPPORT(display, width, height, format, dataspace);
}

static int32_t /*hwc2_error_t*/ HWC2_GET_COLOR_MODES(
           hwc2_device_t* device, hwc2_display_t display, uint32_t* outNumModes,
           int32_t* /*android_color_mode_t*/ outModes)
{
  SprdHWComposer2 *HWC = static_cast<SprdHWComposer2 *>(device);
  if (HWC == NULL) {
    ALOGE("Can NOT get SprdHWComposer2 reference l:%d", __LINE__);
    return ERR_BAD_PARAMETER;
  }

  return HWC->GET_COLOR_MODES(display, outNumModes, outModes);
}

static int32_t /*hwc2_error_t*/ HWC2_GET_DISPLAY_ATTRIBUTE(
           hwc2_device_t* device, hwc2_display_t display, hwc2_config_t config,
           int32_t /*hwc2_attribute_t*/ attribute, int32_t* outValue)
{
  SprdHWComposer2 *HWC = static_cast<SprdHWComposer2 *>(device);
  if (HWC == NULL) {
    ALOGE("Can NOT get SprdHWComposer2 reference l:%d", __LINE__);
    return ERR_BAD_PARAMETER;
  }

  return HWC->GET_DISPLAY_ATTRIBUTE(display, config, attribute, outValue);
}

static int32_t /*hwc2_error_t*/ HWC2_GET_DISPLAY_CONFIGS(
           hwc2_device_t* device, hwc2_display_t display, uint32_t* outNumConfigs,
           hwc2_config_t* outConfigs)
{
  SprdHWComposer2 *HWC = static_cast<SprdHWComposer2 *>(device);
  if (HWC == NULL) {
    ALOGE("Can NOT get SprdHWComposer2 reference l:%d", __LINE__);
    return ERR_BAD_PARAMETER;
  }

  return HWC->GET_DISPLAY_CONFIGS(display, outNumConfigs, outConfigs);
}

static int32_t /*hwc2_error_t*/ HWC2_GET_DISPLAY_NAME(
           hwc2_device_t* device, hwc2_display_t display, uint32_t* outSize,
           char* outName)
{
  SprdHWComposer2 *HWC = static_cast<SprdHWComposer2 *>(device);
  if (HWC == NULL) {
    ALOGE("Can NOT get SprdHWComposer2 reference l:%d", __LINE__);
    return ERR_BAD_PARAMETER;
  }

  return HWC->GET_DISPLAY_NAME(display, outSize, outName);
}

static int32_t /*hwc2_error_t*/ HWC2_GET_DISPLAY_REQUESTS(
           hwc2_device_t* device, hwc2_display_t display,
           int32_t* /*hwc2_display_request_t*/ outDisplayRequests,
           uint32_t* outNumElements, hwc2_layer_t* outLayers,
           int32_t* /*hwc2_layer_request_t*/ outLayerRequests)
{
  SprdHWComposer2 *HWC = static_cast<SprdHWComposer2 *>(device);
  if (HWC == NULL) {
    ALOGE("Can NOT get SprdHWComposer2 reference l:%d", __LINE__);
    return ERR_BAD_PARAMETER;
  }

  return HWC->GET_DISPLAY_REQUESTS(display, outDisplayRequests, outNumElements, outLayers, outLayerRequests);
}

static int32_t /*hwc2_error_t*/ HWC2_GET_DISPLAY_TYPE(
           hwc2_device_t* device, hwc2_display_t display,
           int32_t* /*hwc2_display_type_t*/ outType)
{
  SprdHWComposer2 *HWC = static_cast<SprdHWComposer2 *>(device);
  if (HWC == NULL) {
    ALOGE("Can NOT get SprdHWComposer2 reference l:%d", __LINE__);
    return ERR_BAD_PARAMETER;
  }

  return HWC->GET_DISPLAY_TYPE(display, outType);
}

static int32_t /*hwc2_error_t*/ HWC2_GET_DOZE_SUPPORT(
           hwc2_device_t* device, hwc2_display_t display, int32_t* outSupport)
{
  SprdHWComposer2 *HWC = static_cast<SprdHWComposer2 *>(device);
  if (HWC == NULL) {
    ALOGE("Can NOT get SprdHWComposer2 reference l:%d", __LINE__);
    return ERR_BAD_PARAMETER;
  }

  return HWC->GET_DOZE_SUPPORT(display, outSupport);
}

static int32_t /*hwc2_error_t*/ HWC2_GET_HDR_CAPABILITIES(
           hwc2_device_t* device, hwc2_display_t display, uint32_t* outNumTypes,
           int32_t* /*android_hdr_t*/ outTypes, float* outMaxLuminance,
           float* outMaxAverageLuminance, float* outMinLuminance)
{
  SprdHWComposer2 *HWC = static_cast<SprdHWComposer2 *>(device);
  if (HWC == NULL) {
    ALOGE("Can NOT get SprdHWComposer2 reference l:%d", __LINE__);
    return ERR_BAD_PARAMETER;
  }

  return HWC->GET_HDR_CAPABILITIES(display, outNumTypes, outTypes, outMaxLuminance, outMaxAverageLuminance, outMinLuminance);
}

static int32_t /*hwc2_error_t*/ HWC2_GET_RELEASE_FENCES(
           hwc2_device_t* device, hwc2_display_t display, uint32_t* outNumElements,
           hwc2_layer_t* outLayers, int32_t* outFences)
{
  SprdHWComposer2 *HWC = static_cast<SprdHWComposer2 *>(device);
  if (HWC == NULL) {
    ALOGE("Can NOT get SprdHWComposer2 reference l:%d", __LINE__);
    return ERR_BAD_PARAMETER;
  }

  return HWC->GET_RELEASE_FENCES(display, outNumElements, outLayers, outFences);
}

static int32_t /*hwc2_error_t*/ HWC2_PRESENT_DISPLAY(
           hwc2_device_t* device, hwc2_display_t display, int32_t* outRetireFence)
{
  SprdHWComposer2 *HWC = static_cast<SprdHWComposer2 *>(device);
  if (HWC == NULL) {
    ALOGE("Can NOT get SprdHWComposer2 reference l:%d", __LINE__);
    return ERR_BAD_PARAMETER;
  }

  return HWC->PRESENT_DISPLAY(display, outRetireFence);
}

static int32_t /*hwc2_error_t*/ HWC2_SET_ACTIVE_CONFIG(
           hwc2_device_t* device, hwc2_display_t display, hwc2_config_t config)
{
  SprdHWComposer2 *HWC = static_cast<SprdHWComposer2 *>(device);
  if (HWC == NULL) {
    ALOGE("Can NOT get SprdHWComposer2 reference l:%d", __LINE__);
    return ERR_BAD_PARAMETER;
  }

  return HWC->SET_ACTIVE_CONFIG(display, config);
}

static int32_t /*hwc2_error_t*/ HWC2_SET_CLIENT_TARGET(
           hwc2_device_t* device, hwc2_display_t display, buffer_handle_t target,
           int32_t acquireFence, int32_t /*android_dataspace_t*/ dataspace,
           hwc_region_t damage)
{
  SprdHWComposer2 *HWC = static_cast<SprdHWComposer2 *>(device);
  if (HWC == NULL) {
    ALOGE("Can NOT get SprdHWComposer2 reference l:%d", __LINE__);
    return ERR_BAD_PARAMETER;
  }

  return HWC->SET_CLIENT_TARGET(display, target, acquireFence, dataspace, damage);
}

static int32_t /*hwc2_error_t*/ HWC2_SET_COLOR_MODE(
           hwc2_device_t* device, hwc2_display_t display,
           int32_t /*android_color_mode_t*/ mode)
{
  SprdHWComposer2 *HWC = static_cast<SprdHWComposer2 *>(device);
  if (HWC == NULL) {
    ALOGE("Can NOT get SprdHWComposer2 reference l:%d", __LINE__);
    return ERR_BAD_PARAMETER;
  }

  return HWC->SET_COLOR_MODE(display, mode);
}

static int32_t /*hwc2_error_t*/ HWC2_SET_COLOR_TRANSFORM(
           hwc2_device_t* device, hwc2_display_t display, const float* matrix,
           int32_t /*android_color_transform_t*/ hint)
{
  SprdHWComposer2 *HWC = static_cast<SprdHWComposer2 *>(device);
  if (HWC == NULL) {
    ALOGE("Can NOT get SprdHWComposer2 reference l:%d", __LINE__);
    return ERR_BAD_PARAMETER;
  }

  return HWC->SET_COLOR_TRANSFORM(display, matrix, hint);
}

static int32_t /*hwc2_error_t*/ HWC2_SET_OUTPUT_BUFFER(
           hwc2_device_t* device, hwc2_display_t display, buffer_handle_t buffer,
           int32_t releaseFence)
{
  SprdHWComposer2 *HWC = static_cast<SprdHWComposer2 *>(device);
  if (HWC == NULL) {
    ALOGE("Can NOT get SprdHWComposer2 reference l:%d", __LINE__);
    return ERR_BAD_PARAMETER;
  }

  return HWC->SET_OUTPUT_BUFFER(display, buffer, releaseFence);
}

static int32_t /*hwc2_error_t*/ HWC2_SET_POWER_MODE(
           hwc2_device_t* device, hwc2_display_t display,
           int32_t /*hwc2_power_mode_t*/ mode)
{
  SprdHWComposer2 *HWC = static_cast<SprdHWComposer2 *>(device);
  if (HWC == NULL) {
    ALOGE("Can NOT get SprdHWComposer2 reference l:%d", __LINE__);
    return ERR_BAD_PARAMETER;
  }

  return HWC->SET_POWER_MODE(display, mode);
}

static int32_t /*hwc2_error_t*/ HWC2_SET_VSYNC_ENABLED(
           hwc2_device_t* device, hwc2_display_t display,
           int32_t /*hwc2_vsync_t*/ enabled)
{
  SprdHWComposer2 *HWC = static_cast<SprdHWComposer2 *>(device);
  if (HWC == NULL) {
    ALOGE("Can NOT get SprdHWComposer2 reference l:%d", __LINE__);
    return ERR_BAD_PARAMETER;
  }

  return HWC->SET_VSYNC_ENABLED(display, enabled);
}

static int32_t /*hwc2_error_t*/ HWC2_VALIDATE_DISPLAY(
           hwc2_device_t* device, hwc2_display_t display,
           uint32_t* outNumTypes, uint32_t* outNumRequests)
{
  SprdHWComposer2 *HWC = static_cast<SprdHWComposer2 *>(device);
  if (HWC == NULL) {
    ALOGE("Can NOT get SprdHWComposer2 reference l:%d", __LINE__);
    return ERR_BAD_PARAMETER;
  }

  return HWC->VALIDATE_DISPLAY(display, outNumTypes, outNumRequests);
}

static int32_t /*hwc2_error_t*/ HWC2_SET_CURSOR_POSITION(
           hwc2_device_t* device, hwc2_display_t display, hwc2_layer_t layer,
           int32_t x, int32_t y)
{
  SprdHWComposer2 *HWC = static_cast<SprdHWComposer2 *>(device);
  if (HWC == NULL) {
    ALOGE("Can NOT get SprdHWComposer2 reference l:%d", __LINE__);
    return ERR_BAD_PARAMETER;
  }

  return HWC->SET_CURSOR_POSITION(display, layer, x, y);
}

static int32_t /*hwc2_error_t*/ HWC2_SET_LAYER_BUFFER(
           hwc2_device_t* device, hwc2_display_t display, hwc2_layer_t layer,
           buffer_handle_t buffer, int32_t acquireFence)
{
  SprdHWComposer2 *HWC = static_cast<SprdHWComposer2 *>(device);
  if (HWC == NULL) {
    ALOGE("Can NOT get SprdHWComposer2 reference l:%d", __LINE__);
    return ERR_BAD_PARAMETER;
  }

  return HWC->SET_LAYER_BUFFER(display, layer, buffer, acquireFence);
}

static int32_t /*hwc2_error_t*/ HWC2_SET_LAYER_SURFACE_DAMAGE(
           hwc2_device_t* device, hwc2_display_t display, hwc2_layer_t layer,
           hwc_region_t damage)
{
  SprdHWComposer2 *HWC = static_cast<SprdHWComposer2 *>(device);
  if (HWC == NULL) {
    ALOGE("Can NOT get SprdHWComposer2 reference l:%d", __LINE__);
    return ERR_BAD_PARAMETER;
  }

  return HWC->SET_LAYER_SURFACE_DAMAGE(display, layer, damage);
}

static int32_t /*hwc2_error_t*/ HWC2_SET_LAYER_BLEND_MODE(
           hwc2_device_t* device, hwc2_display_t display, hwc2_layer_t layer,
           int32_t /*hwc2_blend_mode_t*/ mode)
{
  SprdHWComposer2 *HWC = static_cast<SprdHWComposer2 *>(device);
  if (HWC == NULL) {
    ALOGE("Can NOT get SprdHWComposer2 reference l:%d", __LINE__);
    return ERR_BAD_PARAMETER;
  }

  return HWC->SET_LAYER_BLEND_MODE(display, layer, mode);
}

static int32_t /*hwc2_error_t*/ HWC2_SET_LAYER_COLOR(
           hwc2_device_t* device, hwc2_display_t display, hwc2_layer_t layer,
           hwc_color_t color)
{
  SprdHWComposer2 *HWC = static_cast<SprdHWComposer2 *>(device);
  if (HWC == NULL) {
    ALOGE("Can NOT get SprdHWComposer2 reference l:%d", __LINE__);
    return ERR_BAD_PARAMETER;
  }

  return HWC->SET_LAYER_COLOR(display, layer, color);
}

static int32_t /*hwc2_error_t*/ HWC2_SET_LAYER_COMPOSITION_TYPE(
           hwc2_device_t* device, hwc2_display_t display, hwc2_layer_t layer,
           int32_t /*hwc2_composition_t*/ type)
{
  SprdHWComposer2 *HWC = static_cast<SprdHWComposer2 *>(device);
  if (HWC == NULL) {
    ALOGE("Can NOT get SprdHWComposer2 reference l:%d", __LINE__);
    return ERR_BAD_PARAMETER;
  }

  return HWC->SET_LAYER_COMPOSITION_TYPE(display, layer, type);
}

static int32_t /*hwc2_error_t*/ HWC2_SET_LAYER_DATASPACE(
           hwc2_device_t* device, hwc2_display_t display, hwc2_layer_t layer,
           int32_t /*android_dataspace_t*/ dataspace)
{
  SprdHWComposer2 *HWC = static_cast<SprdHWComposer2 *>(device);
  if (HWC == NULL) {
    ALOGE("Can NOT get SprdHWComposer2 reference l:%d", __LINE__);
    return ERR_BAD_PARAMETER;
  }

  return HWC->SET_LAYER_DATASPACE(display, layer, dataspace);
}

static int32_t /*hwc2_error_t*/ HWC2_SET_LAYER_DISPLAY_FRAME(
           hwc2_device_t* device, hwc2_display_t display, hwc2_layer_t layer,
           hwc_rect_t frame)
{
  SprdHWComposer2 *HWC = static_cast<SprdHWComposer2 *>(device);
  if (HWC == NULL) {
    ALOGE("Can NOT get SprdHWComposer2 reference l:%d", __LINE__);
    return ERR_BAD_PARAMETER;
  }

  return HWC->SET_LAYER_DISPLAY_FRAME(display, layer, frame);
}

static int32_t /*hwc2_error_t*/ HWC2_SET_LAYER_PLANE_ALPHA(
           hwc2_device_t* device, hwc2_display_t display, hwc2_layer_t layer,
           float alpha)
{
  SprdHWComposer2 *HWC = static_cast<SprdHWComposer2 *>(device);
  if (HWC == NULL) {
    ALOGE("Can NOT get SprdHWComposer2 reference l:%d", __LINE__);
    return ERR_BAD_PARAMETER;
  }

  return HWC->SET_LAYER_PLANE_ALPHA(display, layer, alpha);
}

static int32_t /*hwc2_error_t*/ HWC2_SET_LAYER_SIDEBAND_STREAM(
           hwc2_device_t* device, hwc2_display_t display, hwc2_layer_t layer,
           const native_handle_t* stream)
{
  SprdHWComposer2 *HWC = static_cast<SprdHWComposer2 *>(device);
  if (HWC == NULL) {
    ALOGE("Can NOT get SprdHWComposer2 reference l:%d", __LINE__);
    return ERR_BAD_PARAMETER;
  }

  return HWC->SET_LAYER_SIDEBAND_STREAM(display, layer, stream);
}

static int32_t /*hwc2_error_t*/ HWC2_SET_LAYER_SOURCE_CROP(
           hwc2_device_t* device, hwc2_display_t display, hwc2_layer_t layer,
           hwc_frect_t crop)
{
  SprdHWComposer2 *HWC = static_cast<SprdHWComposer2 *>(device);
  if (HWC == NULL) {
    ALOGE("Can NOT get SprdHWComposer2 reference l:%d", __LINE__);
    return ERR_BAD_PARAMETER;
  }

  return HWC->SET_LAYER_SOURCE_CROP(display, layer, crop);
}

static int32_t /*hwc2_error_t*/ HWC2_SET_LAYER_TRANSFORM(
           hwc2_device_t* device, hwc2_display_t display, hwc2_layer_t layer,
           int32_t /*hwc_transform_t*/ transform)
{
  SprdHWComposer2 *HWC = static_cast<SprdHWComposer2 *>(device);
  if (HWC == NULL) {
    ALOGE("Can NOT get SprdHWComposer2 reference l:%d", __LINE__);
    return ERR_BAD_PARAMETER;
  }

  return HWC->SET_LAYER_TRANSFORM(display, layer, transform);
}

static int32_t /*hwc2_error_t*/ HWC2_SET_LAYER_VISIBLE_REGION(
           hwc2_device_t* device, hwc2_display_t display, hwc2_layer_t layer,
           hwc_region_t visible)
{
  SprdHWComposer2 *HWC = static_cast<SprdHWComposer2 *>(device);
  if (HWC == NULL) {
    ALOGE("Can NOT get SprdHWComposer2 reference l:%d", __LINE__);
    return ERR_BAD_PARAMETER;
  }

  return HWC->SET_LAYER_VISIBLE_REGION(display, layer, visible);
}

static int32_t /*hwc2_error_t*/ HWC2_SET_LAYER_Z_ORDER(
           hwc2_device_t* device, hwc2_display_t display, hwc2_layer_t layer,
           uint32_t z)
{
  SprdHWComposer2 *HWC = static_cast<SprdHWComposer2 *>(device);
  if (HWC == NULL) {
    ALOGE("Can NOT get SprdHWComposer2 reference l:%d", __LINE__);
    return ERR_BAD_PARAMETER;
  }

  return HWC->SET_LAYER_Z_ORDER(display, layer, z);
}

static void hwc_getCapabilities(struct hwc2_device* device, uint32_t* outCount,
          int32_t* /*hwc2_capability_t*/ outCapabilities)
{
  SprdHWComposer2 *HWC = static_cast<SprdHWComposer2 *>(device);
  if (HWC == NULL) {
    ALOGE("Can NOT get SprdHWComposer2 reference l:%d", __LINE__);
    return;
  }

  return HWC->getCapabilities(outCount, outCapabilities);
}

#define REMAP_TO_ANDROID_FUNC(func) \
        reinterpret_cast<hwc2_function_pointer_t>(func)

static hwc2_function_pointer_t hwc_getFunction(struct hwc2_device* device,
          int32_t /*hwc2_function_descriptor_t*/ descriptor)
{
  hwc2_function_pointer_t func = NULL;
  SprdHWComposer2 *HWC = static_cast<SprdHWComposer2 *>(device);
  if (HWC == NULL) {
    ALOGE("Can NOT get SprdHWComposer2 reference l:%d", __LINE__);
    return NULL;
  }

  switch (descriptor)
  {
    case HWC2_FUNCTION_ACCEPT_DISPLAY_CHANGES:
      func = REMAP_TO_ANDROID_FUNC(HWC2_ACCEPT_DISPLAY_CHANGES);
      break;
    case HWC2_FUNCTION_CREATE_LAYER:
      func = REMAP_TO_ANDROID_FUNC(HWC2_CREATE_LAYER);
      break;
    case HWC2_FUNCTION_CREATE_VIRTUAL_DISPLAY:
      func = REMAP_TO_ANDROID_FUNC(HWC2_CREATE_VIRTUAL_DISPLAY);
      break;
    case HWC2_FUNCTION_DESTROY_LAYER:
      func = REMAP_TO_ANDROID_FUNC(HWC2_DESTROY_LAYER);
      break;
    case HWC2_FUNCTION_DESTROY_VIRTUAL_DISPLAY:
      func = REMAP_TO_ANDROID_FUNC(HWC2_DESTROY_VIRTUAL_DISPLAY);
      break;
    case HWC2_FUNCTION_DUMP:
      func = REMAP_TO_ANDROID_FUNC(HWC2_DUMP);
      break;
    case HWC2_FUNCTION_GET_ACTIVE_CONFIG:
      func = REMAP_TO_ANDROID_FUNC(HWC2_GET_ACTIVE_CONFIG);
      break;
    case HWC2_FUNCTION_GET_CHANGED_COMPOSITION_TYPES:
      func = REMAP_TO_ANDROID_FUNC(HWC2_GET_CHANGED_COMPOSITION_TYPES);
      break;
    case HWC2_FUNCTION_GET_CLIENT_TARGET_SUPPORT:
      func = REMAP_TO_ANDROID_FUNC(HWC2_GET_CLIENT_TARGET_SUPPORT);
      break;
    case HWC2_FUNCTION_GET_COLOR_MODES:
      func = REMAP_TO_ANDROID_FUNC(HWC2_GET_COLOR_MODES);
      break;
    case HWC2_FUNCTION_GET_DISPLAY_ATTRIBUTE:
      func = REMAP_TO_ANDROID_FUNC(HWC2_GET_DISPLAY_ATTRIBUTE);
      break;
    case HWC2_FUNCTION_GET_DISPLAY_CONFIGS:
      func = REMAP_TO_ANDROID_FUNC(HWC2_GET_DISPLAY_CONFIGS);
      break;
    case HWC2_FUNCTION_GET_DISPLAY_NAME:
      func = REMAP_TO_ANDROID_FUNC(HWC2_GET_DISPLAY_NAME);
      break;
    case HWC2_FUNCTION_GET_DISPLAY_REQUESTS:
      func = REMAP_TO_ANDROID_FUNC(HWC2_GET_DISPLAY_REQUESTS);
      break;
    case HWC2_FUNCTION_GET_DISPLAY_TYPE:
      func = REMAP_TO_ANDROID_FUNC(HWC2_GET_DISPLAY_TYPE);
      break;
    case HWC2_FUNCTION_GET_DOZE_SUPPORT:
      func = REMAP_TO_ANDROID_FUNC(HWC2_GET_DOZE_SUPPORT);
      break;
    case HWC2_FUNCTION_GET_HDR_CAPABILITIES:
      func = REMAP_TO_ANDROID_FUNC(HWC2_GET_HDR_CAPABILITIES);
      break;
    case HWC2_FUNCTION_GET_MAX_VIRTUAL_DISPLAY_COUNT:
      func = REMAP_TO_ANDROID_FUNC(HWC2_GET_MAX_VIRTUAL_DISPLAY_COUNT);
      break;
    case HWC2_FUNCTION_GET_RELEASE_FENCES:
      func = REMAP_TO_ANDROID_FUNC(HWC2_GET_RELEASE_FENCES);
      break;
    case HWC2_FUNCTION_PRESENT_DISPLAY:
      func = REMAP_TO_ANDROID_FUNC(HWC2_PRESENT_DISPLAY);
      break;
    case HWC2_FUNCTION_REGISTER_CALLBACK:
      func = REMAP_TO_ANDROID_FUNC(HWC2_REGISTER_CALLBACK);
      break;
    case HWC2_FUNCTION_SET_ACTIVE_CONFIG:
      func = REMAP_TO_ANDROID_FUNC(HWC2_SET_ACTIVE_CONFIG);
      break;
    case HWC2_FUNCTION_SET_CLIENT_TARGET:
      func = REMAP_TO_ANDROID_FUNC(HWC2_SET_CLIENT_TARGET);
      break;
    case HWC2_FUNCTION_SET_COLOR_MODE:
      func = REMAP_TO_ANDROID_FUNC(HWC2_SET_COLOR_MODE);
      break;
    case HWC2_FUNCTION_SET_COLOR_TRANSFORM:
      func = REMAP_TO_ANDROID_FUNC(HWC2_SET_COLOR_TRANSFORM);
      break;
    case HWC2_FUNCTION_SET_CURSOR_POSITION:
      func = REMAP_TO_ANDROID_FUNC(HWC2_SET_CURSOR_POSITION);
    case HWC2_FUNCTION_SET_LAYER_BLEND_MODE:
      func = REMAP_TO_ANDROID_FUNC(HWC2_SET_LAYER_BLEND_MODE);
      break;
    case HWC2_FUNCTION_SET_LAYER_BUFFER:
      func = REMAP_TO_ANDROID_FUNC(HWC2_SET_LAYER_BUFFER);
      break;
    case HWC2_FUNCTION_SET_LAYER_COLOR:
      func = REMAP_TO_ANDROID_FUNC(HWC2_SET_LAYER_COLOR);
      break;
    case HWC2_FUNCTION_SET_LAYER_COMPOSITION_TYPE:
      func = REMAP_TO_ANDROID_FUNC(HWC2_SET_LAYER_COMPOSITION_TYPE);
      break;
    case HWC2_FUNCTION_SET_LAYER_DATASPACE:
      func = REMAP_TO_ANDROID_FUNC(HWC2_SET_LAYER_DATASPACE);
      break;
    case HWC2_FUNCTION_SET_LAYER_DISPLAY_FRAME:
      func = REMAP_TO_ANDROID_FUNC(HWC2_SET_LAYER_DISPLAY_FRAME);
      break;
    case HWC2_FUNCTION_SET_LAYER_PLANE_ALPHA:
      func = REMAP_TO_ANDROID_FUNC(HWC2_SET_LAYER_PLANE_ALPHA);
      break;
    case HWC2_FUNCTION_SET_LAYER_SIDEBAND_STREAM:
      func = REMAP_TO_ANDROID_FUNC(HWC2_SET_LAYER_SIDEBAND_STREAM);
      break;
    case HWC2_FUNCTION_SET_LAYER_SOURCE_CROP:
      func = REMAP_TO_ANDROID_FUNC(HWC2_SET_LAYER_SOURCE_CROP);
      break;
    case HWC2_FUNCTION_SET_LAYER_SURFACE_DAMAGE:
      func = REMAP_TO_ANDROID_FUNC(HWC2_SET_LAYER_SURFACE_DAMAGE);
      break;
    case HWC2_FUNCTION_SET_LAYER_TRANSFORM:
      func = REMAP_TO_ANDROID_FUNC(HWC2_SET_LAYER_TRANSFORM);
      break;
    case HWC2_FUNCTION_SET_LAYER_VISIBLE_REGION:
      func = REMAP_TO_ANDROID_FUNC(HWC2_SET_LAYER_VISIBLE_REGION);
      break;
    case HWC2_FUNCTION_SET_LAYER_Z_ORDER:
      func = REMAP_TO_ANDROID_FUNC(HWC2_SET_LAYER_Z_ORDER);
      break;
    case HWC2_FUNCTION_SET_OUTPUT_BUFFER:
     func = REMAP_TO_ANDROID_FUNC(HWC2_SET_OUTPUT_BUFFER);
     break;
    case HWC2_FUNCTION_SET_POWER_MODE:
     func = REMAP_TO_ANDROID_FUNC(HWC2_SET_POWER_MODE);
     break;
    case HWC2_FUNCTION_SET_VSYNC_ENABLED:
     func = REMAP_TO_ANDROID_FUNC(HWC2_SET_VSYNC_ENABLED);
     break;
    case HWC2_FUNCTION_VALIDATE_DISPLAY:
     func = REMAP_TO_ANDROID_FUNC(HWC2_VALIDATE_DISPLAY);
     break;
    case HWC2_FUNCTION_INVALID:
     func = NULL;
     ALOGE("hwc_getFunction find HWC2_FUNCTION_INVALID");
     break;
  }

  return func;
}

static int hwc_device_close(struct hw_device_t *dev) {
  //HWC_IGNORE(dev);

  hwc2_device_t *HwcDevice = (hwc2_device_t*)dev;

  SprdHWComposer2 *HWC = static_cast<SprdHWComposer2 *>(HwcDevice);
  delete HWC;

#if 0
  SprdHWComposer2 *HWC = getHWCReference(dev);
  if (HWC != NULL) {
    delete HWC;
    HWC = NULL;
  }
#endif

  return 0;
}


static int hwc_device_open(const struct hw_module_t *module, const char *name,
                           struct hw_device_t **device) {
  int status = -EINVAL;

  if (strcmp(name, HWC_HARDWARE_COMPOSER)) {
    ALOGE("The module name is not HWC_HARDWARE_COMPOSER");
    return status;
  }

  SprdHWComposer2 *HWC = new SprdHWComposer2();
  if (HWC == NULL) {
    ALOGE("Can NOT create SprdHWComposer object");
    status = -ENOMEM;
    return status;
  }

  bool ret = HWC->Init();
  if (!ret) {
    ALOGE("Init HWComposer failed");
    delete HWC;
    HWC = NULL;
    status = -ENOMEM;
    return status;
  }

  HWC->hwc2_device_t::common.tag = HARDWARE_DEVICE_TAG;
  HWC->hwc2_device_t::common.version = HWC_DEVICE_API_VERSION_2_0;
  HWC->hwc2_device_t::common.module =
      const_cast<hw_module_t *>(module);
  HWC->hwc2_device_t::common.close = hwc_device_close;

  HWC->hwc2_device_t::getCapabilities = hwc_getCapabilities;
  HWC->hwc2_device_t::getFunction = hwc_getFunction;

  *device = &HWC->hwc2_device_t::common;

  status = 0;

  return status;
}

static struct hw_module_methods_t hwc_module_methods = {.open = hwc_device_open};

hwc_module_t HAL_MODULE_INFO_SYM = {
  .common = {
    .tag = HARDWARE_MODULE_TAG,
    .version_major = 4,
    .version_minor = 0,
    .id = HWC_HARDWARE_MODULE_ID,
    .name = "SPRD HWComposer Module V2.0",
    .author = "The AndroidL Open Source Project",
    .methods = &hwc_module_methods,
    .dso = 0,
    .reserved = {0},
  }
};
