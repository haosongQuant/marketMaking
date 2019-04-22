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
	int openNum = m_strategyConfig["openTime"].size();
	for (int i = 0; i < openNum; ++i)
	{
		Json::Value openInterval = m_strategyConfig["openTime"][i];
		int startTime = openInterval["start"].asInt() * 100 +1;
		int endTime = openInterval["end"].asInt() * 100 + 59;
		m_openTimeList.push_back(make_pair(startTime, endTime));
	}
	m_strategyStatus = CMSPEC01_STATUS_INIT;
	daemonEngine();
};

void cmSepc01::daemonEngine(){

	if (!isInOpenTime())
	{
		if (CMSPEC01_STATUS_STOP != m_strategyStatus)
			m_strategyStatus = CMSPEC01_STATUS_STOP;
	}
	else if (CMSPEC01_STATUS_START != m_strategyStatus)
		startStrategy();

	m_daemonTimer.expires_from_now(boost::posix_time::millisec(1000 * 60)); //ÿ��������һ��
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

void cmSepc01::onRtnMD(futuresMDPtr pFuturesMD)//������Ӧ����: �������飬����quoteEngine�����ź�
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

	if (new_abs > m_upline)
		LOG(INFO) << m_strategyId << ": buy signal!" << endl;
	else if (new_abs < m_downline)
		LOG(INFO) << m_strategyId << ": sell signal!" << endl;

};

void cmSepc01::processOrder(orderRtnPtr pOrder)
{
};

void cmSepc01::processTrade(tradeRtnPtr ptrade)
{
};

//������Ӧ����
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