#ifndef POSDEFINE_H
#define POSDEFINE_H

#include <string>
#include <vector>

#include "rs_type.h"


namespace POS {
	enum ComposeType {
		CT_ViModule,
		CT_VencModule
	};

    struct ConfigInfo {
        std::string Name;
        std::string Start;
        std::string Stop;
        std::string Separator;
        std::string Encoding;
        std::vector<unsigned char> BoundChns;
        RS_U8 CommType;
        RS_U8 PosType;
		RS_U8 ComposeType;
        RS_U8 FontSize;
        RS_U8 Position;
        union {
            RS_U32 Port;             /*UDP、TCP连接端口*/
            struct {
                RS_U8  Baudrate;	 /*波特率，0-1200，1-2400，2-4800，3-9600*/
                RS_U8  DataBit;		 /*数据位，0-8，1-7，2-6，3-5*/
                RS_U8  StopBit;		 /*停止位，0-1，1-2*/
                RS_U8  Check;		 /*校验，0-None，1-Odd，2-Even，3-Mark，4-Space*/
                RS_U8  Number;		 /*编号，范围为1--63*/
            };
        };
};

}

#endif // POSDEFINE_H
