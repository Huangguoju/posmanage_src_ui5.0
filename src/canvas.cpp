#include <vector>
#include "canvas.h"
#include "mpi_sys.h"
#include "hi_tde_api.h"
#if !defined(D1004NR)
#include "hi_comm_ive.h"
#include "hi_ive.h"
#include "mpi_ive.h"
#endif


inline void drawAlphaPixel(uchar *pAddr, uchar fa, uchar fr, uchar fg, uchar fb)
{
    ushort *pMem = (ushort *)pAddr;
    fr >>= 3;
    fg >>= 3;
    fb >>= 3;

    uchar bb = *pMem & 0x001f;
    uchar bg = (*pMem & 0x03e0) >> 5;
    uchar br = (*pMem & 0x7c00) >> 10;
    uchar ba = (*pMem & 0x8000) >> 8;

    uchar compositedGreen = ((bg << 8) + (fg - bg) * fa) >> 8;
    *pAddr = (compositedGreen & 0x7) << 5 | ((bb << 8) + (fb - bb) * fa) >> 8;
    *(pAddr + 1) = ba | (((br << 8) + (fr - br) * fa) >> 8) << 2 | compositedGreen >> 3;
}

inline void drawSolidPixel(uchar *pAddr, uchar fr, uchar fg, uchar fb)
{
    *pAddr = (fg & 0x38) << 2 | fb >> 3;
    *(pAddr + 1) = 0x80 | (fr & 0xf8) >> 1 | fg >> 6;
}

inline void drawAlphaPixel_ARGB8888(uchar* addr, uchar fa, uchar fr, uchar fg,
                                     uchar fb)
{
    addr[2] = ((addr[0] << 8) + (fr - addr[0]) * fa) >> 8;
    addr[1] = ((addr[1] << 8) + (fg - addr[1]) * fa) >> 8;
    addr[0] = ((addr[2] << 8) + (fb - addr[2]) * fa) >> 8;
    int val = fa * addr[3];
    addr[3] = fa + addr[3] - ((val + (val >> 8) + 0x80) >> 8);
}

inline void drawSolidPixel_ARGB8888(uchar* addr, uchar r, uchar g, uchar b)
{
    addr[0] = b;
    addr[1] = g;
    addr[2] = r;
    addr[3] = 255;
}


///////////////////////////////////////////////////////////////////////////////////
Canvas::Canvas(int w, int h, bool bUseMmz)
    : mForeground(0, 0, 0),
      mBackground(0, 0, 0, 0),
      mWidth(w), mHeight(h),
#if defined(USE_ARGB8888)
    mLineLength(w * 4)
#else
    mLineLength(w * 2)
#endif
    , mCharSpacing(0)
{
    mPhyAddr = 0;
    m_pAddr = NULL;
    mSubPhyAddr = 0;
    m_pSubAddr = NULL;
    RS_U32 MemSize = mLineLength * h;
    if (bUseMmz)
    {
        HI_MPI_SYS_MmzAlloc(&mPhyAddr, (void**)&m_pAddr, NULL, RS_NULL, MemSize);
        HI_MPI_SYS_MmzAlloc(&mSubPhyAddr, (void **)&m_pSubAddr, NULL, RS_NULL, MemSize / 4);
    }

    Clear();
}

Canvas::~Canvas()
{
    HI_MPI_SYS_MmzFree(mPhyAddr, m_pAddr);
}

