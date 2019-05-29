#include "strategy/cmMM02.h"
#include "glog\logging.h"

cmMM02::cmMM02(string strategyId, string strategyTyp, string productId, string exchange,
	string quoteAdapterID, string tradeAdapterID, double tickSize, double miniOrderSpread,
	double maxiOrderSpread, double orderQty, int volMulti, int holdingRequirement,
	athenathreadpoolPtr quoteTP, athenathreadpoolPtr tradeTP, infrastructure* infra,
	Json::Value config)
	:m_strategyId(strategyId), m_strategyTyp(strategyTyp), m_productId(productId), m_exchange(exchange),
	m_quoteAdapterID(quoteAdapterID), m_tradeAdapterID(tradeAdapterID), m_tickSize(tickSize),
	m_miniOrderSpread(miniOrderSpread), m_maxiOrderSpread(maxiOrderSpread),
	m_orderQty(orderQty), m_volumeMultiple(volMulti),
	m_quoteTP(quoteTP), m_tradeTP(tradeTP), m_infra(infra), m_cycleId(0), m_pauseReq(false),
	m_breakReq(false), m_strategyConfig(config), m_holdingRequirement(holdingRequirement),
	m_orderCheckTimer(tradeTP->getDispatcher()), 
	m_cancelHedgeTimer(tradeTP->getDispatcher()),
	m_daemonTimer(tradeTP->getDispatcher()), m_pauseLagTimer(tradeTP->getDispatcher())
{
	m_lastQuotePtr = NULL;
	int openNum = m_strategyConfig["openTime"].size();
	for (int i = 0; i < openNum; ++i)
	{
		Json::Value openInterval = m_strategyConfig["openTime"][i];
		int startTime = openInterval["start"].asInt() * 100 +1;
		int endTime = openInterval["end"].asInt() * 100 + 00;
		m_openTimeList.push_back(make_pair(startTime, endTime));
	}
	m_investorPosition[productId][HOLDING_DIR_LONG] 
		= investorPositionPtr(new investorPosition_struct(productId, HOLDING_DIR_LONG));
	m_investorPosition[productId][HOLDING_DIR_SHORT]
		= investorPositionPtr(new investorPosition_struct(productId, HOLDING_DIR_SHORT));
	m_spotOrderSpread = m_miniOrderSpread;
	m_strategyStatus = cmMM02_STATUS_INIT;
	daemonEngine();
};

void cmMM02::startStrategy(){
	cout << m_strategyId << " starting..." << endl;
	if (cmMM02_STATUS_INIT == m_strategyStatus)
	{
		m_infra->subscribeFutures(m_quoteAdapterID, m_exchange, m_productId, bind(&cmMM02::onRtnMD, this, _1));
	}
	m_strategyStatus = cmMM02_STATUS_READY;
};

void cmMM02::resetStrategyStatus(){ //等待行情触发cycle
	m_strategyStatus = cmMM02_STATUS_READY;
};

//行情处理
//    如果策略处于 READY      状态，下单
//    如果策略处于 ORDER_SENT 状态，撤单并重新下单
void cmMM02::quoteEngine()
{
	boost::recursive_mutex::scoped_lock lock(m_strategyStatusLock);
	LOG(INFO) << m_strategyId << " | status: " << m_strategyStatus << endl;
	switch (m_strategyStatus)
	{
	case cmMM02_STATUS_READY:
	{
		startCycle();
		break;
	}
	case cmMM02_STATUS_ORDER_SENT:
	{
		m_strategyStatus = cmMM02_STATUS_CLOSING_POSITION;
		m_tradeTP->getDispatcher().post(boost::bind(&cmMM02::refreshCycle, this)); //,boost::asio::placeholders::error));
		break;
	}
	case cmMM02_STATUS_PAUSE:
	{
		cout << m_strategyId << ": market making paused!" << endl;
		break;
	}
	case cmMM02_STATUS_BREAK:
	{
		cout << m_strategyId << " breaking ..." << endl;
		break;
	}
	}
};

