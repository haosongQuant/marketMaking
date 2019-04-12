#include "strategy/cmMM01.h"

cmMM01::cmMM01(string strategyId, string strategyTyp, string productId, string exchange,
	string quoteAdapterID, string tradeAdapterID, double tickSize, double miniOrderSpread,
	double orderQty,
	athenathreadpoolPtr quoteTP, athenathreadpoolPtr tradeTP, infrastructure* infra)
	:m_strategyId(strategyId), m_strategyTyp(strategyTyp), m_productId(productId), m_exchange(exchange),
	m_quoteAdapterID(quoteAdapterID), m_tradeAdapterID(tradeAdapterID), m_tickSize(tickSize),
	m_miniOrderSpread(miniOrderSpread), m_orderQty(orderQty), m_quoteTP(quoteTP), m_tradeTP(tradeTP),
	m_infra(infra),
	m_cancelConfirmTimer(tradeTP->getDispatcher()), m_cancelHedgeTimer(tradeTP->getDispatcher()),
	m_resetStatusTimer(tradeTP->getDispatcher())
{
	m_strategyStatus  = STRATEGY_STATUS_START;
};

void cmMM01::resetStrategyStatus(){
	m_strategyStatus = STRATEGY_STATUS_READY;
	m_bidOrderRef = 0;
	m_askOrderRef = 0;
	m_cancelBidOrderRC = 0;
	m_cancelAskOrderRC = 0;
	m_isOrderCanceled = false;
};

void cmMM01::startStrategy(){
	cout << m_strategyId << " starting..." << endl;
	if (STRATEGY_STATUS_START == m_strategyStatus)
		m_infra->subscribeFutures(m_quoteAdapterID, m_exchange, m_productId, bind(&cmMM01::onRtnMD, this, _1));
	resetStrategyStatus();
};

void cmMM01::stopStrategy(){
	m_strategyStatus = STRATEGY_STATUS_STOP;
};

void cmMM01::orderPrice(double* bidprice, double* askprice)
{
	int quoteSpread = round((m_lastQuotePtr->askprice[0] - m_lastQuotePtr->bidprice[0]) / m_tickSize);
	if (quoteSpread > m_miniOrderSpread) return;
	switch (quoteSpread)
	{
	case 1:
	case 2:
	{
		*bidprice = m_lastQuotePtr->bidprice[0] - m_tickSize;
		break;
	}
	case 3:
	case 4:
	{
		*bidprice = m_lastQuotePtr->bidprice[0];
		break;
	}
	}
	//*bidprice = m_lastQuotePtr->bidprice[0] + int((quoteSpread - m_miniOrderSpread) / 2) * m_tickSize;
	*bidprice = m_lastQuotePtr->askprice[0]; //测试成交
	*askprice = *bidprice + m_tickSize * m_miniOrderSpread;
};

void cmMM01::sendOrder()
{
	double bidprice=0.0, askprice=0.0;
	orderPrice(&bidprice, &askprice);
	if (0.0 == bidprice || 0.0 == askprice)
	{
		cout << m_strategyId << ": warning | spread is too wide, no order sent." << endl;
		return;
	}

	m_bidOrderRef = 0;
	m_askOrderRef = 0;
	m_isOrderCanceled = false;

	m_bidOrderRef = m_infra->insertOrder(m_tradeAdapterID, m_productId, m_exchange, ORDER_TYPE_LIMIT,
		ORDER_DIR_BUY, POSITION_EFFECT_OPEN, FLAG_SPECULATION, bidprice, m_orderQty,
		bind(&cmMM01::onOrderRtn, this, _1), bind(&cmMM01::onTradeRtn, this, _1));

	m_askOrderRef = m_infra->insertOrder(m_tradeAdapterID, m_productId, m_exchange, ORDER_TYPE_LIMIT,
		ORDER_DIR_SELL, POSITION_EFFECT_OPEN, FLAG_SPECULATION, askprice, m_orderQty,
		bind(&cmMM01::onOrderRtn, this, _1), bind(&cmMM01::onTradeRtn, this, _1));

	m_strategyStatus = STRATEGY_STATUS_ORDER_SENT;
};


//行情响应函数: 更新最新行情，调用quoteEngine处理行情
void cmMM01::onRtnMD(futuresMDPtr pFuturesMD)
{
	{
		boost::mutex::scoped_lock lock(m_lastQuoteLock);
		m_lastQuotePtr = pFuturesMD;
	}
	m_quoteTP->getDispatcher().post(bind(&cmMM01::quoteEngine, this));
};


