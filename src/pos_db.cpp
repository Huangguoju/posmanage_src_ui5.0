
#include "net_driver.h"
#include <boost/format.hpp>
#include <boost/date_time/posix_time/conversion.hpp>
#include <boost/date_time/posix_time/posix_time_types.hpp>
#include <boost/typeof/typeof.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>
#include "sqlite3.h"
#include "pos_db.h"

namespace pos_db
{
	struct service_tag;
	typedef ho::net_service<service_tag> service;

	static config_parm s_config;

	static std::string file_path(const std::string& path, const std::string& file_name)
	{
		std::string str = path;
		if (!str.empty() && *str.rbegin() != '/' && *str.rbegin() != '\\')
			str += '/';
		str += file_name;
		return str;
	}

	struct sqlite_con
	{
		sqlite3 *_db;

		sqlite_con() : _db(NULL) {}

		static int get_version_callback(void *user_parm, int, char **v, char**)
		{
			std::string& version = *(std::string *)user_parm;
			version = v[0];
			return 0;
		}

		std::string get_version()
		{
			std::string version;
			exec("SELECT value FROM t_info WHERE name='version';", &get_version_callback, &version);
			return version;
		}

		void open()
		{
			if (_db)
				return;

			std::string str = file_path(s_config.path, "pos.db");
			int r = sqlite3_open(str.c_str(), &_db);
			if (r || !_db)
			{
				printf("[pos_db] open %s fail.\n", str.c_str());
				close();
				return;
			}

			exec(
				"BEGIN;\n"
				"PRAGMA foreign_keys = ON;\n"
				"CREATE TABLE IF NOT EXISTS t_info (\n"
				"name TEXT,\n"
				"value TEXT\n"
				");\n"
				"CREATE TABLE IF NOT EXISTS t_record (\n"
				"id INTEGER PRIMARY KEY AUTOINCREMENT,\n"
				"pos_id INTEGER,\n"
				"pos_name TEXT,\n"
				"start INTEGER,\n"
				"stop INTEGER,\n"
				"relate_channels TEXT\n"
				");\n"
				"CREATE INDEX IF NOT EXISTS t_record_pos_id_and_start_index\n"
				"ON t_record (pos_id ASC, start ASC);\n"
				"CREATE TABLE IF NOT EXISTS t_item (\n"
				"id INTEGER,\n"
				"i INTEGER,\n"
				"item TEXT,\n"
				"CONSTRAINT fkey0 FOREIGN KEY (id) REFERENCES t_record (id) ON DELETE CASCADE\n"
				");\n"
				"CREATE INDEX IF NOT EXISTS t_item_id_i_index\n"
				"ON t_item (id ASC, i ASC);\n"
				"CREATE TABLE IF NOT EXISTS t_record_terminal (\n"
				"id INTEGER PRIMARY KEY AUTOINCREMENT,\n"
				"pos_id INTEGER,\n"
				"pos_name TEXT,\n"
				"terminal_code TEXT,\n"
				"card_id TEXT,\n"
				"money TEXT,\n"
				"terminal_model TEXT,\n"
				"serial TEXT,\n"
				"time TEXT,\n"
				"dev_time TEXT,\n"
				"relate_channels TEXT\n"
				");\n"
				"CREATE INDEX IF NOT EXISTS t_record_terminal_pos_id_and_time_index\n"
				"ON t_record_terminal (pos_id ASC, time ASC);\n"
				);

			std::string version = get_version();
			if (version.empty())
				exec("INSERT INTO t_info values('version','1');");
			else
				BOOST_ASSERT(version == "1");

			exec("END;\n");
		}

		void exec(const char *cmd, int (*callback)(void*,int,char**,char**) = NULL, void *parm = NULL)
		{
			if (!_db)
			{
				printf("[pos_db] exec %s when db is not opened.\n", cmd);
				return;
			}

			char *err = NULL;
			int r = sqlite3_exec(_db, cmd, callback, parm, &err);
			if (!r) return;

			if (err)
			{
				printf("[pos_db] exec %s fail : %s\n", cmd, err);
				sqlite3_free(err);
			}
			else
				printf("[pos_db] exec %s fail.\n", cmd);
		}

