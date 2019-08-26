
#include "textstreamqueue.h"
#include "commonfunction.h"
#include "intf_media.h"
#include "hi_type.h"
#include "posdevice.h"


TextStreamQueue::TextStreamQueue(int PosId, POS::ConfigInfo *pCfg)
    : m_pPaintBuffer(NULL), m_pPosCfg(pCfg), mPosId(PosId), mXPos(6),
      mRowSpacing(0), mColSpacing(0), mTickCnt(0),mTickClean(0),m_bScrolled(false),
      m_bNeedComposing(true),m_bEndThread(false)
{
    m_bChnNeedPause.resize(POSDevice::ChannelCount(), false);
    m_bChnComposing.resize(POSDevice::ChannelCount(), false);

    //画布宽高：600x700
    int w = 600;
    resize(((w + 7) >> 3) << 3, 700);

#if !defined(D1004NR)
    mViIntelliParam.bSubChnn = RS_FALSE;
    mViIntelliParam.s32Depth  = 1;
    mViIntelliParam.s32MilliSecForGet = 1000;
    mViIntelliParam.s32MilliSecForSend = 100;
#endif
    StartComposing();
}

TextStreamQueue::~TextStreamQueue()
{
    m_bNeedComposing = false;
    StopComposing();

    if (m_pPaintBuffer)
        delete m_pPaintBuffer;
}

void TextStreamQueue::resize(int w, int h)
{
    mWidth = w;
    mHeight = h;
    RSFontMetrics textInfo("Hello", m_pPosCfg->FontSize);
    float FtRatio = textInfo.textPixelSize() / 14.0f;
    int RowHeight = textInfo.textPixelSize() + 5 * FtRatio;
    mMaxRowCnt = mHeight / RowHeight;
    mRowYPos.resize(mMaxRowCnt);
    m_bRowTextChanged.resize(mMaxRowCnt, true);

    int YMargin = (mHeight - (mMaxRowCnt * RowHeight)) / 2;
    for (RS_U32 r = 0; r < mRowYPos.size(); r++)
        mRowYPos[r] = YMargin + r * RowHeight;

    if (m_pPaintBuffer)
        delete m_pPaintBuffer;

    m_pPaintBuffer = new Canvas(mWidth, mHeight);
    m_pPaintBuffer->SetForeground(Color(255, 255, 0));
}

void TextStreamQueue::Append(const char *text)
{
    RSFontMetrics textInfo(text, m_pPosCfg->FontSize);
    textInfo.setWorpWrap(true);
    textInfo.setTextMinRect(0, 0, mWidth - mXPos - 6, mHeight);
    RSStringList textLine = textInfo.subText();

    if (textLine.isEmpty())
        return;

    if (mRowText.size() + textLine.size() > (int)mMaxRowCnt)
    {
        int RmLineCnt = mRowText.size() + textLine.size() - mMaxRowCnt;
        while (RmLineCnt-- > 0)
        {
            mRowText.removeFirst();
            m_bRowTextChanged[RmLineCnt] = true;
        }

        m_bScrolled = true;
    }

    for (RS_S32 r = 0; r < textLine.size(); r++)
    {
        m_bRowTextChanged[mRowText.size()] = true;
        mRowText.append(textLine[r]);
    }

    Update();
}

void TextStreamQueue::Pause(int ChnId)
{
    if (m_bChnNeedPause[ChnId])
        return;

    m_bChnNeedPause[ChnId] = true;
    
#if !defined(D1004NR)
	if (m_pPosCfg->ComposeType == POS::CT_ViModule) 
	{
	    CIntfMedia *pCIntfMedia = CIntfMedia::Instance();
	    RS_S32 s32Ret = pCIntfMedia->StopGetViChnFrame(ChnId, mViIntelliParam);
	    if(RS_SUCCESS != s32Ret)
	        printf("[Pause]:StopGetViChnFrame failed, ChnID=%d,%x \n",ChnId, s32Ret);
	    else
	        printf("[Pause]:StopGetViChnFrame successed,ChnID=%d\n",ChnId);
	}
#endif
}

