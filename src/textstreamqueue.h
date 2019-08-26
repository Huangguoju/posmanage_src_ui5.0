#ifndef TEXTSTREAMQUEUE_H
#define TEXTSTREAMQUEUE_H


#include <deque>
#include "cosd.h"
#include "canvas.h"
#include "posdefine.h"
#include "media_type.h"

class Canvas;
class CIntfMedia;
class TextStreamQueue
{
public:
    TextStreamQueue(int PosId, POS::ConfigInfo *pCfg);
    virtual ~TextStreamQueue();

    void resize(int w, int h);
    void Append(const char *text);
    void Pause(int ChnId);
    void Restore(int ChnId);
    void StartComposing();
    void StopComposing();
    Canvas *PaintBuffer() const { return m_pPaintBuffer; }

private:
    struct ComposingPara {
        RS_VGS_ADD_OSD_S OsdInfo[2];
        TextStreamQueue *pTSQueue;
        CIntfMedia *pCIntfMedia;
        RS_U32 MinW;
        RS_U32 MaxW;
        RS_U8 ChnId;
    };

    static void *ComposingThread(void *Para);
	static void *CanvasClear(void *Para);

    void Composing();
    void Update();


    RSStringList mRowText;
    std::vector<ComposingPara> mCmpozInfo;
    std::vector<int> mRowYPos;
    std::vector<bool> m_bRowTextChanged;
    std::vector<bool> m_bChnComposing;
    std::vector<bool> m_bChnNeedPause;
#if !defined(D1004NR)
    VI_INTELLI_PARAM_S mViIntelliParam;
#endif
    Canvas *m_pPaintBuffer;
    POS::ConfigInfo *m_pPosCfg;
    RS_S32 mPosId;
    RS_U32 mWidth;
    RS_U32 mHeight;
    RS_S32 mXPos;
    RS_U32 mMaxRowCnt;
    RS_U32 mRowSpacing;
    RS_U32 mColSpacing;
    RS_U32 mTickCnt;
	RS_U32 mTickClean;
    bool m_bScrolled;
    bool m_bNeedComposing;
	bool m_bEndThread;
};

#endif // TEXTSTREAMQUEUE_H