		void close()
		{
			if (!_db) return;
			sqlite3_close(_db);
			_db = NULL;
		}

		unsigned long long last_rowid()
		{
			if (_db)
				return (unsigned long long)sqlite3_last_insert_rowid(_db);
			else
				return 0;
		}
	};

	static sqlite_con s_sqlite;

	static void on_config(const config_parm& p)
	{
		s_config = p;
		s_sqlite.close();
		if (!s_config.path.empty())
			s_sqlite.open();
	}

	void config(const config_parm& p)
	{
		if (p.path.empty())
			service::sync_call(boost::bind(&on_config, p));
		else
			service::async_call(boost::bind(&on_config, p));
	}

	static time_t time_to_time_t(const time& t)
	{
		using namespace boost::posix_time;
		ptime::date_type d(t.year, t.month, t.day);
		ptime::time_duration_type td(t.hour, t.min, t.sec);
		ptime pt(d, td);
		return to_time_t(pt);
	}

	static void on_write(const record& rec)
	{
		s_sqlite.exec("BEGIN;");

		boost::format fmt(
			"INSERT INTO t_record(pos_id, pos_name, start, stop, relate_channels)\n"
			"VALUES(%1%, '%2%', %3%, %4%, '%5%');\n"
			);
		fmt % rec.pos_id;
		fmt % rec.pos_name;
		fmt % time_to_time_t(rec.start);
		fmt % time_to_time_t(rec.stop);
		std::string s;
		for (size_t i=0; i<rec.relate_channels.size(); ++i)
			s += boost::lexical_cast<std::string>((int)rec.relate_channels[i]) + ";";
		fmt % s;
		s_sqlite.exec(fmt.str().c_str());

		unsigned long long rowid = s_sqlite.last_rowid();
		if (rowid)
		{
			for (size_t i=0; i<rec.items.size(); ++i)
			{
				boost::format fmt(
					"INSERT INTO t_item(id, i, item)\n"
					"VALUES(%1%, %2%, '%3%');\n"
					);
				fmt % rowid;
				fmt % i;
				fmt % rec.items[i];
				s_sqlite.exec(fmt.str().c_str());
			}
		}
		
		s_sqlite.exec("END;");
	}

	void write(const record& rec)
	{
		service::async_call(boost::bind(&on_write, rec));
	}

	static void time_t_to_time(const char *p, time& ret)
	{
		using namespace boost::posix_time;
		time_t t = boost::lexical_cast<time_t>(p);
		ptime pt = from_time_t(t);
		tm tm_t = to_tm(pt);
		ret.year = (unsigned short)(tm_t.tm_year + 1900);
		ret.month = (unsigned char)(tm_t.tm_mon + 1);
		ret.day = (unsigned char)tm_t.tm_mday;
		ret.hour = (unsigned char)tm_t.tm_hour;
		ret.min = (unsigned char)tm_t.tm_min;
		ret.sec = (unsigned char)tm_t.tm_sec;
	}

	static int query_records_callback(void *user_parm, int, char **v, char**)
	{
		std::vector<record>& records = *(std::vector<record> *)user_parm;
		records.push_back(record());
		record& rec = records.back();
		rec.id = boost::lexical_cast<unsigned long long>(v[0]);
		rec.pos_name = v[1];
		time_t_to_time(v[2], rec.start);
		time_t_to_time(v[3], rec.stop);
		if (strlen(v[4]))
		{
			std::vector<std::string> relate_channels;
			boost::split(relate_channels, v[4], boost::is_any_of(";"));
			for (size_t i=0; i<relate_channels.size()-1; ++i)
				rec.relate_channels.push_back((unsigned char)boost::lexical_cast<int>(relate_channels[i]));
		}
		return 0;
	}