void cmMM02::orderPrice(double* bidprice, double* askprice)
{
	futuresMDPtr plastQuote;
	{
		boost::mutex::scoped_lock lock(m_lastQuoteLock);
		plastQuote = m_lastQuotePtr;
	}
	int quoteSpread = round((plastQuote->askprice[0] - plastQuote->bidprice[0]) / m_tickSize);
	if (quoteSpread > m_maxiOrderSpread) return;
	if (quoteSpread > m_spotOrderSpread)
		m_spotOrderSpread = quoteSpread;
	int spreadDiff = round(m_spotOrderSpread - quoteSpread);
	if (spreadDiff % 2 == 1)
	{
		if (plastQuote->LastPrice >= m_lastPrz_1)
			*bidprice = plastQuote->bidprice[0] - m_tickSize * (spreadDiff-1)/2;
		else
			*bidprice = plastQuote->bidprice[0] - m_tickSize * (spreadDiff+1) / 2;
	}
	else if (spreadDiff <= 2)
	{
		*bidprice = plastQuote->bidprice[0] - m_tickSize * spreadDiff / 2;
	}
	else
	{
		if (plastQuote->LastPrice >= m_lastPrz_1)
			*bidprice = plastQuote->bidprice[0] - m_tickSize * (spreadDiff - 2) / 2;
		else
			*bidprice = plastQuote->bidprice[0] - m_tickSize * (spreadDiff + 2) / 2;
	}
	*askprice = *bidprice + m_tickSize * m_spotOrderSpread;
	if (m_spotOrderSpread > m_miniOrderSpread)
		m_spotOrderSpread--;
	if (m_spotOrderSpread < m_miniOrderSpread)
		m_spotOrderSpread = m_miniOrderSpread;
};

void cmMM02::startCycle()
{
	{
		read_lock lock(m_breakReqLock);
		if (m_breakReq)
		{
			m_strategyStatus = cmMM02_STATUS_BREAK; //interrupt结束后由行情触发新的交易
			return;
		}
	}
	{
		read_lock lock(m_pauseReqLock);
		if (m_pauseReq)
		{
			m_strategyStatus = cmMM02_STATUS_PAUSE; //interrupt结束后由行情触发新的交易
			callPauseHandler();
			return;
		}
	}

	m_cycleId++;
	m_bidOrderRef = 0;
	m_askOrderRef = 0;
	m_cancelBidOrderRC = 0;
	m_cancelAskOrderRC = 0;
	m_orderCheckTimerCancelled = false;
	m_cancelHedgeTimerCancelled = false;
	m_ptradeGrp = tradeGroupBufferPtr(new tradeGroupBuffer());
	m_ptradeGrp->m_Id = m_cycleId;
	LOG(INFO) << m_strategyId << ": starting new cycle. cycleId: " << m_cycleId << endl;
	double bidprice = 0.0, askprice = 0.0;
	orderPrice(&bidprice, &askprice);
	if (0.0 == bidprice || 0.0 == askprice)
	{
		boost::recursive_mutex::scoped_lock lock(m_strategyStatusLock);
		m_strategyStatus = cmMM02_STATUS_READY;
		LOG(WARNING) << m_strategyId << ": warning | spread is too wide, no order sent." << endl;
		return;
	}
	m_bidOrderRef = m_infra->insertOrder(m_tradeAdapterID, m_productId, m_exchange, ORDER_TYPE_LIMIT,
		ORDER_DIR_BUY, POSITION_EFFECT_OPEN, FLAG_SPECULATION, bidprice, m_orderQty,
		bind(&cmMM02::onOrderRtn, this, _1), bind(&cmMM02::onTradeRtn, this, _1));
	if (m_bidOrderRef > 0)
	{
		write_lock lock0(m_orderRef2cycleRWlock);
		m_orderRef2cycle[m_bidOrderRef] = m_ptradeGrp;
		m_ptradeGrp->m_orderIdList.push_back(m_bidOrderRef);
	}

	m_askOrderRef = m_infra->insertOrder(m_tradeAdapterID, m_productId, m_exchange, ORDER_TYPE_LIMIT,
		ORDER_DIR_SELL, POSITION_EFFECT_OPEN, FLAG_SPECULATION, askprice, m_orderQty,
		bind(&cmMM02::onOrderRtn, this, _1), bind(&cmMM02::onTradeRtn, this, _1));
	if (m_askOrderRef > 0)
	{
		write_lock lock0(m_orderRef2cycleRWlock);
		m_orderRef2cycle[m_askOrderRef] = m_ptradeGrp;
		m_ptradeGrp->m_orderIdList.push_back(m_askOrderRef);
	}

	if (m_bidOrderRef == 0 || m_askOrderRef == 0)
		cout << "debug" << endl;
	m_strategyStatus = cmMM02_STATUS_ORDER_SENT;
};

