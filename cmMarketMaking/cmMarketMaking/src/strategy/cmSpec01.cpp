#include <math.h>
#include <numeric>
#include "strategy/cmSpec01.h"
#include "glog\logging.h"

cmSepc01::cmSepc01(string strategyId, string strategyTyp, string productId, string exchange,
	string quoteAdapterID, string tradeAdapterID, double tickSize,
	double orderQty, int volMulti,
	athenathreadpoolPtr quoteTP, athenathreadpoolPtr tradeTP, infrastructure* infra,
	Json::Value config)
	:m_strategyId(strategyId), m_strategyTyp(strategyTyp), m_productId(productId), m_exchange(exchange),
	m_quoteAdapterID(quoteAdapterID), m_tradeAdapterID(tradeAdapterID), m_tickSize(tickSize),
	m_orderQty(orderQty), m_volumeMultiple(volMulti),
	m_quoteTP(quoteTP), m_tradeTP(tradeTP), m_infra(infra), m_strategyConfig(config),
	m_daemonTimer(tradeTP->getDispatcher())
{
	m_lastprice   = -1.0;
	m_lastprice_1 = -1.0;
	m_upline = 50.0;
	m_downline = -50.0;
	int openNum = m_strategyConfig["openTime"].size();
	for (int i = 0; i < openNum; ++i)
	{
		Json::Value openInterval = m_strategyConfig["openTime"][i];
		int startTime = openInterval["start"].asInt() * 100 +1;
		int endTime = openInterval["end"].asInt() * 100 + 59;
		m_openTimeList.push_back(make_pair(startTime, endTime));
	}
	m_strategyStatus = STRATEGY_STATUS_INIT;
	daemonEngine();
};

void cmSepc01::daemonEngine(){

	if (!isInOpenTime())
		if (STRATEGY_STATUS_STOP != m_strategyStatus)
			m_strategyStatus = STRATEGY_STATUS_STOP;
	else if (STRATEGY_STATUS_START != m_strategyStatus)
		startStrategy();

	m_daemonTimer.expires_from_now(boost::posix_time::millisec(1000 * 60)); //每分钟运行一次
	m_daemonTimer.async_wait(boost::bind(&cmSepc01::daemonEngine, this));
};

void cmSepc01::startStrategy(){
	cout << m_strategyId << " starting..." << endl;
	if (STRATEGY_STATUS_INIT == m_strategyStatus)
	{
		m_infra->subscribeFutures(m_quoteAdapterID, m_exchange, m_productId, bind(&cmSepc01::onRtnMD, this, _1));
	}
	m_strategyStatus = STRATEGY_STATUS_START;
};

void cmSepc01::onRtnMD(futuresMDPtr pFuturesMD)//行情响应函数: 更新行情，调用quoteEngine产生信号
{
	m_lastprice_1 = m_lastprice;
	m_lastprice = pFuturesMD->LastPrice;
	m_quoteTP->getDispatcher().post(bind(&cmSepc01::quoteEngine, this));
};

void cmSepc01::quoteEngine()
{
	if (m_lastprice_1 < 0.0)
		return;

	double yield = log(m_lastprice / m_lastprice_1);
	m_yieldBuff_short.push_back(yield);
	m_yieldBuff_long.push_back(yield);
	double priceChg = abs(m_lastprice - m_lastprice_1);
	m_avg_true_range.push_back(priceChg);

	if (!m_yieldBuff_long.full())
		return;

	double ma1 = accumulate(m_yieldBuff_short.begin(), m_yieldBuff_short.end(), 0.0)
		            / m_yieldBuff_short.capacity();
	double ma2 = accumulate(m_yieldBuff_long.begin(), m_yieldBuff_long.end(), 0.0)
		            / m_yieldBuff_long.capacity();
	double avgTrueRange = accumulate(m_avg_true_range.begin(), m_avg_true_range.end(), 0.0) 
		                     / m_avg_true_range.capacity();
	double apcosm;
	if (avgTrueRange != 0.0)
		apcosm = (ma1 - ma2) / avgTrueRange;
	else
		apcosm = 50;

	m_Apcosm_Buff.push_back(apcosm);
	if (!m_Apcosm_Buff.full())
		return;

	vector<double> apcosm_Buff;
	auto iter = m_Apcosm_Buff.begin();
	while (iter != m_Apcosm_Buff.end())
	{
		apcosm_Buff.push_back(*iter);
		iter++;
	}
	sort(apcosm_Buff.begin(), apcosm_Buff.end());
	double qu1 = apcosm_Buff[round(apcosm_Buff_size*0.25)];
	double qu2 = apcosm_Buff[round(apcosm_Buff_size*0.50)];
	double qu3 = apcosm_Buff[round(apcosm_Buff_size*0.75)];

	double new_abs;
	if ((qu3 - qu1) != 0)
		new_abs = (apcosm - qu2) / (qu3 - qu1) * 100;
	else 
		new_abs = 50;

};

