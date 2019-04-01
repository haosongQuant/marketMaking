// This is the main DLL file.
#pragma once

#include "threadpool/threadpool.h"
#include <limits>
using namespace std;

threadpool::threadpool(int threadnum) :m_ioServiceWork(m_ioService), m_pStrand(new boost::asio::strand(m_ioService))
{
	for (int i = 0; i < threadnum; i++)
	{
		boost::system::error_code error;
		m_threadGroup.create_thread(boost::bind(&boost::asio::io_service::run, boost::ref(m_ioService), error));
	}
}