void TextStreamQueue::Restore(int ChnId)
{
    if (!m_bChnNeedPause[ChnId])
        return;

#if !defined(D1004NR)
	if (m_pPosCfg->ComposeType == POS::CT_ViModule) 
	{
	    CIntfMedia *pCIntfMedia = CIntfMedia::Instance();
	    RS_S32 s32Ret = pCIntfMedia->StartGetViChnFrame(ChnId, mViIntelliParam);
	    if(RS_SUCCESS != s32Ret)
	        printf("[Restore]:StartGetViChnFrame failed, ChnID=%d,%x \n",ChnId, s32Ret);
	    else
	        printf("[Restore]:StartGetViChnFrame successed,ChnID=%d\n",ChnId);
	}
#endif
    m_bChnNeedPause[ChnId] = false;
}

void TextStreamQueue::StartComposing()
{
    m_bNeedComposing = true;
    m_bChnComposing.resize(POSDevice::ChannelCount(), false);
#if !defined(D1004NR)
	if (m_pPosCfg->ComposeType == POS::CT_ViModule) 
	{
	    CIntfMedia *pCIntfMedia = CIntfMedia::Instance();
	    RS_S32 s32Ret = 0;

	    const std::vector<unsigned char> &BoundChns = m_pPosCfg->BoundChns;
	    for (size_t ci = 0; ci < BoundChns.size(); ci++)
	    {
	        RS_S32 ChnId = BoundChns[ci];
	        s32Ret = pCIntfMedia->StartGetViChnFrame(ChnId, mViIntelliParam);
	        if(RS_SUCCESS != s32Ret)
			{
				int count = 0;
				while(RS_SUCCESS != s32Ret)
				{	
					printf("[StartComposing]:StartGetViChnFrame failed, ChnID=%d,s32Ret=%x \n",ChnId, s32Ret);
					sleep(1);
					count++;
					if(count > 3)
						break;
					s32Ret = pCIntfMedia->StartGetViChnFrame(ChnId, mViIntelliParam);
				}
				if(count > 3)
					printf("[StartComposing]:StartGetViChnFrame failed three times,Stop trying ! ChnID=%d\n",ChnId);
				else
					printf("[StartComposing]:StartGetViChnFrame successed,ChnID=%d\n",ChnId);
			}
			else
	            printf("[StartComposing]:StartGetViChnFrame successed,ChnID=%d\n",ChnId);
	    }

	    mCmpozInfo.resize(BoundChns.size());
	    ComposingPara *pCmpzInfo = mCmpozInfo.data();
	    for (RS_U32 i = 0; i < mCmpozInfo.size(); i++)
	    {
	        pCmpzInfo[i].pTSQueue = this;
	        pCmpzInfo[i].pCIntfMedia = pCIntfMedia;
	        pCmpzInfo[i].ChnId = m_pPosCfg->BoundChns[i];
            pCmpzInfo[i].OsdInfo[0].stRect.x = 0;
            pCmpzInfo[i].OsdInfo[0].stRect.y = 0;
            pCmpzInfo[i].OsdInfo[0].stRect.w = m_pPaintBuffer->Width();
            pCmpzInfo[i].OsdInfo[0].stRect.h = m_pPaintBuffer->Height();
            pCmpzInfo[i].OsdInfo[0].enPixelFmt = RSPIXEL_FORMAT_RGB_1555;
            pCmpzInfo[i].OsdInfo[0].u32BgAlpha = 0;
            pCmpzInfo[i].OsdInfo[0].u32BgColor = 0x000000;
            pCmpzInfo[i].OsdInfo[0].u32FgAlpha = 255;
            pCmpzInfo[i].OsdInfo[0].u32PhyAddr = m_pPaintBuffer->PhysicalAddr();
            pCmpzInfo[i].OsdInfo[0].u32Stride = m_pPaintBuffer->LineLength();
            pCmpzInfo[i].OsdInfo[1].stRect.x = 0;
            pCmpzInfo[i].OsdInfo[1].stRect.y = 0;
            pCmpzInfo[i].OsdInfo[1].stRect.w = m_pPaintBuffer->SubWidth();
            pCmpzInfo[i].OsdInfo[1].stRect.h = m_pPaintBuffer->SubHeight();
            pCmpzInfo[i].OsdInfo[1].enPixelFmt = RSPIXEL_FORMAT_RGB_1555;
            pCmpzInfo[i].OsdInfo[1].u32BgAlpha = 0;
            pCmpzInfo[i].OsdInfo[1].u32BgColor = 0x000000;
            pCmpzInfo[i].OsdInfo[1].u32FgAlpha = 255;
            pCmpzInfo[i].OsdInfo[1].u32PhyAddr = m_pPaintBuffer->SubPhysicalAddr();
            pCmpzInfo[i].OsdInfo[1].u32Stride = m_pPaintBuffer->SubLineLength();
            pCmpzInfo[i].MinW = 0;
            pCmpzInfo[i].MaxW = 0;

	        pthread_t id;
	        if (CreateNormalThread(TextStreamQueue::ComposingThread, pCmpzInfo + i, &id) == 0){
				printf("[StartComposing]:CreateNormalThread(ComposingThread) successed,ChnID=%d\n",pCmpzInfo[i].ChnId);	
			}else{
				printf("[StartComposing]:CreateNormalThread(ComposingThread) failed,ChnID=%d\n",pCmpzInfo[i].ChnId);
			}
				
	    }
	}
	else if (m_pPosCfg->ComposeType == POS::CT_VencModule)
#endif
	{	
	    CIntfMedia *pCIntfMedia = CIntfMedia::Instance();
	    const std::vector<unsigned char> &BoundChns = m_pPosCfg->BoundChns;
		RS_RECT_S OsdRect;
		OsdRect.x = 920;
		OsdRect.y = 505;
		OsdRect.w = mWidth;
		OsdRect.h = mHeight;
	    for (RS_U32 ci = 0; ci < BoundChns.size(); ci++)
		{
		#if defined(D1004NR)
			if (m_pPosCfg->ComposeType == POS::CT_ViModule) 
				pCIntfMedia->CreatePosOsdForVi(BoundChns[ci], OsdRect);
			else
				pCIntfMedia->CreatePosOsd(BoundChns[ci], OsdRect);
		#else
			pCIntfMedia->CreatePosOsd(BoundChns[ci], OsdRect);
		#endif
			
		}
		mCmpozInfo.resize(BoundChns.size());
	    ComposingPara *pCmpzInfo = mCmpozInfo.data();
		if(0 != mCmpozInfo.size()) // 当没有绑定通道时不创建线程
		{
			pCmpzInfo[0].pTSQueue = this;
			pCmpzInfo[0].ChnId = m_pPosCfg->BoundChns[0];
			m_bEndThread = false;
			Update(); //超级用户切换子用户时，显示以前字符串
			CreateNormalThread(TextStreamQueue::CanvasClear, pCmpzInfo, NULL);
			printf("\033[;31m==========POS:[%d]CreateNormalThread=============\033[0m\n", pCmpzInfo[0].ChnId);
		}
	}
}