void cmMM02::refreshCycle()
{
	CancelOrder(true, true);
};

void cmMM02::CancelOrder(bool confirm, bool restart)//const boost::system::error_code& error)
{
	if (m_orderCheckTimerCancelled)
	{
		LOG(INFO) << m_strategyId << ": cancel order stopped, cycleId: " << m_cycleId << endl;
		return;
	}

	if (m_bidOrderRef == 0 || m_askOrderRef == 0)
		LOG(WARNING) << m_strategyId<<": order ref illegal." << endl;

	//先检查报单回报，减少单边撤单，取代ORDER_CANCEL_ERROR_NOT_FOUND对应逻辑
	//map < int, orderRtnPtr>::iterator bidOrderIter;
	//map < int, orderRtnPtr>::iterator askOrderIter;
	{
		read_lock lock(m_orderRtnBuffLock);
		auto bidOrderIter = m_orderRef2orderRtn.find(m_bidOrderRef);
		auto askOrderIter = m_orderRef2orderRtn.find(m_askOrderRef);
		if (bidOrderIter == m_orderRef2orderRtn.end() ||
			askOrderIter == m_orderRef2orderRtn.end())
		{
			if (bidOrderIter == m_orderRef2orderRtn.end())
			{
				m_infra->queryOrder(m_tradeAdapterID, m_bidOrderRef);
				LOG(WARNING) << m_strategyId << ": order not found, querying order, orderRef: " << m_bidOrderRef << endl;
			}
			if (askOrderIter == m_orderRef2orderRtn.end())
			{
				m_infra->queryOrder(m_tradeAdapterID, m_askOrderRef);
				LOG(WARNING) << m_strategyId << ": order not found, querying order, orderRef: " << m_askOrderRef << endl;
			}
			m_orderCheckTimer.expires_from_now(boost::posix_time::milliseconds(1000));
			m_orderCheckTimer.async_wait(bind(&cmMM02::CancelOrder, this, confirm, restart));// , boost::asio::placeholders::error));
			return;
		}
	}

	if (m_cancelBidOrderRC == 0 || m_cancelBidOrderRC == ORDER_CANCEL_ERROR_SEND_FAIL)
	{
		m_cancelBidOrderRC = m_infra->cancelOrder(m_tradeAdapterID, m_bidOrderRef,
			bind(&cmMM02::onRspCancel, this, _1));
		if (m_cancelBidOrderRC == ORDER_CANCEL_ERROR_SEND_FAIL)
			LOG(WARNING) << m_strategyId << ": send cancel failed, orderRef: " << m_bidOrderRef << endl;
	}
	if (m_cancelAskOrderRC == 0 || m_cancelAskOrderRC == ORDER_CANCEL_ERROR_SEND_FAIL)
	{
		m_cancelAskOrderRC = m_infra->cancelOrder(m_tradeAdapterID, m_askOrderRef,
			bind(&cmMM02::onRspCancel, this, _1));
		if (m_cancelAskOrderRC == ORDER_CANCEL_ERROR_SEND_FAIL)
			LOG(WARNING) << m_strategyId << ": send cancel failed, orderRef: " << m_askOrderRef << endl;
	}

	if (m_cancelBidOrderRC == ORDER_CANCEL_ERROR_SEND_FAIL 
		|| m_cancelAskOrderRC == ORDER_CANCEL_ERROR_SEND_FAIL)
	{
		m_orderCheckTimer.expires_from_now(boost::posix_time::milliseconds(1000));
		m_orderCheckTimer.async_wait(bind(&cmMM02::CancelOrder, this, confirm,  restart)); // , boost::asio::placeholders::error));
		return;
	}
	if (confirm)
		confirmCancel(restart);
};

