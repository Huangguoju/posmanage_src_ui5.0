#ifndef __pos_net_h__
#define __pos_net_h__

#include <string>

namespace pos_net
{
    enum e_pos_type { POS_TYPE_RECEIPTS, POS_TYPE_TERMINAL, POS_TYPE_PLAINTEXT };
	enum e_proto { PROTO_COM, PROTO_UDP, PROTO_TCP };
	enum e_callback_type { CALLBACK_TYPE_START, CALLBACK_TYPE_ITEM, CALLBACK_TYPE_STOP };

	struct parm
	{
		e_pos_type type;
		e_proto proto;
		unsigned short port;
		std::string start_tag;
		std::string stop_tag;
		std::string item_sep;
        std::string encoding;
		// POS_TYPE_TERMINAL callback item = {"terminal_code":"1","card_id":"2","money":"99.99","terminal_model":"4","serial":"5","time":"20170417112459"}
		void (*callback)(e_callback_type type, const char *item, void *user_parm);
		void *user_parm;
	};

	void *start(const parm& p);
	void stop(void **p);
}

#endif // __pos_net_h__
