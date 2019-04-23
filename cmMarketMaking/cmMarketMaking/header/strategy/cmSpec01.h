#pragma once
#include <list>
#include <iostream>
#include "infrastructure.h"
#include "threadpool\threadpool.h"
#include "baseClass/strategyBase.h"
#include "baseClass/orderBase.h"
#include "baseClass/Utils.h"
#include <boost\circular_buffer.hpp>
#include "strategy\cmMM01.h"

using namespace std;

#define apcosm_Buff_size 100

enum enum_cmSepc01_strategy_status
{//策略状态，决定是否发送新的委托
	CMSPEC01_STATUS_INIT,
	CMSPEC01_STATUS_START,
	CMSPEC01_STATUS_STOP,
};

enum enum_cmSepc01_Signal_Typ
{
	CMSPEC01_SIGNAL_LONG,
	CMSPEC01_SIGNAL_SHORT,
	CMSPEC01_SIGNAL_NONE,
};

typedef boost::circular_buffer<double> cirBuff;

class cmSepc01 : public strategyBase
{
private:
	string m_strategyId;
	string m_strategyTyp;
	string m_productId;
	string m_exchange;
	string m_quoteAdapterID;
	string m_tradeAdapterID;
	double m_tickSize;
	double m_orderQty;
	int    m_volumeMultiple;

	infrastructure* m_infra;
	Json::Value m_strategyConfig;
	athenathreadpoolPtr m_quoteTP;
	athenathreadpoolPtr m_tradeTP;
	
public:
	cmSepc01(string strategyId, string strategyTyp, string productId, string exchange,
		string quoteAdapterID, string tradeAdapterID, double tickSize, 
		double orderQty, int volMulti,
		athenathreadpoolPtr quoteTP, athenathreadpoolPtr tradeTP, infrastructure* infra,
		Json::Value config);
	~cmSepc01();
	virtual void startStrategy();
	virtual void stopStrategy(){};

public:
	void registerMasterStrategy(strategyBase *masterStrategy, enum_strategy_type masterStrategyTyp){
		m_masterStrategy = masterStrategy; m_masterStrategyTyp = masterStrategyTyp;};

private:
	list< pair <int, int> > m_openTimeList;
	bool isInOpenTime();

public: //供外部调用的响应函数 | 在策略线程池中调用相应的处理函数
	void onRtnMD(futuresMDPtr);
	void onOrderRtn(orderRtnPtr pOrder){ m_tradeTP->getDispatcher().post(bind(&cmSepc01::processOrder, this, pOrder)); };
	void onTradeRtn(tradeRtnPtr ptrade){ m_tradeTP->getDispatcher().post(bind(&cmSepc01::processTrade, this, ptrade)); };
	void onRspCancel(cancelRtnPtr pCancel){ m_tradeTP->getDispatcher().post(bind(&cmSepc01::processCancelRes, this, pCancel)); };

private:

	boost::mutex m_lastQuoteLock;
	futuresMDPtr m_lastQuotePtr;
	double m_upline;
	double m_downline;
	double m_lastprice;
	double m_lastprice_1;
	boost::mutex m_buffLock;
	cirBuff m_yieldBuff_short{ 10 };
	cirBuff m_yieldBuff_long{ 10 * 20 };
	cirBuff m_avg_true_range{ 10 * 20 };
	cirBuff m_Apcosm_Buff{ apcosm_Buff_size };
	void quoteEngine();

private:
	strategyBase      *m_masterStrategy;
	enum_strategy_type m_masterStrategyTyp;
	bool               m_resumeMaster;
private:
	enum_cmSepc01_strategy_status m_strategyStatus;
	enum_cmSepc01_Signal_Typ      m_signal;
	boost::mutex m_netOpenInterestLock;
	int          m_netOpenInterest;

public:
	void sendOrder();

private:
	int m_orderRef;
	int m_askOrderRef;
	void processOrder(orderRtnPtr);
	void processTrade(tradeRtnPtr);
	void processCancelRes(cancelRtnPtr pCancel);

private:
	athena_lag_timer m_daemonTimer;
	void daemonEngine(); //守护线程引擎

};