void cmMM02::confirmCancel(bool restart)
{
	if (m_orderCheckTimerCancelled)
	{
		LOG(INFO) << m_strategyId << ": confirm order stopped, cycleId: " << m_cycleId << endl;
		return;
	}
	{
		boost::recursive_mutex::scoped_lock lock(m_strategyStatusLock);
		if (cmMM02_STATUS_CLOSING_POSITION == m_strategyStatus)
		{//重新下单
			read_lock lock(m_orderRtnBuffLock);
			auto bidOrderIter = m_orderRef2orderRtn.find(m_bidOrderRef);
			auto askOrderIter = m_orderRef2orderRtn.find(m_askOrderRef);
			if (bidOrderIter == m_orderRef2orderRtn.end()
				|| askOrderIter == m_orderRef2orderRtn.end())
			{
				m_orderCheckTimer.expires_from_now(boost::posix_time::milliseconds(1000));
				m_orderCheckTimer.async_wait(bind(&cmMM02::CancelOrder, this, true, restart));
				return;
			}
			if (ORDER_STATUS_Canceled == bidOrderIter->second->m_orderStatus
				&& ORDER_STATUS_Canceled == askOrderIter->second->m_orderStatus)
			{
				m_tradeTP->getDispatcher().post(bind(&cmMM02::registerTrdGrpMap, this, m_cycleId, m_ptradeGrp));
				if (restart)
					startCycle();
			}
			switch (bidOrderIter->second->m_orderStatus)
			{
			case ORDER_STATUS_NoTradeQueueing:///未成交还在队列中,
			case ORDER_STATUS_NotTouched:///尚未触发,
			case ORDER_STATUS_Touched:///已触发,
			{
				m_infra->queryOrder(m_tradeAdapterID, m_bidOrderRef); 
				break;
			}
			}
			switch (askOrderIter->second->m_orderStatus)
			{
			case ORDER_STATUS_NoTradeQueueing:///未成交还在队列中,
			case ORDER_STATUS_NotTouched:///尚未触发,
			case ORDER_STATUS_Touched:///已触发,
			{
				m_infra->queryOrder(m_tradeAdapterID, m_askOrderRef);
				break;
			}
			}
			m_orderCheckTimer.expires_from_now(boost::posix_time::milliseconds(10));
			m_orderCheckTimer.async_wait(bind(&cmMM02::confirmCancel, this, restart));// , boost::asio::placeholders::error));
			return;

		}
	}
};
void cmMM02::registerOrder(orderRtnPtr pOrder)
{
	write_lock lock(m_orderRtnBuffLock);
	m_orderRef2orderRtn[pOrder->m_orderRef] = pOrder;
};

