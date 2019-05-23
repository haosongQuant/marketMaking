#include <math.h>
#include <numeric>
#include "strategy/cmTestOrder01.h"
#include "glog\logging.h"

cmTestOrder01::cmTestOrder01(string strategyId, string strategyTyp, string productId, string exchange,
	string quoteAdapterID, string tradeAdapterID, athenathreadpoolPtr quoteTP,
	athenathreadpoolPtr tradeTP, infrastructure* infra, Json::Value config)
	:m_strategyId(strategyId), m_strategyTyp(strategyTyp), m_productId(productId), m_exchange(exchange),
	m_quoteAdapterID(quoteAdapterID), m_tradeAdapterID(tradeAdapterID),
	m_quoteTP(quoteTP), m_tradeTP(tradeTP), m_infra(infra), m_strategyConfig(config),
	m_daemonTimer(tradeTP->getDispatcher())
{
	m_strategyStatus = CMTESTORDER01_STATUS_INIT;
	m_isPriceReady = false;
	m_upperLimit = 0.0;
	m_lowerLimit = 0.0;
	m_cancelOrderRC = 0;
	int openNum = m_strategyConfig["openTime"].size();
	for (int i = 0; i < openNum; ++i)
	{
		Json::Value openInterval = m_strategyConfig["openTime"][i];
		int startTime = openInterval["start"].asInt() * 100 +1;
		int endTime = openInterval["end"].asInt() * 100 + 00;
		m_openTimeList.push_back(make_pair(startTime, endTime));
	}
	daemonEngine();
};

void cmTestOrder01::daemonEngine(){

	if (!isInOpenTime() || !m_infra->isAdapterReady(m_tradeAdapterID))
	{
		if (CMTESTORDER01_STATUS_INIT != m_strategyStatus 
			&& CMTESTORDER01_STATUS_STOP != m_strategyStatus)
			m_strategyStatus = CMTESTORDER01_STATUS_STOP;
	}
	else if (CMTESTORDER01_STATUS_INIT == m_strategyStatus 
		|| CMTESTORDER01_STATUS_STOP == m_strategyStatus)
		startStrategy();

	m_daemonTimer.expires_from_now(boost::posix_time::millisec(1000 * 60)); //每分钟运行一次
	m_daemonTimer.async_wait(boost::bind(&cmTestOrder01::daemonEngine, this));
};

void cmTestOrder01::startStrategy(){
	cout << m_strategyId << " starting..." << endl;
	if (CMTESTORDER01_STATUS_INIT == m_strategyStatus)
		m_infra->subscribeFutures(m_quoteAdapterID, m_exchange, m_productId, 
									bind(&cmTestOrder01::onRtnMD, this, _1));
	m_strategyStatus = CMTESTORDER01_STATUS_START;
};

void cmTestOrder01::onRtnMD(futuresMDPtr pFuturesMD)//行情响应函数: 更新行情，调用quoteEngine产生信号
{
	m_quoteTP->getDispatcher().post(bind(&cmTestOrder01::quoteEngine, this, pFuturesMD));
};

void cmTestOrder01::quoteEngine(futuresMDPtr pFuturesMD)
{
	boost::recursive_mutex::scoped_lock lock(m_strategyStatusLock);
	LOG(INFO) << m_strategyId << " | status: " << m_strategyStatus << endl;

	if (!m_isPriceReady)
	{
		m_upperLimit = pFuturesMD->UpperLimitPrice;
		m_lowerLimit = pFuturesMD->LowerLimitPrice;
	}

	switch (m_strategyStatus)
	{
	case CMTESTORDER01_STATUS_START:
	{
		sendLowerLmtOrder();
		m_strategyStatus = CMTESTORDER01_STATUS_LOWERLIMIT_ORDER;
		break; 
	}
	case CMTESTORDER01_STATUS_LOWERLIMIT_ORDER:
	{
		if (cancelOrder())
		{
			sendUpperLmtOrder();
			m_strategyStatus = CMTESTORDER01_STATUS_UPPERLIMIT_ORDER;
		}
		break; 
	}
	case CMTESTORDER01_STATUS_UPPERLIMIT_ORDER:
	{
		if (cancelOrder())
		{
			sendLowerLmtOrder();
			m_strategyStatus = CMTESTORDER01_STATUS_UPPERLIMIT_ORDER;
		}
		break; 
	}
	case CMTESTORDER01_STATUS_STOP:
	{
		cancelOrder();
		break; 
	}
	}
};

