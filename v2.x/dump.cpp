#include "dump.h"
#include <cutils/properties.h>
#include <hardware/hardware.h>
#include <hardware/hwcomposer.h>
#include <ui/Rect.h>
#include <ui/GraphicBufferMapper.h>
#include "AndroidFence.h"
#include "SprdHWLayer.h"

//static char valuePath[PROPERTY_VALUE_MAX];

static int64_t GeometryChangedNum = 0;
static bool GeometryChanged = false;
static bool GeometryChangedFirst = false;
char dumpPath[MAX_DUMP_PATH_LENGTH];
int g_debugFlag = 0;
using namespace android;

static int dump_bmp(const char* filename, void* buffer_addr, unsigned int buffer_format, unsigned int buffer_compress, unsigned int buffer_width, unsigned int buffer_height, unsigned int buffer_header_size)
{
    FILE* fp;
    WORD bfType;
    BITMAPINFO bmInfo;
    RGBQUAD quad;
    int ret = 0;
    fp = fopen(filename, "wb");
    if(!fp)
    {
        ret = -1;
        goto fail_open;
    }
    bfType = 0x4D42;

    memset(&bmInfo, 0, sizeof(BITMAPINFO));

    bmInfo.bmfHeader.bfOffBits = sizeof(WORD) + sizeof(BITMAPINFO);
    bmInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmInfo.bmiHeader.biWidth = buffer_width;
    bmInfo.bmiHeader.biHeight = -buffer_height;
    bmInfo.bmiHeader.biPlanes = 1;

    switch (buffer_format)
    {
    case HAL_PIXEL_FORMAT_RGB_565:
        bmInfo.bmfHeader.bfOffBits += 4*sizeof(U32);
        bmInfo.bmiHeader.biBitCount = 16;
        bmInfo.bmiHeader.biCompression = BI_BITFIELDS;
        quad.rgbRedMask      = 0x001F;
        quad.rgbGreenMask    = 0x07E0;
        quad.rgbBlueMask     = 0xF800;
        quad.rgbReservedMask = 0;
        bmInfo.bmiHeader.biSizeImage = buffer_width * buffer_height * sizeof(U16);
        if (buffer_compress)
            bmInfo.bmiHeader.biSizeImage += buffer_header_size;
        break;

    case HAL_PIXEL_FORMAT_RGBA_8888:
        bmInfo.bmfHeader.bfOffBits += 4*sizeof(U32);
        bmInfo.bmiHeader.biBitCount = 32;
        bmInfo.bmiHeader.biCompression = BI_BITFIELDS;
        quad.rgbRedMask      = 0x00FF0000;
        quad.rgbGreenMask    = 0x0000FF00;
        quad.rgbBlueMask     = 0x000000FF;
        quad.rgbReservedMask = 0xFF000000;
        bmInfo.bmiHeader.biSizeImage = buffer_width * buffer_height * sizeof(U32);
        if (buffer_compress)
            bmInfo.bmiHeader.biSizeImage += buffer_header_size;
        break;
    case HAL_PIXEL_FORMAT_RGBX_8888:/*not sure need investigation*/
        bmInfo.bmfHeader.bfOffBits += 4*sizeof(U32);
        bmInfo.bmiHeader.biBitCount = 32;
        bmInfo.bmiHeader.biCompression = BI_BITFIELDS;
        quad.rgbRedMask      = 0x00FF0000;
        quad.rgbGreenMask    = 0x0000FF00;
        quad.rgbBlueMask     = 0x000000FF;
        quad.rgbReservedMask = 0x00000000;
        bmInfo.bmiHeader.biSizeImage = buffer_width * buffer_height * sizeof(U32);
        if (buffer_compress)
            bmInfo.bmiHeader.biSizeImage += buffer_header_size;
        break;
    case 	HAL_PIXEL_FORMAT_BGRA_8888:/*not sure need investigation*/
        bmInfo.bmfHeader.bfOffBits += 4*sizeof(U32);
        bmInfo.bmiHeader.biBitCount = 32;
        bmInfo.bmiHeader.biCompression = BI_BITFIELDS;
        quad.rgbRedMask      = 0x000000FF;
        quad.rgbGreenMask    = 0x0000FF00;
        quad.rgbBlueMask     = 0x00FF0000;
        quad.rgbReservedMask = 0xFF000000;
        bmInfo.bmiHeader.biSizeImage = buffer_width * buffer_height * sizeof(U32);
        if (buffer_compress)
            bmInfo.bmiHeader.biSizeImage += buffer_header_size;
        break;
    case HAL_PIXEL_FORMAT_RGB_888:/*not sure need investigation*/
        bmInfo.bmfHeader.bfOffBits += 4*sizeof(U32);
        bmInfo.bmiHeader.biBitCount = 24;
        bmInfo.bmiHeader.biCompression = BI_BITFIELDS;
        quad.rgbRedMask      = 0x000000FF;
        quad.rgbGreenMask    = 0x0000FF00;
        quad.rgbBlueMask     = 0x00FF0000;
        quad.rgbReservedMask = 0x00000000;
        bmInfo.bmiHeader.biSizeImage = buffer_width * buffer_height * sizeof(U8) * 3;
        if (buffer_compress)
            bmInfo.bmiHeader.biSizeImage += buffer_header_size;
        break;
#if 0
    /* do not support HAL_PIXEL_FORMAT_RGBA_5551 */
    case HAL_PIXEL_FORMAT_RGBA_5551: /*not sure need investigation*/
        bmInfo.bmfHeader.bfOffBits += 4*sizeof(U32);
        bmInfo.bmiHeader.biBitCount = 16;
        bmInfo.bmiHeader.biCompression = BI_BITFIELDS;
        quad.rgbRedMask      = 0x000000FF;
        quad.rgbGreenMask    = 0x0000FF00;
        quad.rgbBlueMask     = 0x00FF0000;
        quad.rgbReservedMask = 0x00000000;
        bmInfo.bmiHeader.biSizeImage = buffer_width * buffer_height * sizeof(U8) * 2;
        break;
    case HAL_PIXEL_FORMAT_RGBA_4444:/*not sure need investigation*/
        bmInfo.bmfHeader.bfOffBits += 4*sizeof(U32);
        bmInfo.bmiHeader.biBitCount = 16;
        bmInfo.bmiHeader.biCompression = BI_BITFIELDS;
        quad.rgbRedMask      = 0x000000FF;
        quad.rgbGreenMask    = 0x0000FF00;
        quad.rgbBlueMask     = 0x00FF0000;
        quad.rgbReservedMask = 0x00000000;
        bmInfo.bmiHeader.biSizeImage = buffer_width * buffer_height * sizeof(U8) * 2;
        break;
#endif
    case HAL_PIXEL_FORMAT_YCbCr_420_SP:
    case HAL_PIXEL_FORMAT_YCrCb_420_SP:
    case HAL_PIXEL_FORMAT_YCbCr_420_888:
    case HAL_PIXEL_FORMAT_YCbCr_420_P:
    case HAL_PIXEL_FORMAT_YV12:
    case HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED:
        bmInfo.bmfHeader.bfOffBits += 256*sizeof(U32);
        bmInfo.bmiHeader.biBitCount = 8;
        bmInfo.bmiHeader.biCompression = BI_RGB;
        {
            for(int i=0; i<256; i++)
            {
                quad.table[i].rgbRed      = i;
                quad.table[i].rgbGreen    = i;
                quad.table[i].rgbBlue     = i;
                quad.table[i].rgbReserved = 0;
            }
        }
        bmInfo.bmiHeader.biSizeImage = (buffer_width * buffer_height * sizeof(U8) * 3)>>1;
        if (buffer_compress)
            bmInfo.bmiHeader.biSizeImage += buffer_header_size;
        break;

    default:
        assert(false);
    }

    bmInfo.bmfHeader.bfSize = bmInfo.bmfHeader.bfOffBits + bmInfo.bmiHeader.biSizeImage;

    switch (buffer_format)
    {
    case HAL_PIXEL_FORMAT_RGB_565:
    case HAL_PIXEL_FORMAT_RGBA_8888:
    case HAL_PIXEL_FORMAT_RGB_888:
    case HAL_PIXEL_FORMAT_BGRA_8888:
#if 0
    case HAL_PIXEL_FORMAT_RGBA_5551:
    case HAL_PIXEL_FORMAT_RGBA_4444:
#endif
    case HAL_PIXEL_FORMAT_RGBX_8888:
        if (!buffer_compress) {
            fwrite(&bfType, sizeof(WORD), 1, fp);
            fwrite(&bmInfo, sizeof(BITMAPINFO), 1, fp);
            fwrite(&quad, 4*sizeof(U32), 1, fp);
        }
        break;
    case HAL_PIXEL_FORMAT_YCbCr_420_SP:
    case HAL_PIXEL_FORMAT_YCrCb_420_SP:
    case HAL_PIXEL_FORMAT_YCbCr_420_P:
    case HAL_PIXEL_FORMAT_YCbCr_420_888:
    case HAL_PIXEL_FORMAT_YV12:
    case HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED:
        //fwrite(&quad, 256*sizeof(U32), 1, fp);
        break;
    }
    fwrite(buffer_addr, bmInfo.bmiHeader.biSizeImage, 1, fp);
    fclose(fp);
    return ret;
fail_open:
    ALOGE("dump layer failed to open path is:%s" , filename);
    return ret;
}
static int dump_layer(const char* path ,const char* pSrc , const char* ptype ,  int width , int height , int format , int comperss, int header_size, int randNum ,  int index , int LayerIndex = 0) {
    char fileName[MAX_DUMP_PATH_LENGTH + MAX_DUMP_FILENAME_LENGTH];
    static int cnt = 0;
    switch(format)
    {
        case HAL_PIXEL_FORMAT_RGBA_8888:
            if (comperss)
                sprintf(fileName , "%s%d_%d_%s_%d_rgba_%dx%d_%d.raw" ,path, cnt,randNum , ptype , LayerIndex , width, height,index);
            else
                sprintf(fileName , "%s%d_%d_%s_%d_rgba_%dx%d_%d.bmp" ,path, cnt,randNum , ptype , LayerIndex , width, height,index);
            break;
        case HAL_PIXEL_FORMAT_RGBX_8888:
            if (comperss)
                sprintf(fileName , "%s%d_%d_%s_%d_rgbx_%dx%d_%d.raw" ,path, cnt,randNum , ptype , LayerIndex , width, height,index);
            else
                sprintf(fileName , "%s%d_%d_%s_%d_rgbx_%dx%d_%d.bmp" ,path, cnt,randNum , ptype , LayerIndex , width, height,index);
            break;
        case HAL_PIXEL_FORMAT_BGRA_8888:
            if (comperss)
                sprintf(fileName , "%s%d_%d_%s_%d_bgra_%dx%d_%d.raw" ,path, cnt,randNum , ptype , LayerIndex ,width, height,index);
            else
                sprintf(fileName , "%s%d_%d_%s_%d_bgra_%dx%d_%d.bmp" ,path, cnt,randNum , ptype , LayerIndex ,width, height,index);
            break;
        case HAL_PIXEL_FORMAT_RGB_888:
            if (comperss)
                sprintf(fileName , "%s%d_%d_%s_%d_rgb888_%dx%d_%d.raw" ,path, cnt,randNum , ptype , LayerIndex ,width, height,index);
            else
                sprintf(fileName , "%s%d_%d_%s_%d_rgb888_%dx%d_%d.bmp" ,path, cnt,randNum , ptype , LayerIndex ,width, height,index);
            break;
#if 0
        case HAL_PIXEL_FORMAT_RGBA_5551:
            sprintf(fileName , "%s%d_%d_%s_%d_rgba5551_%dx%d_%d.bmp" ,path, cnt,randNum , ptype , LayerIndex , width, height,index);
            break;
        case HAL_PIXEL_FORMAT_RGBA_4444:
            sprintf(fileName , "%s%d_%d_%s_%d_rgba4444_%dx%d_%d.bmp" ,path,cnt, randNum , ptype , LayerIndex ,width, height,index);
            break;
#endif
        case HAL_PIXEL_FORMAT_RGB_565:
            if (comperss)
                sprintf(fileName , "%s%d_%d_%s_%d_rgb565_%dx%d_%d.raw" ,path,cnt, randNum , ptype , LayerIndex , width, height,index);
            else
                sprintf(fileName , "%s%d_%d_%s_%d_rgb565_%dx%d_%d.bmp" ,path,cnt, randNum , ptype , LayerIndex , width, height,index);
            break;
        case HAL_PIXEL_FORMAT_YCbCr_420_SP:
            if (comperss)
                sprintf(fileName , "%s%d_%d_%s_%d_ybrsp_%dx%d_%d.raw" ,path,cnt, randNum , ptype , LayerIndex , width, height,index);
            else
                sprintf(fileName , "%s%d_%d_%s_%d_ybrsp_%dx%d_%d.yuv" ,path,cnt, randNum , ptype , LayerIndex , width, height,index);
            break;
        case HAL_PIXEL_FORMAT_YCrCb_420_SP:
        case HAL_PIXEL_FORMAT_YCbCr_420_888:
        case HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED:
            if (comperss)
                sprintf(fileName , "%s%d_%d_%s_%d_yrbsp_%dx%d_%d.raw" ,path,cnt, randNum , ptype , LayerIndex , width, height,index);
            else
                sprintf(fileName , "%s%d_%d_%s_%d_yrbsp_%dx%d_%d.yuv" ,path,cnt, randNum , ptype , LayerIndex , width, height,index);
            break;
        case HAL_PIXEL_FORMAT_YV12:
            if (comperss)
                sprintf(fileName , "%s%d_%d_%s_%d_yv12_%dx%d_%d.yuv" ,path, cnt,randNum , ptype , LayerIndex , width, height,index);
            else
                sprintf(fileName , "%s%d_%d_%s_%d_yv12_%dx%d_%d.yuv" ,path, cnt,randNum , ptype , LayerIndex , width, height,index);
            break;
        case HAL_PIXEL_FORMAT_YCbCr_420_P:
            if (comperss)
                sprintf(fileName , "%s%d_%d_%s_%d_ybrp_%dx%d_%d.yuv" ,path, cnt,randNum , ptype , LayerIndex , width, height,index);
            else
                sprintf(fileName , "%s%d_%d_%s_%d_ybrp_%dx%d_%d.yuv" ,path, cnt,randNum , ptype , LayerIndex , width, height,index);
            break;
        default:
            ALOGE("dump layer failed because of error format %d" , format);
            return -2;
    }
    cnt++;
    return dump_bmp(fileName , (void*)pSrc, format, comperss, width, height, header_size);
}