void cmMM02::processOrder(orderRtnPtr pOrder)
{ 
	registerOrder(pOrder);
	tradeGroupBufferPtr ptradeGrp = nullptr; //用于获取该order所在的闭环
	{
		read_lock lock1(m_orderRef2cycleRWlock);
		auto iter = m_orderRef2cycle.find(pOrder->m_orderRef);
		if (iter != m_orderRef2cycle.end())
			ptradeGrp = iter->second;
	}
	if (ptradeGrp)
	{
		//记录交易日
		if (ptradeGrp->m_tradingDate == "")
			ptradeGrp->m_tradingDate = pOrder->m_tradingDay;

		//记录有效挂单时间
		if (ptradeGrp->m_start_milliSec == 0.0)
		{
			switch (pOrder->m_orderStatus)
			{
			case ORDER_STATUS_PartTradedQueueing:///部分成交还在队列中,
			case ORDER_STATUS_NoTradeQueueing:///未成交还在队列中,
			case ORDER_STATUS_Touched:///已触发,
			{
				ptradeGrp->m_start_milliSec = UTC::GetMilliSecs();
				break; 
			}
			}
		}
		else if (ptradeGrp->m_end_milliSec == 0.0)
		{
			switch (pOrder->m_orderStatus)
			{
			case ORDER_STATUS_Canceled: ///撤单,
			case ORDER_STATUS_AllTraded:///全部成交,
			case ORDER_STATUS_PartTradedNotQueueing:///部分成交不在队列中,
			case ORDER_STATUS_NoTradeNotQueueing:///未成交不在队列中,
			{
				ptradeGrp->m_end_milliSec = UTC::GetMilliSecs();
				break;
			}
			}
		}
	}
};

void cmMM02::registerTradeRtn(tradeRtnPtr pTrade){

	if (pTrade)
	{
		enum_holding_dir_type targetHoldingDir;
		switch (pTrade->m_orderDir)
		{
		case ORDER_DIR_BUY:
		{
			targetHoldingDir = HOLDING_DIR_LONG;
			break;
		}
		case ORDER_DIR_SELL:
		{
			targetHoldingDir = HOLDING_DIR_SHORT;
			break;
		}
		}
		switch (pTrade->m_positionEffectTyp)
		{
		case POSITION_EFFECT_OPEN:
		{
			write_lock lock(m_investorPositionLock);
			m_investorPosition[m_productId][targetHoldingDir]->m_position += pTrade->m_volume;
			break;
		}
		case POSITION_EFFECT_CLOSE:
		{
			write_lock lock(m_investorPositionLock);
			m_investorPosition[m_productId][targetHoldingDir]->m_position -= pTrade->m_volume;
			break;
		}
		}
		if (!m_isHoldingRequireFilled)
		{
			read_lock lock(m_investorPositionLock);
			int validHoldingVol = m_investorPosition[m_productId][HOLDING_DIR_LONG]->m_position
							   <= m_investorPosition[m_productId][HOLDING_DIR_SHORT]->m_position ?
								m_investorPosition[m_productId][HOLDING_DIR_LONG]->m_position :
								m_investorPosition[m_productId][HOLDING_DIR_SHORT]->m_position;
			if (validHoldingVol >= m_holdingRequirement)
				m_isHoldingRequireFilled = true;
		}
		//write_lock lock(m_tradeRtnBuffLock);
		//m_orderRef2tradeRtn[pTrade->m_orderRef][pTrade->m_tradeId] = pTrade;
	}
};

void cmMM02::logTrade(tradeRtnPtr ptrade)
{
	LOG(INFO) << "," << m_strategyId << ",trade_rtn"
		<< ", orderRef:" << ptrade->m_orderRef
		<< ", tradeDate:" << ptrade->m_tradeDate
		<< ", InstrumentID:" << ptrade->m_instId
		<< ", Direction:" << ptrade->m_orderDir
		<< ", Price:" << ptrade->m_price
		<< ", volume:" << ptrade->m_volume << endl;
};

