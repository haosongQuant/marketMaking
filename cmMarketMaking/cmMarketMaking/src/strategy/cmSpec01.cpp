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
	m_orderQty(orderQty), m_volumeMultiple(volMulti), m_masterStrategy(nullptr),
	m_quoteTP(quoteTP), m_tradeTP(tradeTP), m_infra(infra), m_strategyConfig(config),
	m_daemonTimer(tradeTP->getDispatcher())
{
	m_lastprice   = -1.0;
	m_lastprice_1 = -1.0;
	m_upline = 50.0;
	m_downline = -50.0;
	m_tradingDate = "";
	int openNum = m_strategyConfig["openTime"].size();
	for (int i = 0; i < openNum; ++i)
	{
		Json::Value openInterval = m_strategyConfig["openTime"][i];
		int startTime = openInterval["start"].asInt() * 100 +1;
		int endTime = openInterval["end"].asInt() * 100 + 00;
		m_openTimeList.push_back(make_pair(startTime, endTime));
	}
	m_resumeMaster = false;
	m_strategyStatus = CMSPEC01_STATUS_INIT;
	m_signal == CMSPEC01_SIGNAL_NONE;
	m_toSendSignal = CMSPEC01_SIGNAL_NONE;
	m_netOpenInterest = 0;
	daemonEngine();
};

void cmSepc01::daemonEngine(){

	if (!isInOpenTime() || !m_infra->isAdapterReady(m_tradeAdapterID))
	{
		if (CMSPEC01_STATUS_INIT != m_strategyStatus && CMSPEC01_STATUS_STOP != m_strategyStatus)
			m_strategyStatus = CMSPEC01_STATUS_STOP;
	}
	else if (CMSPEC01_STATUS_INIT == m_strategyStatus || CMSPEC01_STATUS_STOP == m_strategyStatus)
		startStrategy();

	m_daemonTimer.expires_from_now(boost::posix_time::millisec(1000 * 60)); //每分钟运行一次
	m_daemonTimer.async_wait(boost::bind(&cmSepc01::daemonEngine, this));
};

void cmSepc01::startStrategy(){
	cout << m_strategyId << " starting..." << endl;
	if (CMSPEC01_STATUS_INIT == m_strategyStatus)
	{
		m_infra->subscribeFutures(m_quoteAdapterID, m_exchange, m_productId, bind(&cmSepc01::onRtnMD, this, _1));
	}
	m_strategyStatus = CMSPEC01_STATUS_START;
};

void cmSepc01::onRtnMD(futuresMDPtr pFuturesMD)//行情响应函数: 更新行情，调用quoteEngine产生信号
{ 
	{
		boost::mutex::scoped_lock lock(m_lastQuoteLock);
		m_lastQuotePtr = pFuturesMD;
		m_lastprice_1 = m_lastprice;
		m_lastprice = pFuturesMD->LastPrice; 
	}
	m_quoteTP->getDispatcher().post(bind(&cmSepc01::quoteEngine, this));
};