void Canvas::Sync()
{
    TDE_HANDLE s32Handle;
    TDE2_SURFACE_S stHiSrcSurface;
    TDE2_SURFACE_S stHiDstSurface;
    TDE2_RECT_S stHiSrcRect;
    TDE2_RECT_S stHiDstRect;
    HI_S32 s32Ret = HI_SUCCESS;

    memset(&stHiSrcSurface, 0, sizeof(stHiSrcSurface));
    memset(&stHiDstSurface, 0, sizeof(stHiDstSurface));

    stHiSrcSurface.u32PhyAddr = mPhyAddr;
    stHiSrcSurface.u32Width = mWidth;
    stHiSrcSurface.u32Height = mHeight;
    stHiSrcSurface.u32Stride = mWidth * 2;
    stHiSrcSurface.enColorFmt = TDE2_COLOR_FMT_ARGB1555;

    stHiDstSurface.u32PhyAddr = mSubPhyAddr;
    stHiDstSurface.u32Width = mWidth / 2;
    stHiDstSurface.u32Height = mHeight / 2;
    stHiDstSurface.u32Stride = stHiDstSurface.u32Width * 2;
    stHiDstSurface.enColorFmt = TDE2_COLOR_FMT_ARGB1555;

    stHiSrcRect.s32Xpos = 0;
    stHiSrcRect.s32Ypos = 0;
    stHiSrcRect.u32Width = mWidth;
    stHiSrcRect.u32Height = mHeight;

    stHiDstRect.s32Xpos = 0;
    stHiDstRect.s32Ypos = 0;
    stHiDstRect.u32Width = mWidth / 2;
    stHiDstRect.u32Height = mHeight / 2;

    /*1. open TDE*/
    s32Ret = HI_TDE2_Open();
    if (s32Ret != HI_SUCCESS)
    {
        printf("[Canvas]: HI_TDE2_Open failed !s32Ret = %x\n", s32Ret);
        return;
    }

    /*2.Begin a job*/
    s32Handle = HI_TDE2_BeginJob();
    if (HI_ERR_TDE_INVALID_HANDLE == s32Handle ||
        HI_ERR_TDE_DEV_NOT_OPEN == s32Handle)
    {
        printf("[Canvas]: HI_TDE2_Open failed !s32Ret = %x\n", s32Ret);
        HI_TDE2_Close();
        return;
    }
    /*3.Quick Copy*/
    s32Ret = HI_TDE2_QuickResize(s32Handle, &stHiSrcSurface, &stHiSrcRect, &stHiDstSurface, &stHiDstRect);
    if (s32Ret != HI_SUCCESS)
    {
        HI_TDE2_CancelJob(s32Handle);
        HI_TDE2_Close();
        printf("[Canvas]: HI_TDE2_Open failed !s32Ret = %x\n", s32Ret);
        return;
    }

    /*4.submit job*/
    HI_BOOL bSync = HI_FALSE;
    HI_BOOL bBlock = HI_TRUE;
    HI_U32 u32TimeOut = 100; // 100 ms

    s32Ret = HI_TDE2_EndJob(s32Handle, bSync, bBlock, u32TimeOut);
    if (s32Ret != HI_SUCCESS)
    {
        HI_TDE2_Close();
        printf("[Canvas]: HI_TDE2_Open failed !s32Ret = %x\n", s32Ret);
        return;
    }

    /*5.close TDE*/
    HI_TDE2_Close();
}

void Canvas::SolidFill(int x, int y, int w, int h, const Color &color)
{
#if defined(USE_ARGB8888)
    int r = color.Red();
    int g = color.Green();
    int b = color.Blue();
    int a = color.Alpha();

    int stride = w * 4;
    int step = mLineLength - stride;
    register uchar *pDst = m_pAddr + y * mLineLength
                           + x * 4;
    uchar *pMaxAddr = pDst + h * mLineLength;
    while (pDst < pMaxAddr) {
        register uchar *pDstMax = pDst + stride;
        while (pDst < pDstMax) {
            *pDst++ = b;
            *pDst++ = g;
            *pDst++ = r;
            *pDst++ = a;
        }
        pDst += step;
    }
#else

#if defined(D1004NR)
    int r = color.Red() >> 3;
    int g = color.Green() >> 3;
    int b = color.Blue() >> 3;
    int a = color.Alpha() ? 0x80 : 0x00;

    uchar lowByte = b | ((g & 0x07) << 5);
    uchar highByte = a | (r << 2) | (g >> 3);

    int stride = w * 2;
    int step = mLineLength - stride;
    register uchar *pDst = m_pAddr + y * mLineLength
                           + x * 2;
    uchar *pMaxAddr = pDst + h * mLineLength;
    while (pDst < pMaxAddr) {
        register uchar *pDstMax = pDst + stride;
        while (pDst < pDstMax) {
            *pDst++ = lowByte;
            *pDst++ = highByte;
        }
        pDst += step;
    }
#else
    (void_t)x;
    (void_t)y;
    (void_t)w;
    (void_t)h;
    (void)color;
    IVE_DATA_S DmaSrc;
    DmaSrc.u32PhyAddr = mPhyAddr;
    DmaSrc.pu8VirAddr = m_pAddr;
    DmaSrc.u16Stride = mLineLength;
    DmaSrc.u16Width = mWidth * 2;
    DmaSrc.u16Height = mHeight;

    IVE_DMA_CTRL_S DmaCtrl;
    DmaCtrl.enMode = IVE_DMA_MODE_SET_8BYTE;
    DmaCtrl.u64Val = 0;

    IVE_HANDLE Handle;
    HI_MPI_IVE_DMA(&Handle, &DmaSrc, NULL, &DmaCtrl, HI_FALSE);
#endif // defined(D1004NR)
    
#endif
}

