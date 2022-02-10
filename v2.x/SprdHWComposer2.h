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
 ** File: SprdHWComposer.h            DESCRIPTION                             *
 **                                   comunicate with SurfaceFlinger and      *
 **                                   other class objects of HWComposer       *
 ******************************************************************************
 ******************************************************************************
 ** Author:         zhongjun.chen@spreadtrum.com                              *
 *****************************************************************************/

#ifndef _SPRD_HWCOMPOSER2_H
#define _SPRD_HWCOMPOSER2_H

#include <hardware/hwcomposer2.h>
#include <fcntl.h>
#include <errno.h>

#include <EGL/egl.h>

#include <utils/RefBase.h>
#include <cutils/properties.h>
#include <cutils/atomic.h>
#include <cutils/log.h>

#include "SprdPrimaryDisplayDevice/SprdPrimaryDisplayDevice.h"
#include "SprdVirtualDisplayDevice/SprdVirtualDisplayDevice.h"
#include "SprdExternalDisplayDevice/SprdExternalDisplayDevice.h"
#include "SprdDisplayDevice.h"
#include "SprdUtil.h"
#if defined USE_ADF_DISPLAY
#include "SprdADFWrapper.h"
#elif defined HWC_SUPPORT_FBD_DISPLAY
#include "SprdFrameBufferDevice.h"
#else
#include "drm/SprdDrm.h"
#endif
#include "dump.h"

using namespace android;

class SprdDisplayCore;

class SprdHWComposer2 : public hwc2_device_t {
 public:
  SprdHWComposer2()
      : mPrimaryDisplay(0),
        mExternalDisplay(0),
        mVirtualDisplay(0),
        mDisplayCore(0),

        mInitFlag(0),
        mDebugFlag(0),
        mDumpFlag(0) {}

  ~SprdHWComposer2();

  /*
   *  Allocate and initialize the local objects used by HWComposer
   * */
  bool Init();

  /* getCapabilities(..., outCount, outCapabilities)
   *
   * Provides a list of capabilities (described in the definition of
   * hwc2_capability_t above) supported by this device. This list must
   * not change after the device has been loaded.
   *
   * Parameters:
   *   outCount - if outCapabilities was NULL, the number of capabilities
   *       which would have been returned; if outCapabilities was not NULL,
   *       the number of capabilities returned, which must not exceed the
   *       value stored in outCount prior to the call
   *   outCapabilities - a list of capabilities supported by this device; may
   *       be NULL, in which case this function must write into outCount the
   *       number of capabilities which would have been written into
   *       outCapabilities
   */
  void getCapabilities(uint32_t* outCount, int32_t* /*hwc2_capability_t*/ outCapabilities);

   /* createVirtualDisplay(..., width, height, format, outDisplay)
    * Descriptor: HWC2_FUNCTION_CREATE_VIRTUAL_DISPLAY
    * Must be provided by all HWC2 devices
    *
    * Creates a new virtual display with the given width and height. The format
    * passed into this function is the default format requested by the consumer of
    * the virtual display output buffers. If a different format will be returned by
    * the device, it should be returned in this parameter so it can be set properly
    * when handing the buffers to the consumer.
    *
    * The display will be assumed to be on from the time the first frame is
    * presented until the display is destroyed.
    *
    * Parameters:
    *   width - width in pixels
    *   height - height in pixels
    *   format - prior to the call, the default output buffer format selected by
    *       the consumer; after the call, the format the device will produce
    *   outDisplay - the newly-created virtual display; pointer will be non-NULL
    *
    * Returns HWC2_ERROR_NONE or one of the following errors:
    *   HWC2_ERROR_UNSUPPORTED - the width or height is too large for the device to
    *       be able to create a virtual display
    *   HWC2_ERROR_NO_RESOURCES - the device is unable to create a new virtual
    *       display at this time
    */
   int32_t /*hwc2_error_t*/ CREATE_VIRTUAL_DISPLAY(
            uint32_t width, uint32_t height,
            int32_t* /*android_pixel_format_t*/ format, hwc2_display_t* outDisplay);

   /* destroyVirtualDisplay(..., display)
    * Descriptor: HWC2_FUNCTION_DESTROY_VIRTUAL_DISPLAY
    * Must be provided by all HWC2 devices
    *
    * Destroys a virtual display. After this call all resources consumed by this
    * display may be freed by the device and any operations performed on this
    * display should fail.
    *
    * Parameters:
    *   display - the virtual display to destroy
    *
    * Returns HWC2_ERROR_NONE or one of the following errors:
    *   HWC2_ERROR_BAD_DISPLAY - an invalid display handle was passed in
    *   HWC2_ERROR_BAD_PARAMETER - the display handle which was passed in does not
    *       refer to a virtual display
    */
   int32_t /*hwc2_error_t*/ DESTROY_VIRTUAL_DISPLAY(hwc2_display_t display);

   /* dump(..., outSize, outBuffer)
    * Descriptor: HWC2_FUNCTION_DUMP
    * Must be provided by all HWC2 devices
    *
    * Retrieves implementation-defined debug information, which will be displayed
    * during, for example, `dumpsys SurfaceFlinger`.
    *
    * If called with outBuffer == NULL, the device should store a copy of the
    * desired output and return its length in bytes in outSize. If the device
    * already has a stored copy, that copy should be purged and replaced with a
    * fresh copy.
    *
    * If called with outBuffer != NULL, the device should copy its stored version
    * of the output into outBuffer and store how many bytes of data it copied into
    * outSize. Prior to this call, the client will have populated outSize with the
    * maximum number of bytes outBuffer can hold. The device must not write more
    * than this amount into outBuffer. If the device does not currently have a
    * stored copy, then it should return 0 in outSize.
    *
    * Any data written into outBuffer need not be null-terminated.
    *
    * Parameters:
    *   outSize - if outBuffer was NULL, the number of bytes needed to copy the
    *       device's stored output; if outBuffer was not NULL, the number of bytes
    *       written into it, which must not exceed the value stored in outSize
    *       prior to the call; pointer will be non-NULL
    *   outBuffer - the buffer to write the dump output into; may be NULL as
    *       described above; data written into this buffer need not be
    *       null-terminated
    */
  void DUMP(uint32_t* outSize, char* outBuffer);

   /* getMaxVirtualDisplayCount(...)
    * Descriptor: HWC2_FUNCTION_GET_MAX_VIRTUAL_DISPLAY_COUNT
    * Must be provided by all HWC2 devices
    *
    * Returns the maximum number of virtual displays supported by this device
    * (which may be 0). The client will not attempt to create more than this many
    * virtual displays on this device. This number must not change for the lifetime
    * of the device.
    */
  uint32_t GET_MAX_VIRTUAL_DISPLAY_COUNT(void);

