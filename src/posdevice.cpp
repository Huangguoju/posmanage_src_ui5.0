
#include "posdevice.h"
#include "posdataanalyzer.h"
#include "textstreamqueue.h"
#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>
#include "pos_db.h"
#include <boost/algorithm/string.hpp>
#include <boost/typeof/typeof.hpp>
#include <string.h>
#include "pos_terminal_parser.h"

int POSDevice::mChnCnt = 0;

POSDevice::POSDevice(int PosId, const POS::ConfigInfo &Cfg)
    : mPosCfgInfo(Cfg), m_pServId(NULL), mPosId(PosId),m_cd(libiconv_t(-1)),isStarted(false)
{
    m_pDisplayer = new TextStreamQueue(mPosId, &mPosCfgInfo);
    m_pAnalyzer = new PosDataAnalyzer(mPosId, &mPosCfgInfo);

    if (mPosCfgInfo.Encoding != "UTF-8" && 0 == mPosCfgInfo.CommType){
        m_cd = libiconv_open("UTF-8", mPosCfgInfo.Encoding.c_str());
        if (m_cd == libiconv_t(-1)){
            printf("POSDevice: libiconv_open(to=[UTF-8], from=[%s]) failed !\n",
                   mPosCfgInfo.Encoding.c_str());
        }
    }
    memset(m_buf, 0x0, sizeof(m_buf));
    pos_net::parm PosPara;
    CreateServPara(PosPara);
    m_pServId = pos_net::start(PosPara);
}

POSDevice::~POSDevice()
{
    pos_net::stop(&m_pServId);
    if (m_cd != libiconv_t(-1)){
        libiconv_close(m_cd);
        m_cd = libiconv_t(-1);
    }
    delete m_pDisplayer;
    delete m_pAnalyzer;
}

void POSDevice::Config(const POS::ConfigInfo &Cfg)
{
    bool bComposeParaChanged = false;
    bool bEncodingChanged = false;
    pos_net::stop(&m_pServId);

    if (mPosCfgInfo.BoundChns != Cfg.BoundChns 
        || mPosCfgInfo.ComposeType != Cfg.ComposeType
        || mPosCfgInfo.FontSize != Cfg.FontSize)
    {
        m_pDisplayer->StopComposing();
        bComposeParaChanged = true;
    }
    if(Cfg.CommType == 0 && mPosCfgInfo.Encoding != Cfg.Encoding){
        if (m_cd != libiconv_t(-1)){
            libiconv_close(m_cd);
            m_cd = libiconv_t(-1);
        }
        bEncodingChanged = true;
    }


    mPosCfgInfo.Name = Cfg.Name;
    mPosCfgInfo.Start = Cfg.Start;
    mPosCfgInfo.Stop = Cfg.Stop;
    mPosCfgInfo.Separator = Cfg.Separator;
    mPosCfgInfo.Encoding = Cfg.Encoding;
    mPosCfgInfo.CommType = Cfg.CommType;
    mPosCfgInfo.BoundChns = Cfg.BoundChns;
    mPosCfgInfo.PosType = Cfg.PosType;
	mPosCfgInfo.ComposeType = Cfg.ComposeType;
    mPosCfgInfo.FontSize = Cfg.FontSize;
    mPosCfgInfo.Position = Cfg.Position;
    if (Cfg.CommType == 0) /* Serial */
    {
        mPosCfgInfo.Baudrate = Cfg.Baudrate;
        mPosCfgInfo.DataBit = Cfg.DataBit;
        mPosCfgInfo.StopBit = Cfg.StopBit;
        mPosCfgInfo.Check = Cfg.Check;
        mPosCfgInfo.Number = Cfg.Number;

    }
    else
    {
        mPosCfgInfo.Port = Cfg.Port;
    }
    isStarted = false;
    memset(m_buf, 0, 512);
    if(bEncodingChanged){
        if (mPosCfgInfo.Encoding != "UTF-8")
        {
            m_cd = libiconv_open("UTF-8", mPosCfgInfo.Encoding.c_str());
            if (m_cd == libiconv_t(-1)){
                printf("POSDevice: libiconv_open(to=[UTF-8], from=[%s]) failed !\n",
                       mPosCfgInfo.Encoding.c_str());
            }
        }
    }

    if (bComposeParaChanged){
        m_pDisplayer->resize(600, 700);//canvas size
        m_pDisplayer->StartComposing();
    }

    pos_net::parm PosPara;
    CreateServPara(PosPara);
    m_pServId = pos_net::start(PosPara);
}