//处理报单的成交回报
//    1、将策略状态设置为 TRADED_HEDGING
//    2、如果尚未撤单，下撤单指令
//	  3、将spot_spread设为maxiSpread
//    4、下对冲单
//    5、等待1s钟，调用对冲指令处理函数 cancelHedgeOrder()
void cmMM02::processTrade(tradeRtnPtr ptrade)
{
	m_tradeTP->getDispatcher().post(boost::bind(&cmMM02::registerTradeRtn, this, ptrade));
	logTrade(ptrade);
	enum_cmMM02_strategy_status status;
	{
		boost::recursive_mutex::scoped_lock lock(m_strategyStatusLock);

		read_lock lock1(m_orderRef2cycleRWlock);
		if (m_cycleId != m_orderRef2cycle[ptrade->m_orderRef]->m_Id) //如果所在的cycle已经结束, 不做处理
		{
			LOG(WARNING) << m_strategyId <<": old cycle's trade rtn received!" << endl;
			return;
		}

		status = m_strategyStatus;
		m_strategyStatus = cmMM02_STATUS_TRADED_HEDGING;
		if (cmMM02_STATUS_ORDER_SENT == status)
		{//撤单
			m_cancelBidOrderRC = 0;
			m_cancelAskOrderRC = 0;
			m_tradeTP->getDispatcher().post(boost::bind(&cmMM02::CancelOrder, this, false, false)); // , boost::asio::placeholders::error));
		}
	}

	//同价对冲
	sendHedgeOrder(ptrade);

	m_spotOrderSpread = m_maxiOrderSpread;

	//等待1s
	if (cmMM02_STATUS_TRADED_HEDGING != status)
	{
		LOG(INFO) << m_strategyId << ": waiting 1s to cancel hedge order!" << endl;
		m_cancelHedgeTimer.expires_from_now(boost::posix_time::milliseconds(1000));
		m_cancelHedgeTimer.async_wait(boost::bind(&cmMM02::cancelHedgeOrder, this)); // , boost::asio::placeholders::error));
	}

};

//撤单响应函数
void cmMM02::processCancelRes(cancelRtnPtr pCancel)
{
}

void cmMM02::sendHedgeOrder(tradeRtnPtr ptrade)//同价对冲
{
	enum_order_dir_type dir = (ptrade->m_orderDir == ORDER_DIR_BUY) ? ORDER_DIR_SELL : ORDER_DIR_BUY;
	enum_position_effect_type positionEffect = m_isHoldingRequireFilled ? POSITION_EFFECT_CLOSE : POSITION_EFFECT_OPEN;

	int m_hedgeOrderRef = m_infra->insertOrder(m_tradeAdapterID, m_productId, m_exchange, ORDER_TYPE_LIMIT,
		dir, positionEffect, FLAG_SPECULATION, ptrade->m_price, ptrade->m_volume,
		bind(&cmMM02::onHedgeOrderRtn, this, _1), bind(&cmMM02::onHedgeTradeRtn, this, _1));

	if (m_hedgeOrderRef > 0)
	{
		write_lock lock0(m_orderRef2cycleRWlock);
		m_orderRef2cycle[m_bidOrderRef] = m_ptradeGrp;
		m_ptradeGrp->m_orderIdList.push_back(m_hedgeOrderRef);

		//记录对冲量和撤销（完成）状态
		write_lock lock1(m_hedgeOrderVolLock);
		m_hedgeOrderVol[m_hedgeOrderRef] = ((dir == ORDER_DIR_BUY) ?
			ptrade->m_volume : (ptrade->m_volume * -1));
		m_hedgeOrderCancelRC[m_hedgeOrderRef] = 0;
	}
};