   /* registerCallback(..., descriptor, callbackData, pointer)
    * Descriptor: HWC2_FUNCTION_REGISTER_CALLBACK
    * Must be provided by all HWC2 devices
    *
    * Provides a callback for the device to call. All callbacks take a callbackData
    * item as the first parameter, so this value should be stored with the callback
    * for later use. The callbackData may differ from one callback to another. If
    * this function is called multiple times with the same descriptor, later
    * callbacks replace earlier ones.
    *
    * Parameters:
    *   descriptor - which callback should be set
    *   callBackdata - opaque data which must be passed back through the callback
    *   pointer - a non-NULL function pointer corresponding to the descriptor
    *
    * Returns HWC2_ERROR_NONE or one of the following errors:
    *   HWC2_ERROR_BAD_PARAMETER - descriptor was invalid
    */
   int32_t /*hwc2_error_t*/ REGISTER_CALLBACK(
           int32_t /*hwc2_callback_descriptor_t*/ descriptor,
           hwc2_callback_data_t callbackData, hwc2_function_pointer_t pointer);

   /* acceptDisplayChanges(...)
    * Descriptor: HWC2_FUNCTION_ACCEPT_DISPLAY_CHANGES
    * Must be provided by all HWC2 devices
    *
    * Accepts the changes required by the device from the previous validateDisplay
    * call (which may be queried using getChangedCompositionTypes) and revalidates
    * the display. This function is equivalent to requesting the changed types from
    * getChangedCompositionTypes, setting those types on the corresponding layers,
    * and then calling validateDisplay again.
    *
    * After this call it must be valid to present this display. Calling this after
    * validateDisplay returns 0 changes must succeed with HWC2_ERROR_NONE, but
    * should have no other effect.
    *
    * Returns HWC2_ERROR_NONE or one of the following errors:
    *   HWC2_ERROR_BAD_DISPLAY - an invalid display handle was passed in
    *   HWC2_ERROR_NOT_VALIDATED - validateDisplay has not been called
    */
   int32_t /*hwc2_error_t*/ ACCEPT_DISPLAY_CHANGES(
           hwc2_display_t display);

   /* createLayer(..., outLayer)
    * Descriptor: HWC2_FUNCTION_CREATE_LAYER
    * Must be provided by all HWC2 devices
    *
    * Creates a new layer on the given display.
    *
    * Parameters:
    *   outLayer - the handle of the new layer; pointer will be non-NULL
    *
    * Returns HWC2_ERROR_NONE or one of the following errors:
    *   HWC2_ERROR_BAD_DISPLAY - an invalid display handle was passed in
    *   HWC2_ERROR_NO_RESOURCES - the device was unable to create this layer
    */
   int32_t /*hwc2_error_t*/ CREATE_LAYER(
           hwc2_display_t display, hwc2_layer_t* outLayer);

   /* destroyLayer(..., layer)
    * Descriptor: HWC2_FUNCTION_DESTROY_LAYER
    * Must be provided by all HWC2 devices
    *
    * Destroys the given layer.
    *
    * Parameters:
    *   layer - the handle of the layer to destroy
    *
    * Returns HWC2_ERROR_NONE or one of the following errors:
    *   HWC2_ERROR_BAD_DISPLAY - an invalid display handle was passed in
    *   HWC2_ERROR_BAD_LAYER - an invalid layer handle was passed in
    */
  int32_t /*hwc2_error_t*/ DESTROY_LAYER(
           hwc2_display_t display, hwc2_layer_t layer);

   /* getActiveConfig(..., outConfig)
    * Descriptor: HWC2_FUNCTION_GET_ACTIVE_CONFIG
    * Must be provided by all HWC2 devices
    *
    * Retrieves which display configuration is currently active.
    *
    * If no display configuration is currently active, this function must return
    * HWC2_ERROR_BAD_CONFIG and place no configuration handle in outConfig. It is
    * the responsibility of the client to call setActiveConfig with a valid
    * configuration before attempting to present anything on the display.
    *
    * Parameters:
    *   outConfig - the currently active display configuration; pointer will be
    *       non-NULL
    *
    * Returns HWC2_ERROR_NONE or one of the following errors:
    *   HWC2_ERROR_BAD_DISPLAY - an invalid display handle was passed in
    *   HWC2_ERROR_BAD_CONFIG - no configuration is currently active
    */
   int32_t /*hwc2_error_t*/ GET_ACTIVE_CONFIG(
           hwc2_display_t display,
           hwc2_config_t* outConfig);

   /* getChangedCompositionTypes(..., outNumElements, outLayers, outTypes)
    * Descriptor: HWC2_FUNCTION_GET_CHANGED_COMPOSITION_TYPES
    * Must be provided by all HWC2 devices
    *
    * Retrieves the layers for which the device requires a different composition
    * type than had been set prior to the last call to validateDisplay. The client
    * will either update its state with these types and call acceptDisplayChanges,
    * or will set new types and attempt to validate the display again.
    *
    * outLayers and outTypes may be NULL to retrieve the number of elements which
    * will be returned. The number of elements returned must be the same as the
    * value returned in outNumTypes from the last call to validateDisplay.
    *
    * Parameters:
    *   outNumElements - if outLayers or outTypes were NULL, the number of layers
    *       and types which would have been returned; if both were non-NULL, the
    *       number of elements returned in outLayers and outTypes, which must not
    *       exceed the value stored in outNumElements prior to the call; pointer
    *       will be non-NULL
    *   outLayers - an array of layer handles
    *   outTypes - an array of composition types, each corresponding to an element
    *       of outLayers
    *
    * Returns HWC2_ERROR_NONE or one of the following errors:
    *   HWC2_ERROR_BAD_DISPLAY - an invalid display handle was passed in
    *   HWC2_ERROR_NOT_VALIDATED - validateDisplay has not been called for this
    *       display
    */
   int32_t /*hwc2_error_t*/ GET_CHANGED_COMPOSITION_TYPES(
           hwc2_display_t display,
           uint32_t* outNumElements, hwc2_layer_t* outLayers,
           int32_t* /*hwc2_composition_t*/ outTypes);