void TextStreamQueue::StopComposing()
{
    m_bNeedComposing = false;
	m_bEndThread = true;
#if !defined(D1004NR)
	if (m_pPosCfg->ComposeType == POS::CT_ViModule)
	{
	    while (1)
	    {
	        bool bStoped = true;
	        for (RS_U32 i = 0; i < m_bChnComposing.size(); i++)
	        {
	            if (m_bChnComposing[i])
	            {
	                bStoped = false;
	                break;
	            }
	        }

	        if (bStoped)
	            break;

	        usleep(50000);
	    }

	    CIntfMedia *pCIntfMedia = CIntfMedia::Instance();
	    RS_S32 s32Ret = 0;

	    const std::vector<unsigned char> &BoundChns = m_pPosCfg->BoundChns;
	    for (RS_U32 ci = 0; ci < BoundChns.size(); ci++)
	    {
	        RS_S32 ChnId = BoundChns[ci];
	        s32Ret = pCIntfMedia->StopGetViChnFrame(ChnId, mViIntelliParam);
	        if(RS_SUCCESS != s32Ret)
	            printf("[StopComposing]:StopGetViChnFrame failed, ChnID=%d,%x \n",ChnId, s32Ret);
	        else
	            printf("[StopComposing]:StopGetViChnFrame successed,ChnID=%d\n",ChnId);
	    }
	}
	else if (m_pPosCfg->ComposeType == POS::CT_VencModule)
#endif
	{
	
		while (1)
		{
			bool bStoped = true;
			for (RS_U32 i = 0; i < m_bChnComposing.size(); i++)
			{
				if (m_bChnComposing[i])
				{
					bStoped = false;
					break;
				}
			}
		
			if (bStoped)
				break;
		
			usleep(50000);
		}
		CIntfMedia *pCIntfMedia = CIntfMedia::Instance();
	    const std::vector<unsigned char> &BoundChns = m_pPosCfg->BoundChns;
	    for (RS_U32 ci = 0; ci < BoundChns.size(); ci++)
	    {
	    #if defined(D1004NR)
		    if (m_pPosCfg->ComposeType == POS::CT_ViModule)
				pCIntfMedia->DestroyPosOsdForVi(BoundChns[ci]);
			else
				pCIntfMedia->DestroyPosOsd(BoundChns[ci]);
	    #else
			pCIntfMedia->DestroyPosOsd(BoundChns[ci]);
		#endif
		}
		printf("==========DestroyPosOsd=============\n");
	}
}

