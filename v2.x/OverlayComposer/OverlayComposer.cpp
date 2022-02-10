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
 ** 16/08/2013    Hardware Composer   Add a new feature to Harware composer,  *
 **                                   verlayComposer use GPU to do the        *
 **                                   Hardware layer blending on Overlay      *
 **                                   buffer, and then post the OVerlay       *
 **                                   buffer to Display                       *
 ******************************************************************************
 ** Author:         zhongjun.chen@spreadtrum.com                              *
 *****************************************************************************/

#include "OverlayComposer.h"
#include "GLErro.h"
#include "Layer.h"
#include "SyncThread.h"


namespace android
{


OverlayComposer::OverlayComposer(SprdDisplayPlane *displayPlane, sp<OverlayNativeWindow> NativeWindow)
    : mDisplayPlane(displayPlane),
      mDebugFlag(0),
      mList(NULL),
      mNumLayer(0),
      mReleaseFenceFd(-1),
      InitFlag(0),
      mWindow(NativeWindow),
      mDisplay(EGL_NO_DISPLAY), mSurface(EGL_NO_SURFACE),
      mContext(EGL_NO_CONTEXT),
      mConfig(0),
      mFlags(0),
      mMaxTextureSize(0),
      mWormholeTexName(-1),
      mProtectedTexName(-1),
      mOVCFBTargetLayer(NULL),
      DeinitFlag(false)
{
    sem_init(&cmdSem, 0, 0);
    sem_init(&doneSem, 0, 0);
    sem_init(&displaySem, 0, 0);

    InitSem();
    ALOGD("OverlayComposer::OverlayComposer done.");
};


OverlayComposer::~OverlayComposer()
{
    /* delete some layer object from list
     * Now, these object are useless
     * */
    for (DrawLayerList::iterator it = mDrawLayerList.begin();
         it != mDrawLayerList.end(); it++)
    {
        Layer *mL = *it;
        delete mL;
        mL = NULL;
        //mDrawLayerList.erase(it);
    }
    mDrawLayerList.clear();

    sem_destroy(&cmdSem);
    sem_destroy(&doneSem);
    sem_destroy(&displaySem);
    ALOGD("OverlayComposer::~OverlayComposer done.");
}

void OverlayComposer::onFirstRef()
{
    run("OverlayComposer", PRIORITY_URGENT_DISPLAY + PRIORITY_MORE_FAVORABLE);
}

status_t OverlayComposer::readyToRun()
{
    bool ret = initEGL();
    if (!ret)
    {
        ALOGE("Init EGL ENV failed");
        return -1;
    }

    initOpenGLES();

    return NO_ERROR;
}

bool OverlayComposer::threadLoop()
{

    sem_wait(&cmdSem);

    if(mExitPending)
    {
        deInitOpenGLES();
        deInitEGL();
	 DeinitFlag = true;
        mExitPending = false;
        return false;
    }

    composerHWLayers();

    //glFinish();

    /* *******************************
     * waiting display
     * *******************************/
    //sem_wait(&displaySem);
    swapBuffers();

    sem_post(&doneSem);

    return true;
}

void OverlayComposer::requestThreadLoopExit()
{
    if (DeinitFlag == true)
    {
        return;
    }

    mExitPending = true;
    sem_post(&cmdSem);
}

status_t OverlayComposer::selectConfigForPixelFormat(
        EGLDisplay dpy,
        EGLint const* attrs,
        PixelFormat format,
        EGLConfig* outConfig)
{
    EGLConfig config = NULL;
    EGLint numConfigs = -1, n=0;
    eglGetConfigs(dpy, NULL, 0, &numConfigs);
    EGLConfig* const configs = new EGLConfig[numConfigs];
    eglChooseConfig(dpy, attrs, configs, numConfigs, &n);
    for (int i=0 ; i<n ; i++) {
        EGLint nativeVisualId = 0;
        eglGetConfigAttrib(dpy, configs[i], EGL_NATIVE_VISUAL_ID, &nativeVisualId);
        if (nativeVisualId>0 && format == nativeVisualId) {
            *outConfig = configs[i];
            delete [] configs;
            return NO_ERROR;
        }
    }
    delete [] configs;
    return NAME_NOT_FOUND;
}

bool OverlayComposer::initEGL()
{

    if (mWindow == NULL)
    {
        ALOGE("NativeWindow is NULL");
        return false;
    }

    int format;
    ANativeWindow const * const window = mWindow.get();
    window->query(window, NATIVE_WINDOW_FORMAT, &format);

    EGLint w, h, dummy;
    EGLint numConfigs=0;
    EGLSurface surface;
    EGLContext context;
    EGLBoolean result;
    status_t err;

    // initialize EGL
    EGLint attribs[] = {
            EGL_RENDERABLE_TYPE, EGL_OPENGL_ES_BIT,
            EGL_SURFACE_TYPE,       EGL_WINDOW_BIT,
            EGL_RED_SIZE, 8,
            EGL_GREEN_SIZE, 8,
            EGL_BLUE_SIZE, 8,
            EGL_NONE,               0,
            EGL_NONE
    };


    EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    checkEGLErrors("eglGetDisplay");
    eglInitialize(display, NULL, NULL);
    eglGetConfigs(display, NULL, 0, &numConfigs);

    EGLConfig config = NULL;
    err = selectConfigForPixelFormat(display, attribs, format, &config);
    ALOGE_IF(err, "couldn't find an EGLConfig matching the screen format");

    EGLint r,g,b,a;
    eglGetConfigAttrib(display, config, EGL_RED_SIZE,   &r);
    eglGetConfigAttrib(display, config, EGL_GREEN_SIZE, &g);
    eglGetConfigAttrib(display, config, EGL_BLUE_SIZE,  &b);
    eglGetConfigAttrib(display, config, EGL_ALPHA_SIZE, &a);

    //if (window->isUpdateOnDemand()) {
    //    mFlags |= PARTIAL_UPDATES;
    //}

    //if (eglGetConfigAttrib(display, config, EGL_CONFIG_CAVEAT, &dummy) == EGL_TRUE) {
    //   if (dummy == EGL_SLOW_CONFIG)
    //       mFlags |= SLOW_CONFIG;
    //}


     /*
     * Create our main surface
     */

    surface = eglCreateWindowSurface(display, config, mWindow.get(), NULL);
    checkEGLErrors("eglCreateWindowSurface");

    //if (mFlags & PARTIAL_UPDATES) {
    //    // if we have partial updates, we definitely don't need to
    //    // preserve the backbuffer, which may be costly.
    //    eglSurfaceAttrib(display, surface,
    //            EGL_SWAP_BEHAVIOR, EGL_BUFFER_DESTROYED);
    //}

    /*
     * Create our OpenGL ES context
     */
//#define EGL_IMG_context_priority
//#define HAS_CONTEXT_PRIORITY
    EGLint contextAttributes[] = {
#ifdef EGL_IMG_context_priority
#ifdef HAS_CONTEXT_PRIORITY
#warning "using EGL_IMG_context_priority"
        EGL_CONTEXT_PRIORITY_LEVEL_IMG, EGL_CONTEXT_PRIORITY_HIGH_IMG,
#endif
#endif
        EGL_NONE, EGL_NONE
    };
    context = eglCreateContext(display, config, NULL, contextAttributes);
    checkEGLErrors("eglCreateContext");
    mDisplay = display;
    mConfig  = config;
    mSurface = surface;
    mContext = context;
    //mFormat  = ;
    //mPageFlipCount = 0;

    /*
     * Gather OpenGL ES extensions
     */

    result = eglMakeCurrent(display, surface, surface, context);
    checkEGLErrors("eglMakeCurrent");
    if (!result) {
        ALOGE("Couldn't create a working GLES context. check logs. exiting...");
        return false;
    }

    //GLExtensions& extensions(GLExtensions::getInstance());
    //extensions.initWithGLStrings(
    //        glGetString(GL_VENDOR),
    //        glGetString(GL_RENDERER),
    //        glGetString(GL_VERSION),
    //        glGetString(GL_EXTENSIONS),
    //        eglQueryString(display, EGL_VENDOR),
    //        eglQueryString(display, EGL_VERSION),
    //        eglQueryString(display, EGL_EXTENSIONS));

    //glGetIntegerv(GL_MAX_TEXTURE_SIZE, &mMaxTextureSize);
    //glGetIntegerv(GL_MAX_VIEWPORT_DIMS, mMaxViewportDims);

    //ALOGI("EGL informations:");
    //ALOGI("# of configs : %d", numConfigs);
    //ALOGI("vendor    : %s", extensions.getEglVendor());
    //ALOGI("version   : %s", extensions.getEglVersion());
    //ALOGI("extensions: %s", extensions.getEglExtension());
    //ALOGI("Client API: %s", eglQueryString(display, EGL_CLIENT_APIS)?:"Not Supported");
    //ALOGI("EGLSurface: %d-%d-%d-%d, config=%p", r, g, b, a, config);

    //ALOGI("OpenGL informations:");
    //ALOGI("vendor    : %s", extensions.getVendor());
    //ALOGI("renderer  : %s", extensions.getRenderer());
    //ALOGI("version   : %s", extensions.getVersion());
    //ALOGI("extensions: %s", extensions.getExtension());
    //ALOGI("GL_MAX_TEXTURE_SIZE = %d", mMaxTextureSize);
    //ALOGI("GL_MAX_VIEWPORT_DIMS = %d x %d", mMaxViewportDims[0], mMaxViewportDims[1]);
    //ALOGI("flags = %08x", mFlags);

    // Unbind the context from this thread
    //eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);

    ALOGD("OverlayComposer::initEGL done.");
    return true;
}

void OverlayComposer::deInitEGL()
{
    eglMakeCurrent(mDisplay, EGL_NO_SURFACE,
                   EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglDestroyContext(mDisplay, mContext);
    eglDestroySurface(mDisplay, mSurface);
    eglTerminate(mDisplay);
    mDisplay = EGL_NO_DISPLAY;
    ALOGD("OverlayComposer::deInitEGL done.");
}

bool OverlayComposer::initOpenGLES()
{
    // Initialize OpenGL|ES
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    glPixelStorei(GL_PACK_ALIGNMENT, 4);
    glEnableClientState(GL_VERTEX_ARRAY);
    glShadeModel(GL_FLAT);
    glDisable(GL_DITHER);
    glDisable(GL_CULL_FACE);

    const uint16_t g0 = pack565(0x0F,0x1F,0x0F);
    const uint16_t g1 = pack565(0x17,0x2f,0x17);
    const uint16_t wormholeTexData[4] = { g0, g1, g1, g0 };
    glGenTextures(1, &mWormholeTexName);
    glBindTexture(GL_TEXTURE_2D, mWormholeTexName);
    glTexParameterx(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameterx(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameterx(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameterx(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 2, 2, 0,
            GL_RGB, GL_UNSIGNED_SHORT_5_6_5, wormholeTexData);

    const uint16_t protTexData[] = { pack565(0x03, 0x03, 0x03) };
    glGenTextures(1, &mProtectedTexName);
    glBindTexture(GL_TEXTURE_2D, mProtectedTexName);
    glTexParameterx(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameterx(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameterx(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameterx(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 1, 1, 0,
            GL_RGB, GL_UNSIGNED_SHORT_5_6_5, protTexData);

    unsigned int mFBWidth  = mDisplayPlane->getWidth();
    unsigned int mFBHeight = mDisplayPlane->getHeight();

    glViewport(0, 0, mFBWidth, mFBHeight);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    // put the origin in the left-bottom corner
    glOrthof(0, mFBWidth, 0, mFBHeight, 0, 1);
    // l=0, r=w ; b=0, t=h
    ALOGD("SPRD_SR OverlayComposer::initOpenGLES done.");

    return true;
}

void OverlayComposer::deInitOpenGLES()
{
    glDeleteTextures(1, &mWormholeTexName);
    glDeleteTextures(1, &mProtectedTexName);
}

void OverlayComposer::caculateLayerRect(SprdHWLayer *l, struct sprdRectF *rect, sprdRect *rV)
{
    if (l == NULL || rect == NULL)
    {
        ALOGE("overlayDevice::caculateLayerRect, input parameters is NULL");
        return;
    }

    native_handle_t *private_h = l->getBufferHandle();

    if (private_h == NULL)
    {
        ALOGE("overlayDevice::caculateLayerRect, buffer handle is NULL");
        return;
    }
    float sourceLeft   = l->getSprdSRCRectF()->left;
    float sourceTop    = l->getSprdSRCRectF()->top;
    float sourceRight  = l->getSprdSRCRectF()->right;
    float sourceBottom = l->getSprdSRCRectF()->bottom;


    rect->left = MAX(sourceLeft, 0);
    rect->top = MAX(sourceTop, 0);
    rect->right = MIN(sourceRight, ADP_WIDTH(private_h));
    rect->bottom = MIN(sourceBottom, ADP_HEIGHT(private_h));

    rV->left   = l->getSprdFBRect()->left;
    rV->top    = l->getSprdFBRect()->top;
    rV->right  = l->getSprdFBRect()->right;
    rV->bottom = l->getSprdFBRect()->bottom;
}

void OverlayComposer::ClearOverlayComposerBuffer()
{
    static GLfloat vertices[] = {
        0.0f, 0.0f,
        0.0f, 0.0f,
        0.0f,  0.0f,
        0.0f,  0.0f
    };

    glColor4f(0.0f, 0.0f, 0.0f, 1.0f);
    glDisable(GL_TEXTURE_EXTERNAL_OES);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_BLEND);

    glVertexPointer(2, GL_FLOAT, 0, vertices);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
}

/*
void printRegion(hwc_region_t *region, const char *name)
{
	if(region==NULL ||name==NULL)
	{
		ALOGE("OverlayComposer: region:%p, name:%p is NULL!", region, name);
		return;
	}
	if(region->numRects>0 && region->rects==NULL)
	{
		ALOGE("OverlayComposer:region->numRects:%zu > 0, but region->rects:%p is NULL!", region->numRects,region->rects);
		return;
	}

	for(int i=0;i<region->numRects;i++)
	{
		ALOGI_IF(mDebugFlag ,"OverlayComposer:%s[%d]{%d,%d,%d,%d}",name,i,region->rects[i].left,region->rects[i].top,region->rects[i].right,region->rects[i].bottom);
	}
}
*/

void OverlayComposer::orRect(sprdRegion_t *selfRect, const sprdRegion_t *newRect)
{
	if(selfRect==NULL ||newRect==NULL)
	{
		ALOGE("OverlayComposer:selfRect:%p newRect:%p", selfRect, newRect);
		return;
	}

	ALOGI_IF(mDebugFlag,"OverlayComposer:orRect {%d,%d,%d,%d} | {%d,%d,%d,%d} => {%d,%d,%d,%d}",
			selfRect->left,selfRect->top,selfRect->right,selfRect->bottom,
			newRect->left,newRect->top,newRect->right,newRect->bottom,
			MIN(selfRect->left, newRect->left),MIN(selfRect->top, newRect->top),
			MAX(selfRect->right, newRect->right),MAX(selfRect->bottom, newRect->bottom));
	selfRect->left = MIN(selfRect->left, newRect->left);
	selfRect->top = MIN(selfRect->top, newRect->top);
	selfRect->right = MAX(selfRect->right, newRect->right);
	selfRect->bottom= MAX(selfRect->bottom, newRect->bottom);
}

void OverlayComposer::orRegion(sprdRegion_t *selfRect, VisibleRegion_t *region)
{
	if(region==NULL ||selfRect==NULL)
	{
		ALOGE("OverlayComposer:orRegion() region:%p selfRect:%p", region, selfRect);
		return;
	}
	if(region->numRects>0 && region->rects==NULL)
	{
		ALOGE("OverlayComposer:orRegion() region->numRects:%zu region->rects:%p", region->numRects,region->rects);
		return;
	}
	ALOGI_IF(mDebugFlag,"\n\nOverlayComposer:orRegion() region->numRects:%d",region->numRects);
	for(size_t i=0;i<region->numRects;i++)
	{
		orRect(selfRect, &region->rects[i]);
	}
}

/*
func:clearOSDTarget
desc: on iwhale2, OSD menu remain here when user want it to be hidden. so we will clear these dirty regions to fix it.
method: define a static rect_fullscreen and treat it as a full screen rect, overlay visibleRegionScreen of each layer to get a rect_sum,
             if rect_sum is smaller than rect_fullscreen, then we clear the target buffer.
             in struct hwc_layer_1 defination, there are detailed explaination about visibleRegionScreen surfaceDamage sourceCropf.
*/
int OverlayComposer::clearOSDTarget()
{
	sprdRegion_t rect_sum = {{8192}, {8192}, 0, 0, 0, 0};
	static sprdRegion_t rect_fullscreen = {{8192}, {8192}, 0, 0, 0, 0};
	//static int glClearCount = 0;

	if (mList == NULL)
	{
		ALOGE("The HWC List is NULL");
		return -1;
	}

	if (mNumLayer <= 0)
	{
		ALOGE("Cannot find HWC layers");
		return -1;
	}
	/*
	if(mList->numHwLayers > 2)
	{
		glClearCount=0;
	}
	*/
	for (unsigned int i = 0; i < mNumLayer; i++)
	{
		SprdHWLayer  *pL = mList[i];
		if (pL == NULL)
		{
			ALOGI_IF(mDebugFlag,"OverlayComposer:Layers[%d] is null", i);
			continue;
		}

		if (pL->InitCheck() == false)
		{
			ALOGI_IF(mDebugFlag,"OverlayComposer:Layers[%d]'s compositionType is not HWC_OVERLAY", i);
			continue;
		}

		if (pL->getCompositionType()== COMPOSITION_SOLID_COLOR)
		{
			ALOGI_IF(mDebugFlag,"OverlayComposer:Layers[%d]'s compositionType is SOLID_COLOR", i);
			continue;
		}


		//printRegion(&pL->visibleRegionScreen, "visibleRegionScreen");
		ALOGI_IF(mDebugFlag,"OverlayComposer:Layers[%d] added to rect_sum", i);
		orRegion(&rect_sum, pL->getVisibleRegion());
	}

	orRect(&rect_fullscreen, &rect_sum);

	if(/*glClearCount < PLANE_BUFFER_NUMBER
		&& */(rect_sum.left > rect_fullscreen.left
		|| rect_sum.top > rect_fullscreen.top
		|| rect_sum.right < rect_fullscreen.right
		|| rect_sum.bottom < rect_fullscreen.bottom))
	{
		ALOGI_IF(mDebugFlag,"OverlayComposer:clearOSDTarget {%d,%d,%d,%d} < {%d,%d,%d,%d}, glClear!",
				rect_sum.left,rect_sum.top,rect_sum.right,rect_sum.bottom,
				rect_fullscreen.left,rect_fullscreen.top,rect_fullscreen.right,rect_fullscreen.bottom);
		glColor4f(0.0f, 0.0f, 0.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);
		//glClearCount++;
	}
	return 0;
}

int OverlayComposer::composerHWLayers()
{
    int status = -1;
    uint32_t numLayer = 0;

    if (mList == NULL)
    {
        ALOGE("The HWC List is NULL");
        status = -1;
        return status;
    }

    numLayer = mNumLayer;
    if (numLayer <= 0)
    {
        ALOGE("Cannot find HWC layers");
        status = -1;
        return status;
    }

#ifdef TARGET_GPU_PLATFORM
#if (TARGET_GPU_PLATFORM == rogue)
    clearOSDTarget();
#endif
#endif

    /*
     *  Window Manager will do the Rotation Animation,
     *  here skip the Rotation Animation frame.
     * */
    //if ((mList->flags & HWC_ANIMATION_ROTATION_END) != HWC_ANIMATION_ROTATION_END)
    //{
    //    //ALOGI("Skip Rotation Animation");
    //    status = 0;
    //    return status;
    //}

    bool  ovc_skip_flag = false;
    for (uint32_t i = 0; i < numLayer; i++)
    {
        SprdHWLayer  *pL = mList[i];
        if (pL == NULL)
        {
            //numLayer--;
            //ALOGE("Find %dth layer is NULL", i);
            continue;
        }

        if (pL->InitCheck() == false)
        {
            //numLayer--;
            //ALOGE("Find %dth layer InitCheck failed", i);
            if(ovc_skip_flag == true)
                continue;
            ovc_skip_flag = true;
            pL = mOVCFBTargetLayer;
        }

        native_handle_t *pH = pL->getBufferHandle();
        if (pH == NULL)
        {
           //ALOGD("%dth Layer handle is NULL", i);
            //numLayer--;
            continue;
        }

        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();

        Layer *L = new Layer(this, pH, pL->getAcquireFence());//will wait acq fence
        if (L == NULL)
        {
            ALOGE("The %dth Layer object is NULL", numLayer);
            status = -1;
            return status;
        }

        struct sprdRectF r;
        struct sprdRect  rV;

        memset(&r, 0, sizeof(struct sprdRect));
        caculateLayerRect(pL, &r, &rV);

        L->setLayerTransform(pL->getTransform());
        L->setLayerRect(&r, &rV);
        L->setLayerAlpha(pL->getPlaneAlphaF());
        L->setBlendFlag(pL->getBlendMode());

        L->draw();

        /*
         * Store the Layer object to a list
         **/
        mDrawLayerList.push_back(L);
    }


    status = 0;

    return status;
}

bool OverlayComposer::onComposer(SprdHWLayer** list, uint32_t LayerNum,
             SprdHWLayer *FBTargetLayer)
{
    if (list == NULL)
    {
        ALOGE("hwc_layer_list is NULL");
        return false;
    }

    queryDebugFlag(&mDebugFlag);

    mList       = list;
    mNumLayer = LayerNum;
    mOVCFBTargetLayer = FBTargetLayer;

    /*
     *  Send signal to composer thread to start
     *  composer work.
     * */
    sem_post(&cmdSem);

    /*
     *  Waiting composer work done.
     * */
    sem_wait(&doneSem);

    return true;
}

void OverlayComposer::onDisplay()
{
    //exhaustAllSem();

    //sem_wait(&displaySem);

    /*
     *  Sync thread.
     *  return until OverlayNativeWindow::queueBuffer finished
     * */
    //semWaitTimedOut(1000);
}

bool OverlayComposer::swapBuffers()
{
    eglSwapBuffers(mDisplay, mSurface);

    /*
     *  Here, We generate a release fence fd for all Source Android
     *  Layers
     * */
    mReleaseFenceFd = Layer::getReleaseFenceFd();
    ALOGI_IF(g_debugFlag,"<02-2> OverlayComposerScheldule() return, src rlsFd:%d",
            mReleaseFenceFd);
    /* delete some layer object from list
     * Now, these object are useless
     * */
    for (DrawLayerList::iterator it = mDrawLayerList.begin();
         it != mDrawLayerList.end(); it++)
    {
        Layer *mL = *it;
        delete mL;
        mL = NULL;
        //mDrawLayerList.erase(it);
    }
    mDrawLayerList.clear();

    return true;
}

int  OverlayComposer::getReleaseFence()
{
    int fenceFd = -1;
    if (mReleaseFenceFd >= 0)
    {
        fenceFd = dup(mReleaseFenceFd);
        ALOGI_IF(g_debugFlag,"<02-2> SF get src rlsFd:%d=dup(%d) from OverlayComposerThread, close(%d)",
            fenceFd,mReleaseFenceFd,mReleaseFenceFd);
        close(mReleaseFenceFd);
        mReleaseFenceFd = -1;
    }

    return fenceFd;
}

};