   /* getClientTargetSupport(..., width, height, format, dataspace)
    * Descriptor: HWC2_FUNCTION_GET_CLIENT_TARGET_SUPPORT
    * Must be provided by all HWC2 devices
    *
    * Returns whether a client target with the given properties can be handled by
    * the device.
    *
    * The valid formats can be found in android_pixel_format_t in
    * <system/graphics.h>.
    *
    * For more about dataspaces, see setLayerDataspace.
    *
    * This function must return true for a client target with width and height
    * equal to the active display configuration dimensions,
    * HAL_PIXEL_FORMAT_RGBA_8888, and HAL_DATASPACE_UNKNOWN. It is not required to
    * return true for any other configuration.
    *
    * Parameters:
    *   width - client target width in pixels
    *   height - client target height in pixels
    *   format - client target format
    *   dataspace - client target dataspace, as described in setLayerDataspace
    *
    * Returns HWC2_ERROR_NONE if the given configuration is supported or one of the
    * following errors:
    *   HWC2_ERROR_BAD_DISPLAY - an invalid display handle was passed in
    *   HWC2_ERROR_UNSUPPORTED - the given configuration is not supported
    */
   int32_t /*hwc2_error_t*/ GET_CLIENT_TARGET_SUPPORT(
           hwc2_display_t display, uint32_t width,
           uint32_t height, int32_t /*android_pixel_format_t*/ format,
           int32_t /*android_dataspace_t*/ dataspace);

   /* getColorModes(..., outNumModes, outModes)
    * Descriptor: HWC2_FUNCTION_GET_COLOR_MODES
    * Must be provided by all HWC2 devices
    *
    * Returns the color modes supported on this display.
    *
    * The valid color modes can be found in android_color_mode_t in
    * <system/graphics.h>. All HWC2 devices must support at least
    * HAL_COLOR_MODE_NATIVE.
    *
    * outNumModes may be NULL to retrieve the number of modes which will be
    * returned.
    *
    * Parameters:
    *   outNumModes - if outModes was NULL, the number of modes which would have
    *       been returned; if outModes was not NULL, the number of modes returned,
    *       which must not exceed the value stored in outNumModes prior to the
    *       call; pointer will be non-NULL
    *   outModes - an array of color modes
    *
    * Returns HWC2_ERROR_NONE or one of the following errors:
    *   HWC2_ERROR_BAD_DISPLAY - an invalid display handle was passed in
    */
   int32_t /*hwc2_error_t*/ GET_COLOR_MODES(
           hwc2_display_t display, uint32_t* outNumModes,
           int32_t* /*android_color_mode_t*/ outModes);

   /* getDisplayAttribute(..., config, attribute, outValue)
    * Descriptor: HWC2_FUNCTION_GET_DISPLAY_ATTRIBUTE
    * Must be provided by all HWC2 devices
    *
    * Returns a display attribute value for a particular display configuration.
    *
    * Any attribute which is not supported or for which the value is unknown by the
    * device must return a value of -1.
    *
    * Parameters:
    *   config - the display configuration for which to return attribute values
    *   attribute - the attribute to query
    *   outValue - the value of the attribute; the pointer will be non-NULL
    *
    * Returns HWC2_ERROR_NONE or one of the following errors:
    *   HWC2_ERROR_BAD_DISPLAY - an invalid display handle was passed in
    *   HWC2_ERROR_BAD_CONFIG - config does not name a valid configuration for this
    *       display
    */
   int32_t /*hwc2_error_t*/ GET_DISPLAY_ATTRIBUTE(
           hwc2_display_t display, hwc2_config_t config,
           int32_t /*hwc2_attribute_t*/ attribute, int32_t* outValue);

   /* getDisplayConfigs(..., outNumConfigs, outConfigs)
    * Descriptor: HWC2_FUNCTION_GET_DISPLAY_CONFIGS
    * Must be provided by all HWC2 devices
    *
    * Returns handles for all of the valid display configurations on this display.
    *
    * outConfigs may be NULL to retrieve the number of elements which will be
    * returned.
    *
    * Parameters:
    *   outNumConfigs - if outConfigs was NULL, the number of configurations which
    *       would have been returned; if outConfigs was not NULL, the number of
    *       configurations returned, which must not exceed the value stored in
    *       outNumConfigs prior to the call; pointer will be non-NULL
    *   outConfigs - an array of configuration handles
    *
    * Returns HWC2_ERROR_NONE or one of the following errors:
    *   HWC2_ERROR_BAD_DISPLAY - an invalid display handle was passed in
    */
   int32_t /*hwc2_error_t*/ GET_DISPLAY_CONFIGS(
           hwc2_display_t display, uint32_t* outNumConfigs,
           hwc2_config_t* outConfigs);

   /* getDisplayName(..., outSize, outName)
    * Descriptor: HWC2_FUNCTION_GET_DISPLAY_NAME
    * Must be provided by all HWC2 devices
    *
    * Returns a human-readable version of the display's name.
    *
    * outName may be NULL to retrieve the length of the name.
    *
    * Parameters:
    *   outSize - if outName was NULL, the number of bytes needed to return the
    *       name if outName was not NULL, the number of bytes written into it,
    *       which must not exceed the value stored in outSize prior to the call;
    *       pointer will be non-NULL
    *   outName - the display's name
    *
    * Returns HWC2_ERROR_NONE or one of the following errors:
    *   HWC2_ERROR_BAD_DISPLAY - an invalid display handle was passed in
    */
   int32_t /*hwc2_error_t*/ GET_DISPLAY_NAME(
           hwc2_display_t display, uint32_t* outSize,
           char* outName);

   /* getDisplayRequests(..., outDisplayRequests, outNumElements, outLayers,
    *     outLayerRequests)
    * Descriptor: HWC2_FUNCTION_GET_DISPLAY_REQUESTS
    * Must be provided by all HWC2 devices
    *
    * Returns the display requests and the layer requests required for the last
    * validated configuration.
    *
    * Display requests provide information about how the client should handle the
    * client target. Layer requests provide information about how the client
    * should handle an individual layer.
    *
    * If outLayers or outLayerRequests is NULL, the required number of layers and
    * requests must be returned in outNumElements, but this number may also be
    * obtained from validateDisplay as outNumRequests (outNumElements must be equal
    * to the value returned in outNumRequests from the last call to
    * validateDisplay).
    *
    * Parameters:
    *   outDisplayRequests - the display requests for the current validated state
    *   outNumElements - if outLayers or outLayerRequests were NULL, the number of
    *       elements which would have been returned, which must be equal to the
    *       value returned in outNumRequests from the last validateDisplay call on
    *       this display; if both were not NULL, the number of elements in
    *       outLayers and outLayerRequests, which must not exceed the value stored
    *       in outNumElements prior to the call; pointer will be non-NULL
    *   outLayers - an array of layers which all have at least one request
    *   outLayerRequests - the requests corresponding to each element of outLayers
    *
    * Returns HWC2_ERROR_NONE or one of the following errors:
    *   HWC2_ERROR_BAD_DISPLAY - an invalid display handle was passed in
    *   HWC2_ERROR_NOT_VALIDATED - validateDisplay has not been called for this
    *       display
    */
   int32_t /*hwc2_error_t*/ GET_DISPLAY_REQUESTS(
           hwc2_display_t display,
           int32_t* /*hwc2_display_request_t*/ outDisplayRequests,
           uint32_t* outNumElements, hwc2_layer_t* outLayers,
           int32_t* /*hwc2_layer_request_t*/ outLayerRequests);

