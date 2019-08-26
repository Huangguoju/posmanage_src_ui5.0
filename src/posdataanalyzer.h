#ifndef POSDATAANALYZER_H
#define POSDATAANALYZER_H


#include <stack>
#include <vector>
#include <string>

#include "pos_db.h"
#include "pos_net.h"
#include "posdefine.h"

class PosDealData
{
    /* pos_db_time start; */
    /* pos_db_time stop;  */
    std::vector<std::string> Item;
};


class PosDataAnalyzer
{
public:
    PosDataAnalyzer(int PosId, POS::ConfigInfo *pCfgInfo);
    void addItem(pos_net::e_callback_type type, const char *item);
    void writePlainText(const char *text);

private:
    //std::stack<PosDealData> mActiveDeals;
    pos_db::record m_record;/*先不考虑同时多个start*/
    POS::ConfigInfo *m_pPosCfg;
    int mPosId;
};

#endif // POSDATAANALYZER_H