static int getDumpPath(char *pPath)
{
    int mDebugFlag = 0;
    char value[PROPERTY_VALUE_MAX];

    queryDebugFlag(&mDebugFlag);

    if(0 == property_get("debug.hwc.dumppath" , value , "0")) {
        ALOGE_IF(mDebugFlag, "fail to getDumpPath not set path");
        return -1;
    }
    if(strchr(value , '/') != NULL) {
        sprintf(pPath , "%s" , value);
		ALOGE_IF(mDebugFlag, "getDumpPath %s",pPath);
        return 0;
    } else
        pPath[0] = 0;
    ALOGE_IF(mDebugFlag, "fail to getDumpPath path format error");
    return -2;
}

void queryDebugFlag(int *debugFlag)
{
    char value[PROPERTY_VALUE_MAX];
    static int openFileFlag = 0;

    if (debugFlag == NULL)
    {
        ALOGE("queryDebugFlag, input parameter is NULL");
        return;
    }

    property_get("debug.hwc.info", value, "0");

    if (atoi(value) == 1)
    {
        *debugFlag = 1;
    }
    if (atoi(value) == 2)
    {
        *debugFlag = 0;
    }
    g_debugFlag = *debugFlag;
#define HWC_LOG_PATH "/data/hwc.cfg"
    if (access(HWC_LOG_PATH, R_OK) != 0)
    {
        return;
    }

    FILE *fp = NULL;
    char * pch;
    char cfg[100];

    fp = fopen(HWC_LOG_PATH, "r");
    if (fp != NULL)
    {
        if (openFileFlag == 0)
        {
            int ret;
            memset(cfg, '\0', 100);
            ret = fread(cfg, 1, 99, fp);
            if (ret < 1) {
                ALOGE("fread return size is wrong %d", ret);
            }
            cfg[sizeof(cfg) - 1] = 0;
            pch = strstr(cfg, "enable");
            if (pch != NULL)
            {
                *debugFlag = 1;
                openFileFlag = 1;
            }
        }
        else
        {
            *debugFlag = 1;
        }
        fclose(fp);
    }
}