   /* getDisplayType(..., outType)
    * Descriptor: HWC2_FUNCTION_GET_DISPLAY_TYPE
    * Must be provided by all HWC2 devices
    *
    * Returns whether the given display is a physical or virtual display.
    *
    * Parameters:
    *   outType - the type of the display; pointer will be non-NULL
    *
    * Returns HWC2_ERROR_NONE or one of the following errors:
    *   HWC2_ERROR_BAD_DISPLAY - an invalid display handle was passed in
    */
   int32_t /*hwc2_error_t*/ GET_DISPLAY_TYPE(
           hwc2_display_t display,
           int32_t* /*hwc2_display_type_t*/ outType);

   /* getDozeSupport(..., outSupport)
    * Descriptor: HWC2_FUNCTION_GET_DOZE_SUPPORT
    * Must be provided by all HWC2 devices
    *
    * Returns whether the given display supports HWC2_POWER_MODE_DOZE and
    * HWC2_POWER_MODE_DOZE_SUSPEND. DOZE_SUSPEND may not provide any benefit over
    * DOZE (see the definition of hwc2_power_mode_t for more information), but if
    * both DOZE and DOZE_SUSPEND are no different from HWC2_POWER_MODE_ON, the
    * device should not claim support.
    *
    * Parameters:
    *   outSupport - whether the display supports doze modes (1 for yes, 0 for no);
    *       pointer will be non-NULL
    *
    * Returns HWC2_ERROR_NONE or one of the following errors:
    *   HWC2_ERROR_BAD_DISPLAY - an invalid display handle was passed in
    */
   int32_t /*hwc2_error_t*/ GET_DOZE_SUPPORT(
           hwc2_display_t display, int32_t* outSupport);

   /* getHdrCapabilities(..., outNumTypes, outTypes, outMaxLuminance,
    *     outMaxAverageLuminance, outMinLuminance)
    * Descriptor: HWC2_FUNCTION_GET_HDR_CAPABILITIES
    * Must be provided by all HWC2 devices
    *
    * Returns the high dynamic range (HDR) capabilities of the given display, which
    * are invariant with regard to the active configuration.
    *
    * Displays which are not HDR-capable must return no types in outTypes and set
    * outNumTypes to 0.
    *
    * If outTypes is NULL, the required number of HDR types must be returned in
    * outNumTypes.
    *
    * Parameters:
    *   outNumTypes - if outTypes was NULL, the number of types which would have
    *       been returned; if it was not NULL, the number of types stored in
    *       outTypes, which must not exceed the value stored in outNumTypes prior
    *       to the call; pointer will be non-NULL
    *   outTypes - an array of HDR types, may have 0 elements if the display is not
    *       HDR-capable
    *   outMaxLuminance - the desired content maximum luminance for this display in
    *       cd/m^2; pointer will be non-NULL
    *   outMaxAverageLuminance - the desired content maximum frame-average
    *       luminance for this display in cd/m^2; pointer will be non-NULL
    *   outMinLuminance - the desired content minimum luminance for this display in
    *       cd/m^2; pointer will be non-NULL
    *
    * Returns HWC2_ERROR_NONE or one of the following errors:
    *   HWC2_ERROR_BAD_DISPLAY - an invalid display handle was passed in
    */
   int32_t /*hwc2_error_t*/ GET_HDR_CAPABILITIES(
           hwc2_display_t display, uint32_t* outNumTypes,
           int32_t* /*android_hdr_t*/ outTypes, float* outMaxLuminance,
           float* outMaxAverageLuminance, float* outMinLuminance);

   /* getReleaseFences(..., outNumElements, outLayers, outFences)
    * Descriptor: HWC2_FUNCTION_GET_RELEASE_FENCES
    * Must be provided by all HWC2 devices
    *
    * Retrieves the release fences for device layers on this display which will
    * receive new buffer contents this frame.
    *
    * A release fence is a file descriptor referring to a sync fence object which
    * will be signaled after the device has finished reading from the buffer
    * presented in the prior frame. This indicates that it is safe to start writing
    * to the buffer again. If a given layer's fence is not returned from this
    * function, it will be assumed that the buffer presented on the previous frame
    * is ready to be written.
    *
    * The fences returned by this function should be unique for each layer (even if
    * they point to the same underlying sync object), and ownership of the fences
    * is transferred to the client, which is responsible for closing them.
    *
    * If outLayers or outFences is NULL, the required number of layers and fences
    * must be returned in outNumElements.
    *
    * Parameters:
    *   outNumElements - if outLayers or outFences were NULL, the number of
    *       elements which would have been returned; if both were not NULL, the
    *       number of elements in outLayers and outFences, which must not exceed
    *       the value stored in outNumElements prior to the call; pointer will be
    *       non-NULL
    *   outLayers - an array of layer handles
    *   outFences - an array of sync fence file descriptors as described above,
    *       each corresponding to an element of outLayers
    *
    * Returns HWC2_ERROR_NONE or one of the following errors:
    *   HWC2_ERROR_BAD_DISPLAY - an invalid display handle was passed in
    */
   int32_t /*hwc2_error_t*/ GET_RELEASE_FENCES(
           hwc2_display_t display, uint32_t* outNumElements,
           hwc2_layer_t* outLayers, int32_t* outFences);

   /* presentDisplay(..., outRetireFence)
    * Descriptor: HWC2_FUNCTION_PRESENT_DISPLAY
    * Must be provided by all HWC2 devices
    *
    * Presents the current display contents on the screen (or in the case of
    * virtual displays, into the output buffer).
    *
    * Prior to calling this function, the display must be successfully validated
    * with validateDisplay. Note that setLayerBuffer and setLayerSurfaceDamage
    * specifically do not count as layer state, so if there are no other changes
    * to the layer state (or to the buffer's properties as described in
    * setLayerBuffer), then it is safe to call this function without first
    * validating the display.
    *
    * If this call succeeds, outRetireFence will be populated with a file
    * descriptor referring to a retire sync fence object. For physical displays,
    * this fence will be signaled when the result of composition of the prior frame
    * is no longer necessary (because it has been copied or replaced by this
    * frame). For virtual displays, this fence will be signaled when writes to the
    * output buffer have completed and it is safe to read from it.
    *
    * Parameters:
    *   outRetireFence - a sync fence file descriptor as described above; pointer
    *       will be non-NULL
    *
    * Returns HWC2_ERROR_NONE or one of the following errors:
    *   HWC2_ERROR_BAD_DISPLAY - an invalid display handle was passed in
    *   HWC2_ERROR_NO_RESOURCES - no valid output buffer has been set for a virtual
    *       display
    *   HWC2_ERROR_NOT_VALIDATED - validateDisplay has not successfully been called
    *       for this display
    */
   int32_t /*hwc2_error_t*/ PRESENT_DISPLAY(
           hwc2_display_t display, int32_t* outRetireFence);