void POSDevice::PauseOsd(int ChnId)
{
    for (size_t i = 0; i < mPosCfgInfo.BoundChns.size(); i++)
    {
        if (mPosCfgInfo.BoundChns[i] == ChnId)
        {
            m_pDisplayer->Pause(ChnId);
            break;
        }
    }
}

void POSDevice::RestoreOsd(int ChnId)
{
    for (size_t i = 0; i < mPosCfgInfo.BoundChns.size(); i++)
    {
        if (mPosCfgInfo.BoundChns[i] == ChnId)
        {
            m_pDisplayer->Restore(ChnId);
            break;
        }
    }
}

void POSDevice::on_data_loop(char *& buf, size_t& size)
{
    POS::ConfigInfo mPosCfgInfoTmp = mPosCfgInfo;
    mPosCfgInfoTmp.Start += mPosCfgInfoTmp.Separator;
    while(size)
    {
        if (!isStarted)
        {
            if (size < mPosCfgInfoTmp.Start.size())
                return;
            BOOST_AUTO(r, boost::find_first(buf, mPosCfgInfoTmp.Start));
            if (r.begin() == r.end())
                return;
            PosDataRecv(pos_net::CALLBACK_TYPE_START, NULL, this);
            isStarted = true;
            size = buf + size - r.end();
            strcpy(buf, buf + mPosCfgInfoTmp.Start.size());
            continue;
        }
        if (mPosCfgInfoTmp.Separator.empty())
        {
            BOOST_AUTO(r, boost::find_first(buf, mPosCfgInfoTmp.Stop));
            if (r.begin() == r.end())
            {
                PosDataRecv(pos_net::CALLBACK_TYPE_ITEM, buf, this);
                memset(m_buf, 0, 512);
                return;
            }
            if (r.begin() != buf)
            {
                *r.begin() = '\0';
                PosDataRecv(pos_net::CALLBACK_TYPE_ITEM, buf, this);
            }
            isStarted = false;
            memset(m_buf, 0, 512);
            PosDataRecv(pos_net::CALLBACK_TYPE_STOP, NULL, this);
            return;
        }
        if (size < mPosCfgInfoTmp.Separator.size())
            return;
        BOOST_AUTO(r, boost::find_first(buf, mPosCfgInfoTmp.Separator));
        if (r.begin() == r.end())
            return;
        if (r.begin() != buf)
        {
            *r.begin() = '\0';
            if (mPosCfgInfoTmp.Stop == buf){
                PosDataRecv(pos_net::CALLBACK_TYPE_STOP, NULL, this);
                isStarted = false;
                memset(m_buf, 0, 512);
                return;
            }else{
                PosDataRecv(pos_net::CALLBACK_TYPE_ITEM, buf, this);
            }
        }
        size = buf + size - r.end();
        strcpy(buf, r.end());
    }
}

void POSDevice::remove_extra_space(char *txt, size_t &size)
{
    char *ps = txt, *pm = txt, *pe = txt;
    bool matching = false;
    for (size_t i = 0; i < size; i++)
    {
        if (txt[i] == ' ')
        {
            if (matching)
            {
                pe = txt + i;
            }
            else
            {
               matching = true;
               pe = pm = txt + i;
            }
        }
        else if (matching)
        {
            if (txt[i] != '\n')
            {
                while (pm <= pe)
                    *(ps++) = *(pm++);
            }

            matching = false;
        }

        if (!matching)
        {
            pm = pe = txt + i;
            *(ps++) = txt[i];
        }
    }

    *(ps++) = '\0';
    size = ps - txt;
}

void POSDevice::convert_encoding(char *&buf, size_t &size)
{
    if (m_cd == libiconv_t(-1))
        return;
    char *pi = buf;
    char m_cvt_buf[512];
    char *po = m_cvt_buf;
    size_t in_left = size, out_left = sizeof(m_cvt_buf);
    if (libiconv(m_cd, &pi, &in_left, &po, &out_left) == 0)
    {
        memset(m_buf, 0, 512);
        strcpy(m_buf, m_cvt_buf);
        size = sizeof(m_cvt_buf) - out_left;
        m_buf[size] = '\0';
    }
}

void POSDevice::Pos485String(char *buf)
{
    size_t size = strlen(buf);
    if (mPosCfgInfo.PosType == pos_net::POS_TYPE_RECEIPTS)
    {
        if((strlen(m_buf)+strlen(buf) >= 512) || strlen(buf) >= 512){
            memset(m_buf, 0, 512);
            isStarted = false;
            printf("[POSDevice] max_line_size\n");
            return;
        }
        if(!isStarted)
            strcpy(m_buf, buf);
        else
            strcat(m_buf, buf);

        size = strlen(m_buf);
        char *pi = m_buf;
        convert_encoding(pi, size);

        char *p = m_buf;
        on_data_loop(p, size);
    }
    else if (mPosCfgInfo.PosType == pos_net::POS_TYPE_TERMINAL)
    {   
    }
    else if (mPosCfgInfo.PosType == pos_net::POS_TYPE_PLAINTEXT)
    {
        strcpy(m_buf, buf);
        remove_extra_space(m_buf, size);
        size = strlen(m_buf);
        char *pi = m_buf;
        convert_encoding(pi, size);
        char *p = m_buf;
        if (size > 0)
            PosDataRecv(pos_net::CALLBACK_TYPE_ITEM, p, this);
    }
}

