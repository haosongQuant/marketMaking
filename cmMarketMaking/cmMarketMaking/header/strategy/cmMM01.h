#pragma once

#include <iostream>
#include "infrastructure.h"
#include "threadpool\threadpool.h"
#include "baseClass/strategyBase.h"
#include "baseClass/orderBase.h"

using namespace std;

enum enum_cmMM01_strategy_status
{//策略状态，决定是否发送新的委托
	STRATEGY_STATUS_START,
	STRATEGY_STATUS_READY,
	STRATEGY_STATUS_ORDER_SENT,
	STRATEGY_STATUS_CLOSING_POSITION,
	STRATEGY_STATUS_TRADED_HEDGING,
	STRATEGY_STATUS_TRADED_NET_HEDGING,
};

enum enum_cmMM01_strategy_order_type
{//报单类型
	MARKETMAKING_ORDER,
	HEDGE_ORDER,
	NET_HEDGE_ORDER,
};

class cmMM01 : public strategyBase
{
private:
	string m_strategyId;
	string m_strategyTyp;
	string m_productId;
	string m_exchange;
	string m_quoteAdapterID;
	string m_tradeAdapterID;
	double m_tickSize;
	double m_miniOrderSpread;
	double m_orderQty;

	infrastructure* m_infra;

	athenathreadpoolPtr m_quoteTP;
	athenathreadpoolPtr m_tradeTP;

public:
	cmMM01(string strategyId, string strategyTyp, string productId, string exchange, 
		string quoteAdapterID, string tradeAdapterID, double tickSize, double miniOrderSpread,
		double orderQty,
		athenathreadpoolPtr quoteTP, athenathreadpoolPtr tradeTP, infrastructure* infra);
	~cmMM01();
	virtual void startStrategy();
	virtual void stopAdapter();
	void orderPrice(double* bidprice, double* askprice);

private:
	void resetStrategyStatus();

private:
	enum_cmMM01_strategy_status m_strategyStatus;
	boost::mutex                m_strategyStatusLock;

	map<int, enum_cmMM01_strategy_order_type> m_orderTypMap;
	boost::mutex                              m_orderTypMapLock;

public:
	void onRtnMD(futuresMDPtr pFuturesMD);
	void onOrderRtn(orderRtnPtr);
	void onTradeRtn(tradeRtnPtr);
	void onHedgeOrderRtn(orderRtnPtr);
	void onHedgeTradeRtn(tradeRtnPtr);
	void onNetHedgeOrderRtn(orderRtnPtr);
	void onNetHedgeTradeRtn(tradeRtnPtr);
	void onRspCancel(cancelRtnPtr);

private:
	futuresMDPtr   m_lastQuotePtr;
	boost::mutex   m_lastQuoteLock;
	void quoteEngine();
	void sentOrder();

private:
	int m_bidOrderRef;
	int m_askOrderRef;

private:
	int m_cancelBidOrderRef;
	int m_cancelAskOrderRef;
	athena_lag_timer m_cancelConfirmTimer;
	map<int, bool>   m_isOrderCanceled;
	boost::mutex     m_isOrderCanceledLock;
	void confirmCancel_sendOrder();

private:
	map< int, double > m_hedgeOrderVol;
	boost::mutex	   m_hedgeOrderVolLock;
	athena_lag_timer   m_cancelHedgeTimer;
	void cancelHedgeOrder();
	void confirmCancel_hedgeOrder();
	
private:
	map< int, double > m_NetHedgeOrderVol;
	boost::mutex	   m_NetHedgeOrderVolLock;
};