	static void on_query_records(const query_records_parm& p)
	{
		boost::format fmt(
			"SELECT id, pos_name, start, stop, relate_channels FROM t_record\n"
            "WHERE pos_id=%1% AND start>=%2% AND start<=%3% %4% LIMIT %5%;\n"
			);
		fmt % p.pos_id;
		fmt % time_to_time_t(p.begin);
		fmt % time_to_time_t(p.end);
		if (p.keys.empty())
			fmt % "";
		else
		{
			std::string keys_str = "AND ( ";
			keys_str += " EXISTS (SELECT 1 from t_item WHERE id=t_record.id AND item LIKE '%" + p.keys[0] + "%') ";
			for (size_t i=1; i<p.keys.size(); ++i)
			{
				static const char *or_and[] = { "OR", "AND" };
				keys_str += or_and[p.is_and];
				keys_str += " EXISTS (SELECT 1 from t_item WHERE id=t_record.id AND item LIKE '%" + p.keys[i] + "%') ";
			}
			keys_str += " )";
			fmt % keys_str;
		}
		fmt % p.max_records;

		std::vector<record> records;
		s_sqlite.exec(fmt.str().c_str(), &query_records_callback, &records);
		for (size_t i=0; i<records.size(); ++i)
			records[i].pos_id = p.pos_id;
		if (p.callback)
			p.callback(records, p.user_parm);
	}

	void query_records(const query_records_parm& p)
	{
		service::async_call(boost::bind(&on_query_records, p));
	}

	static int query_items_callback(void *user_parm, int, char **v, char**)
	{
		std::vector<std::string>& items = *(std::vector<std::string> *)user_parm;
		items.push_back(v[0]);
		return 0;
	}

	void on_query_items(const query_items_parm& p)
	{
		boost::format fmt(
			"SELECT item FROM t_item\n"
			"WHERE id=%1% ORDER BY i ASC;\n"
			);
		fmt % p.record_id;

		std::vector<std::string> items;
		s_sqlite.exec(fmt.str().c_str(), &query_items_callback, &items);
		if (p.callback)
			p.callback(items, p.user_parm);
	}

	void query_items(const query_items_parm& p)
	{
		service::async_call(boost::bind(&on_query_items, p));
	}

	static void on_write_terminal_record(const std::string& rec)
	{
		s_sqlite.exec("BEGIN;");

		boost::format fmt(
			"INSERT INTO t_record_terminal(pos_id, pos_name, terminal_code, card_id, money, terminal_model, serial, time, dev_time, relate_channels)\n"
			"VALUES(%1%, '%2%', '%3%', '%4%', '%5%', '%6%', '%7%', '%8%', '%9%', '%10%');\n"
			);

		rapidjson::Document doc;
		doc.Parse<0>(rec.c_str());
		BOOST_ASSERT(!doc.HasParseError());

		fmt % doc["pos_id"].GetInt64();
		fmt % doc["pos_name"].GetString();
		fmt % doc["terminal_code"].GetString();
		fmt % doc["card_id"].GetString();
		fmt % doc["money"].GetString();
		fmt % doc["terminal_model"].GetString();
		fmt % doc["serial"].GetString();
		fmt % doc["time"].GetString();

		boost::posix_time::ptime now = boost::posix_time::second_clock::local_time();
		BOOST_AUTO(d, now.date());
		BOOST_AUTO(t, now.time_of_day());
		char dev_time[32];
		sprintf(dev_time, "%04d%02d%02d%02d%02d%02d", (int)d.year(), (int)d.month(), (int)d.day(), (int)t.hours(), (int)t.minutes(), (int)t.seconds());
		fmt % dev_time;

		rapidjson::StringBuffer sb;
		rapidjson::Writer<rapidjson::StringBuffer> w(sb);
		doc["relate_channels"].Accept(w);
		fmt % sb.GetString();

		s_sqlite.exec(fmt.str().c_str());

		s_sqlite.exec("END;");
	}