//行情处理
//    如果策略处于 READY      状态，下单
//    如果策略处于 ORDER_SENT 状态，撤单并异步调用confirmCancel_sendOrder(),重新下单
void cmMM01::quoteEngine()
{
	switch (m_strategyStatus)
	{
	case STRATEGY_STATUS_READY:
	{
		sendOrder();
		break;
	}
	case STRATEGY_STATUS_ORDER_SENT:
	{
		m_strategyStatus = STRATEGY_STATUS_CLOSING_POSITION;

		m_cancelBidOrderRC = 0;
		m_cancelAskOrderRC = 0;
		CancelOrder();

		break;
	}
	}
};

//撤单响应函数
void cmMM01::onRspCancel(cancelRtnPtr pCancel)
{
}

void cmMM01::CancelOrder()
{
	if (m_cancelBidOrderRC == 0 || m_cancelBidOrderRC == ORDER_CANCEL_ERROR_NOT_FOUND)
		m_cancelBidOrderRC = m_infra->cancelOrder(m_tradeAdapterID, m_bidOrderRef,
			bind(&cmMM01::onRspCancel, this, _1));
	if (m_cancelAskOrderRC == 0 || m_cancelAskOrderRC == ORDER_CANCEL_ERROR_NOT_FOUND)
		m_cancelAskOrderRC = m_infra->cancelOrder(m_tradeAdapterID, m_askOrderRef,
			bind(&cmMM01::onRspCancel, this, _1));

	if (m_cancelBidOrderRC == ORDER_CANCEL_ERROR_NOT_FOUND ||
		m_cancelAskOrderRC == ORDER_CANCEL_ERROR_NOT_FOUND)
	{
		cout << m_strategyId << ": waiting to cancel order." << endl;
		m_cancelConfirmTimer.expires_from_now(boost::posix_time::milliseconds(5000));
		m_cancelConfirmTimer.async_wait(boost::bind(&cmMM01::CancelOrder, this));
	}
	else
	{
		m_isOrderCanceled = true;
		if (STRATEGY_STATUS_CLOSING_POSITION == m_strategyStatus)
		{//重新下单
			sendOrder();
		}
	}
};

//撤单后重新下单
void cmMM01::confirmCancel_sendOrder(){
};

void cmMM01::onOrderRtn(orderRtnPtr pOrder)
{
}


//处理报单的成交回报
//    1、将策略状态设置为 TRADED_HEDGING
//    2、如果尚未撤单，下撤单指令
//    3、下对冲单
//    4、等待1s钟，调用对冲指令处理函数 cancelHedgeOrder()
void cmMM01::onTradeRtn(tradeRtnPtr ptrade)
{
	enum_cmMM01_strategy_status status = m_strategyStatus;
	m_strategyStatus = STRATEGY_STATUS_TRADED_HEDGING;
	if (STRATEGY_STATUS_ORDER_SENT == status)
	{//撤单
		m_cancelBidOrderRC = 0;
		m_cancelAskOrderRC = 0;
		CancelOrder();
	}

	//同价对冲
	enum_order_dir_type dir = (ptrade->m_orderDir == ORDER_DIR_BUY) ? ORDER_DIR_SELL : ORDER_DIR_BUY;
	int m_hedgeOrderRef = m_infra->insertOrder(m_tradeAdapterID, m_productId, m_exchange, ORDER_TYPE_LIMIT,
		dir, POSITION_EFFECT_OPEN, FLAG_SPECULATION, ptrade->m_price , ptrade->m_volume,
		bind(&cmMM01::onHedgeOrderRtn, this, _1), bind(&cmMM01::onHedgeTradeRtn, this, _1));
	if (m_hedgeOrderRef > 0)
	{//记录对冲量和撤销（完成）状态
		boost::mutex::scoped_lock lock(m_hedgeOrderVolLock);
		m_hedgeOrderVol[m_hedgeOrderRef] = ((dir == ORDER_DIR_BUY) ? 
			ptrade->m_volume : (ptrade->m_volume * -1));
		m_hedgeOrderCancelRC[m_hedgeOrderRef] = 0;
	}

	//等待1s
	if (STRATEGY_STATUS_TRADED_HEDGING != status)
	{
		cout << m_strategyId << ": waiting 1s to cancel hedge order!" << endl;
		m_cancelHedgeTimer.expires_from_now(boost::posix_time::milliseconds(1000));
		m_cancelHedgeTimer.async_wait(boost::bind(&cmMM01::cancelHedgeOrder, this,
			boost::asio::placeholders::error));
	}
}


