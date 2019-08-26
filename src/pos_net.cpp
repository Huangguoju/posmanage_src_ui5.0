
#include "net_driver.h"
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ip/udp.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/typeof/typeof.hpp>
#include "pos_net.h"
#include "pos_terminal_parser.h"
#include "iconv.h"

namespace pos_net
{
	struct service_tag;
	typedef ho::net_service<service_tag> service;

	using namespace boost::asio::ip;

	static const size_t c_max_line = 512;

	struct server_base
	{
		parm _parm;
		bool _has_started;

        server_base(const parm& p)
            : _parm(p), _has_started(false), m_cd(libiconv_t(-1))
		{
			_parm.start_tag += _parm.item_sep;

            if (_parm.encoding != "UTF-8")
            {
                m_cd = libiconv_open("UTF-8", _parm.encoding.c_str());
                if (m_cd == libiconv_t(-1))
                {
                    printf("pos_net: libiconv_open(to=[UTF-8], from=[%s]) failed !\n",
                           _parm.encoding.c_str());
                }
            }
		}

		void invoke_callback(e_callback_type type, const char *item = NULL)
		{
			if (_parm.callback)
				_parm.callback(type, item, _parm.user_parm);
		}

        void on_data(char *buf, size_t& size)
		{
            if (_parm.type == POS_TYPE_RECEIPTS)
				on_data_cashing(buf, size);
            else if (_parm.type == POS_TYPE_TERMINAL)
				on_data_terminal(buf, size);
            else if (_parm.type == POS_TYPE_PLAINTEXT)
                on_data_plaintext(buf, size);
		}

		void on_data_terminal(char *buf, size_t& size)
		{
			std::string msg = parse_terminal_msg(buf, size);
			if (!msg.empty())
            {
                strcpy(buf, msg.c_str());
                convert_encoding(buf, size);
                invoke_callback(CALLBACK_TYPE_ITEM, buf);
            }
            size = 0;
		}

		void on_data_cashing(char *buf, size_t& size)
		{
            convert_encoding(buf, size);
			char *p = buf;
			on_data_loop(p, size);
			if (p == buf)
				return;

			memmove(buf, p, size + 1);
		}

        void on_data_plaintext(char *buf, size_t &size)
        {
            remove_extra_space(buf, size);
            convert_encoding(buf, size);
            if (size > 0)
                invoke_callback(CALLBACK_TYPE_ITEM, buf);

            size = 0;
        }

		void on_data_loop(char *& buf, size_t& size)
		{
			while (size)
			{
				if (!_has_started)
				{
					if (size < _parm.start_tag.size())
						return;
					BOOST_AUTO(r, boost::find_first(buf, _parm.start_tag));
					if (r.begin() == r.end())
					{
						size = _parm.start_tag.size()-1;
						buf = r.begin() - size;
						return;
					}
					invoke_callback(CALLBACK_TYPE_START);
					_has_started = true;
					size = buf + size - r.end();
					buf = r.end();
					continue;
				}
				if (_parm.item_sep.empty())
				{
					BOOST_AUTO(r, boost::find_first(buf, _parm.stop_tag));
					if (r.begin() == r.end())
					{
						invoke_callback(CALLBACK_TYPE_ITEM, buf);
						size = 0;
						buf = r.end();
						return;
					}
					if (r.begin() != buf)
					{
						*r.begin() = '\0';
						invoke_callback(CALLBACK_TYPE_ITEM, buf);
					}
					invoke_callback(CALLBACK_TYPE_STOP);
					_has_started = false;
					size = buf + size - r.end();
					buf = r.end();
					continue;
				}
				if (size < _parm.item_sep.size())
					return;
				BOOST_AUTO(r, boost::find_first(buf, _parm.item_sep));
				if (r.begin() == r.end())
					return;
				if (r.begin() != buf)
				{
					*r.begin() = '\0';
					if (_parm.stop_tag == buf)
					{
						invoke_callback(CALLBACK_TYPE_STOP);
						_has_started = false;
					}
					else
						invoke_callback(CALLBACK_TYPE_ITEM, buf);
				}
				size = buf + size - r.end();
				buf = r.end();		
			}
		}

        void remove_extra_space(char *txt, size_t &size)
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

        void convert_encoding(char *&buf, size_t &size)
        {
            if (m_cd == libiconv_t(-1))
                return;

            char *pi = buf;
            char *po = m_cvt_buf;
            size_t in_left = size, out_left = sizeof(m_cvt_buf);
            if (libiconv(m_cd, &pi, &in_left, &po, &out_left) == 0)
            {
                buf = m_cvt_buf;
                size = sizeof(m_cvt_buf) - out_left;
                buf[size] = '\0';
            }
        }

