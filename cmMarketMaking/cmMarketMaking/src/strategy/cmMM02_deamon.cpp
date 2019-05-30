#include "strategy/cmMM02.h"
#include "glog\logging.h"

bool cmMM02::isInOpenTime()
{
	time_t t;
	tm* local;
	t = time(NULL);
	local = localtime(&t);
	int timeSec = (local->tm_hour * 100 + local->tm_min) * 100 + local->tm_sec;
	for (auto item : m_openTimeList)
		if (item.first <= timeSec && timeSec <= item.second)
			return true;
	return false;
};

bool cmMM02::isOrderComplete(int orderRef, bool &isTraded)
{
	bool isTrdComplete = true;
	//int orderTradedVol = 0;
	auto iter01 = m_orderRef2orderRtn.find(orderRef);
	if (iter01 == m_orderRef2orderRtn.end() ||
		iter01->second->m_orderStatus == ORDER_STATUS_Unknown)
	{
		m_tradeTP->getDispatcher().post(boost::bind(&infrastructure::queryOrder,
			m_infra, m_tradeAdapterID, orderRef)); //如果报单回报未返回或状态未知，发起报单查询
		return false;
	}

	switch (iter01->second->m_orderStatus)
	{
		//遇到下面几个中间状态，撤单
	case ORDER_STATUS_PartTradedQueueing:///部分成交还在队列中,
	case ORDER_STATUS_NoTradeQueueing:///未成交还在队列中,
	case ORDER_STATUS_NotTouched:///尚未触发,
	case ORDER_STATUS_Touched:///已触发,
	{
		m_infra->cancelOrder(m_tradeAdapterID, orderRef, bind(&cmMM02::onRspCancel, this, _1));
		isTrdComplete = false;
		break;
	}
	//遇到下面2个状态, 返回已交易
	case ORDER_STATUS_AllTraded:///全部成交,
	case ORDER_STATUS_PartTradedNotQueueing:///部分成交不在队列中,
	{
		isTraded = true;
		break;
	}
	case ORDER_STATUS_Canceled: ///撤单,
	case ORDER_STATUS_NoTradeNotQueueing:///未成交不在队列中,
	case ORDER_STATUS_TerminatedFromCancel:///撤单时，返回报单已成交或撤销,
	{
		break;
	}
	}//end:处理每个状态
	return isTrdComplete;
};

void cmMM02::daemonEngine(){

	//if (!m_infra->isAdapterReady(m_tradeAdapterID))
	if (!isInOpenTime() || !m_infra->isAdapterReady(m_tradeAdapterID))
	{
		if (cmMM02_STATUS_INIT != m_strategyStatus)
		{
			write_lock lock(m_breakReqLock);
			m_breakReq = true;
		}
		m_daemonTimer.expires_from_now(boost::posix_time::millisec(1000 * 60));
		m_daemonTimer.async_wait(boost::bind(&cmMM02::daemonEngine, this));
		return;
	}
	if (!m_isInvestorPositionReady)
	{
		m_infra->queryInitPosition(m_tradeAdapterID, m_productId, m_initPosition);
		for (auto positionPtr : m_initPosition)
			m_investorPosition[m_productId][positionPtr->m_holdingDirection] = positionPtr;
		int validHoldingVol = m_investorPosition[m_productId][HOLDING_DIR_LONG]->m_position
						   <= m_investorPosition[m_productId][HOLDING_DIR_SHORT]->m_position ?
							  m_investorPosition[m_productId][HOLDING_DIR_LONG]->m_position :
							  m_investorPosition[m_productId][HOLDING_DIR_SHORT]->m_position;
		if (validHoldingVol >= m_holdingRequirement)
			m_isHoldingRequireFilled = true;
		m_isInvestorPositionReady = true;
	}
	{
		boost::recursive_mutex::scoped_lock lock(m_strategyStatusLock);
		switch (m_strategyStatus)
		{
		case cmMM02_STATUS_INIT:
		case cmMM02_STATUS_BREAK:
		{
			write_lock lock(m_breakReqLock);
			m_breakReq = false;
			startStrategy();
			break;
		}
		}
	}

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

	read_lock lock2(m_orderRtnBuffLock);
	read_lock lock3(m_tradeRtnBuffLock);
	int totalTradedVol = 0; //已完成的闭环的全部成交量
	for each(auto item in m_tradeGrpBuffer)
	{
		bool isTrdGrpComplete = true;
		bool isTraded = false;
		for each(auto orderRef in item->m_orderIdList)
		{
			if (!isOrderComplete(orderRef, isTraded))
			{
				isTrdGrpComplete = false;
				break;
			}
		}//end: 循环处理每一个order

		if (!isTrdGrpComplete)//闭环未完成
			m_aliveTrdGrp.push_back(item);
		else 
		{	//闭环完成
			LOG(INFO) << m_strategyId << ": -----------CycleStatisticsStart-----------" << endl;
			LOG(INFO) << "," << m_strategyId << ",validTime," << item->m_tradingDate << "," << item->m_Id << ","
					  << ((item->m_start_milliSec != 0.0 && item->m_end_milliSec != 0.0) ?
						(item->m_end_milliSec - item->m_start_milliSec) : 0.0) << endl;
			LOG(INFO) << m_strategyId << "-----------CycleStatisticsEnd-----------" << endl;

			if (isTraded)//有交易发生
			{
				int  cycleTradedVol = 0;
				for each(auto orderRef in item->m_orderIdList)
				{
					int orderTradedVol = 0;
					orderRtnPtr pOrder = m_orderRef2orderRtn[orderRef];
					cycleTradedVol += pOrder->m_direction == ORDER_DIR_BUY ?
						pOrder->m_volumeTraded : (pOrder->m_volumeTraded * -1);
				}//end: 循环处理每一个order
				totalTradedVol += cycleTradedVol;
				LOG(INFO) << m_strategyId << ", cycle: " << item->m_Id 
										  << ", cycleTradedVol: " << cycleTradedVol << endl;
			}
		}

	} //end: 循环处理每一个闭环

	LOG(INFO) << m_strategyId << ", totalTradedVol: " << totalTradedVol << endl;
	if (totalTradedVol != 0)
	{
		m_cycleHedgeVol = totalTradedVol;
		interrupt(boost::bind(&cmMM02::sendCycleNetHedgeOrder, this));
	}

	m_daemonTimer.expires_from_now(boost::posix_time::millisec(1000*60)); //每分钟运行一次
	m_daemonTimer.async_wait(boost::bind(&cmMM02::daemonEngine, this));
};