void Canvas::Clear(const Color &color)
{
    if (color.isValid())
        SolidFill(0, 0, mWidth, mHeight, color);
    else
        SolidFill(0, 0, mWidth, mHeight, mBackground);

    Sync();
}

void Canvas::DrawText(int x, int y, const char *pString, RS_U8 font)
{
    if (!IsEmpty(pString)) {
		OsdTextInfo Info;
	    Info.UseBgColor = false;
	    Info.string = pString;
		
	    if(Info.string == NULL || strlen(Info.string) == 0)
	        return;
        RSFontMetrics textInfo(Info.string, font);
	    const RSVector<const GlyphBitmap *> glyphVct = textInfo.glyphBitmap();
	    int fontSize = glyphVct.size();

	    int wordIndex = 0;
	    while (wordIndex < fontSize)
	    {
	        int w = glyphVct[wordIndex]->sizeInfo.width;
	        int h = glyphVct[wordIndex]->sizeInfo.height;
	        int rx = glyphVct[wordIndex]->sizeInfo.x;
	        int bytePerLine = glyphVct[wordIndex]->sizeInfo.bytesPerLine;
	        int xoffset = glyphVct[wordIndex]->sizeInfo.advance;
	        int curX = x + rx;
            int curY = y + 16 - glyphVct[wordIndex]->sizeInfo.yUpper;
	        uchar* addr = m_pAddr + curY * mLineLength + curX * 2;
	        uchar r = mForeground.Red();
	        uchar g = mForeground.Green();
	        uchar b = mForeground.Blue();

	        for(int j = 0; j < h; ++j)
	        {
	            for(int i = 0; i < w; ++i)
	            {
	                int pixIndex = j * bytePerLine + i;
	                int px = curX + i;
	                int py = curY + j;
	                if (px >= 0 && px < mWidth && py >= 0 && py < mHeight)
	                {
	                    int a = glyphVct[wordIndex]->fontMetrics[pixIndex];
	                    switch (a)
	                    {
	                    case 0:
	                        break;
	                    case 255:
	                        drawSolidPixel(&addr[i * 2], r, g, b);
	                        break;
	                    default:
                            drawSolidPixel(&addr[i * 2], 45, 45, 45);
	                        drawAlphaPixel(&addr[i * 2], a, r, g, b);
	                    }
	                }
	            }
	            addr += mLineLength;
	        }

	        x += xoffset;
	        wordIndex++;
	    }

    }
}

void Canvas::Trim(char *text)
{
    int Length = strlen(text) + 1;
    if (Length <= 1)
        return;

    std::vector<char> txt(Length);
    char *pBuf = txt.data();
    memcpy(pBuf, text, Length);
    char *pStart = pBuf, *pEnd = pBuf + Length;
    while (pStart < pEnd && IsSpace(*pStart))
        pStart++;

    while (pEnd > pStart && IsSpace(*pEnd))
        pEnd--;

    Length = pEnd - pStart + 1;
    memcpy(text, pStart, Length);
    text[Length] = 0;
}

bool Canvas::IsSpace(const char ch) const
{
    if ((ch == '\t') || (ch == '\n') || (ch == '\v') || (ch == '\f')
            || (ch == '\r') || (ch == ' '))
        return true;

    return false;
}
