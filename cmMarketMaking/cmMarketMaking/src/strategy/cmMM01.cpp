#include "strategy/cmMM01.h"

cmMM01::cmMM01(string strategyId, string strategyTyp, string productId, string exchange,
	string quoteAdapterID, string tradeAdapterID, double tickSize, double miniOrderSpread,
	double orderQty,
	athenathreadpoolPtr quoteTP, athenathreadpoolPtr tradeTP, infrastructure* infra)
	:m_strategyId(strategyId), m_strategyTyp(strategyTyp), m_productId(productId), m_exchange(exchange),
	m_quoteAdapterID(quoteAdapterID), m_tradeAdapterID(tradeAdapterID), m_tickSize(tickSize),
	m_miniOrderSpread(miniOrderSpread), m_orderQty(orderQty), m_quoteTP(quoteTP), m_tradeTP(tradeTP), m_infra(infra),
	m_cancelConfirmTimer(tradeTP->getDispatcher()), m_cancelHedgeTimer(tradeTP->getDispatcher())
{
	m_strategyStatus = STRATEGY_STATUS_START;
};

void cmMM01::resetStrategyStatus(){
	boost::mutex::scoped_lock lock(m_strategyStatusLock);
	m_strategyStatus = STRATEGY_STATUS_READY;
};

void cmMM01::startStrategy(){
	cout << m_strategyId << " starting..." << endl;
	{
		boost::mutex::scoped_lock lock(m_strategyStatusLock);
		if (STRATEGY_STATUS_START == m_strategyStatus)
			m_infra->subscribeFutures(m_quoteAdapterID, m_exchange, m_productId, bind(&cmMM01::onRtnMD, this, _1));
	}
	resetStrategyStatus();
};

void cmMM01::stopStrategy(){
	boost::mutex::scoped_lock lock(m_strategyStatusLock);
	m_strategyStatus = STRATEGY_STATUS_STOP;
};

void cmMM01::orderPrice(double* bidprice, double* askprice)
{
	double quoteSpread = (m_lastQuotePtr->askprice[0] - m_lastQuotePtr->bidprice[0]) / m_tickSize;
	if (quoteSpread > m_miniOrderSpread) return;
	//*bidprice = m_lastQuotePtr->bidprice[0] + int((quoteSpread - m_miniOrderSpread) / 2) * m_tickSize;
	*bidprice = m_lastQuotePtr->askprice[0]; //测试成交
	*askprice = *bidprice + m_tickSize * m_miniOrderSpread;
};

void cmMM01::onRtnMD(futuresMDPtr pFuturesMD)
{
	{
		boost::mutex::scoped_lock lock(m_lastQuoteLock);
		m_lastQuotePtr = pFuturesMD;
	}
	m_quoteTP->getDispatcher().post(bind(&cmMM01::quoteEngine, this));
};

void cmMM01::sendOrder()
{
	double bidprice, askprice;
	orderPrice(&bidprice, &askprice);

	m_bidOrderRef = m_infra->insertOrder(m_tradeAdapterID, m_productId, m_exchange, ORDER_TYPE_LIMIT,
		ORDER_DIR_BUY, POSITION_EFFECT_OPEN, FLAG_SPECULATION, bidprice, m_orderQty,
		bind(&cmMM01::onOrderRtn, this, _1), bind(&cmMM01::onTradeRtn, this, _1));

	m_askOrderRef = m_infra->insertOrder(m_tradeAdapterID, m_productId, m_exchange, ORDER_TYPE_LIMIT,
		ORDER_DIR_SELL, POSITION_EFFECT_OPEN, FLAG_SPECULATION, askprice, m_orderQty,
		bind(&cmMM01::onOrderRtn, this, _1), bind(&cmMM01::onTradeRtn, this, _1));

	{
		boost::mutex::scoped_lock lock(m_strategyStatusLock);
		m_strategyStatus = STRATEGY_STATUS_ORDER_SENT;
	}
};
void cmMM01::quoteEngine()
{
	enum_cmMM01_strategy_status status;
	{
		boost::mutex::scoped_lock lock(m_strategyStatusLock);
		status = m_strategyStatus;
	}
	switch (status)
	{
	case STRATEGY_STATUS_READY:
	{
		sendOrder();
		break;
	}
	case STRATEGY_STATUS_ORDER_SENT:
	{
		{
			boost::mutex::scoped_lock lock(m_isOrderCanceledLock);
			m_cancelBidOrderRef = m_infra->cancelOrder(m_tradeAdapterID, m_bidOrderRef,
				bind(&cmMM01::onRspCancel, this, _1));
			if (m_cancelBidOrderRef > 0)
				m_isOrderCanceled[m_cancelBidOrderRef] = true;
			m_cancelAskOrderRef = m_infra->cancelOrder(m_tradeAdapterID, m_askOrderRef,
				bind(&cmMM01::onRspCancel, this, _1));
			if (m_cancelAskOrderRef > 0)
				m_isOrderCanceled[m_cancelAskOrderRef] = true;
		}
		{
			boost::mutex::scoped_lock lock(m_strategyStatusLock);
			m_strategyStatus = STRATEGY_STATUS_CLOSING_POSITION;
		}
		m_cancelConfirmTimer.expires_from_now(boost::posix_time::milliseconds(5));
		m_cancelConfirmTimer.async_wait(boost::bind(&cmMM01::confirmCancel_sendOrder, this));
		break;
	}
	}
};

