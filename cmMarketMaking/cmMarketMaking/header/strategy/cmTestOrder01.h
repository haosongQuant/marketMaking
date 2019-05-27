#pragma once
#include <list>
#include <iostream>
#include "infrastructure.h"
#include "threadpool\threadpool.h"
#include "baseClass/strategyBase.h"
#include "baseClass/orderBase.h"
#include "baseClass/Utils.h"

using namespace std;

enum enum_cmTestOrder01_strategy_status
{//策略状态，决定是否发送新的委托
	CMTESTORDER01_STATUS_INIT,
	CMTESTORDER01_STATUS_START,
	CMTESTORDER01_STATUS_LOWERLIMIT_ORDER,
	CMTESTORDER01_STATUS_UPPERLIMIT_ORDER,
	CMTESTORDER01_STATUS_STOP,
};

class cmTestOrder01 : public strategyBase
{
private:
	string m_strategyId;
	string m_strategyTyp;
	string m_productId;
	string m_exchange;
	string m_quoteAdapterID;
	string m_tradeAdapterID;
	enum_cmTestOrder01_strategy_status m_strategyStatus;
	boost::recursive_mutex             m_strategyStatusLock;

	infrastructure* m_infra;
	Json::Value m_strategyConfig;
	athenathreadpoolPtr m_quoteTP;
	athenathreadpoolPtr m_tradeTP;
	
public:
	cmTestOrder01(string strategyId, string strategyTyp, string productId, string exchange,
		string quoteAdapterID, string tradeAdapterID,
		athenathreadpoolPtr quoteTP, athenathreadpoolPtr tradeTP, infrastructure* infra,
		Json::Value config);
	~cmTestOrder01();
	virtual void startStrategy();
	virtual void stopStrategy();
	
private:
	list< pair <int, int> > m_openTimeList;
	bool isInOpenTime();

public: //供外部调用的响应函数 | 在策略线程池中调用相应的处理函数
	void onRtnMD(futuresMDPtr);
	void onOrderRtn(orderRtnPtr pOrder){ m_tradeTP->getDispatcher().post(bind(&cmTestOrder01::processOrder, this, pOrder)); };
	void onTradeRtn(tradeRtnPtr ptrade){ m_tradeTP->getDispatcher().post(bind(&cmTestOrder01::processTrade, this, ptrade)); };
	void onRspCancel(cancelRtnPtr pCancel){ m_tradeTP->getDispatcher().post(bind(&cmTestOrder01::processCancelRes, this, pCancel)); };
	virtual void registerOrder(orderRtnPtr pOrder);

private:
	bool   m_isPriceReady;
	double m_upperLimit;
	double m_lowerLimit;

private:
	athena_lag_timer m_daemonTimer;
	void daemonEngine();
	void quoteEngine(futuresMDPtr pFuturesMD);

public:
	void sendUpperLmtOrder();
	void sendLowerLmtOrder();
	bool cancelOrder();

private:
	int m_orderRef;
	int m_cancelOrderRC;
	void processOrder(orderRtnPtr);
	void processTrade(tradeRtnPtr);
	void processCancelRes(cancelRtnPtr pCancel);

private:
	boost::shared_mutex     m_orderRtnBuffLock;
	map < int, orderRtnPtr> m_orderRef2orderRtn;  //orderRef -> orderRtn
};