void cmTestOrder01::sendUpperLmtOrder()
{
	enum_order_dir_type dir = ORDER_DIR_SELL;
	double price = m_upperLimit;
	unsigned int vol = 1;
	m_orderRef = m_infra->insertOrder(m_tradeAdapterID, m_productId, m_exchange,
		ORDER_TYPE_LIMIT, dir, POSITION_EFFECT_OPEN, FLAG_SPECULATION, price, vol,
		bind(&cmTestOrder01::onOrderRtn, this, _1),
		bind(&cmTestOrder01::onTradeRtn, this, _1));
	if (m_orderRef > 0)
	{
		LOG(INFO) << m_strategyId << ": send order succ. code: " << m_productId
			<< ", dir: " << ((dir == ORDER_DIR_BUY) ? "buy, vol: " : "sell, vol: ")
			<< vol << endl;
	}
};
void cmTestOrder01::sendLowerLmtOrder()
{
	enum_order_dir_type dir = ORDER_DIR_BUY;
	double price = m_lowerLimit;
	unsigned int vol = 1;
	m_orderRef = m_infra->insertOrder(m_tradeAdapterID, m_productId, m_exchange,
		ORDER_TYPE_LIMIT, dir, POSITION_EFFECT_OPEN, FLAG_SPECULATION, price, vol,
		bind(&cmTestOrder01::onOrderRtn, this, _1),
		bind(&cmTestOrder01::onTradeRtn, this, _1));
	if (m_orderRef > 0)
	{
		LOG(INFO) << m_strategyId << ": send order succ. code: " << m_productId
			<< ", dir: " << ((dir == ORDER_DIR_BUY) ? "buy, vol: " : "sell, vol: ")
			<< vol << endl;
	}
};

bool cmTestOrder01::cancelOrder()
{
	if (m_orderRef == 0)
	{
		LOG(WARNING) << m_strategyId << ": order ref is 0." << endl;
		return false;
	}
	{
		read_lock lock(m_orderRtnBuffLock);
		auto orderIter = m_orderRef2orderRtn.find(m_orderRef);
		if (orderIter == m_orderRef2orderRtn.end())
		{
			m_infra->queryOrder(m_tradeAdapterID, m_orderRef);
			LOG(WARNING) << m_strategyId << ": order not found, querying order, orderRef: " << m_orderRef << endl;
			return false;
		}
		else if (orderIter->second->m_orderStatus == ORDER_STATUS_Canceled
			|| orderIter->second->m_orderStatus == ORDER_STATUS_AllTraded
			|| orderIter->second->m_orderStatus == ORDER_STATUS_PartTradedNotQueueing)
		{
			LOG(WARNING) << m_strategyId << ": order cancelled, orderRef: " << m_orderRef << endl;
			m_cancelOrderRC = 0;
			return true;
		}
	}

	if (m_cancelOrderRC == 0 || m_cancelOrderRC == ORDER_CANCEL_ERROR_SEND_FAIL)
	{
		m_cancelOrderRC = m_infra->cancelOrder(m_tradeAdapterID, m_orderRef,
			bind(&cmTestOrder01::onRspCancel, this, _1));
		if (m_cancelOrderRC == ORDER_CANCEL_ERROR_SEND_FAIL)
			LOG(WARNING) << m_strategyId << ": send cancel failed, orderRef: " << m_orderRef << endl;
		else
			LOG(WARNING) << m_strategyId << ": send cancel succ, orderRef: " << m_orderRef << endl;
	}

	return false;
};

void cmTestOrder01::processOrder(orderRtnPtr pOrder)
{
	registerOrder(pOrder);
};

void cmTestOrder01::registerOrder(orderRtnPtr pOrder)
{
	write_lock lock(m_orderRtnBuffLock);
	m_orderRef2orderRtn[pOrder->m_orderRef] = pOrder;
};

void cmTestOrder01::processTrade(tradeRtnPtr ptrade)
{ 
	LOG(INFO) << "," << m_strategyId << ",spec_tradeRtn"
		<< ", orderRef:" << ptrade->m_orderRef
		<< ", tradeDate:" << ptrade->m_tradeDate
		<< ", InstrumentID:" << ptrade->m_instId
		<< ", Direction:" << ptrade->m_orderDir
		<< ", Price:" << ptrade->m_price
		<< ", volume:" << ptrade->m_volume << endl;
};

//撤单响应函数
void cmTestOrder01::processCancelRes(cancelRtnPtr pCancel)
{
}

bool cmTestOrder01::isInOpenTime()
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