void cmMM01::onRspCancel(cancelRtnPtr pCancel)
{
	//boost::mutex::scoped_lock lock(m_isOrderCanceledLock);
	//m_isOrderCanceled[pCancel->m_cancelOrderRef] = pCancel->m_isCancelSucc;
}

void cmMM01::confirmCancel_sendOrder(){
	enum_cmMM01_strategy_status status;
	{
		boost::mutex::scoped_lock lock(m_strategyStatusLock);
		status = m_strategyStatus;
	}
	if (STRATEGY_STATUS_CLOSING_POSITION == status)
		if (m_isOrderCanceled[m_cancelBidOrderRef] && m_isOrderCanceled[m_cancelAskOrderRef])
			sendOrder();
		else
		{
			m_cancelConfirmTimer.expires_from_now(boost::posix_time::milliseconds(10));
			m_cancelConfirmTimer.async_wait(boost::bind(&cmMM01::confirmCancel_sendOrder, this));
		}
};

void cmMM01::onOrderRtn(orderRtnPtr pOrder)
{

}

void cmMM01::onTradeRtn(tradeRtnPtr ptrade)
{
	enum_cmMM01_strategy_status status;
	{
		boost::mutex::scoped_lock lock(m_strategyStatusLock);
		status = m_strategyStatus;
		if (STRATEGY_STATUS_TRADED_HEDGING != status)
			m_strategyStatus = STRATEGY_STATUS_TRADED_HEDGING;
	}
	if (STRATEGY_STATUS_ORDER_SENT == status)
	{//撤单
		m_cancelBidOrderRef = m_infra->cancelOrder(m_tradeAdapterID, m_bidOrderRef,
			bind(&cmMM01::onRspCancel, this, _1));
		m_cancelAskOrderRef = m_infra->cancelOrder(m_tradeAdapterID, m_askOrderRef,
			bind(&cmMM01::onRspCancel, this, _1));
	}

	//同价对冲
	enum_order_dir_type dir = (ptrade->m_orderDir == ORDER_DIR_BUY) ? ORDER_DIR_SELL : ORDER_DIR_BUY;
	int m_hedgeOrderRef = m_infra->insertOrder(m_tradeAdapterID, m_productId, m_exchange, ORDER_TYPE_LIMIT,
		dir, POSITION_EFFECT_OPEN, FLAG_SPECULATION, ptrade->m_price , ptrade->m_volume,
		bind(&cmMM01::onHedgeOrderRtn, this, _1), bind(&cmMM01::onHedgeTradeRtn, this, _1));
	{//记录对冲量
		boost::mutex::scoped_lock lock(m_hedgeOrderVolLock);
		m_hedgeOrderVol[m_hedgeOrderRef] = ((dir == ORDER_DIR_BUY) ? 
			ptrade->m_volume : (ptrade->m_volume * -1));
	}

	//等待1s
	if (STRATEGY_STATUS_TRADED_HEDGING != status)
	{
		cout << m_strategyId << " waiting 1s to cancel hedge order!" << endl;
		m_cancelHedgeTimer.expires_from_now(boost::posix_time::milliseconds(10000));
		m_cancelHedgeTimer.async_wait(boost::bind(&cmMM01::cancelHedgeOrder, this));
	}
}

