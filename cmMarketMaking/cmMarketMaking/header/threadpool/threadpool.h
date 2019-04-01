#pragma once
#include <boost\asio.hpp>
#include <boost\thread.hpp>

typedef boost::shared_ptr<boost::asio::strand> StrandPtr;

class threadpool : public boost::enable_shared_from_this<threadpool>
{
private:
	boost::asio::io_service             m_ioService;
	boost::asio::io_service::work       m_ioServiceWork;
	boost::thread_group                 m_threadGroup;
	StrandPtr							m_pStrand;
public:
	threadpool(int threadnum);
	StrandPtr getDispatcherStrand(){ return m_pStrand; };
	boost::asio::io_service& getDispatcher() { return m_ioService; };
	void finish(){m_ioService.stop();m_threadGroup.join_all();};
};
typedef boost::shared_ptr<threadpool> athenathreadpoolPtr;

typedef boost::asio::deadline_timer athena_lag_timer;
