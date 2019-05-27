#pragma once
#include <list>
#include <iostream>
#include "infrastructure.h"
#include "threadpool\threadpool.h"
#include "baseClass/strategyBase.h"
#include "baseClass/orderBase.h"
#include "baseClass/Utils.h"
#include "baseClass/UTC.h"
#include <boost/date_time.hpp>

using namespace std;
using namespace boost::posix_time;
using namespace athenaUTC;

enum enum_cmMM02_strategy_status
{//策略状态，决定是否发送新的委托
	cmMM02_STATUS_INIT,
	cmMM02_STATUS_READY,
	cmMM02_STATUS_ORDER_SENT,
	cmMM02_STATUS_CLOSING_POSITION,
	cmMM02_STATUS_TRADED_HEDGING,
	cmMM02_STATUS_TRADED_NET_HEDGING,
	cmMM02_STATUS_PAUSE,
	cmMM02_STATUS_BREAK
};

enum enum_cmMM02_strategy_order_type
{//报单类型
	cmMM02_MARKETMAKING_ORDER,
	cmMM02_HEDGE_ORDER,
	cmMM02_NET_HEDGE_ORDER,
};

class cmMM02 : public strategyBase
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
	double m_spotOrderSpread;
	double m_maxiOrderSpread;
	double m_orderQty;
	int    m_volumeMultiple;
	int    m_holdingRequirement;

	infrastructure* m_infra;
	Json::Value m_strategyConfig;
	athenathreadpoolPtr m_quoteTP;
	athenathreadpoolPtr m_tradeTP;

	list< pair <int, int> > m_openTimeList;
	bool isInOpenTime();

public:
	cmMM02(string strategyId, string strategyTyp, string productId, string exchange, 
		string quoteAdapterID, string tradeAdapterID, double tickSize, double miniOrderSpread,
		double maxiOrderSpread, double orderQty, int volMulti, int holdingRequirement,
		athenathreadpoolPtr quoteTP, athenathreadpoolPtr tradeTP, infrastructure* infra,
		Json::Value config);
	~cmMM02();
	virtual void startStrategy();
	virtual void stopStrategy(){};

private:
	void resetStrategyStatus();

private:
	enum_cmMM02_strategy_status m_strategyStatus;
	boost::recursive_mutex      m_strategyStatusLock;

public: //供外部调用的响应函数 | 在策略线程池中调用相应的处理函数
	void onRtnMD(futuresMDPtr pFuturesMD)//行情响应函数: 更新最新行情，调用quoteEngine处理行情
	{
		{
			boost::mutex::scoped_lock lock(m_lastQuoteLock);
			if (m_lastQuotePtr)
				m_lastPrz_1 = m_lastQuotePtr->LastPrice;
			m_lastQuotePtr = pFuturesMD;
		}
		//m_quoteTP->getDispatcher().post(bind(&cmMM02::quoteEngine, this));
		quoteEngine();
	};
	void onOrderRtn(orderRtnPtr pOrder){ m_tradeTP->getDispatcher().post(bind(&cmMM02::processOrder, this, pOrder)); };
	void onTradeRtn(tradeRtnPtr ptrade){ m_tradeTP->getDispatcher().post(bind(&cmMM02::processTrade, this, ptrade)); };
	void onHedgeOrderRtn(orderRtnPtr pOrder){ m_tradeTP->getDispatcher().post(bind(&cmMM02::processHedgeOrderRtn, this, pOrder)); };
	void onHedgeTradeRtn(tradeRtnPtr ptrade){ m_tradeTP->getDispatcher().post(bind(&cmMM02::processHedgeTradeRtn, this, ptrade)); };
	void onNetHedgeOrderRtn(orderRtnPtr pOrder){ m_tradeTP->getDispatcher().post(bind(&cmMM02::processNetHedgeOrderRtn, this, pOrder)); };
	void onNetHedgeTradeRtn(tradeRtnPtr ptrade){ m_tradeTP->getDispatcher().post(bind(&cmMM02::processNetHedgeTradeRtn, this, ptrade)); };
	void onCycleNetHedgeOrderRtn(orderRtnPtr pOrder){ m_tradeTP->getDispatcher().post(bind(&cmMM02::processCycleNetHedgeOrderRtn, this, pOrder)); };
	void onCycleNetHedgeTradeRtn(tradeRtnPtr ptrade){ m_tradeTP->getDispatcher().post(bind(&cmMM02::processCycleNetHedgeTradeRtn, this, ptrade)); };
	void onRspCancel(cancelRtnPtr pCancel){ m_tradeTP->getDispatcher().post(bind(&cmMM02::processCancelRes, this, pCancel)); };
	virtual void registerOrder(orderRtnPtr pOrder);