		virtual void start() = 0;
		virtual void stop() = 0;

        virtual ~server_base()
        {
            if (m_cd != libiconv_t(-1))
            {
                libiconv_close(m_cd);
                m_cd = libiconv_t(-1);
            }
        }

        char m_cvt_buf[c_max_line + 1];
        libiconv_t m_cd;
	};

	struct tcp_server : server_base
	{
		struct session 
		{
			tcp::socket _socket;
			bool _dead;
			char _buf[c_max_line + 1];
			size_t _recv_len;
			tcp_server *_server;

			session(tcp_server *s)
				: _socket(service::get_io_service()),
				_dead(false),
				_recv_len(0),
				_server(s)
			{

			}

			void start()
			{
				_socket.async_read_some(
					boost::asio::buffer(_buf+_recv_len, c_max_line-_recv_len),
					boost::bind(&session::on_recv, this, _1, _2)
					);
			}

			void on_recv(const boost::system::error_code& e, size_t size)
			{
				if (_dead)
					return;
				if (e)
				{
					stop();
					return;
				}
				else if (size == 0)
					start();

				_recv_len += size;
				_buf[_recv_len] = '\0';
				_server->on_data(_buf, _recv_len);
				if (_recv_len == c_max_line)
				{
					printf("[pos_net] max_line_size\n");
					stop();
				}
				else
					start();
			}

			void stop()
			{
				if (!_dead)
				{
					_dead = true;
					_socket.close();
					_server->_session = NULL;
					service::async_delete(this);
				}
			}
		};

		tcp::acceptor _acceptor;
		session *_session;
		bool _dead;
		
		tcp_server(const parm& p)
			: server_base(p), 
			_acceptor(service::get_io_service(), tcp::endpoint(tcp::v4(), p.port)),
			_session(NULL),
			_dead(false)
		{

		}

		virtual void start()
		{
			session *s = new session(this);
			_acceptor.async_accept(
				s->_socket,
				boost::bind(&tcp_server::on_accept, this, s, _1)
				);
		}

		void on_accept(session *s, const boost::system::error_code& e)
		{
			if (_dead)
			{
				delete s;
				return;
			}
			if (e)
			{
				delete s;
				start();
				return;
			}
			
			if (_session)
				_session->stop();
			_session = s;
			_session->start();
			start();
		}

		virtual void stop()
		{
			if (!_dead)
			{
				_dead = true;
				_acceptor.close();
				if (_session)
					_session->stop();
				service::async_delete(this);
			}
		}
	};

	struct udp_server : server_base
	{
		udp::socket _socket;
		udp::endpoint _endpoint;
		char _buf[c_max_line + 1];
		size_t _recv_len;
		bool _dead;

		udp_server(const parm& p)
			: server_base(p),
			_socket(service::get_io_service(), udp::endpoint(udp::v4(), p.port)),
			_recv_len(0),
			_dead(false)
		{

		}

		virtual void start()
		{
			_socket.async_receive_from(
				boost::asio::buffer(_buf+_recv_len, c_max_line-_recv_len),
				_endpoint,
				boost::bind(&udp_server::on_recv, this, _1, _2)
				);
		}

		void on_recv(const boost::system::error_code& e, size_t size)
		{
			if (_dead)
				return;
			if (e || size == 0)
			{
				start();
				return;
			}

			_recv_len += size;
			_buf[_recv_len] = '\0';
			on_data(_buf, _recv_len);
			if (_recv_len == c_max_line)
			{
				printf("[pos_net] max_line_size\n");
				_recv_len = 0;
			}
			start();
		}

		virtual void stop()
		{
			if (!_dead)
			{
				_dead = true;
				_socket.close();
				service::async_delete(this);
			}
		}
	};

	void *start(const parm& p)
	{
		server_base *ret;
		try
		{
			switch (p.proto)
			{
			case PROTO_TCP :
				ret = new tcp_server(p);
				break;
			case PROTO_UDP :
				ret = new udp_server(p);
				break;
			default :
				return NULL;
			}
		}
		catch (const std::exception& e)
		{
			printf("[pos_net] start error(%s)\n", e.what());
			return NULL;
		}
		service::async_call(boost::bind(&server_base::start, ret));
		return ret;
	}

	void stop(void **p)
	{
		if (p && *p)
		{
			service::sync_call(boost::bind(&server_base::stop, (server_base *)(*p)));
			*p = NULL;
		}
	}
}