void cmSepc01::quoteEngine()
{

	if (m_strategyStatus != CMSPEC01_STATUS_START)
		return;

	if (m_lastprice_1 < 0.0)
		return;

	double yield = log(m_lastprice / m_lastprice_1);
	double priceChg = abs(m_lastprice - m_lastprice_1);
	double ma1, ma2, avgTrueRange, apcosm;
	vector<double> apcosm_Buff;

	{
		boost::mutex::scoped_lock lock(m_buffLock);
		m_yieldBuff_short.push_back(yield);
		m_yieldBuff_long.push_back(yield);
		m_avg_true_range.push_back(priceChg);

		if (!m_yieldBuff_long.full())
			return;

		ma1 = accumulate(m_yieldBuff_short.begin(), m_yieldBuff_short.end(), 0.0)
			/ m_yieldBuff_short.capacity();
		ma2 = accumulate(m_yieldBuff_long.begin(), m_yieldBuff_long.end(), 0.0)
			/ m_yieldBuff_long.capacity();
		avgTrueRange = accumulate(m_avg_true_range.begin(), m_avg_true_range.end(), 0.0)
			/ m_avg_true_range.capacity();
		if (avgTrueRange != 0.0)
			apcosm = (ma1 - ma2) / avgTrueRange;
		else
			apcosm = 50;

		m_Apcosm_Buff.push_back(apcosm);
		if (!m_Apcosm_Buff.full())
			return;

		auto iter = m_Apcosm_Buff.begin();
		while (iter != m_Apcosm_Buff.end())
		{
			apcosm_Buff.push_back(*iter);
			iter++;
		}
	}

	sort(apcosm_Buff.begin(), apcosm_Buff.end());
	double qu1 = apcosm_Buff[round(apcosm_Buff_size*0.25)];
	double qu2 = apcosm_Buff[round(apcosm_Buff_size*0.50)];
	double qu3 = apcosm_Buff[round(apcosm_Buff_size*0.75)];

	double new_abs;
	if ((qu3 - qu1) != 0)
		new_abs = (apcosm - qu2) / (qu3 - qu1) * 100;
	else 
		new_abs = 50.0;

	double ema_alpha = 0.5;
	double new_abs_ema = 0.0;
	{
		boost::mutex::scoped_lock lock1(m_newAbsLock);
		m_newAbs_Buff.push_front(new_abs);
		if (!m_newAbs_Buff.full())
			return;
		auto iter = m_newAbs_Buff.begin();
		new_abs_ema = *iter;
		iter++;
		while (iter != m_newAbs_Buff.end())
		{
			new_abs_ema = new_abs_ema * (1-ema_alpha) + (*iter) * ema_alpha;
			iter++;
		}
	}

	LOG(INFO) << m_strategyId << ", new_abs_ema: " << new_abs_ema << endl;

	if (new_abs_ema > m_upline)
		m_signal = CMSPEC01_SIGNAL_LONG;
	else if (new_abs_ema < m_downline)
		m_signal = CMSPEC01_SIGNAL_SHORT;
	else
		m_signal = CMSPEC01_SIGNAL_NONE;

	if ((new_abs_ema > m_upline && m_netOpenInterest <= 0)
		|| (new_abs_ema < m_downline && m_netOpenInterest > 0))
	{
		{
			boost::mutex::scoped_lock lock(m_strategyStatusLock);
			if (CMSPEC01_STATUS_START != m_strategyStatus)
				return;
			switch (m_masterStrategyTyp)
			{
			case STRATEGY_cmMM01:
			{
				cmMM01 *pStrategy = (cmMM01 *)m_masterStrategy;
				enum_strategy_interrupt_result interRuptRc = 
					pStrategy->tryInterrupt(boost::bind(&cmSepc01::sendOrder, this));
				m_toSendSignal = m_signal;
				switch (interRuptRc)
				{
				case STRATEGY_INTERRUPT_BREAKING:
				{
					m_resumeMaster = false;
					m_strategyStatus = CMSPEC01_STATUS_ORDER_SENT;
					sendOrder();
					break;
				}
				case STRATEGY_INTERRUPT_WAIT_CALLBACK:
				{
					m_resumeMaster = true;
					m_strategyStatus = CMSPEC01_STATUS_ORDER_SENT;
					break;
				}
				}
				break;
			}
			case STRATEGY_cmMM02:
			{
				cmMM02 *pStrategy = (cmMM02 *)m_masterStrategy;
				enum_strategy_interrupt_result interRuptRc =
					pStrategy->tryInterrupt(boost::bind(&cmSepc01::sendOrder, this));
				m_toSendSignal = m_signal;
				switch (interRuptRc)
				{
				case STRATEGY_INTERRUPT_BREAKING:
				{
					m_resumeMaster = false;
					m_strategyStatus = CMSPEC01_STATUS_ORDER_SENT;
					sendOrder();
					break;
				}
				case STRATEGY_INTERRUPT_WAIT_CALLBACK:
				{
					m_resumeMaster = true;
					m_strategyStatus = CMSPEC01_STATUS_ORDER_SENT;
					break;
				}
				}
				break;
			}
			}
		}
	}
};