void cmSepc01::orderPrice(double* bidprice, double* askprice)
{
	futuresMDPtr plastQuote;
	{
		boost::mutex::scoped_lock lock(m_lastQuoteLock);
		plastQuote = m_lastQuotePtr;
	}
	int quoteSpread = round((plastQuote->askprice[0] - plastQuote->bidprice[0]) / m_tickSize);
	if (quoteSpread > m_miniOrderSpread) return;
	switch (quoteSpread)
	{
	case 1:
	case 2:
	{
		*bidprice = plastQuote->bidprice[0] - m_tickSize;
		break;
	}
	case 3:
	case 4:
	{
		*bidprice = plastQuote->bidprice[0];
		break;
	}
	}
	//*bidprice = plastQuote->bidprice[0] + int((quoteSpread - m_miniOrderSpread) / 2) * m_tickSize;
	//*bidprice = plastQuote->askprice[0]; //测试成交
	*askprice = *bidprice + m_tickSize * m_miniOrderSpread;
};

void cmSepc01::startCycle()
{
	{
		read_lock lock(m_breakReqLock);
		if (m_breakReq)
		{
			m_strategyStatus = STRATEGY_STATUS_BREAK; //interrupt结束后由行情触发新的交易
			return;
		}
	}
	{
		read_lock lock(m_pauseReqLock);
		if (m_pauseReq)
		{
			m_strategyStatus = STRATEGY_STATUS_PAUSE; //interrupt结束后由行情触发新的交易
			callPauseHandler();
			return;
		}
	}

	m_cycleId++;
	m_bidOrderRef = 0;
	m_askOrderRef = 0;
	m_cancelBidOrderRC = 0;
	m_cancelAskOrderRC = 0;
	m_cancelConfirmTimerCancelled = false;
	m_cancelHedgeTimerCancelled = false;
	m_ptradeGrp = tradeGroupBufferPtr(new tradeGroupBuffer());
	m_ptradeGrp->m_Id = m_cycleId;
	LOG(INFO) << m_strategyId << ": starting new cycle." << endl;
	double bidprice = 0.0, askprice = 0.0;
	orderPrice(&bidprice, &askprice);
	if (0.0 == bidprice || 0.0 == askprice)
	{
		LOG(INFO) << m_strategyId << ": warning | spread is too wide, no order sent." << endl;
		return;
	}
	m_bidOrderRef = m_infra->insertOrder(m_tradeAdapterID, m_productId, m_exchange, ORDER_TYPE_LIMIT,
		ORDER_DIR_BUY, POSITION_EFFECT_OPEN, FLAG_SPECULATION, bidprice, m_orderQty,
		bind(&cmSepc01::onOrderRtn, this, _1), bind(&cmSepc01::onTradeRtn, this, _1));
	if (m_bidOrderRef > 0)
	{
		write_lock lock0(m_orderRef2cycleRWlock);
		m_orderRef2cycle[m_bidOrderRef] = m_cycleId;
		m_ptradeGrp->m_orderIdList.push_back(m_bidOrderRef);
	}

	m_askOrderRef = m_infra->insertOrder(m_tradeAdapterID, m_productId, m_exchange, ORDER_TYPE_LIMIT,
		ORDER_DIR_SELL, POSITION_EFFECT_OPEN, FLAG_SPECULATION, askprice, m_orderQty,
		bind(&cmSepc01::onOrderRtn, this, _1), bind(&cmSepc01::onTradeRtn, this, _1));
	if (m_askOrderRef > 0)
	{
		write_lock lock0(m_orderRef2cycleRWlock);
		m_orderRef2cycle[m_askOrderRef] = m_cycleId;
		m_ptradeGrp->m_orderIdList.push_back(m_askOrderRef);
	}

	if (m_bidOrderRef == 0 || m_askOrderRef == 0)
		cout << "debug" << endl;
	m_strategyStatus = STRATEGY_STATUS_ORDER_SENT;
};

void cmSepc01::refreshCycle()
{
	CancelOrder(true);
};