void cmMM01::cancelHedgeOrder(){

	boost::mutex::scoped_lock lock(m_hedgeOrderVolLock);
	if (m_hedgeOrderVol.size() > 0)
	{
		boost::mutex::scoped_lock lock1(m_isOrderCanceledLock);
		for (auto iter = m_hedgeOrderVol.begin(); iter != m_hedgeOrderVol.end();)
		{
			//撤销对冲单
			int cancelOrderRef = m_infra->cancelOrder(m_tradeAdapterID, iter->first,
				bind(&cmMM01::onRspCancel, this, _1));

			if (cancelOrderRef > 0)
			//记录对冲单撤销状态
				m_isOrderCanceled[iter->first] = true;
			iter++;
		}
		m_cancelConfirmTimer.expires_from_now(boost::posix_time::milliseconds(10));
		m_cancelConfirmTimer.async_wait(boost::bind(&cmMM01::confirmCancel_hedgeOrder, this));
	}
};

void cmMM01::confirmCancel_hedgeOrder()
{
	boost::mutex::scoped_lock lock(m_hedgeOrderVolLock);
	if (m_hedgeOrderVol.size() > 0)
	{
		bool isAllhedgeOrderCancelled = true;
		boost::mutex::scoped_lock lock1(m_isOrderCanceledLock);
		for (auto iter = m_hedgeOrderVol.begin(); iter != m_hedgeOrderVol.end();iter++) {
			if (!m_isOrderCanceled[iter->first])
			{
				isAllhedgeOrderCancelled = false;
				break;
			}
		}
		if (!isAllhedgeOrderCancelled)
		{
			m_cancelConfirmTimer.expires_from_now(boost::posix_time::milliseconds(5));
			m_cancelConfirmTimer.async_wait(boost::bind(&cmMM01::confirmCancel_hedgeOrder, this));
		}
		else
		{
			{
				boost::mutex::scoped_lock lock(m_strategyStatusLock);
				m_strategyStatus = STRATEGY_STATUS_TRADED_NET_HEDGING;
			}
			double netHedgeVol = 0.0;
			for (auto iter = m_hedgeOrderVol.begin(); iter != m_hedgeOrderVol.end(); iter++) 
				netHedgeVol += iter->second;
			if (0.0 != netHedgeVol)
			{
				//轧差市价对冲
				enum_order_dir_type dir = netHedgeVol > 0.0 ? ORDER_DIR_BUY : ORDER_DIR_SELL;
				double price = (dir == ORDER_DIR_BUY) ? 
					m_lastQuotePtr->UpperLimitPrice : m_lastQuotePtr->LowerLimitPrice;
				int netHedgeOrderRef = m_infra->insertOrder(m_tradeAdapterID, m_productId, m_exchange,
					ORDER_TYPE_LIMIT, dir, POSITION_EFFECT_OPEN, FLAG_SPECULATION, price, fabs(netHedgeVol),
					bind(&cmMM01::onNetHedgeOrderRtn, this, _1), bind(&cmMM01::onNetHedgeTradeRtn, this, _1));
				{
					//记录轧差对冲量
					boost::mutex::scoped_lock lock(m_NetHedgeOrderVolLock);
					m_NetHedgeOrderVol[netHedgeOrderRef] = netHedgeVol;
				}
			}
			else
				resetStrategyStatus();
		}
	}
};

void cmMM01::onHedgeOrderRtn(orderRtnPtr pOrder)
{

}

void cmMM01::onHedgeTradeRtn(tradeRtnPtr ptrade)
{
	boost::mutex::scoped_lock lock(m_hedgeOrderVolLock);
	m_hedgeOrderVol[ptrade->m_orderRef] -= ((ptrade->m_orderDir == ORDER_DIR_BUY) 
		? ptrade->m_volume : (ptrade->m_volume*-1));
	if (0.0 == m_hedgeOrderVol[ptrade->m_orderRef])
		m_hedgeOrderVol.erase(ptrade->m_orderRef);

	//同价对冲单全部成交
	if (m_hedgeOrderVol.size() == 0)
		resetStrategyStatus();
}


void cmMM01::onNetHedgeOrderRtn(orderRtnPtr pOrder)
{

}

void cmMM01::onNetHedgeTradeRtn(tradeRtnPtr ptrade)
{
	boost::mutex::scoped_lock lock(m_NetHedgeOrderVolLock);
	m_NetHedgeOrderVol[ptrade->m_orderRef] -= ((ptrade->m_orderDir == ORDER_DIR_BUY)
		? ptrade->m_volume : (ptrade->m_volume*-1));

	//轧差对冲全部成交
	if (0.0 == m_NetHedgeOrderVol[ptrade->m_orderRef])
		resetStrategyStatus();
}