void *TextStreamQueue::CanvasClear(void *Para)
{
	ComposingPara *pCmpzInfo = static_cast<ComposingPara *>(Para);
	TextStreamQueue *pThiz = pCmpzInfo->pTSQueue;
	RS_U8 ChnId = pCmpzInfo->ChnId;
	pThiz->m_bChnComposing[ChnId] = true;

	//pThiz->mTickClean = 0; //invalid!  Variables can't  be initialized in a thread 
    while (pThiz->m_bNeedComposing) // 删除图标
    {
        if (pThiz->mTickClean)
        {  	
			if(pThiz->m_bEndThread)
				break;
			mSleep(100);
			pThiz->mTickClean++;
            if (pThiz->mTickClean >= 100) // ten sec
            {
                pThiz->m_pPaintBuffer->Clear();
				pThiz->mRowText.clear();
				pThiz->Update();  
            }
        }		
	}
	printf("\033[;31m==========POS:[%d]Thread Exit=============\033[0m\n", ChnId);
	pThiz->m_bChnComposing[ChnId] = false;

	return NULL;
}

void *TextStreamQueue::ComposingThread(void *Para)
{
#if !defined(D1004NR)
    ComposingPara *pCmpzInfo = static_cast<ComposingPara *>(Para);
    TextStreamQueue *pThiz = pCmpzInfo->pTSQueue;
    CIntfMedia *pCIntfMedia = pCmpzInfo->pCIntfMedia;
    RS_U8 ChnId = pCmpzInfo->ChnId;
    RS_VIDEO_FRAME_INFO_S VideoFrame;
    RS_VGS_TASK_ATTR_S VgsInfo;
    VI_INTELLI_PARAM_S &stViIntelliParam = pThiz->mViIntelliParam;
    int s32Ret = RS_SUCCESS;
	int s32Count = 0;

    pThiz->m_bChnComposing[ChnId] = true;
    while (pThiz->m_bNeedComposing)
    {
        if (pThiz->mTickCnt)
        {
            pThiz->mTickCnt++;
            if (pThiz->mTickCnt > 400 * pThiz->m_pPosCfg->BoundChns.size())
            {
                pThiz->mTickCnt = 0;
                pThiz->m_pPaintBuffer->Clear();
				pThiz->mRowText.clear();
            }
        }

        if (pThiz->m_bChnNeedPause[ChnId])
        {
            mSleep(100);
            continue;
        }

        s32Ret = pCIntfMedia->GetViChnFrame(ChnId,  stViIntelliParam, VideoFrame);
#if 1
		if(RS_SUCCESS != s32Ret)
		{
			s32Count++;
			if(5 == s32Count)
			{
				printf("[ComposingThread]:GetViChnFrame failed ChnId:%d %x\n", ChnId, s32Ret);
				pCIntfMedia->StopGetViChnFrame(ChnId, stViIntelliParam);
				sleep(1);
				pCIntfMedia->StartGetViChnFrame(ChnId, stViIntelliParam);
				continue;
			}
            continue;			
		}
		else
		{
			s32Count = 0;
		}
#else		
        if(RS_SUCCESS != s32Ret)
        {
            printf("[ComposingThread]:GetViChnFrame failed %x\n",s32Ret);
            pCIntfMedia->StopGetViChnFrame(ChnId, stViIntelliParam);
            sleep(1);
            pCIntfMedia->StartGetViChnFrame(ChnId, stViIntelliParam);
            continue;
        }
#endif
        RS_U32 u32Width  = VideoFrame.stVFrame.u32Width;
        RS_U32 u32Height = VideoFrame.stVFrame.u32Height;
        if (u32Height <= 0 || u32Width <= 0)
            continue;

        if (pCmpzInfo->MinW == 0)
        {
            pCmpzInfo->MinW = u32Width;
            pCmpzInfo->MaxW = u32Width;
        }

        if (u32Width > pCmpzInfo->MaxW)
            pCmpzInfo->MaxW = u32Width;

        if (u32Width < pCmpzInfo->MinW)
            pCmpzInfo->MinW = u32Width;

        bool bMainImg = u32Width == pCmpzInfo->MaxW;
        RS_VGS_ADD_OSD_S &OsdInfo = bMainImg ? pCmpzInfo->OsdInfo[0] : pCmpzInfo->OsdInfo[1];

        //画布宽高
        RS_U32 OsdW = OsdInfo.stRect.w;
        RS_U32 OsdH = OsdInfo.stRect.h;
        if (OsdW > u32Width || OsdH  > u32Height)
        {
            pCIntfMedia->FreeViChnFrame(ChnId,	stViIntelliParam, VideoFrame);
            continue;
        }

        CIntfMedia *pCIntfMedia = CIntfMedia::Instance();
        RS_VGS_HANDLE hVgsHandle;

        s32Ret = pCIntfMedia->VgsBeginJob(&hVgsHandle);
        if (s32Ret != HI_SUCCESS)
        {
            printf("[ComposingThread]:HI_MPI_VGS_BeginJob  fail,Error(%#x)\n",s32Ret);
            pCIntfMedia->VgsCancelJob(&hVgsHandle);
            pCIntfMedia->FreeViChnFrame(ChnId,	stViIntelliParam, VideoFrame);
            continue;
        }

        VgsInfo.stImgIn  = VideoFrame;
        VgsInfo.stImgOut = VideoFrame;
        //画布位置,x=0,y=0(左上角零点),必须为偶数
        switch(pThiz->m_pPosCfg->Position)
        {
            case 0:
                OsdInfo.stRect.x = (int(u32Width - OsdW - 50 * (u32Width / 1000.0) + 1) >> 1) << 1;
                OsdInfo.stRect.y = 0;
                break;//右上角
            case 1:
                OsdInfo.stRect.x = ((int(u32Width - OsdW) / 2) >> 1) << 1;
                OsdInfo.stRect.y = 0;
                break;//上中
            case 2:
                OsdInfo.stRect.x = 0;
                OsdInfo.stRect.y = 0;
                break;//左上角
            case 3:
                OsdInfo.stRect.x = ((int(u32Width - OsdW) / 2) >> 1) << 1;
                OsdInfo.stRect.y = (((u32Height - OsdH) / 2) >> 1) << 1;
                break;//居中
            case 4:
                OsdInfo.stRect.x = (int(u32Width - OsdW - 50 * (u32Width / 1000.0) + 1) >> 1) << 1;
                OsdInfo.stRect.y = (((u32Height - OsdH) / 2 + 1) >> 1) << 1;
                break;//中右
            case 5:
                OsdInfo.stRect.x = 0;
                OsdInfo.stRect.y = (((u32Height - OsdH) / 2) >> 1) << 1;
                break;//中左
            case 6:
                OsdInfo.stRect.x = (int(u32Width - OsdW - 50 * (u32Width / 1000.0) + 1) >> 1) << 1;
                OsdInfo.stRect.y = ((u32Height - OsdH) >> 1) << 1;
                break;//右下角
            case 7:
                OsdInfo.stRect.x = ((int(u32Width - OsdW) / 2) >> 1) << 1;
                OsdInfo.stRect.y = ((u32Height - OsdH) >> 1) << 1;
                break;//下中
            case 8:
                OsdInfo.stRect.x = 0;
                OsdInfo.stRect.y = ((u32Height - OsdH) >> 1) << 1;
                break;//左下角
            default:
                OsdInfo.stRect.x = (int(u32Width - OsdW - 50 * (u32Width / 1000.0) + 1) >> 1) << 1;
                OsdInfo.stRect.y = (((u32Height - OsdH) / 2 + 1) >> 1) << 1;
                break;
        }

        pCIntfMedia->VgsAddOsdTaskArray(&hVgsHandle, &VgsInfo, &OsdInfo, 1);
        s32Ret = pCIntfMedia->VgsEndJob(&hVgsHandle);
        if (s32Ret != HI_SUCCESS)
        {
            printf("[ComposingThread]:HI_MPI_VGS_EndJob fail,Error(%#x)\n",s32Ret);
            pCIntfMedia->VgsCancelJob(&hVgsHandle);
            pCIntfMedia->FreeViChnFrame(ChnId,	stViIntelliParam, VideoFrame);
            continue;
        }

        s32Ret = pCIntfMedia->FreeViChnFrame(ChnId,	stViIntelliParam, VideoFrame);
        if(RS_SUCCESS != s32Ret)
            printf("[ComposingThread]:FreeViChnFrame failed %x\n",s32Ret);
    }

    pThiz->m_bChnComposing[ChnId] = false;
#endif
    return NULL;
}