void cmMM01::confirmCancel_resetStatus()//确定原始挂单已撤销，重置策略状态为 READY
{
	if (!m_isOrderCanceled)
	{
		m_resetStatusTimer.expires_from_now(boost::posix_time::milliseconds(100));
		m_resetStatusTimer.async_wait(boost::bind(&cmMM01::confirmCancel_resetStatus, this));
	}
	else
		resetStrategyStatus();
};

//对冲成交处理函数
void cmMM01::onHedgeTradeRtn(tradeRtnPtr ptrade)
{
	boost::mutex::scoped_lock lock(m_hedgeOrderVolLock);
	m_hedgeOrderVol[ptrade->m_orderRef] -= ((ptrade->m_orderDir == ORDER_DIR_BUY)
		? ptrade->m_volume : (ptrade->m_volume*-1));
	if (0.0 == m_hedgeOrderVol[ptrade->m_orderRef])
		m_hedgeOrderVol.erase(ptrade->m_orderRef); //删除已经完成的对冲指令

	//同价对冲单全部成交
	if (m_hedgeOrderVol.size() == 0)
	{
		m_hedgeOrderVol.clear();
		m_cancelHedgeTimer.cancel();
		confirmCancel_resetStatus();
	}
}

//处理对冲指令：如果对冲指令未成交，撤单，并异步调用轧差市价对冲函数 confirmCancel_hedgeOrder()
void cmMM01::cancelHedgeOrder(const boost::system::error_code& error){

	if (error)
	{
		cout << m_strategyId << ": hedge timer cancelled" << endl;
		return;
	}

	boost::mutex::scoped_lock lock(m_hedgeOrderVolLock);
	if (m_hedgeOrderVol.size() > 0)   //存在未完成的对冲指令
	{
		bool isHedgeOrderConfirmed = true;
		for (auto iter = m_hedgeOrderVol.begin(); iter != m_hedgeOrderVol.end();)
		{
			//撤销对冲单
			if (m_hedgeOrderCancelRC[iter->first] == 0 ||
				m_hedgeOrderCancelRC[iter->first] == ORDER_CANCEL_ERROR_NOT_FOUND)
			{
				int cancelOrderRC = m_infra->cancelOrder(m_tradeAdapterID, iter->first,
					bind(&cmMM01::onRspCancel, this, _1));
				m_hedgeOrderCancelRC[iter->first] = cancelOrderRC;

				if (ORDER_CANCEL_ERROR_NOT_FOUND == cancelOrderRC) // 发送撤单失败
					isHedgeOrderConfirmed = false;
			}
			iter++;
		}
		if (!isHedgeOrderConfirmed)
		{
			cout << m_strategyId << ": waiting to cancel hedge order." << endl;
			m_cancelHedgeTimer.expires_from_now(boost::posix_time::milliseconds(100));
			m_cancelHedgeTimer.async_wait(boost::bind(&cmMM01::cancelHedgeOrder, this,
				boost::asio::placeholders::error));
		}
		else
			confirmCancel_hedgeOrder();
	}
};


//轧差市价对冲
void cmMM01::confirmCancel_hedgeOrder()
{
	//boost::mutex::scoped_lock lock(m_hedgeOrderVolLock); //在 cancelHedgeOrder() 中互斥
	if (m_hedgeOrderVol.size() > 0)
	{
		m_strategyStatus = STRATEGY_STATUS_TRADED_NET_HEDGING;

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
			if (netHedgeOrderRef > 0)
			{
				//记录轧差对冲量
				boost::mutex::scoped_lock lock(m_NetHedgeOrderVolLock);
				m_NetHedgeOrderVol = netHedgeVol;
				m_hedgeOrderVol.clear();
			}
			else
				cout << m_strategyId << " ERROR: send net hedge order failed, rc = " << netHedgeOrderRef << endl;
		}
		else
			confirmCancel_resetStatus();
	}
};

//轧差市价成交处理函数
void cmMM01::onNetHedgeTradeRtn(tradeRtnPtr ptrade)
{
	boost::mutex::scoped_lock lock(m_NetHedgeOrderVolLock);
	m_NetHedgeOrderVol -= ((ptrade->m_orderDir == ORDER_DIR_BUY)
		? ptrade->m_volume : (ptrade->m_volume*-1));

	cout << m_strategyId << ": net hedge order left: " << m_NetHedgeOrderVol << endl;
	if (0.0 == m_NetHedgeOrderVol) //轧差对冲全部成交
	{
		confirmCancel_resetStatus();
	}
}

void cmMM01::onHedgeOrderRtn(orderRtnPtr pOrder)
{

}

void cmMM01::onNetHedgeOrderRtn(orderRtnPtr pOrder)
{

}