	void write_terminal_record(const std::string& rec)
	{
		service::async_call(boost::bind(&on_write_terminal_record, rec));
	}

	static int query_terminal_records_callback(void *user_parm, int, char **v, char**)
	{
		rapidjson::Document& res_doc = *(rapidjson::Document *)user_parm;
		rapidjson::Value rec(rapidjson::kObjectType);
		rapidjson::Value mem_value;
		rec.AddMember("pos_id", boost::lexical_cast<unsigned long long>(v[0]), res_doc.GetAllocator());
		mem_value.SetString(v[1], res_doc.GetAllocator());
		rec.AddMember("pos_name", mem_value, res_doc.GetAllocator());
		mem_value.SetString(v[2], res_doc.GetAllocator());
		rec.AddMember("terminal_code", mem_value, res_doc.GetAllocator());
		mem_value.SetString(v[3], res_doc.GetAllocator());
		rec.AddMember("card_id", mem_value, res_doc.GetAllocator());
		mem_value.SetString(v[4], res_doc.GetAllocator());
		rec.AddMember("money", mem_value, res_doc.GetAllocator());
		mem_value.SetString(v[5], res_doc.GetAllocator());
		rec.AddMember("terminal_model", mem_value, res_doc.GetAllocator());
		mem_value.SetString(v[6], res_doc.GetAllocator());
		rec.AddMember("serial", mem_value, res_doc.GetAllocator());
		mem_value.SetString(v[7], res_doc.GetAllocator());
		rec.AddMember("time", mem_value, res_doc.GetAllocator());
		mem_value.SetString(v[8], res_doc.GetAllocator());
		rec.AddMember("dev_time", mem_value, res_doc.GetAllocator());

		rapidjson::Document relate_channels_doc(&res_doc.GetAllocator());
		relate_channels_doc.Parse<0>(v[9]);
		BOOST_ASSERT(!relate_channels_doc.HasParseError());
		rapidjson::Value& relate_channels = relate_channels_doc;
		rec.AddMember("relate_channels", relate_channels, res_doc.GetAllocator());
		
		res_doc.PushBack(rec, res_doc.GetAllocator());
		return 0;
	}

	static void on_query_terminal_records(const query_terminal_records_parm& p)
	{
		boost::format fmt(
			"SELECT pos_id, pos_name, terminal_code, card_id, money, terminal_model, serial, time, dev_time, relate_channels FROM t_record_terminal\n"
			"WHERE pos_id=%1% AND time>='%2%' AND time<='%3%' %4% LIMIT %5%;\n"
			);
		rapidjson::Document doc;
		doc.Parse<0>(p.query_info.c_str());
		BOOST_ASSERT(!doc.HasParseError());

		fmt % doc["pos_id"].GetInt64();
		fmt % doc["begin"].GetString();
		fmt % doc["end"].GetString();

		std::string keys;
		const char *key_names[] =
		{
			"terminal_code",
			"card_id",
			"terminal_model",
			"serial"
		};
		for (size_t i=0; i<sizeof(key_names)/sizeof(key_names[0]); ++i)
		{
			if (doc.HasMember(key_names[i]))
			{
				const char *v = doc[key_names[i]].GetString();
				if (*v)
					keys += std::string(" AND ") + key_names[i] + " LIKE '%" + v + "%'";
			}
		}
		fmt % keys;

		fmt % doc["max_records"].GetUint64();

		rapidjson::Document res_doc;
		res_doc.SetArray();
		s_sqlite.exec(fmt.str().c_str(), &query_terminal_records_callback, &res_doc);
		if (p.callback)
		{
			rapidjson::StringBuffer sb;
			rapidjson::Writer<rapidjson::StringBuffer> w(sb);
			res_doc.Accept(w);
			p.callback(sb.GetString(), p.user_parm);
		}
	}

	void query_records(const query_terminal_records_parm& p)
	{
		service::async_call(boost::bind(&on_query_terminal_records, p));
	}
}