   /* setActiveConfig(..., config)
    * Descriptor: HWC2_FUNCTION_SET_ACTIVE_CONFIG
    * Must be provided by all HWC2 devices
    *
    * Sets the active configuration for this display. Upon returning, the given
    * display configuration should be active and remain so until either this
    * function is called again or the display is disconnected.
    *
    * Parameters:
    *   config - the new display configuration
    *
    * Returns HWC2_ERROR_NONE or one of the following errors:
    *   HWC2_ERROR_BAD_DISPLAY - an invalid display handle was passed in
    *   HWC2_ERROR_BAD_CONFIG - the configuration handle passed in is not valid for
    *       this display
    */
   int32_t /*hwc2_error_t*/ SET_ACTIVE_CONFIG(
           hwc2_display_t display, hwc2_config_t config);

   /* setClientTarget(..., target, acquireFence, dataspace, damage)
    * Descriptor: HWC2_FUNCTION_SET_CLIENT_TARGET
    * Must be provided by all HWC2 devices
    *
    * Sets the buffer handle which will receive the output of client composition.
    * Layers marked as HWC2_COMPOSITION_CLIENT will be composited into this buffer
    * prior to the call to presentDisplay, and layers not marked as
    * HWC2_COMPOSITION_CLIENT should be composited with this buffer by the device.
    *
    * The buffer handle provided may be null if no layers are being composited by
    * the client. This must not result in an error (unless an invalid display
    * handle is also provided).
    *
    * Also provides a file descriptor referring to an acquire sync fence object,
    * which will be signaled when it is safe to read from the client target buffer.
    * If it is already safe to read from this buffer, -1 may be passed instead.
    * The device must ensure that it is safe for the client to close this file
    * descriptor at any point after this function is called.
    *
    * For more about dataspaces, see setLayerDataspace.
    *
    * The damage parameter describes a surface damage region as defined in the
    * description of setLayerSurfaceDamage.
    *
    * Will be called before presentDisplay if any of the layers are marked as
    * HWC2_COMPOSITION_CLIENT. If no layers are so marked, then it is not
    * necessary to call this function. It is not necessary to call validateDisplay
    * after changing the target through this function.
    *
    * Parameters:
    *   target - the new target buffer
    *   acquireFence - a sync fence file descriptor as described above
    *   dataspace - the dataspace of the buffer, as described in setLayerDataspace
    *   damage - the surface damage region
    *
    * Returns HWC2_ERROR_NONE or one of the following errors:
    *   HWC2_ERROR_BAD_DISPLAY - an invalid display handle was passed in
    *   HWC2_ERROR_BAD_PARAMETER - the new target handle was invalid
    */
   int32_t /*hwc2_error_t*/ SET_CLIENT_TARGET(
           hwc2_display_t display, buffer_handle_t target,
           int32_t acquireFence, int32_t /*android_dataspace_t*/ dataspace,
           hwc_region_t damage);

   /* setColorMode(..., mode)
    * Descriptor: HWC2_FUNCTION_SET_COLOR_MODE
    * Must be provided by all HWC2 devices
    *
    * Sets the color mode of the given display.
    *
    * Upon returning from this function, the color mode change must have fully
    * taken effect.
    *
    * The valid color modes can be found in android_color_mode_t in
    * <system/graphics.h>. All HWC2 devices must support at least
    * HAL_COLOR_MODE_NATIVE, and displays are assumed to be in this mode upon
    * hotplug.
    *
    * Parameters:
    *   mode - the mode to set
    *
    * Returns HWC2_ERROR_NONE or one of the following errors:
    *   HWC2_ERROR_BAD_DISPLAY - an invalid display handle was passed in
    *   HWC2_ERROR_BAD_PARAMETER - mode is not a valid color mode
    *   HWC2_ERROR_UNSUPPORTED - mode is not supported on this display
    */
   int32_t /*hwc2_error_t*/ SET_COLOR_MODE(
           hwc2_display_t display,
           int32_t /*android_color_mode_t*/ mode);

   /* setColorTransform(..., matrix, hint)
    * Descriptor: HWC2_FUNCTION_SET_COLOR_TRANSFORM
    * Must be provided by all HWC2 devices
    *
    * Sets a color transform which will be applied after composition.
    *
    * If hint is not HAL_COLOR_TRANSFORM_ARBITRARY, then the device may use the
    * hint to apply the desired color transform instead of using the color matrix
    * directly.
    *
    * If the device is not capable of either using the hint or the matrix to apply
    * the desired color transform, it should force all layers to client composition
    * during validateDisplay.
    *
    * The matrix provided is an affine color transformation of the following form:
    *
    * |r.r r.g r.b 0|
    * |g.r g.g g.b 0|
    * |b.r b.g b.b 0|
    * |Tr  Tg  Tb  1|
    *
    * This matrix will be provided in row-major form: {r.r, r.g, r.b, 0, g.r, ...}.
    *
    * Given a matrix of this form and an input color [R_in, G_in, B_in], the output
    * color [R_out, G_out, B_out] will be:
    *
    * R_out = R_in * r.r + G_in * g.r + B_in * b.r + Tr
    * G_out = R_in * r.g + G_in * g.g + B_in * b.g + Tg
    * B_out = R_in * r.b + G_in * g.b + B_in * b.b + Tb
    *
    * Parameters:
    *   matrix - a 4x4 transform matrix (16 floats) as described above
    *   hint - a hint value which may be used instead of the given matrix unless it
    *       is HAL_COLOR_TRANSFORM_ARBITRARY
    *
    * Returns HWC2_ERROR_NONE or one of the following errors:
    *   HWC2_ERROR_BAD_DISPLAY - an invalid display handle was passed in
    *   HWC2_ERROR_BAD_PARAMETER - hint is not a valid color transform hint
    */
   int32_t /*hwc2_error_t*/ SET_COLOR_TRANSFORM(
           hwc2_display_t display, const float* matrix,
           int32_t /*android_color_transform_t*/ hint);