//对冲成交处理函数
void cmMM02::processHedgeTradeRtn(tradeRtnPtr ptrade)
{
	m_tradeTP->getDispatcher().post(boost::bind(&cmMM02::registerTradeRtn, this, ptrade));
	logTrade(ptrade);

	write_lock lock(m_hedgeOrderVolLock);
	m_hedgeOrderVol[ptrade->m_orderRef] -= ((ptrade->m_orderDir == ORDER_DIR_BUY)
		? ptrade->m_volume : (ptrade->m_volume*-1));
	if (0.0 == m_hedgeOrderVol[ptrade->m_orderRef])
		m_hedgeOrderVol.erase(ptrade->m_orderRef); //删除已经完成的对冲指令

	//同价对冲单全部成交
	if (m_hedgeOrderVol.size() == 0)
	{
		m_hedgeOrderVol.clear();
		m_cancelHedgeTimerCancelled = true;
		m_cancelHedgeTimer.cancel();
		m_orderCheckTimerCancelled = true;
		m_orderCheckTimer.cancel();

		m_tradeTP->getDispatcher().post(bind(&cmMM02::registerTrdGrpMap, this, m_cycleId, m_ptradeGrp));
		startCycle();
	}

}

void cmMM02::processHedgeOrderRtn(orderRtnPtr pOrder)
{
	registerOrder(pOrder);
}

//处理对冲指令：如果对冲指令未成交，撤单，并异步调用轧差市价对冲函数 confirmCancel_hedgeOrder()
void cmMM02::cancelHedgeOrder()//const boost::system::error_code& error){
{
	if (m_cancelHedgeTimerCancelled)
	{
		LOG(INFO) << m_strategyId << ": hedge timer cancelled." << endl;
		return;
	}

	read_lock lock(m_hedgeOrderVolLock);
	LOG(INFO) << m_strategyId << ": cancelling hedge order..." << endl;
	if (m_hedgeOrderVol.size() > 0)   //存在未完成的对冲指令
	{
		bool isHedgeOrderConfirmed = true;
		for (auto iter = m_hedgeOrderVol.begin(); iter != m_hedgeOrderVol.end();)
		{
			//撤销对冲单
			if (m_hedgeOrderCancelRC[iter->first] == 0)
			{
				int cancelOrderRC = m_infra->cancelOrder(m_tradeAdapterID, iter->first,
					bind(&cmMM02::onRspCancel, this, _1));
				m_hedgeOrderCancelRC[iter->first] = cancelOrderRC;
				if (cancelOrderRC < 0)
					isHedgeOrderConfirmed = false;
			}
			else if(m_hedgeOrderCancelRC[iter->first] == ORDER_CANCEL_ERROR_NOT_FOUND)
			{
				m_infra->queryOrder(m_tradeAdapterID, iter->first);
				m_hedgeOrderCancelRC[iter->first] = 0;
				m_cancelHedgeTimer.expires_from_now(boost::posix_time::milliseconds(1000));
				m_cancelHedgeTimer.async_wait(boost::bind(&cmMM02::cancelHedgeOrder, this));// ,boost::asio::placeholders::error));
				return;
			}
			else if (m_hedgeOrderCancelRC[iter->first] == ORDER_CANCEL_ERROR_SEND_FAIL)
			{
				m_hedgeOrderCancelRC[iter->first] = 0;
				m_cancelHedgeTimer.expires_from_now(boost::posix_time::milliseconds(1000));
				m_cancelHedgeTimer.async_wait(boost::bind(&cmMM02::cancelHedgeOrder, this)); // , boost::asio::placeholders::error));
				return;
			}
			iter++;
		}
		if (!isHedgeOrderConfirmed)
		{
			m_tradeTP->getDispatcher().post(boost::bind(&cmMM02::cancelHedgeOrder, this)); // ,boost::asio::placeholders::error));
		}
		else
			confirmCancel_hedgeOrder();
	}
	else
		LOG(INFO) << m_strategyId << ": hedge_Order_Vol dic is empty!" << endl;

};


