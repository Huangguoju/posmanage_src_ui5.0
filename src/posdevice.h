#ifndef POSDEVICE_H
#define POSDEVICE_H


#include "posdefine.h"
#include "pos_net.h"
#include "iconv.h"


class PosDataAnalyzer;
class TextStreamQueue;
class POSDevice
{
public:
    POSDevice(int PosId, const POS::ConfigInfo &Cfg);
    virtual ~POSDevice();

    void Config(const POS::ConfigInfo &Cfg);
    void PauseOsd(int ChnId);
    void RestoreOsd(int ChnId);
    void Pos485String(char *item);
    const std::string &Name() const { return mPosCfgInfo.Name; }

    static void SetChannelCount(int count) { mChnCnt = count; }
    static int ChannelCount() { return mChnCnt; }

private:
    static void PosDataRecv(pos_net::e_callback_type type, const char *item,
                            void *pObj);

    void CreateServPara(pos_net::parm &PosPara);
    void on_data_loop(char *& buf, size_t& size);
    void convert_encoding(char *&buf, size_t &size);
    void remove_extra_space(char *txt, size_t &size);

    POS::ConfigInfo mPosCfgInfo;
    TextStreamQueue *m_pDisplayer;
    PosDataAnalyzer *m_pAnalyzer;
    void *m_pServId;
    int mPosId;

    libiconv_t m_cd;
    bool isStarted;
    char m_buf[512];

    static int mChnCnt;
};

#endif // POSDEVICE_H