void cmSepc01::CancelOrder(bool restart)//const boost::system::error_code& error)
{
	if (m_cancelConfirmTimerCancelled)
	{
		LOG(INFO) << m_strategyId << ": cancel order stopped, cycleId: " << m_cycleId << endl;
		return;
	}

	if (m_bidOrderRef == 0 || m_askOrderRef == 0)
		cout << "debug" << endl;

	if (m_cancelBidOrderRC == 0 || m_cancelBidOrderRC == ORDER_CANCEL_ERROR_NOT_FOUND ||
		m_cancelBidOrderRC == ORDER_CANCEL_ERROR_SEND_FAIL)
		m_cancelBidOrderRC = m_infra->cancelOrder(m_tradeAdapterID, m_bidOrderRef,
		bind(&cmSepc01::onRspCancel, this, _1));
	if (m_cancelAskOrderRC == 0 || m_cancelAskOrderRC == ORDER_CANCEL_ERROR_NOT_FOUND ||
		m_cancelAskOrderRC == ORDER_CANCEL_ERROR_SEND_FAIL)
		m_cancelAskOrderRC = m_infra->cancelOrder(m_tradeAdapterID, m_askOrderRef,
		bind(&cmSepc01::onRspCancel, this, _1));

	if (m_cancelBidOrderRC == ORDER_CANCEL_ERROR_NOT_FOUND || m_cancelAskOrderRC == ORDER_CANCEL_ERROR_NOT_FOUND)
	{
		if (m_cancelBidOrderRC == ORDER_CANCEL_ERROR_NOT_FOUND)
			LOG(WARNING) << m_strategyId << ": order not found in adapter, querying order, orderRef: " << m_bidOrderRef << endl;
	    if (m_cancelAskOrderRC == ORDER_CANCEL_ERROR_NOT_FOUND)
			LOG(WARNING) << m_strategyId << ": order not found in adapter, querying order, orderRef: " << m_askOrderRef << endl;
		m_infra->queryOrder(m_tradeAdapterID);
		m_cancelConfirmTimer.expires_from_now(boost::posix_time::milliseconds(1000*10));
		m_cancelConfirmTimer.async_wait(bind(&cmSepc01::CancelOrder, this, restart));// , boost::asio::placeholders::error));
	}
	else if (m_cancelBidOrderRC == ORDER_CANCEL_ERROR_SEND_FAIL || m_cancelAskOrderRC == ORDER_CANCEL_ERROR_SEND_FAIL)
	{
		if (m_cancelBidOrderRC == ORDER_CANCEL_ERROR_SEND_FAIL)
			LOG(WARNING) << m_strategyId << ": send cancel failed, orderRef: " << m_bidOrderRef << endl;
		if (m_cancelAskOrderRC == ORDER_CANCEL_ERROR_SEND_FAIL)
			LOG(WARNING) << m_strategyId << ": send cancel failed, orderRef: " << m_askOrderRef << endl;
		m_cancelConfirmTimer.expires_from_now(boost::posix_time::milliseconds(1000*5));
		m_cancelConfirmTimer.async_wait(bind(&cmSepc01::CancelOrder, this, restart)); // , boost::asio::placeholders::error));
	}
	else if (restart)
	{
		boost::recursive_mutex::scoped_lock lock(m_strategyStatusLock);
		if (STRATEGY_STATUS_CLOSING_POSITION == m_strategyStatus)
		{//重新下单
			m_tradeTP->getDispatcher().post(bind(&cmSepc01::registerTrdGrpMap, this, m_cycleId, m_ptradeGrp));
			startCycle();
		}
	}
};

void cmSepc01::processOrder(orderRtnPtr pOrder)
{
	write_lock lock(m_orderRtnBuffLock);
	m_orderRef2orderRtn[pOrder->m_orderRef] = pOrder;
};