void queryDumpFlag(int *dumpFlag)
{
    if (dumpFlag == NULL)
    {
        ALOGE("queryDumpFlag, input parameter is NULL");
        return;
    }

    char value[PROPERTY_VALUE_MAX];

    if (0 != property_get("debug.hwc.dumpflag", value, "0"))
    {
        int flag =atoi(value);

        if (flag != 0)
        {
            *dumpFlag = flag;
        }
        else
        {
            *dumpFlag = 0;
        }
    }
    else
    {
        *dumpFlag = 0;
    }
}


void queryIntFlag(const char* strProperty,int *IntFlag)
{
    if (IntFlag == NULL || strProperty == NULL)
    {
        ALOGE("queryIntFlag, input parameter is NULL");
        return;
    }

    char value[PROPERTY_VALUE_MAX];

    if (0 != property_get(strProperty, value, "0"))
    {
        int flag =atoi(value);

        if (flag != 0)
        {
            *IntFlag = flag;
        }
        else
        {
            *IntFlag = 0;
        }
    }
    else
    {
        *IntFlag = 0;
    }
}


int dumpImage(LIST& list)
{
    static int index = 0;

/*
    if (list->flags & HWC_GEOMETRY_CHANGED)
    {
        if (GeometryChangedFirst == false)
        {
            GeometryChangedFirst = true;
            GeometryChangedNum = 0;
        }
        else
        {
            GeometryChangedNum++;
        }
        GeometryChanged = true;
    }
    else
    {
        GeometryChanged = false;
    }*/
    GeometryChanged = true;

    getDumpPath(dumpPath);
    if (GeometryChanged)
    {
        index = 0;
    }

    for (size_t i =0; i < list.size(); i++)
    {
        SprdHWLayer *l = list[i];
        native_handle_t *pH = l->getBufferHandle();
        if (pH == NULL)
        {
            continue;
        }

        Rect bounds(ADP_STRIDE(pH), ADP_HEIGHT(pH));
        void* vaddr;

        waitAcquireFence(list);
        GraphicBufferMapper::get().lock((buffer_handle_t)pH, GRALLOC_USAGE_SW_READ_OFTEN, bounds, &vaddr);

        dump_layer(dumpPath, (char *)vaddr, "Layer", ADP_STRIDE(pH), ADP_HEIGHT(pH),
			ADP_FORMAT(pH), ADP_COMPRESSED(pH), ADP_HEADERSIZER(pH), GeometryChangedNum, index, i);

        GraphicBufferMapper::get().unlock((buffer_handle_t)pH);
    }

    index++;

    return 0;
}

