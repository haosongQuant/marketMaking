#pragma once
#include <vector>
#include <hash_map>
#include <list>
#include <iostream>
#include <boost\shared_ptr.hpp>
#include <boost\unordered_map.hpp>
#include <boost\bind.hpp>
#include <boost\thread\mutex.hpp>
#include <boost\smart_ptr\detail\spinlock.hpp>
#include "infrastructureStruct.h"
#include "baseClass\orderBase.h"
#include "baseClass\adapterBase.h"
#include "threadpool\threadpool.h"
#include "ctp\tradeAdapter_CTP.h"
#include "ctp\quoteAdapter_CTP.h"
#include "tap\tradeAdapter_TAP.h"
#include "tap\quoteAdapter_TAP.h"
#include "json\json.h"
//#include "adapterConfig.h"

using namespace std;

class infrastructure
{
private:
	string      m_date;
	Json::Value m_config;
	athenathreadpoolPtr m_quoteTP;
	athenathreadpoolPtr m_tradeTP;

public:
	infrastructure(Json::Value config);
	void init();

public:
	void queryOrder(string adapterID);
	void queryOrder(string adapterID, int orderRef);

public:
	void onRtnCtpInstruments(string adapterID, CThostFtdcInstrumentField* inst);
	void onRtnCtpOrder(string adapterID, CThostFtdcOrderFieldPtr pOrder);
	void onRtnCtpTrade(string adapterID, CThostFtdcTradeField *pTrade);
	void onRtnCTPOrderActionErr(string adapterID, CThostFtdcOrderActionField *pOrderAction,
		CThostFtdcRspInfoField *pRspInfo);
	void onRespCtpCancel(string adapterID, CThostFtdcInputOrderActionField *pInputOrderAction,
		CThostFtdcRspInfoField *pRspInfo); // not used right now
	void onRtnTapOrder(string adapterID, TapAPIOrderInfoNotice *pOrder);
	void onRtnTapTrade(string adapterID, TapAPIFillInfo *pTrade);

private: // adapter

	map<string, enum_adapterType> m_adapterTypeMap; // adapterID -> adapterType
	void registerAdapterType(string, string);

	boost::unordered_map<string, adapterBase*> m_adapters;
	map<string, bool> m_isAdapterReady;
public:
	void onAdapterLogin(string adapterID);
	void onAdapterLogout(string adapterID);
	void onFrontDisconnected(string adapterID);
	void deleteAdapter(string adapterID, string adapterType, bool reCreate);
	bool isAdapterReady(string adapterID);

	//行情
private:
	map < string, //exchange
		map < string, //instrument
		enum_productCategory > > m_productCategory;
	map< string, //adapterID
		//map<string, exchange
		map<string, //instrumentID
		list<boost::function<void(futuresMDPtr)> > > >  m_futuresMDHandler;
	boost::mutex m_futuresMDHandlerLock;
	void registerFuturesQuoteHandler(string adapterID, string exchange, string instList, boost::function<void(futuresMDPtr)> handler);
	void onFuturesTick(string adapterID, futuresMDPtr pQuote);

	//list<boost::function<void(quoteDetailPtr)> > m_AllInstrHandler;
	//boost::mutex m_AllInstrHandlerLock;

public:
	void subscribeFutures(string adapterID, string exchange, string instList, boost::function<void(futuresMDPtr)> handler);
	//void registerAllInstrHandler(boost::function<void(quoteDetailPtr)> handler);
	void onRtnCtpQuote(string adapterID, CThostFtdcDepthMarketDataField*); // call onCtpTick
	void onRtnTapQuote(string adapterID, TapAPIQuoteWhole *);              // call onCtpTick

	//下单
public:
	int insertOrder(string adapterID, string instrument, string exchange, enum_order_type orderType, 
		enum_order_dir_type dir, enum_position_effect_type positionEffect, enum_hedge_flag hedgeflag,
		double price, unsigned int volume,
		boost::function<void(orderRtnPtr)> orderRtnhandler, boost::function<void(tradeRtnPtr)> tradeRtnhandler);
	int cancelOrder(string adapterID, int orderRef, boost::function<void(cancelRtnPtr)> cancelRtnhandler);

private:
	map<enum_adapterType, map<enum_order_type, char> > m_orderTypeMap;
	map<enum_adapterType, map<enum_order_dir_type, char> > m_orderDirMap;
	map<enum_adapterType, map<char, enum_order_dir_type> > m_orderDirMapRev;
	map<enum_adapterType, map<char, enum_order_status> > m_orderStatusMapRev;
	map<enum_adapterType, map<enum_position_effect_type, char> > m_positinEffectMap;
	map<enum_adapterType, map<enum_hedge_flag, char> > m_hedgeFlagMap;
	void genOrderParmMap();

	map<string, //adapterID
		map<int, boost::function<void(orderRtnPtr)> > > m_orderRtnHandlers;
	map<string, //adapterID
		map< int, boost::function<void(tradeRtnPtr)> > > m_tradeRtnHandlers;
	map < string, //adapterID
		map<int, boost::function<void(cancelRtnPtr)> > > m_cancelRtnHandlers;

private:
	void initAdapters();
};