   /* setOutputBuffer(..., buffer, releaseFence)
    * Descriptor: HWC2_FUNCTION_SET_OUTPUT_BUFFER
    * Must be provided by all HWC2 devices
    *
    * Sets the output buffer for a virtual display. That is, the buffer to which
    * the composition result will be written.
    *
    * Also provides a file descriptor referring to a release sync fence object,
    * which will be signaled when it is safe to write to the output buffer. If it
    * is already safe to write to the output buffer, -1 may be passed instead. The
    * device must ensure that it is safe for the client to close this file
    * descriptor at any point after this function is called.
    *
    * Must be called at least once before presentDisplay, but does not have any
    * interaction with layer state or display validation.
    *
    * Parameters:
    *   buffer - the new output buffer
    *   releaseFence - a sync fence file descriptor as described above
    *
    * Returns HWC2_ERROR_NONE or one of the following errors:
    *   HWC2_ERROR_BAD_DISPLAY - an invalid display handle was passed in
    *   HWC2_ERROR_BAD_PARAMETER - the new output buffer handle was invalid
    *   HWC2_ERROR_UNSUPPORTED - display does not refer to a virtual display
    */
   int32_t /*hwc2_error_t*/ SET_OUTPUT_BUFFER(
           hwc2_display_t display, buffer_handle_t buffer,
           int32_t releaseFence);

   /* setPowerMode(..., mode)
    * Descriptor: HWC2_FUNCTION_SET_POWER_MODE
    * Must be provided by all HWC2 devices
    *
    * Sets the power mode of the given display. The transition must be complete
    * when this function returns. It is valid to call this function multiple times
    * with the same power mode.
    *
    * All displays must support HWC2_POWER_MODE_ON and HWC2_POWER_MODE_OFF. Whether
    * a display supports HWC2_POWER_MODE_DOZE or HWC2_POWER_MODE_DOZE_SUSPEND may
    * be queried using getDozeSupport.
    *
    * Parameters:
    *   mode - the new power mode
    *
    * Returns HWC2_ERROR_NONE or one of the following errors:
    *   HWC2_ERROR_BAD_DISPLAY - an invalid display handle was passed in
    *   HWC2_ERROR_BAD_PARAMETER - mode was not a valid power mode
    *   HWC2_ERROR_UNSUPPORTED - mode was a valid power mode, but is not supported
    *       on this display
    */
   int32_t /*hwc2_error_t*/ SET_POWER_MODE(
           hwc2_display_t display,
           int32_t /*hwc2_power_mode_t*/ mode);

   /* setVsyncEnabled(..., enabled)
    * Descriptor: HWC2_FUNCTION_SET_VSYNC_ENABLED
    * Must be provided by all HWC2 devices
    *
    * Enables or disables the vsync signal for the given display. Virtual displays
    * never generate vsync callbacks, and any attempt to enable vsync for a virtual
    * display though this function must return HWC2_ERROR_NONE and have no other
    * effect.
    *
    * Parameters:
    *   enabled - whether to enable or disable vsync
    *
    * Returns HWC2_ERROR_NONE or one of the following errors:
    *   HWC2_ERROR_BAD_DISPLAY - an invalid display handle was passed in
    *   HWC2_ERROR_BAD_PARAMETER - enabled was an invalid value
    */
   int32_t /*hwc2_error_t*/ SET_VSYNC_ENABLED(
           hwc2_display_t display,
           int32_t /*hwc2_vsync_t*/ enabled);

   /* validateDisplay(..., outNumTypes, outNumRequests)
    * Descriptor: HWC2_FUNCTION_VALIDATE_DISPLAY
    * Must be provided by all HWC2 devices
    *
    * Instructs the device to inspect all of the layer state and determine if
    * there are any composition type changes necessary before presenting the
    * display. Permitted changes are described in the definition of
    * hwc2_composition_t above.
    *
    * Also returns the number of layer requests required
    * by the given layer configuration.
    *
    * Parameters:
    *   outNumTypes - the number of composition type changes required by the
    *       device; if greater than 0, the client must either set and validate new
    *       types, or call acceptDisplayChanges to accept the changes returned by
    *       getChangedCompositionTypes; must be the same as the number of changes
    *       returned by getChangedCompositionTypes (see the declaration of that
    *       function for more information); pointer will be non-NULL
    *   outNumRequests - the number of layer requests required by this layer
    *       configuration; must be equal to the number of layer requests returned
    *       by getDisplayRequests (see the declaration of that function for
    *       more information); pointer will be non-NULL
    *
    * Returns HWC2_ERROR_NONE if no changes are necessary and it is safe to present
    * the display using the current layer state. Otherwise returns one of the
    * following errors:
    *   HWC2_ERROR_BAD_DISPLAY - an invalid display handle was passed in
    *   HWC2_ERROR_HAS_CHANGES - outNumTypes was greater than 0 (see parameter list
    *       for more information)
    */
   int32_t /*hwc2_error_t*/ VALIDATE_DISPLAY(
           hwc2_display_t display,
           uint32_t* outNumTypes, uint32_t* outNumRequests);

   /*
    * Layer Functions
    *
    * These are functions which operate on layers, but which do not modify state
    * that must be validated before use. See also 'Layer State Functions' below.
    *
    * All of these functions take as their first three parameters a device pointer,
    * a display handle for the display which contains the layer, and a layer
    * handle, so these parameters are omitted from the described parameter lists.
    */

   /* setCursorPosition(..., x, y)
    * Descriptor: HWC2_FUNCTION_SET_CURSOR_POSITION
    * Must be provided by all HWC2 devices
    *
    * Asynchonously sets the position of a cursor layer.
    *
    * Prior to validateDisplay, a layer may be marked as HWC2_COMPOSITION_CURSOR.
    * If validation succeeds (i.e., the device does not request a composition
    * change for that layer), then once a buffer has been set for the layer and it
    * has been presented, its position may be set by this function at any time
    * between presentDisplay and any subsequent validateDisplay calls for this
    * display.
    *
    * Once validateDisplay is called, this function will not be called again until
    * the validate/present sequence is completed.
    *
    * May be called from any thread so long as it is not interleaved with the
    * validate/present sequence as described above.
    *
    * Parameters:
    *   x - the new x coordinate (in pixels from the left of the screen)
    *   y - the new y coordinate (in pixels from the top of the screen)
    *
    * Returns HWC2_ERROR_NONE or one of the following errors:
    *   HWC2_ERROR_BAD_DISPLAY - an invalid display handle was passed in
    *   HWC2_ERROR_BAD_LAYER - the layer is invalid or is not currently marked as
    *       HWC2_COMPOSITION_CURSOR
    *   HWC2_ERROR_NOT_VALIDATED - the device is currently in the middle of the
    *       validate/present sequence
    */
   int32_t /*hwc2_error_t*/ SET_CURSOR_POSITION(
           hwc2_display_t display, hwc2_layer_t layer,
           int32_t x, int32_t y);