void cmSepc01::resumeMaster()
{
	if (m_resumeMaster)
	{
		switch (m_masterStrategyTyp)
		{
		case STRATEGY_cmMM01:
		{
			cmMM01 *pStrategy = (cmMM01 *)m_masterStrategy;
			pStrategy->resume();
		}
		case STRATEGY_cmMM02:
		{
			cmMM02 *pStrategy = (cmMM02 *)m_masterStrategy;
			pStrategy->resume();
		}
		}
	}
	else
		LOG(INFO) << m_strategyId << ": master strategy not resumed!" << endl;
};

void cmSepc01::sendOrder(){

	if (m_signal != m_toSendSignal)
	{
		resumeMaster();
		m_toSendSignal = CMSPEC01_SIGNAL_NONE;
		return;
	}
	enum_order_dir_type dir;
	if (m_signal == CMSPEC01_SIGNAL_LONG)
		dir = ORDER_DIR_BUY;
	else if (m_signal == CMSPEC01_SIGNAL_SHORT)
		dir = ORDER_DIR_SELL;

	double price;
	{
		boost::mutex::scoped_lock lock(m_lastQuoteLock);
		price = (dir == ORDER_DIR_BUY) ?
		//	m_lastQuotePtr->UpperLimitPrice : m_lastQuotePtr->LowerLimitPrice;
			(m_lastQuotePtr->askprice[0] + m_tickSize * 2.0) : (m_lastQuotePtr->bidprice[0] - m_tickSize * 2.0);
	}

	unsigned int vol;
	if (dir == ORDER_DIR_BUY)
		vol = m_orderQty;// -m_netOpenInterest;
	else
		vol = m_orderQty;// +m_netOpenInterest;
		
	if (vol > 0)
	{
		m_orderRef = m_infra->insertOrder(m_tradeAdapterID, m_productId, m_exchange,
			ORDER_TYPE_LIMIT, dir, 
			(m_netOpenInterest == 0 ? POSITION_EFFECT_OPEN : POSITION_EFFECT_CLOSE),
			FLAG_SPECULATION, price, vol,
			bind(&cmSepc01::onOrderRtn, this, _1),
			bind(&cmSepc01::onTradeRtn, this, _1));
		if (m_orderRef > 0)
		{
			LOG(INFO) << m_strategyId << ": send order succ. code: " << m_productId
				<< ", dir: " << ((dir == ORDER_DIR_BUY) ? "buy, vol: " : "sell, vol: ")
				<< vol << endl;
		}
	}
	m_toSendSignal = CMSPEC01_SIGNAL_NONE;
};

void cmSepc01::processOrder(orderRtnPtr pOrder)
{
	write_lock lock(m_tradingDtRWLock);
	m_tradingDate = string(pOrder->m_tradingDay);
};

void cmSepc01::processTrade(tradeRtnPtr ptrade)
{ 
	{
		boost::mutex::scoped_lock lock(m_netOpenInterestLock);
		m_netOpenInterest += (ptrade->m_orderDir == ORDER_DIR_BUY ? ptrade->m_volume : (ptrade->m_volume * -1)); 
	}
	resumeMaster();
	{
		boost::mutex::scoped_lock lock1(m_strategyStatusLock);
		m_strategyStatus = CMSPEC01_STATUS_START;
	}
	{
		read_lock lock2(m_tradingDtRWLock);
		LOG(INFO) << "," << m_strategyId << ",spec_tradeRtn"
			<< ", orderRef:" << ptrade->m_orderRef
			<< ", tradeDate:" << m_tradingDate
			<< ", InstrumentID:" << ptrade->m_instId
			<< ", Direction:" << ptrade->m_orderDir
			<< ", Price:" << ptrade->m_price
			<< ", volume:" << ptrade->m_volume << endl;
	}
};

//撤单响应函数
void cmSepc01::processCancelRes(cancelRtnPtr pCancel)
{
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