//处理报单的成交回报
//    1、将策略状态设置为 TRADED_HEDGING
//    2、如果尚未撤单，下撤单指令
//    3、下对冲单
//    4、等待1s钟，调用对冲指令处理函数 cancelHedgeOrder()
void cmSepc01::processTrade(tradeRtnPtr ptrade)
{
	m_tradeTP->getDispatcher().post(boost::bind(&cmSepc01::registerTradeRtn, this, ptrade));

	enum_cmSepc01_strategy_status status;
	{
		boost::recursive_mutex::scoped_lock lock(m_strategyStatusLock);

		read_lock lock1(m_orderRef2cycleRWlock);
		if (m_cycleId != m_orderRef2cycle[ptrade->m_orderRef]) //如果所在的cycle已经结束, 不做处理
			return;

		status = m_strategyStatus;
		m_strategyStatus = STRATEGY_STATUS_TRADED_HEDGING;
		if (STRATEGY_STATUS_ORDER_SENT == status)
		{//撤单
			m_cancelBidOrderRC = 0;
			m_cancelAskOrderRC = 0;
			m_tradeTP->getDispatcher().post(boost::bind(&cmSepc01::CancelOrder, this, false)); // , boost::asio::placeholders::error));
		}
	}

	//同价对冲
	sendHedgeOrder(ptrade);

	//等待1s
	if (STRATEGY_STATUS_TRADED_HEDGING != status)
	{
		LOG(INFO) << m_strategyId << ": waiting 1s to cancel hedge order!" << endl;
		m_cancelHedgeTimer.expires_from_now(boost::posix_time::milliseconds(1000));
		m_cancelHedgeTimer.async_wait(boost::bind(&cmSepc01::cancelHedgeOrder, this)); // , boost::asio::placeholders::error));
	}

};

//撤单响应函数
void cmSepc01::processCancelRes(cancelRtnPtr pCancel)
{
}

void cmSepc01::sendHedgeOrder(tradeRtnPtr ptrade)//同价对冲
{
	enum_order_dir_type dir = (ptrade->m_orderDir == ORDER_DIR_BUY) ? ORDER_DIR_SELL : ORDER_DIR_BUY;

	int m_hedgeOrderRef = m_infra->insertOrder(m_tradeAdapterID, m_productId, m_exchange, ORDER_TYPE_LIMIT,
		dir, POSITION_EFFECT_OPEN, FLAG_SPECULATION, ptrade->m_price, ptrade->m_volume,
		bind(&cmSepc01::onHedgeOrderRtn, this, _1), bind(&cmSepc01::onHedgeTradeRtn, this, _1));

	if (m_hedgeOrderRef > 0)
	{
		write_lock lock0(m_orderRef2cycleRWlock);
		m_orderRef2cycle[m_bidOrderRef] = m_cycleId;
		m_ptradeGrp->m_orderIdList.push_back(m_hedgeOrderRef);

		//记录对冲量和撤销（完成）状态
		write_lock lock1(m_hedgeOrderVolLock);
		m_hedgeOrderVol[m_hedgeOrderRef] = ((dir == ORDER_DIR_BUY) ?
			ptrade->m_volume : (ptrade->m_volume * -1));
		m_hedgeOrderCancelRC[m_hedgeOrderRef] = 0;
	}
};

//对冲成交处理函数
void cmSepc01::processHedgeTradeRtn(tradeRtnPtr ptrade)
{
	m_tradeTP->getDispatcher().post(boost::bind(&cmSepc01::registerTradeRtn, this, ptrade));

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
		m_cancelConfirmTimerCancelled = true;
		m_cancelConfirmTimer.cancel();

		m_tradeTP->getDispatcher().post(bind(&cmSepc01::registerTrdGrpMap, this, m_cycleId, m_ptradeGrp));
		startCycle();
	}

}

void cmSepc01::processHedgeOrderRtn(orderRtnPtr pOrder)
{
	write_lock lock(m_orderRtnBuffLock);
	m_orderRef2orderRtn[pOrder->m_orderRef] = pOrder;
}