void TextStreamQueue::Update()
{
    if (m_bScrolled)
        m_pPaintBuffer->Clear();

    for (RS_S32 r = 0; r < mRowText.size(); r++)
    {
        if (!m_bRowTextChanged[r] && !m_bScrolled)
            continue;
        m_pPaintBuffer->DrawText(mXPos, mRowYPos[r], mRowText[r].toUtf8().constData(), m_pPosCfg->FontSize);
        m_bRowTextChanged[r] = false;
    }

    m_bScrolled = false;
    mTickCnt = 1;
	mTickClean = 1;

    m_pPaintBuffer->Sync();

#if !defined(D1004NR)
	if (m_pPosCfg->ComposeType == POS::CT_VencModule) 
#endif
	{
		if (!m_pPaintBuffer)
			return;
			
		CIntfMedia *pCIntfMedia = CIntfMedia::Instance();
	    const std::vector<unsigned char> &BoundChns = m_pPosCfg->BoundChns;
		RS_SIZE_S OsdSize;
		OsdSize.w = mWidth;
		OsdSize.h = mHeight;
		RS_U8 *pBitMap = m_pPaintBuffer->VirtualAddr();
	    for (RS_U32 ci = 0; ci < BoundChns.size(); ci++)
		{		
		#if defined(D1004NR)
			if (m_pPosCfg->ComposeType == POS::CT_ViModule)
				pCIntfMedia->UpdatePosOsdForVi(BoundChns[ci], OsdSize, pBitMap);
			else
				pCIntfMedia->UpdatePosOsd(BoundChns[ci], OsdSize, pBitMap);
		#else
			pCIntfMedia->UpdatePosOsd(BoundChns[ci], OsdSize, pBitMap);
		#endif
		}
	}
}
