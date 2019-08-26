#ifndef CANVAS_H
#define CANVAS_H

#include "rs_type.h"
#include "cosd.h"
#include "rsfontmetrics.h"

//#define USE_ARGB8888
struct OsdTextInfo
{
    int x;
    int y;           /*坐标*/
    const char *string; /*字符串*/
    color_t color;      /*字的颜色*/
    color_t bg;         /*背景的颜色*/
    bool UseBgColor;    /*是否使用背景色*/
};

class Color
{
public:
    Color() : mRed(0), mGreen(0), mBlue(0), mAlpha(0), m_bValid(false)
    { }

    Color(uchar r, uchar g, uchar b, uchar a = 255)
        : mRed(r), mGreen(g), mBlue(b), mAlpha(a), m_bValid(true)
    { }

    inline uchar Red() const { return mRed; }
    inline uchar Green() const { return mGreen; }
    inline uchar Blue() const { return mBlue; }
    inline uchar Alpha() const { return mAlpha; }
    inline bool isValid() const { return m_bValid; }

private:
    uchar mRed;
    uchar mGreen;
    uchar mBlue;
    uchar mAlpha;
    bool m_bValid;
};

class Canvas
{
public:
    Canvas(int w, int h, bool bUseMmz = true);
    virtual ~Canvas();

    inline int Width() const { return mWidth; }
    inline int Height() const { return mHeight; }
    inline int SubWidth() const { return mWidth / 2; }
    inline int SubHeight() const { return mHeight / 2; }
    inline int LineLength() const { return mLineLength; }
    inline int SubLineLength() const { return mLineLength / 2; }
    inline int PhysicalAddr() const { return mPhyAddr; }
    inline int SubPhysicalAddr() const { return mSubPhyAddr; }
    inline RS_U8 *VirtualAddr() const { return m_pAddr; }

    inline void SetForeground(const Color &color) { mForeground = color; }
    inline void SetBackground(const Color &color) { mBackground = color; }
    inline void SetCharSpacing(int spacing) { mCharSpacing = spacing; }
    void DrawText(int x, int y, const char *text, RS_U8 font);
    void SolidFill(int x, int y, int w, int h, const Color &color);
    void Clear(const Color &color = Color());
    void Sync();

private:
    void Trim(char *text);
    bool IsSpace(const char ch) const;
    inline bool IsEmpty(const char *text) const { return text[0] == 0; }
    inline bool isInClipRect(int x, int y)
    { return x >= 0 && x < mWidth && y >= 0 && y < mHeight; }

    Color mForeground;
    Color mBackground;
    RS_U32 mPhyAddr;
    RS_U8 *m_pAddr;
    RS_U32 mSubPhyAddr;
    RS_U8 *m_pSubAddr;
    int mWidth;
    int mHeight;
    int mLineLength;
    int mCharSpacing;
};

#endif // CANVAS_H