//处理对冲指令：如果对冲指令未成交，撤单，并异步调用轧差市价对冲函数 confirmCancel_hedgeOrder()
void cmSepc01::cancelHedgeOrder()//const boost::system::error_code& error){
{
	if (m_cancelHedgeTimerCancelled)
	{
		LOG(INFO) << m_strategyId << ": hedge timer cancelled" << endl;
		return;
	}

	read_lock lock(m_hedgeOrderVolLock);
	if (m_hedgeOrderVol.size() > 0)   //存在未完成的对冲指令
	{
		bool isHedgeOrderConfirmed = true;
		for (auto iter = m_hedgeOrderVol.begin(); iter != m_hedgeOrderVol.end();)
		{
			//撤销对冲单
			if (m_hedgeOrderCancelRC[iter->first] == 0)
			{
				int cancelOrderRC = m_infra->cancelOrder(m_tradeAdapterID, iter->first,
					bind(&cmSepc01::onRspCancel, this, _1));
				m_hedgeOrderCancelRC[iter->first] = cancelOrderRC;
				if (cancelOrderRC < 0)
					isHedgeOrderConfirmed = false;
			}
			else if(m_hedgeOrderCancelRC[iter->first] == ORDER_CANCEL_ERROR_NOT_FOUND)
			{
				m_infra->queryOrder(m_tradeAdapterID);
				m_cancelHedgeTimer.expires_from_now(boost::posix_time::milliseconds(1000*10));
				m_cancelHedgeTimer.async_wait(boost::bind(&cmSepc01::cancelHedgeOrder, this));// ,boost::asio::placeholders::error));
				return;
			}
			else if (m_hedgeOrderCancelRC[iter->first] == ORDER_CANCEL_ERROR_SEND_FAIL)
			{
				m_hedgeOrderCancelRC[iter->first] = 0;
				m_cancelHedgeTimer.expires_from_now(boost::posix_time::milliseconds(1000*5));
				m_cancelHedgeTimer.async_wait(boost::bind(&cmSepc01::cancelHedgeOrder, this)); // , boost::asio::placeholders::error));
				return;
			}
			iter++;
		}
		if (!isHedgeOrderConfirmed)
		{
			m_tradeTP->getDispatcher().post(boost::bind(&cmSepc01::cancelHedgeOrder, this)); // ,boost::asio::placeholders::error));
		}
		else
			confirmCancel_hedgeOrder();
	}
};


//轧差市价对冲
void cmSepc01::confirmCancel_hedgeOrder()
{
	//boost::mutex::scoped_lock lock(m_hedgeOrderVolLock); //在 cancelHedgeOrder() 中互斥
	if (m_hedgeOrderVol.size() > 0)
	{
		{
			boost::recursive_mutex::scoped_lock lock(m_strategyStatusLock);
			m_strategyStatus = STRATEGY_STATUS_TRADED_NET_HEDGING;
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
			m_cancelConfirmTimerCancelled = true;
			m_cancelConfirmTimer.cancel();

			m_tradeTP->getDispatcher().post(bind(&cmSepc01::registerTrdGrpMap, this, m_cycleId, m_ptradeGrp));
			startCycle();
		}
	}
};

void cmSepc01::sendNetHedgeOrder(double netHedgeVol)
{
	//轧差市价对冲
	enum_order_dir_type dir = netHedgeVol > 0.0 ? ORDER_DIR_BUY : ORDER_DIR_SELL;
	double price = (dir == ORDER_DIR_BUY) ?
		m_lastQuotePtr->UpperLimitPrice : m_lastQuotePtr->LowerLimitPrice;
	int netHedgeOrderRef = m_infra->insertOrder(m_tradeAdapterID, m_productId, m_exchange,
		ORDER_TYPE_LIMIT, dir, POSITION_EFFECT_OPEN, FLAG_SPECULATION, price, fabs(netHedgeVol),
		bind(&cmSepc01::onNetHedgeOrderRtn, this, _1), bind(&cmSepc01::onNetHedgeTradeRtn, this, _1));
	if (netHedgeOrderRef > 0)
	{
		write_lock lock0(m_orderRef2cycleRWlock);
		m_orderRef2cycle[netHedgeOrderRef] = m_cycleId;
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
void cmSepc01::processNetHedgeTradeRtn(tradeRtnPtr ptrade)
{
	m_tradeTP->getDispatcher().post(boost::bind(&cmSepc01::registerTradeRtn, this, ptrade));

	boost::mutex::scoped_lock lock(m_NetHedgeOrderVolLock);
	m_NetHedgeOrderVol -= ((ptrade->m_orderDir == ORDER_DIR_BUY)
		? ptrade->m_volume : (ptrade->m_volume*-1));

	LOG(INFO) << m_strategyId << ": net hedge order left: " << m_NetHedgeOrderVol << endl;
	if (0.0 == m_NetHedgeOrderVol) //轧差对冲全部成交
	{
		m_cancelConfirmTimerCancelled = true;
		m_cancelConfirmTimer.cancel();

		m_tradeTP->getDispatcher().post(bind(&cmSepc01::registerTrdGrpMap, this, m_cycleId, m_ptradeGrp));
		startCycle();
	}
}

void cmSepc01::processNetHedgeOrderRtn(orderRtnPtr pOrder)
{
	write_lock lock(m_orderRtnBuffLock);
	m_orderRef2orderRtn[pOrder->m_orderRef] = pOrder;
}

bool cmSepc01::isInOpenTime()
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