void cmMM02::sendCycleNetHedgeOrder()
{
	int hedgeVol = m_cycleHedgeVol * -1;
	sendCycleNetHedgeOrder(hedgeVol);
};

void cmMM02::sendCycleNetHedgeOrder(int hedgeVol)
{
	enum_order_dir_type dir = hedgeVol > 0.0 ? ORDER_DIR_BUY : ORDER_DIR_SELL;
	double price = (dir == ORDER_DIR_BUY) ?
	//	m_lastQuotePtr->UpperLimitPrice : m_lastQuotePtr->LowerLimitPrice;
		(m_lastQuotePtr->askprice[0] + m_tickSize * 2.0) : (m_lastQuotePtr->bidprice[0] - m_tickSize * 2.0);
	int cycleNetHedgeOrderRef = m_infra->insertOrder(m_tradeAdapterID, m_productId, m_exchange,
		ORDER_TYPE_LIMIT, dir, POSITION_EFFECT_OPEN, FLAG_SPECULATION, price, abs(hedgeVol),
		bind(&cmMM02::onCycleNetHedgeOrderRtn, this, _1),
		bind(&cmMM02::onCycleNetHedgeTradeRtn, this, _1));
	if (cycleNetHedgeOrderRef > 0)
	{
		//write_lock lock0(m_orderRef2cycleRWlock);
		//m_orderRef2cycle[cycleNetHedgeOrderRef] = m_cycleId;
		LOG(INFO) << m_strategyId << ": send cycle net hedge order succ. code: " << m_productId
			<< ", dir: " << ((dir == ORDER_DIR_BUY) ? "buy, vol: " : "sell, vol: ")
			<< abs(hedgeVol) << endl;
		{
			boost::mutex::scoped_lock lock(m_cycleNetHedgeVolLock);
			m_cycleNetHedgeVol = hedgeVol;
		}
	}
	else
		LOG(INFO) << m_strategyId << " | ERROR: send cycle net hedge order failed, rc = " << cycleNetHedgeOrderRef << endl;
};
void cmMM02::processCycleNetHedgeOrderRtn(orderRtnPtr pOrder){
	write_lock lock(m_orderRtnBuffLock);
	m_orderRef2orderRtn[pOrder->m_orderRef] = pOrder;
};

void cmMM02::processCycleNetHedgeTradeRtn(tradeRtnPtr ptrade)
{

	m_tradeTP->getDispatcher().post(boost::bind(&cmMM02::registerTradeRtn, this, ptrade));
	LOG(INFO) << m_strategyId << ",cycleNetHedgeTrade Rtn." << endl;
	logTrade(ptrade);
	{
		boost::mutex::scoped_lock lock(m_cycleNetHedgeVolLock);
		m_cycleNetHedgeVol -= ptrade->m_orderDir == ORDER_DIR_BUY ? ptrade->m_volume :
			(ptrade->m_volume * -1);
		if (m_cycleNetHedgeVol == 0)
			resume();
	}
};

void cmMM02::interrupt(boost::function<void()> pauseHandler)
{
	if (!pause(pauseHandler) && m_strategyStatus != cmMM02_STATUS_BREAK)
	{
		m_pauseLagTimer.expires_from_now(boost::posix_time::millisec(1000));
		m_pauseLagTimer.async_wait(boost::bind(&cmMM02::interrupt, this, pauseHandler));
	}
};


enum_strategy_interrupt_result cmMM02::tryInterrupt(boost::function<void()> pauseHandler){
	if (m_strategyStatus == cmMM02_STATUS_BREAK)
		return STRATEGY_INTERRUPT_BREAKING;
	else if (pause(pauseHandler))
		return STRATEGY_INTERRUPT_WAIT_CALLBACK;
	else
		return STRATEGY_INTERRUPT_FAIL;
};

bool cmMM02::pause(boost::function<void()> pauseHandler)
{
	write_lock lock(m_pauseReqLock);
	if (m_pauseReq)
		return false;
	else
	{
		m_pauseReq = true;
		m_oneTimeMMPausedHandler = pauseHandler;
		LOG(INFO) << m_strategyId << " pause signal received." << endl;
		return true;
	}
};

void cmMM02::callPauseHandler()
{
	if (m_oneTimeMMPausedHandler)
	{
		m_oneTimeMMPausedHandler();
		LOG(INFO) << m_strategyId << " pause handler called." << endl;
		m_oneTimeMMPausedHandler = NULL;
	}
};

void cmMM02::resume()
{
	boost::recursive_mutex::scoped_lock lock(m_strategyStatusLock); 
	write_lock lock1(m_pauseReqLock);
	m_strategyStatus = cmMM02_STATUS_READY;
	m_pauseReq = false; 
	LOG(INFO) << m_strategyId << " resumed." << endl;
	cout << m_strategyId << ": market making resumed." << endl;
};