private:
	double         m_lastPrz_1 = 0.0;
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
	void logTrade(tradeRtnPtr);

private:
	int m_cancelBidOrderRC;
	int m_cancelAskOrderRC;
	athena_lag_timer m_orderCheckTimer;
	bool m_orderCheckTimerCancelled;
	void CancelOrder(bool, bool);// const boost::system::error_code& error);
	void confirmCancel(bool);// const boost::system::error_code& error);
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

//for record position
private:
	bool m_isInvestorPositionReady = false;
	bool m_isHoldingRequireFilled = false;
	list<investorPositionPtr> m_initPosition;
	boost::shared_mutex m_investorPositionLock;
	//instrument->direction->position
	map<string, map<enum_holding_dir_type, investorPositionPtr> > m_investorPosition;

//for interrupt
private:
	bool                    m_breakReq;
	boost::shared_mutex     m_breakReqLock;
	bool                    m_pauseReq;
	boost::shared_mutex     m_pauseReqLock;
	boost::function<void()> m_oneTimeMMPausedHandler;
	void callPauseHandler();

private:
		boost::shared_mutex     m_orderRtnBuffLock;
		map < int, orderRtnPtr> m_orderRef2orderRtn;  //orderRef -> orderRtn

		map < int, map<string, tradeRtnPtr> > m_orderRef2tradeRtn;  //orderRef -> tradeRtn
		boost::shared_mutex     m_tradeRtnBuffLock;
		void registerTradeRtn(tradeRtnPtr pTrade);

public:

	virtual enum_strategy_interrupt_result tryInterrupt(boost::function<void()> pauseHandler);
	virtual void interrupt(boost::function<void()> pauseHandler);
	virtual bool pause(boost::function<void()> pauseHandler);
	virtual void resume();

private: // for clear cycle
	int                     m_cycleId;
	tradeGroupBufferPtr     m_ptradeGrp; //用于记录单个交易闭环的所有报单号
	map < int, tradeGroupBufferPtr >  m_orderRef2cycle;     //orderRef -> cycle Id
	boost::shared_mutex     m_orderRef2cycleRWlock; //用于m_orderRef2cycle的读写锁

	list<tradeGroupBufferPtr>      m_tradeGrpBuffer;//用于每个交易闭环清理现场
	list<tradeGroupBufferPtr>      m_aliveTrdGrp;//用于每个交易闭环清理现场
	map <int, tradeGroupBufferPtr> m_cycle2tradeGrp;//cycle Id -> trade group pointer
	boost::mutex            m_cycle2tradeGrpLock;   //用于互斥访问m_cycle2tradeGrp
	void registerTrdGrpMap(int cycleId, tradeGroupBufferPtr pGrp){ //向m_cycle2tradeGrp中插入记录
		boost::mutex::scoped_lock lock(m_cycle2tradeGrpLock);
		m_cycle2tradeGrp[cycleId] = pGrp;
	};

	int  m_cycleHedgeVol;
	void daemonEngine(); //守护线程引擎
	bool isOrderComplete(int orderRef, bool &isTraded);
	void sendCycleNetHedgeOrder();
	void sendCycleNetHedgeOrder(int);
	void processCycleNetHedgeOrderRtn(orderRtnPtr);
	void processCycleNetHedgeTradeRtn(tradeRtnPtr);
	athena_lag_timer m_daemonTimer;
	athena_lag_timer m_pauseLagTimer;
	double        m_cycleNetHedgeVol;
	boost::mutex  m_cycleNetHedgeVolLock;
};