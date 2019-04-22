#pragma once
#include <list>
#include <iostream>
#include "infrastructure.h"
#include "threadpool\threadpool.h"
#include "baseClass/strategyBase.h"
#include "baseClass/orderBase.h"
#include "baseClass/Utils.h"
#include <boost\circular_buffer.hpp>

using namespace std;

#define apcosm_Buff_size 100

enum enum_cmSepc01_strategy_status
{//策略状态，决定是否发送新的委托
	STRATEGY_STATUS_INIT,
	STRATEGY_STATUS_START,
	STRATEGY_STATUS_STOP,
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

	list< pair <int, int> > m_openTimeList;
	bool isInOpenTime();

private:
	enum_cmSepc01_strategy_status m_strategyStatus;

public:
	cmSepc01(string strategyId, string strategyTyp, string productId, string exchange,
		string quoteAdapterID, string tradeAdapterID, double tickSize, 
		double orderQty, int volMulti,
		athenathreadpoolPtr quoteTP, athenathreadpoolPtr tradeTP, infrastructure* infra,
		Json::Value config);
	~cmSepc01();
	virtual void startStrategy();
	virtual void stopStrategy(){};


public: //供外部调用的响应函数 | 在策略线程池中调用相应的处理函数
	void onRtnMD(futuresMDPtr);
	void onOrderRtn(orderRtnPtr pOrder){ m_tradeTP->getDispatcher().post(bind(&cmSepc01::processOrder, this, pOrder)); };
	void onTradeRtn(tradeRtnPtr ptrade){ m_tradeTP->getDispatcher().post(bind(&cmSepc01::processTrade, this, ptrade)); };
	void onHedgeOrderRtn(orderRtnPtr pOrder){ m_tradeTP->getDispatcher().post(bind(&cmSepc01::processHedgeOrderRtn, this, pOrder)); };
	void onHedgeTradeRtn(tradeRtnPtr ptrade){ m_tradeTP->getDispatcher().post(bind(&cmSepc01::processHedgeTradeRtn, this, ptrade)); };
	void onNetHedgeOrderRtn(orderRtnPtr pOrder){ m_tradeTP->getDispatcher().post(bind(&cmSepc01::processNetHedgeOrderRtn, this, pOrder)); };
	void onNetHedgeTradeRtn(tradeRtnPtr ptrade){ m_tradeTP->getDispatcher().post(bind(&cmSepc01::processNetHedgeTradeRtn, this, ptrade)); };
	void onCycleNetHedgeOrderRtn(orderRtnPtr pOrder){ m_tradeTP->getDispatcher().post(bind(&cmSepc01::processCycleNetHedgeOrderRtn, this, pOrder)); };
	void onCycleNetHedgeTradeRtn(tradeRtnPtr ptrade){ m_tradeTP->getDispatcher().post(bind(&cmSepc01::processCycleNetHedgeTradeRtn, this, ptrade)); };
	void onRspCancel(cancelRtnPtr pCancel){ m_tradeTP->getDispatcher().post(bind(&cmSepc01::processCancelRes, this, pCancel)); };

private:
	double m_upline;
	double m_downline;
	double m_lastprice;
	double m_lastprice_1;
	cirBuff m_yieldBuff_short{ 10 };
	cirBuff m_yieldBuff_long{ 10 * 20 };
	cirBuff m_avg_true_range{ 10 * 20 };
	cirBuff m_Apcosm_Buff{ apcosm_Buff_size };
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
	void CancelOrder(bool);// const boost::system::error_code& error);
	void processCancelRes(cancelRtnPtr);

private:

	void sendHedgeOrder(tradeRtnPtr); //同价格对冲
	void processHedgeOrderRtn(orderRtnPtr);
	void processHedgeTradeRtn(tradeRtnPtr);

	boost::shared_mutex m_hedgeOrderVolLock;
	map< int, double > m_hedgeOrderVol;
	map< int, int >    m_hedgeOrderCancelRC;
	void cancelHedgeOrder();// const boost::system::error_code& error);
	void confirmCancel_hedgeOrder();
	
private:
	double        m_NetHedgeOrderVol;
	boost::mutex  m_NetHedgeOrderVolLock;
	void sendNetHedgeOrder(double);
	void processNetHedgeOrderRtn(orderRtnPtr);
	void processNetHedgeTradeRtn(tradeRtnPtr);
	
private:
		map < int, orderRtnPtr> m_orderRef2orderRtn;  //orderRef -> orderRtn
		boost::shared_mutex     m_orderRtnBuffLock;

		map < int, map<string, tradeRtnPtr> > m_orderRef2tradeRtn;  //orderRef -> tradeRtn
		boost::shared_mutex     m_tradeRtnBuffLock;
		void registerTradeRtn(tradeRtnPtr pTrade){
			if (pTrade)
			{
				write_lock lock(m_tradeRtnBuffLock);
				m_orderRef2tradeRtn[pTrade->m_orderRef][pTrade->m_tradeId] = pTrade;
			}
		};

public:
	void interruptMM(boost::function<void()> pauseHandler);
	bool pauseMM(boost::function<void()> pauseHandler);
	void resumeMM();

private: // for clear cycle
	tradeGroupBufferPtr     m_ptradeGrp; //用于记录单个交易闭环的所有报单号
	map < int, int >        m_orderRef2cycle;     //orderRef -> cycle Id
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
	bool isOrderComplete(int orderRef, int& tradedVol);
	void sendCycleNetHedgeOrder();
	void sendCycleNetHedgeOrder(int);
	void processCycleNetHedgeOrderRtn(orderRtnPtr);
	void processCycleNetHedgeTradeRtn(tradeRtnPtr);
	athena_lag_timer m_daemonTimer;
	double        m_cycleNetHedgeVol;
	boost::mutex  m_cycleNetHedgeVolLock;
};