#pragma once

#include <iostream>
#include "infrastructure.h"
#include "threadpool\threadpool.h"
#include "baseClass/strategyBase.h"
#include "baseClass/orderBase.h"
#include "baseClass/Utils.h"

using namespace std;

enum enum_cmMM01_strategy_status
{//策略状态，决定是否发送新的委托
	STRATEGY_STATUS_START,
	STRATEGY_STATUS_READY,
	STRATEGY_STATUS_ORDER_SENT,
	STRATEGY_STATUS_CLOSING_POSITION,
	STRATEGY_STATUS_TRADED_HEDGING,
	STRATEGY_STATUS_TRADED_NET_HEDGING,
	STRATEGY_STATUS_PAUSE,
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
	virtual void stopStrategy(){};

private:
	void resetStrategyStatus();

private:
	enum_cmMM01_strategy_status m_strategyStatus;
	boost::recursive_mutex      m_strategyStatusLock;

public: //供外部调用的响应函数 | 在策略线程池中调用相应的处理函数
	void onRtnMD(futuresMDPtr pFuturesMD)//行情响应函数: 更新最新行情，调用quoteEngine处理行情
	{
		{
			boost::mutex::scoped_lock lock(m_lastQuoteLock);
			m_lastQuotePtr = pFuturesMD;
		}
		m_quoteTP->getDispatcher().post(bind(&cmMM01::quoteEngine, this));
	};
	void onOrderRtn(orderRtnPtr pOrder){ m_tradeTP->getDispatcher().post(bind(&cmMM01::processOrder, this, pOrder)); };
	void onTradeRtn(tradeRtnPtr ptrade){ m_tradeTP->getDispatcher().post(bind(&cmMM01::processTrade, this, ptrade)); };
	void onHedgeOrderRtn(orderRtnPtr pOrder){ m_tradeTP->getDispatcher().post(bind(&cmMM01::processHedgeOrderRtn, this, pOrder)); };
	void onHedgeTradeRtn(tradeRtnPtr ptrade){ m_tradeTP->getDispatcher().post(bind(&cmMM01::processHedgeTradeRtn, this, ptrade)); };
	void onNetHedgeOrderRtn(orderRtnPtr pOrder){ m_tradeTP->getDispatcher().post(bind(&cmMM01::processNetHedgeOrderRtn, this, pOrder)); };
	void onNetHedgeTradeRtn(tradeRtnPtr ptrade){ m_tradeTP->getDispatcher().post(bind(&cmMM01::processNetHedgeTradeRtn, this, ptrade)); };
	void onRspCancel(cancelRtnPtr pCancel){ m_tradeTP->getDispatcher().post(bind(&cmMM01::processCancelRes, this, pCancel)); };

private:
	futuresMDPtr   m_lastQuotePtr;
	boost::mutex   m_lastQuoteLock;
	void quoteEngine();

private:
	int m_bidOrderRef;
	int m_askOrderRef;
	void startCycle();
	void refreshCycle();
	void orderPrice(double* bidprice, double* askprice); //计算挂单价格
	void processOrder(orderRtnPtr);
	void processTrade(tradeRtnPtr);

private:
	int m_cancelBidOrderRC;
	int m_cancelAskOrderRC;
	athena_lag_timer m_cancelConfirmTimer;
	bool m_cancelConfirmTimerCancelled;
	void CancelOrder(bool);// const boost::system::error_code& error);
	void processCancelRes(cancelRtnPtr);

private:

	void sendHedgeOrder(tradeRtnPtr); //同价格对冲
	void processHedgeOrderRtn(orderRtnPtr);
	void processHedgeTradeRtn(tradeRtnPtr);

	boost::shared_mutex m_hedgeOrderVolLock;
	map< int, double > m_hedgeOrderVol;
	map< int, int >    m_hedgeOrderCancelRC;
	athena_lag_timer   m_cancelHedgeTimer;
	bool m_cancelHedgeTimerCancelled;
	void cancelHedgeOrder();// const boost::system::error_code& error);
	void confirmCancel_hedgeOrder();
	
private:
	double        m_NetHedgeOrderVol;
	boost::mutex  m_NetHedgeOrderVolLock;
	void sendNetHedgeOrder(double);
	void processNetHedgeOrderRtn(orderRtnPtr);
	void processNetHedgeTradeRtn(tradeRtnPtr);

//for interrupt
private:
	bool                    m_pauseReq;
	boost::shared_mutex     m_pauseReqLock;
	boost::function<void()> m_oneTimeMMPausedHandler;
	void callPauseHandler();

public:
	bool pauseMM(boost::function<void()> pauseHandler);
	void resumeMM();

private: // for clear cycle
	int                     m_cycleId;
	map < int, list <int> > m_cycle2orderRef;     //cycle Id -> orderRef
	map < int, int >        m_orderRef2cycle;     //orderRef -> cycle Id
	boost::shared_mutex     m_orderRef2cycleRWlock; //用于m_orderRef2cycle的读写锁

	map < int, orderRtnPtr> m_orderRef2orderRtn;  //orderRef -> orderRtn
	boost::shared_mutex     m_orderRtnBuffLock;
	void checkCycleDone(list<int> & orderRefList);

};