int dumpOverlayImage(native_handle_t* buffer, const char *name, int fencefd)
{
    static int index = 0;

    getDumpPath(dumpPath);

    Rect bounds(ADP_STRIDE(buffer), ADP_HEIGHT(buffer));
    void* vaddr;

    String8 fence_name(name);
    FenceWaitForever(fence_name, fencefd);

    GraphicBufferMapper::get().lock((buffer_handle_t)buffer, GRALLOC_USAGE_SW_READ_OFTEN, bounds, &vaddr);

    dump_layer(dumpPath, (char const*)vaddr, name,
                   ADP_STRIDE(buffer), ADP_HEIGHT(buffer),
                   ADP_FORMAT(buffer), ADP_COMPRESSED(buffer), ADP_HEADERSIZER(buffer), 0, index);

    GraphicBufferMapper::get().unlock((buffer_handle_t)buffer);

    index++;

    return 0;
}


void dumpFrameBuffer(SprdHWLayer *fb)
{
    static int index = 0;

    if (fb == NULL)
    {
        ALOGE("fb layer is NULL, cannot dump");
        return;
    }

    native_handle_t *pH = fb->getBufferHandle();
    if (pH == NULL)
    {
        ALOGE("fb handle is NULL, cannot dump");
        return;
    }

    Rect bounds(ADP_STRIDE(pH), ADP_HEIGHT(pH));
    void* vaddr;

    String8 fence_name("FBT");
    FenceWaitForever(fence_name, fb->getAcquireFence());
    GraphicBufferMapper::get().lock((buffer_handle_t)pH, GRALLOC_USAGE_SW_READ_OFTEN, bounds, &vaddr);

    getDumpPath(dumpPath);

    dump_layer(dumpPath, (char *)vaddr, "Fbt", ADP_STRIDE(pH), ADP_HEIGHT(pH),
	ADP_FORMAT(pH), ADP_COMPRESSED(pH), ADP_HEADERSIZER(pH), 0, index, 0);

    GraphicBufferMapper::get().unlock((buffer_handle_t)pH);

    index++;
}
void headdump(String8& result)
{
  result.append("-------------------------------------------------------");
  result.append("------------------------------------------------------\n");
  result.append(" comp type |   format | fbc | pitch | height | transform |    blend |");
  result.append(" alpha | zorder |   dl   dt   dr   db |\n");
  result.append("-------------------------------------------------------");
  result.append("------------------------------------------------------\n");
}
void dumpacce(SprdHWLayer* HWLayerCurrent, String8& result)
{
  if(HWLayerCurrent)
  {
    switch(HWLayerCurrent->getAccelerator())
    {
      case ACCELERATOR_DISPC: result.append("       DPU |"); break;
      case ACCELERATOR_GSP: result.append("       GSP |"); break;
      default: result.append("       GPU |"); break;
    }
  }
}
void dumpformat(SprdHWLayer* HWLayerCurrent, String8& result)
{
  if(HWLayerCurrent)
  {
    switch(HWLayerCurrent->getLayerFormat())
    {
      case HAL_PIXEL_FORMAT_RGBA_8888: result.append(" RGBA8888 |"); break;
      case HAL_PIXEL_FORMAT_RGBX_8888: result.append(" RGBX8888 |"); break;
      case HAL_PIXEL_FORMAT_RGB_565: result.append("  RGB565  |"); break;
      case HAL_PIXEL_FORMAT_YCbCr_422_SP: result.append(" YCbCr422 |"); break;
      case HAL_PIXEL_FORMAT_YCrCb_422_SP: result.append(" YCrCb422 |"); break;
      case HAL_PIXEL_FORMAT_YCbCr_420_SP: result.append(" YCbCr420 |"); break;
      case HAL_PIXEL_FORMAT_YCrCb_420_SP: result.append(" YCrCb420 |"); break;
      case HAL_PIXEL_FORMAT_YCbCr_420_888: result.append(" YCbCr420_888 |"); break;
      case HAL_PIXEL_FORMAT_YV12: result.append("     YV12 |"); break;
      default: result.append("    OTHER |"); break;
    }
  }
}
void dumptransform(SprdHWLayer* HWLayerCurrent, String8& result)
{
  if(HWLayerCurrent)
  {
    switch(HWLayerCurrent->getTransform())
    {
      case HAL_TRANSFORM_FLIP_H: result.append("    FLIP_H |"); break;
      case HAL_TRANSFORM_FLIP_V: result.append("    FLIP_V |"); break;
      case HAL_TRANSFORM_ROT_90: result.append("        90 |"); break;
      case HAL_TRANSFORM_ROT_180: result.append("       180 |"); break;
      case HAL_TRANSFORM_ROT_270: result.append("       270 |"); break;
      case (HAL_TRANSFORM_ROT_90 | HAL_TRANSFORM_FLIP_H): result.append(" 90&FLIP_H |"); break;
      case (HAL_TRANSFORM_ROT_90 | HAL_TRANSFORM_FLIP_V): result.append(" 90&FLIP_V |"); break;
      default: result.append("         0 |"); break;
    }
  }
}
void dumpblend(SprdHWLayer* HWLayerCurrent, String8& result)
{
  if(HWLayerCurrent)
  {
    switch(HWLayerCurrent->getBlendMode())
    {
      case SPRD_HWC_BLENDING_NONE: result.append("     NONE |"); break;
      case SPRD_HWC_BLENDING_PREMULT: result.append("  PREMULT |"); break;
      case SPRD_HWC_BLENDING_COVERAGE: result.append(" COVERAGE |"); break;
      default: result.append("     NONE |"); break;
    }
  }
}
void dumpalpha(SprdHWLayer* HWLayerCurrent, String8& result)
{
  if(HWLayerCurrent)
  {
    result.appendFormat("   %3u |", HWLayerCurrent->getPlaneAlpha());
  }
}
void dumpzorder(SprdHWLayer* HWLayerCurrent,String8& result)
{
  if(HWLayerCurrent)
  {
    result.appendFormat("     %2u |", HWLayerCurrent->getZOrder());
  }
}
void dumpinfbc(SprdHWLayer* HWLayerCurrent, String8& result)
{
  if(HWLayerCurrent)
  {
    native_handle_t *handle = HWLayerCurrent->getBufferHandle();
    if(handle)
    {
      bool inFBC = ADP_COMPRESSED(handle);
      if(inFBC) result.append("   Y |");
      else result.append("   N |");
    }
    else result.append("     |");
  }
}
void dumpdamageregion(SprdHWLayer* HWLayerCurrent, String8& result)
{
  if(HWLayerCurrent)
  {
    DamageRegion_t *damageregion = HWLayerCurrent->getDamageRegion();
    if(damageregion)
    {
      if((*damageregion).rects)
      result.appendFormat(" %4u %4u %4u %4u |", (*damageregion).rects->left,(*damageregion).rects->top,(*damageregion).rects->right,(*damageregion).rects->bottom);
    }
  }
}
void dumpvisibleregion(SprdHWLayer* HWLayerCurrent, String8& result)
{
}
void dumppitch(SprdHWLayer* HWLayerCurrent, String8& result)
{
  if(HWLayerCurrent)
  {
    native_handle_t *handle = HWLayerCurrent->getBufferHandle();
    if(handle)
    {
      uint32_t pitch = ADP_STRIDE(handle);
      result.appendFormat(" %5u |", pitch);
    }
    else result.append("       |");
  }
}
void dumpheight(SprdHWLayer* HWLayerCurrent, String8& result)
{
  if(HWLayerCurrent)
  {
    native_handle_t *handle = HWLayerCurrent->getBufferHandle();
    if(handle)
    {
      bool inFBC = ADP_COMPRESSED(handle);
      uint32_t height = (inFBC ? ADP_VSTRIDE(handle) : ADP_HEIGHT(handle));
      result.appendFormat(" %6u |", height);
    }
    else result.append("        |");
  }
}
void dumpinput(SprdHWLayer* HWLayerCurrent, String8& result)
{
  if(HWLayerCurrent)
  {
    dumpacce(HWLayerCurrent, result);
    dumpformat(HWLayerCurrent, result);
    dumpinfbc(HWLayerCurrent, result);
    dumppitch(HWLayerCurrent, result);
    dumpheight(HWLayerCurrent, result);
    dumptransform(HWLayerCurrent, result);
    dumpblend(HWLayerCurrent, result);
    dumpalpha(HWLayerCurrent, result);
    dumpzorder(HWLayerCurrent, result);
    dumpdamageregion(HWLayerCurrent, result);
    result.append("\n");
    result.append("-------------------------------------------------------");
    result.append("------------------------------------------------------\n");
  }
}
void dumpout(SprdHWLayer* HWLayerCurrent, String8& result)
{
  if(HWLayerCurrent)
  {
    dumpformat(HWLayerCurrent, result);
    dumpinfbc(HWLayerCurrent, result);
    dumppitch(HWLayerCurrent, result);
    dumpheight(HWLayerCurrent, result);
    result.append("\n");
  }
}