//轧差市价对冲
void cmMM02::confirmCancel_hedgeOrder()
{
	//boost::mutex::scoped_lock lock(m_hedgeOrderVolLock); //在 cancelHedgeOrder() 中互斥
	if (m_hedgeOrderVol.size() > 0)
	{
		{
			boost::recursive_mutex::scoped_lock lock(m_strategyStatusLock);
			m_strategyStatus = cmMM02_STATUS_TRADED_NET_HEDGING;
		}
		double netHedgeVol = 0.0;
		for (auto iter = m_hedgeOrderVol.begin(); iter != m_hedgeOrderVol.end(); iter++)
			netHedgeVol += iter->second;
		if (0.0 != netHedgeVol)
		{
			//轧差市价对冲
			sendNetHedgeOrder(netHedgeVol);
		}
		else
		{
			m_orderCheckTimerCancelled = true;
			m_orderCheckTimer.cancel();

			m_tradeTP->getDispatcher().post(bind(&cmMM02::registerTrdGrpMap, this, m_cycleId, m_ptradeGrp));
			startCycle();
		}
	}
};

void cmMM02::sendNetHedgeOrder(double netHedgeVol)
{
	//轧差市价对冲
	enum_order_dir_type dir = netHedgeVol > 0.0 ? ORDER_DIR_BUY : ORDER_DIR_SELL;
	enum_position_effect_type positionEffect = m_isHoldingRequireFilled ? POSITION_EFFECT_CLOSE : POSITION_EFFECT_OPEN;

	double price = (dir == ORDER_DIR_BUY) ?
		//m_lastQuotePtr->UpperLimitPrice : m_lastQuotePtr->LowerLimitPrice;
		(m_lastQuotePtr->askprice[0] + m_tickSize * 2.0) : (m_lastQuotePtr->bidprice[0] - m_tickSize * 2.0);
	int netHedgeOrderRef = m_infra->insertOrder(m_tradeAdapterID, m_productId, m_exchange,
		ORDER_TYPE_LIMIT, dir, positionEffect, FLAG_SPECULATION, price, fabs(netHedgeVol),
		bind(&cmMM02::onNetHedgeOrderRtn, this, _1), bind(&cmMM02::onNetHedgeTradeRtn, this, _1));
	if (netHedgeOrderRef > 0)
	{
		write_lock lock0(m_orderRef2cycleRWlock);
		m_orderRef2cycle[netHedgeOrderRef] = m_ptradeGrp;
		m_ptradeGrp->m_orderIdList.push_back(netHedgeOrderRef);

		//记录轧差对冲量
		boost::mutex::scoped_lock lock(m_NetHedgeOrderVolLock);
		m_NetHedgeOrderVol = netHedgeVol;
		m_hedgeOrderVol.clear();
	}
	else
		LOG(INFO) << m_strategyId << " ERROR: send net hedge order failed, rc = " << netHedgeOrderRef << endl;
};

//轧差市价成交处理函数
void cmMM02::processNetHedgeTradeRtn(tradeRtnPtr ptrade)
{
	m_tradeTP->getDispatcher().post(boost::bind(&cmMM02::registerTradeRtn, this, ptrade));
	logTrade(ptrade);

	boost::mutex::scoped_lock lock(m_NetHedgeOrderVolLock);
	m_NetHedgeOrderVol -= ((ptrade->m_orderDir == ORDER_DIR_BUY)
		? ptrade->m_volume : (ptrade->m_volume*-1));

	LOG(INFO) << m_strategyId << ": net hedge order left: " << m_NetHedgeOrderVol << endl;
	if (0.0 == m_NetHedgeOrderVol) //轧差对冲全部成交
	{
		m_orderCheckTimerCancelled = true;
		m_orderCheckTimer.cancel();

		m_tradeTP->getDispatcher().post(bind(&cmMM02::registerTrdGrpMap, this, m_cycleId, m_ptradeGrp));
		startCycle();
	}
}

void cmMM02::processNetHedgeOrderRtn(orderRtnPtr pOrder)
{
	registerOrder(pOrder);
}