void POSDevice::CreateServPara(pos_net::parm &PosPara)
{
    PosPara.type= (pos_net::e_pos_type)mPosCfgInfo.PosType;
    PosPara.proto = (pos_net::e_proto)mPosCfgInfo.CommType;
    PosPara.start_tag = mPosCfgInfo.Start;
    PosPara.stop_tag = mPosCfgInfo.Stop;
    PosPara.item_sep = mPosCfgInfo.Separator;
    PosPara.encoding = mPosCfgInfo.Encoding;
    PosPara.port = mPosCfgInfo.Port;
    PosPara.user_parm = this;
    PosPara.callback = POSDevice::PosDataRecv;
}

void POSDevice::PosDataRecv(pos_net::e_callback_type type, const char *item,
                            void *pObj)
{
    POSDevice *pThiz = static_cast<POSDevice *>(pObj);
    if (pThiz->mPosCfgInfo.PosType == pos_net::POS_TYPE_RECEIPTS)
    {
        if (item)
            pThiz->m_pDisplayer->Append(item);
        else if (type == pos_net::CALLBACK_TYPE_STOP)
            pThiz->m_pDisplayer->Append("\n");

        pThiz->m_pAnalyzer->addItem(type, item);
    }
    else if (pThiz->mPosCfgInfo.PosType == pos_net::POS_TYPE_TERMINAL)
	{
		const char *keywork[] = {"terminal_code","card_id","money","terminal_model","serial","time","pos_id","pos_name","relate_channels"};
		char itemtext[256] ={0};
		rapidjson::Document doc;	// Default template parameter uses UTF8 and MemoryPoolAllocator.
		if (doc.Parse<0>(item).HasParseError())
			return;

		assert(doc.IsObject());	// Document is a JSON value represents the root of DOM. Root can be either an object or array.

		for(int i = 0; i <6; i++)
		{
			assert(doc.HasMember(keywork[i]));
            assert(doc[keywork[i]].IsString());
            strcat(itemtext,keywork[i]);
            strcat(itemtext,":");
            strcat(itemtext,doc[keywork[i]].GetString());
            strcat(itemtext,"\n");
		}

		strcat(itemtext,"\n");
		
		/*写数据内容需要在传入字符基础上增加POSID NAME BOUNDCHS三个字段数据*/
		doc.AddMember(rapidjson::StringRef(keywork[6]), pThiz->mPosId, doc.GetAllocator());
		doc.AddMember(rapidjson::StringRef(keywork[7]), rapidjson::StringRef(pThiz->mPosCfgInfo.Name.c_str()), doc.GetAllocator());
		rapidjson::Document::AllocatorType& allocator = doc.GetAllocator();
		rapidjson::Value contact(rapidjson::kArrayType);
	    for (size_t i = 0; i < pThiz->mPosCfgInfo.BoundChns.size(); i++)
	    {
			contact.PushBack(pThiz->mPosCfgInfo.BoundChns[i], allocator);
	    }

		doc.AddMember(rapidjson::StringRef(keywork[8]), contact, doc.GetAllocator());

		rapidjson::StringBuffer sb;
		rapidjson::Writer<rapidjson::StringBuffer> w(sb);
		doc.Accept(w);
		//判断接收的数据是否正常
        if(strlen(doc["card_id"].GetString())> 20 || strlen(doc["terminal_code"].GetString()) > 8
			|| strlen(doc["terminal_model"].GetString())> 16 || strlen(doc["serial"].GetString())> 8)
		{
			printf("Waring !!! The Param maybe too long or error , Please Check !!!");
			return;
		}
		else
		{
            pThiz->m_pDisplayer->Append(itemtext);
			//写入数据库
			std::string query_info = sb.GetString();
			pos_db::write_terminal_record(query_info);
			
		    pThiz->m_pAnalyzer->addItem(type, sb.GetString());
		}
    }
    else
    {
        if (item)
        {
            std::string text(item);
            text += '\n';
            pThiz->m_pDisplayer->Append(text.c_str());
            pThiz->m_pAnalyzer->writePlainText(item);
        }
    }
}
