#include "strategy/cmMM01.h"
#include "glog\logging.h"


bool cmMM01::pauseMM(boost::function<void()> pauseHandler)
{
	write_lock lock(m_pauseReqLock);
	if (m_pauseReq)
		return false;
	else
	{
		m_pauseReq = true;
		m_oneTimeMMPausedHandler = pauseHandler;
		return true;
	}
};

void cmMM01::callPauseHandler()
{
	if (m_oneTimeMMPausedHandler)
	{
		m_oneTimeMMPausedHandler();
		m_oneTimeMMPausedHandler = NULL;
	}
};

void cmMM01::resumeMM()
{
	{
		boost::recursive_mutex::scoped_lock lock(m_strategyStatusLock); 
		write_lock lock1(m_pauseReqLock);
		m_strategyStatus = STRATEGY_STATUS_READY;
		m_pauseReq = false; 
		cout << m_strategyId << " resumed." << endl;
	}
};