   /* setLayerBuffer(..., buffer, acquireFence)
    * Descriptor: HWC2_FUNCTION_SET_LAYER_BUFFER
    * Must be provided by all HWC2 devices
    *
    * Sets the buffer handle to be displayed for this layer. If the buffer
    * properties set at allocation time (width, height, format, and usage) have not
    * changed since the previous frame, it is not necessary to call validateDisplay
    * before calling presentDisplay unless new state needs to be validated in the
    * interim.
    *
    * Also provides a file descriptor referring to an acquire sync fence object,
    * which will be signaled when it is safe to read from the given buffer. If it
    * is already safe to read from the buffer, -1 may be passed instead. The
    * device must ensure that it is safe for the client to close this file
    * descriptor at any point after this function is called.
    *
    * This function must return HWC2_ERROR_NONE and have no other effect if called
    * for a layer with a composition type of HWC2_COMPOSITION_SOLID_COLOR (because
    * it has no buffer) or HWC2_COMPOSITION_SIDEBAND or HWC2_COMPOSITION_CLIENT
    * (because synchronization and buffer updates for these layers are handled
    * elsewhere).
    *
    * Parameters:
    *   buffer - the buffer handle to set
    *   acquireFence - a sync fence file descriptor as described above
    *
    * Returns HWC2_ERROR_NONE or one of the following errors:
    *   HWC2_ERROR_BAD_LAYER - an invalid layer handle was passed in
    *   HWC2_ERROR_BAD_PARAMETER - the buffer handle passed in was invalid
    */
   int32_t /*hwc2_error_t*/ SET_LAYER_BUFFER(
           hwc2_display_t display, hwc2_layer_t layer,
           buffer_handle_t buffer, int32_t acquireFence);

   /* setLayerSurfaceDamage(..., damage)
    * Descriptor: HWC2_FUNCTION_SET_LAYER_SURFACE_DAMAGE
    * Must be provided by all HWC2 devices
    *
    * Provides the region of the source buffer which has been modified since the
    * last frame. This region does not need to be validated before calling
    * presentDisplay.
    *
    * Once set through this function, the damage region remains the same until a
    * subsequent call to this function.
    *
    * If damage.numRects > 0, then it may be assumed that any portion of the source
    * buffer not covered by one of the rects has not been modified this frame. If
    * damage.numRects == 0, then the whole source buffer must be treated as if it
    * has been modified.
    *
    * If the layer's contents are not modified relative to the prior frame, damage
    * will contain exactly one empty rect([0, 0, 0, 0]).
    *
    * The damage rects are relative to the pre-transformed buffer, and their origin
    * is the top-left corner. They will not exceed the dimensions of the latched
    * buffer.
    *
    * Parameters:
    *   damage - the new surface damage region
    *
    * Returns HWC2_ERROR_NONE or one of the following errors:
    *   HWC2_ERROR_BAD_LAYER - an invalid layer handle was passed in
    */
   int32_t /*hwc2_error_t*/ SET_LAYER_SURFACE_DAMAGE(
           hwc2_display_t display, hwc2_layer_t layer,
           hwc_region_t damage);

   /*
    * Layer State Functions
    *
    * These functions modify the state of a given layer. They do not take effect
    * until the display configuration is successfully validated with
    * validateDisplay and the display contents are presented with presentDisplay.
    *
    * All of these functions take as their first three parameters a device pointer,
    * a display handle for the display which contains the layer, and a layer
    * handle, so these parameters are omitted from the described parameter lists.
    */

   /* setLayerBlendMode(..., mode)
    * Descriptor: HWC2_FUNCTION_SET_LAYER_BLEND_MODE
    * Must be provided by all HWC2 devices
    *
    * Sets the blend mode of the given layer.
    *
    * Parameters:
    *   mode - the new blend mode
    *
    * Returns HWC2_ERROR_NONE or one of the following errors:
    *   HWC2_ERROR_BAD_LAYER - an invalid layer handle was passed in
    *   HWC2_ERROR_BAD_PARAMETER - an invalid blend mode was passed in
    */
   int32_t /*hwc2_error_t*/ SET_LAYER_BLEND_MODE(
           hwc2_display_t display, hwc2_layer_t layer,
           int32_t /*hwc2_blend_mode_t*/ mode);

   /* setLayerColor(..., color)
    * Descriptor: HWC2_FUNCTION_SET_LAYER_COLOR
    * Must be provided by all HWC2 devices
    *
    * Sets the color of the given layer. If the composition type of the layer is
    * not HWC2_COMPOSITION_SOLID_COLOR, this call must return HWC2_ERROR_NONE and
    * have no other effect.
    *
    * Parameters:
    *   color - the new color
    *
    * Returns HWC2_ERROR_NONE or one of the following errors:
    *   HWC2_ERROR_BAD_LAYER - an invalid layer handle was passed in
    */
   int32_t /*hwc2_error_t*/ SET_LAYER_COLOR(
           hwc2_display_t display, hwc2_layer_t layer,
           hwc_color_t color);

   /* setLayerCompositionType(..., type)
    * Descriptor: HWC2_FUNCTION_SET_LAYER_COMPOSITION_TYPE
    * Must be provided by all HWC2 devices
    *
    * Sets the desired composition type of the given layer. During validateDisplay,
    * the device may request changes to the composition types of any of the layers
    * as described in the definition of hwc2_composition_t above.
    *
    * Parameters:
    *   type - the new composition type
    *
    * Returns HWC2_ERROR_NONE or one of the following errors:
    *   HWC2_ERROR_BAD_LAYER - an invalid layer handle was passed in
    *   HWC2_ERROR_BAD_PARAMETER - an invalid composition type was passed in
    *   HWC2_ERROR_UNSUPPORTED - a valid composition type was passed in, but it is
    *       not supported by this device
    */
   int32_t /*hwc2_error_t*/ SET_LAYER_COMPOSITION_TYPE(
           hwc2_display_t display, hwc2_layer_t layer,
           int32_t /*hwc2_composition_t*/ type);

   /* setLayerDataspace(..., dataspace)
    * Descriptor: HWC2_FUNCTION_SET_LAYER_DATASPACE
    * Must be provided by all HWC2 devices
    *
    * Sets the dataspace that the current buffer on this layer is in.
    *
    * The dataspace provides more information about how to interpret the buffer
    * contents, such as the encoding standard and color transform.
    *
    * See the values of android_dataspace_t in <system/graphics.h> for more
    * information.
    *
    * Parameters:
    *   dataspace - the new dataspace
    *
    * Returns HWC2_ERROR_NONE or one of the following errors:
    *   HWC2_ERROR_BAD_LAYER - an invalid layer handle was passed in
    */
   int32_t /*hwc2_error_t*/ SET_LAYER_DATASPACE(
           hwc2_display_t display, hwc2_layer_t layer,
           int32_t /*android_dataspace_t*/ dataspace);

   /* setLayerDisplayFrame(..., frame)
    * Descriptor: HWC2_FUNCTION_SET_LAYER_DISPLAY_FRAME
    * Must be provided by all HWC2 devices
    *
    * Sets the display frame (the portion of the display covered by a layer) of the
    * given layer. This frame will not exceed the display dimensions.
    *
    * Parameters:
    *   frame - the new display frame
    *
    * Returns HWC2_ERROR_NONE or one of the following errors:
    *   HWC2_ERROR_BAD_LAYER - an invalid layer handle was passed in
    */
   int32_t /*hwc2_error_t*/ SET_LAYER_DISPLAY_FRAME(
           hwc2_display_t display, hwc2_layer_t layer,
           hwc_rect_t frame);

