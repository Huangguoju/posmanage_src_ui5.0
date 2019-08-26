#include <sys/time.h>
#include <string.h>

#include "posdataanalyzer.h"

static void GetOsTime(pos_db::time &t)
{
	struct timeval tvTime;
	struct tm tm;
    memset(&tm, 0, sizeof(tm));

	gettimeofday(&tvTime, NULL);
	localtime_r(&tvTime.tv_sec, &tm);
	t.year  = tm.tm_year + 1900;
	t.month = tm.tm_mon + 1;
	t.day   = tm.tm_mday;
	t.hour  = tm.tm_hour;
	t.min   = tm.tm_min;
	t.sec   = tm.tm_sec;
}

PosDataAnalyzer::PosDataAnalyzer(int PosId, POS::ConfigInfo *pCfgInfo)
    : m_pPosCfg(pCfgInfo), mPosId(PosId)
{
}

void PosDataAnalyzer::addItem(pos_net::e_callback_type type, const char *item)
{
	if (pos_net::CALLBACK_TYPE_START == type)
	{
		m_record.pos_id = mPosId;
		m_record.pos_name = m_pPosCfg->Name;
		GetOsTime(m_record.start);
		m_record.relate_channels = m_pPosCfg->BoundChns;
        m_record.items.clear();
    }
	else if (pos_net::CALLBACK_TYPE_ITEM == type)
	{
		m_record.items.push_back(item);
	}
	else if (pos_net::CALLBACK_TYPE_STOP == type)
	{
		GetOsTime(m_record.stop);
		pos_db::write(m_record);
    }
}

void PosDataAnalyzer::writePlainText(const char *text)
{
    pos_db::time tm;
    GetOsTime(tm);

    m_record.pos_id = mPosId;
    m_record.pos_name = m_pPosCfg->Name;
    m_record.start = tm;
    m_record.stop = tm;
    m_record.relate_channels = m_pPosCfg->BoundChns;
    m_record.items.push_back(text);
    pos_db::write(m_record);

    m_record.items.clear();
}

