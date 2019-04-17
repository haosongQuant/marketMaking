#include "strategy/cmMM01.h"
#include "glog\logging.h"

void cmMM01::daemonEngine(){
	m_tradeGrpBuffer.clear();

	for each(auto item in m_aliveTrdGrp)
		m_tradeGrpBuffer.push_back(item);
	m_aliveTrdGrp.clear();
	{
		boost::mutex::scoped_lock lock(m_cycle2tradeGrpLock);
		for each(auto item in m_cycle2tradeGrp)
			m_tradeGrpBuffer.push_back(item.second);
		m_cycle2tradeGrp.clear();
	}

	for each(auto item in m_tradeGrpBuffer)
	{
		map<int, bool> isOrderComplete;
		for each(auto orderRef in item->m_orderIdList)
		{
			map < int, orderRtnPtr>::iterator iter01;
			{
				read_lock lock2(m_orderRtnBuffLock);
				iter01 = m_orderRef2orderRtn.find(orderRef); 
				if (iter01 == m_orderRef2orderRtn.end() || 
					iter01->second->m_orderStatus == ORDER_STATUS_Unknown)
				{
					m_infra->queryOrder(m_tradeAdapterID); //如果报单回报未返回或状态未知，发起报单查询
					continue;
				}
			}
			switch (iter01->second->m_orderStatus)
			{
				//遇到下面几个状态，撤单
				case ORDER_STATUS_PartTradedQueueing:///部分成交还在队列中,
				case ORDER_STATUS_PartTradedNotQueueing:///部分成交不在队列中,
				case ORDER_STATUS_NoTradeQueueing:///未成交还在队列中,
				case ORDER_STATUS_NoTradeNotQueueing:///未成交不在队列中,
				case ORDER_STATUS_NotTouched:///尚未触发,
				case ORDER_STATUS_Touched:///已触发,
				{
					m_infra->cancelOrder(m_tradeAdapterID, orderRef,bind(&cmMM01::onRspCancel, this, _1));
					break;
				}
				//遇到下面几个状态，计算对冲量
				case ORDER_STATUS_Canceled: ///撤单,
				case ORDER_STATUS_AllTraded:///全部成交,
				{
					break;
				}
			}
		}
	}

	m_daemonTimer.expires_from_now(boost::posix_time::millisec(1000*60)); //每分钟运行一次
	m_daemonTimer.async_wait(boost::bind(&cmMM01::daemonEngine, this));
};

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