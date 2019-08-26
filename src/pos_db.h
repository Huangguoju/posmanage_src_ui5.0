#ifndef __pos_db_h__
#define __pos_db_h__

#include <string>
#include <vector>

namespace pos_db
{
	struct config_parm
	{
		std::string path;
		size_t size;
	};

	void config(const config_parm& p);

	struct time
	{
		unsigned short year;
		unsigned char month;
		unsigned char day;
		unsigned char hour;
		unsigned char min;
		unsigned char sec;
	};

	struct record
	{
		unsigned long long id;
		int pos_id;
		std::string pos_name;
		time start;
		time stop;
		std::vector<unsigned char> relate_channels;
		std::vector<std::string> items;
	};

	void write(const record& rec);

	struct query_records_parm
	{
		int pos_id;
		time begin;
		time end;
		std::vector<std::string> keys;
		bool is_and;
		unsigned int max_records;
		void (*callback)(std::vector<record>& records, void *user_parm);
		void *user_parm;
	};

	void query_records(const query_records_parm& p);

	struct query_items_parm
	{
		unsigned long long record_id;
		void (*callback)(std::vector<std::string>& items, void *user_parm);
		void *user_parm;
	};

	void query_items(const query_items_parm& p);

	//{"pos_id":1,"pos_name":"name","relate_channels":[1,7,8],"terminal_code":"1","card_id":"2","money":"99.99","terminal_model":"4","serial":"5","time":"20170417112459"}
	void write_terminal_record(const std::string& rec);

	struct query_terminal_records_parm
	{
		//{"pos_id":1,"begin":"20170417112459","end":"20170417112459","max_records":2000,"terminal_code":"1","card_id":"2","terminal_model":"4","serial":"5"}
		std::string query_info;
		//[{},{}...] {} : {"pos_id":1,"pos_name":"name","relate_channels":[1,7,8],"terminal_code":"1","card_id":"2","money":"99.99","terminal_model":"4","serial":"5","time":"20170417112459", "dev_time":"20170417112550"}
		void (*callback)(const std::string& records, void *user_parm);
		void *user_parm;
	};

	void query_records(const query_terminal_records_parm& p);
}

#endif // __pos_db_h__