   /* setLayerPlaneAlpha(..., alpha)
    * Descriptor: HWC2_FUNCTION_SET_LAYER_PLANE_ALPHA
    * Must be provided by all HWC2 devices
    *
    * Sets an alpha value (a floating point value in the range [0.0, 1.0]) which
    * will be applied to the whole layer. It can be conceptualized as a
    * preprocessing step which applies the following function:
    *   if (blendMode == HWC2_BLEND_MODE_PREMULTIPLIED)
    *       out.rgb = in.rgb * planeAlpha
    *   out.a = in.a * planeAlpha
    *
    * If the device does not support this operation on a layer which is marked
    * HWC2_COMPOSITION_DEVICE, it must request a composition type change to
    * HWC2_COMPOSITION_CLIENT upon the next validateDisplay call.
    *
    * Parameters:
    *   alpha - the plane alpha value to apply
    *
    * Returns HWC2_ERROR_NONE or one of the following errors:
    *   HWC2_ERROR_BAD_LAYER - an invalid layer handle was passed in
    */
   int32_t /*hwc2_error_t*/ SET_LAYER_PLANE_ALPHA(
           hwc2_display_t display, hwc2_layer_t layer,
           float alpha);

   /* setLayerSidebandStream(..., stream)
    * Descriptor: HWC2_FUNCTION_SET_LAYER_SIDEBAND_STREAM
    * Provided by HWC2 devices which support HWC2_CAPABILITY_SIDEBAND_STREAM
    *
    * Sets the sideband stream for this layer. If the composition type of the given
    * layer is not HWC2_COMPOSITION_SIDEBAND, this call must return HWC2_ERROR_NONE
    * and have no other effect.
    *
    * Parameters:
    *   stream - the new sideband stream
    *
    * Returns HWC2_ERROR_NONE or one of the following errors:
    *   HWC2_ERROR_BAD_LAYER - an invalid layer handle was passed in
    *   HWC2_ERROR_BAD_PARAMETER - an invalid sideband stream was passed in
    */
   int32_t /*hwc2_error_t*/ SET_LAYER_SIDEBAND_STREAM(
           hwc2_display_t display, hwc2_layer_t layer,
           const native_handle_t* stream);

   /* setLayerSourceCrop(..., crop)
    * Descriptor: HWC2_FUNCTION_SET_LAYER_SOURCE_CROP
    * Must be provided by all HWC2 devices
    *
    * Sets the source crop (the portion of the source buffer which will fill the
    * display frame) of the given layer. This crop rectangle will not exceed the
    * dimensions of the latched buffer.
    *
    * If the device is not capable of supporting a true float source crop (i.e., it
    * will truncate or round the floats to integers), it should set this layer to
    * HWC2_COMPOSITION_CLIENT when crop is non-integral for the most accurate
    * rendering.
    *
    * If the device cannot support float source crops, but still wants to handle
    * the layer, it should use the following code (or similar) to convert to
    * an integer crop:
    *   intCrop.left = (int) ceilf(crop.left);
    *   intCrop.top = (int) ceilf(crop.top);
    *   intCrop.right = (int) floorf(crop.right);
    *   intCrop.bottom = (int) floorf(crop.bottom);
    *
    * Parameters:
    *   crop - the new source crop
    *
    * Returns HWC2_ERROR_NONE or one of the following errors:
    *   HWC2_ERROR_BAD_LAYER - an invalid layer handle was passed in
    */
   int32_t /*hwc2_error_t*/ SET_LAYER_SOURCE_CROP(
           hwc2_display_t display, hwc2_layer_t layer,
           hwc_frect_t crop);

   /* setLayerTransform(..., transform)
    * Descriptor: HWC2_FUNCTION_SET_LAYER_TRANSFORM
    * Must be provided by all HWC2 devices
    *
    * Sets the transform (rotation/flip) of the given layer.
    *
    * Parameters:
    *   transform - the new transform
    *
    * Returns HWC2_ERROR_NONE or one of the following errors:
    *   HWC2_ERROR_BAD_LAYER - an invalid layer handle was passed in
    *   HWC2_ERROR_BAD_PARAMETER - an invalid transform was passed in
    */
   int32_t /*hwc2_error_t*/ SET_LAYER_TRANSFORM(
           hwc2_display_t display, hwc2_layer_t layer,
           int32_t /*hwc_transform_t*/ transform);

   /* setLayerVisibleRegion(..., visible)
    * Descriptor: HWC2_FUNCTION_SET_LAYER_VISIBLE_REGION
    * Must be provided by all HWC2 devices
    *
    * Specifies the portion of the layer that is visible, including portions under
    * translucent areas of other layers. The region is in screen space, and will
    * not exceed the dimensions of the screen.
    *
    * Parameters:
    *   visible - the new visible region, in screen space
    *
    * Returns HWC2_ERROR_NONE or one of the following errors:
    *   HWC2_ERROR_BAD_LAYER - an invalid layer handle was passed in
    */
   int32_t /*hwc2_error_t*/ SET_LAYER_VISIBLE_REGION(
           hwc2_display_t display, hwc2_layer_t layer,
           hwc_region_t visible);

   /* setLayerZOrder(..., z)
    * Descriptor: HWC2_FUNCTION_SET_LAYER_Z_ORDER
    * Must be provided by all HWC2 devices
    *
    * Sets the desired Z order (height) of the given layer. A layer with a greater
    * Z value occludes a layer with a lesser Z value.
    *
    * Parameters:
    *   z - the new Z order
    *
    * Returns HWC2_ERROR_NONE or one of the following errors:
    *   HWC2_ERROR_BAD_LAYER - an invalid layer handle was passed in
    */
   int32_t /*hwc2_error_t*/ SET_LAYER_Z_ORDER(
           hwc2_display_t display, hwc2_layer_t layer,
           uint32_t z);

 private:
  SprdPrimaryDisplayDevice *mPrimaryDisplay;
  SprdExternalDisplayDevice *mExternalDisplay;
  SprdVirtualDisplayDevice *mVirtualDisplay;
  SprdDisplayCore *mDisplayCore;
  int mInitFlag;
  int mDebugFlag;
  int mDumpFlag;
  String8 mResult;

  int parseDisplayAttributes(const uint32_t *attributes, AttributesSet *dpyAttr, int32_t *value);

  int DevicePropertyProbe(SprdDisplayClient *Client);
};

#endif  // #ifndef _SPRD_HWCOMPOSER_H
