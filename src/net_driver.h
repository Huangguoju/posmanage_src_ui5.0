#ifndef __net_driver_h__
#define __net_driver_h__

#define BOOST_ALL_NO_LIB
#define BOOST_ERROR_CODE_HEADER_ONLY
#define BOOST_SYSTEM_NO_DEPRECATED
#define BOOST_CHRONO_HEADER_ONLY
#include <boost/asio/io_service.hpp>
#include <boost/bind.hpp>
#include <boost/core/checked_delete.hpp>

#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#include <unistd.h>
#endif
#include <boost/noncopyable.hpp>
#include <boost/assert.hpp>

namespace ho
{
	struct thread : boost::noncopyable
	{
	private:
		struct thread_impl_base
		{
			virtual ~thread_impl_base() {}
			virtual void run() = 0;
		};

		template <typename F>
		struct thread_impl : public thread_impl_base
		{
			explicit thread_impl(const F& f) : _f(f) {}
			void run() { _f(); }
		private:
			F _f;
		};

		static
#ifdef _WIN32
			DWORD WINAPI
#else
			void *
#endif
			thread_fun(void *parm)
		{
			thread_impl_base *t = (thread_impl_base *)parm;
			t->run();
			delete t;
			return 0;
		}

	public:
		static void sleep(unsigned int ms)
		{
#ifdef _WIN32
			Sleep(ms);
#else
			BOOST_VERIFY(!usleep(ms * 1000));
#endif
		}

		thread() : _attached(false) {}
		~thread()
		{
			detach();
		}

		template <typename F>
		void operator=(const F& f)
		{
			detach();
			thread_impl_base *p = new thread_impl<F>(f);
#ifdef _WIN32
			_h = CreateThread(NULL, 0, &thread_fun, p, 0, NULL);
			if (!_h)
#else
			if (pthread_create(&_h, NULL, &thread_fun, p))
#endif
			{
				delete p;
			}
			else
				_attached = true;
			BOOST_ASSERT(_attached);
		}

		void join()
		{
			if (!_attached)
				return;
#ifdef _WIN32
			BOOST_VERIFY(WaitForSingleObject(_h, INFINITE) == WAIT_OBJECT_0);
			BOOST_VERIFY(CloseHandle(_h));
#else
			BOOST_VERIFY(!pthread_join(_h, NULL));
#endif
			_attached = false;
		}

	private:
		void detach()
		{
			if (!_attached)
				return;
#ifdef _WIN32
			BOOST_VERIFY(CloseHandle(_h));
#else
			BOOST_VERIFY(!pthread_detach(_h));
#endif
			_attached = false;
		}

#ifdef _WIN32
		HANDLE
#else
		pthread_t
#endif
			_h;
		bool _attached;
	};

	struct mutex : boost::noncopyable
	{
		mutex()
		{
#ifdef _WIN32
			InitializeCriticalSection(&_h);
#else
			BOOST_VERIFY(!pthread_mutex_init(&_h, NULL));
#endif
		}

		~mutex()
		{
#ifdef _WIN32
			DeleteCriticalSection(&_h);
#else
			BOOST_VERIFY(!pthread_mutex_destroy(&_h));
#endif
		}

		void lock()
		{
#ifdef _WIN32
			EnterCriticalSection(&_h);
#else
			BOOST_VERIFY(!pthread_mutex_lock(&_h));
#endif
		}

		void unlock()
		{
#ifdef _WIN32
			LeaveCriticalSection(&_h);
#else
			BOOST_VERIFY(!pthread_mutex_unlock(&_h));
#endif
		}

	private:
#ifdef _WIN32
		CRITICAL_SECTION
#else
		pthread_mutex_t
#endif
			_h;
		friend struct event;
	};

	struct lock_guard : boost::noncopyable
	{
		lock_guard(mutex& m) : _mutex(&m)
		{
			_mutex->lock();
		}

		~lock_guard()
		{
			_mutex->unlock();
		}

	private:
		mutex *_mutex;
	};

	struct event : boost::noncopyable
	{
		event()
		{
#ifdef _WIN32
			_h = CreateEvent(NULL, false, false, NULL);
			BOOST_ASSERT(_h);
#else
			BOOST_VERIFY(!pthread_cond_init(&_cond, NULL));
			_set = false;
#endif
		}

		~event()
		{
#ifdef _WIN32
			BOOST_VERIFY(CloseHandle(_h));
#else
			BOOST_VERIFY(!pthread_cond_destroy(&_cond));
#endif
		}

		void wait()
		{
#ifdef _WIN32
			BOOST_VERIFY(WaitForSingleObject(_h, INFINITE) == WAIT_OBJECT_0);
#else
			lock_guard lock(_mutex);
			while (!_set)
				pthread_cond_wait(&_cond, &_mutex._h);
			_set = false;
#endif
		}

		void notify()
		{
#ifdef _WIN32
			BOOST_VERIFY(SetEvent(_h));
#else
			lock_guard lock(_mutex);
			_set = true;
			pthread_cond_signal(&_cond);
#endif
		}

	private:
#ifdef _WIN32
		HANDLE _h;
#else
		mutex _mutex;
		pthread_cond_t _cond;
		volatile bool _set;
#endif
	};

	template <typename Tag>
	struct net_service
	{
		typedef boost::asio::io_service io_service;

		static io_service& get_io_service()
		{
			return instance()->_io_service;
		}

		template <typename F>
		static void async_call(const F& f)
		{
			get_io_service().post(f);
		}

		template <typename F>
		static void sync_call(const F& f)
		{
			ho::event e;
			get_io_service().dispatch(sync_handler<F>(f, &e));
			e.wait();
		}

		template <typename T>
		static void async_delete(T *x)
		{
			get_io_service().post(boost::bind(&boost::checked_delete<T>, x));
		}

		static void shutdown()
		{
			instance().reset();
		}

		~net_service()
		{
			delete _work;
			_thread.join();
		}

	private:
		template <typename F>
		struct sync_handler
		{
			sync_handler(const F& f, event *e) : _f(f), _e(e) {}
			void operator()()
			{
				_f();
				_e->notify();
			}
			F _f;
			event *_e;
		};

	private:
		typedef std::auto_ptr<net_service> ptr;

		static ptr& instance()
		{
			static ptr p;
			if (!p.get())
				p.reset(new net_service);
			return p;
		}

		net_service()
		{
			_work = new io_service::work(_io_service);
			_thread = boost::bind(&io_service::run, &_io_service);
		}	

		io_service _io_service;
		io_service::work *_work;
		ho::thread _thread;
	};
}

#endif // __net_driver_h__
