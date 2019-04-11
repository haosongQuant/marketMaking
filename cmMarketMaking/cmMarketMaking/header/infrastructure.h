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

struct futuresContractDetail
{
public:
	///合约代码
	string m_code;
	///交易所代码
	string m_exchange;
	///合约名称
	string m_InstrumentName;
	///合约在交易所的代码
	string m_ExchangeInstID;
	///产品代码
	string m_ProductID;
	///产品类型
	char m_ProductClass;
	///交割年份
	int m_DeliveryYear;
	///交割月
	int m_DeliveryMonth;
	///市价单最大下单量
	int m_MaxMarketOrderVolume;
	///市价单最小下单量
	int m_MinMarketOrderVolume;
	///限价单最大下单量
	int m_MaxLimitOrderVolume;
	///限价单最小下单量
	int m_MinLimitOrderVolume;
	///合约数量乘数
	int m_VolumeMultiple;
	///最小变动价位
	double m_PriceTick;
	///创建日
	string m_CreateDate;
	///上市日
	string m_OpenDate;
	///到期日
	string m_ExpireDate;
	///开始交割日
	string m_StartDelivDate;
	///结束交割日
	string m_EndDelivDate;
	///合约生命周期状态
	char m_InstLifePhase;
	///当前是否交易
	int m_IsTrading;
	///持仓类型
	char m_PositionType;
	///持仓日期类型
	char m_PositionDateType;
	///多头保证金率
	double m_LongMarginRatio;
	///空头保证金率
	double m_ShortMarginRatio;
	///是否使用大额单边保证金算法
	char m_MaxMarginSideAlgorithm;
	///基础商品代码
	string m_UnderlyingInstrID;
	///执行价
	double m_StrikePrice;
	///合约基础商品乘数
	double m_UnderlyingMultiple;
	///组合类型
	char m_CombinationType;

public:
	futuresContractDetail(){};

	futuresContractDetail(CThostFtdcInstrumentField* inst):m_code(string(inst->InstrumentID)), m_exchange(string(inst->ExchangeID)),
		m_InstrumentName(string(inst->InstrumentName)), m_ExchangeInstID(string(inst->ExchangeInstID)),
		m_ProductID(string(inst->ProductID)), m_ProductClass(inst->ProductClass), m_DeliveryYear(inst->DeliveryYear),
		m_DeliveryMonth(inst->DeliveryMonth), m_MaxMarketOrderVolume(inst->MaxMarketOrderVolume),
		m_MinMarketOrderVolume(inst->MinMarketOrderVolume), m_MaxLimitOrderVolume(inst->MaxLimitOrderVolume),
		m_MinLimitOrderVolume(inst->MinLimitOrderVolume), m_VolumeMultiple(inst->VolumeMultiple), m_PriceTick(inst->PriceTick),
		m_CreateDate(string(inst->CreateDate)), m_OpenDate(string(inst->OpenDate)),	m_ExpireDate(string(inst->ExpireDate)),
		m_StartDelivDate(string(inst->StartDelivDate)), m_EndDelivDate(string(inst->EndDelivDate)), m_InstLifePhase(inst->InstLifePhase),
		m_IsTrading(inst->IsTrading), m_PositionType(inst->PositionType), m_PositionDateType(inst->PositionDateType),
		m_LongMarginRatio(inst->LongMarginRatio), m_ShortMarginRatio(inst->ShortMarginRatio),
		m_MaxMarginSideAlgorithm(inst->MaxMarginSideAlgorithm), m_UnderlyingInstrID(string(inst->UnderlyingInstrID)),
		m_StrikePrice(inst->StrikePrice), m_UnderlyingMultiple(inst->UnderlyingMultiple), m_CombinationType(inst->CombinationType)
	{};

	friend ostream& operator<<(ostream& out, const futuresContractDetail& s)
	{
		out << "合约代码: " << s.m_code << endl	<< "交易所代码: " << s.m_exchange << endl	<< "合约名称: " << s.m_InstrumentName << endl
			<< "合约在交易所的代码: " << s.m_ExchangeInstID << endl	<< "产品代码: " << s.m_ProductID << endl
			<< "产品类型: " << s.m_ProductClass << endl	<< "交割年份: " << s.m_DeliveryYear << endl	<< "交割月: " << s.m_DeliveryMonth << endl
			<< "市价单最大下单量: " << s.m_MaxMarketOrderVolume << endl	<< "市价单最小下单量: " << s.m_MinMarketOrderVolume << endl
			<< "限价单最大下单量: " << s.m_MaxLimitOrderVolume << endl	<< "限价单最小下单量: " << s.m_MinLimitOrderVolume << endl
			<< "合约数量乘数: " << s.m_VolumeMultiple << endl	<< "最小变动价位: " << s.m_PriceTick << endl
			<< "创建日: " << s.m_CreateDate << endl	<< "上市日: " << s.m_OpenDate << endl	<< "到期日: " << s.m_ExpireDate << endl
			<< "开始交割日: " << s.m_StartDelivDate << endl	<< "结束交割日: " << s.m_EndDelivDate << endl	<< "合约生命周期状态: " << s.m_InstLifePhase << endl
			<< "当前是否交易: " << s.m_IsTrading << endl	<< "持仓类型: " << s.m_PositionType << endl	<< "持仓日期类型: " << s.m_PositionDateType << endl
			<< "多头保证金率: " << s.m_LongMarginRatio << endl	<< "空头保证金率: " << s.m_ShortMarginRatio << endl
			<< "是否使用大额单边保证金算法: " << s.m_MaxMarginSideAlgorithm << endl	<< "基础商品代码: " << s.m_UnderlyingInstrID << endl
			<< "执行价: " << s.m_StrikePrice << endl << "合约基础商品乘数: " << s.m_UnderlyingMultiple << endl
			<< "组合类型: " << s.m_CombinationType << endl;
		return out;
	}

};
typedef boost::shared_ptr<futuresContractDetail> futuresContractDetailPtr;

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
	//bool isInfrastructureReady();

	//for adapter logout or disconnection.
private:
	//boost::unordered_map<string, list<boost::function<void(string, bool)> > > m_adapterMonitorList;
	//boost::mutex m_adapterMonitorListLock;
	//void callAdapterMonitor(string adapterID, bool isAdapterAlive);
	//bool isAdapterReady(string adapterID);
public:
	//bool registerAdapterMonitor(string adapterID, boost::function<void(string, bool)> adapterMonitor);

	//合约
private: //futures contract
	//map<string,  // adapterID
	//	map<string, futuresContractDetailPtr> > m_futuresContracts0; // contract code -> contract detail
	//boost::unordered_map<string,        //adapterID
	//	boost::unordered_map<string,    //exchange
	//	boost::unordered_map<string,    //productID
	//	boost::unordered_map<string, futuresContractDetailPtr> > > > m_futuresContracts1;
	//map<string,        //adapterID
	//	map<string,    //exchange
	//	map<string,    //productID
	//	map<string, futuresContractDetailPtr> > > > m_futuresContracts1; // contract code -> contract detail

public:
	void onRtnCtpInstruments(string adapterID, CThostFtdcInstrumentField* inst);
	void onRtnCtpOrder(string adapterID, CThostFtdcOrderField *pOrder);
	void onRtnCtpTrade(string adapterID, CThostFtdcTradeField *pTrade);
	void onRtnTapOrder(string adapterID, TapAPIOrderInfoNotice *pOrder);
	void onRtnTapTrade(string adapterID, TapAPIFillInfo *pTrade);
/*	map<string, map<string, futuresContractDetailPtr> > & getFuturesContracts(){ return m_futuresContracts0; };
	map<string,map<string,map<string,
		map<string, futuresContractDetailPtr> > > >& getFuturesContracts1(){ return m_futuresContracts1; }*/;

private: // adapter

	map<string, enum_adapterType> m_adapterTypeMap; // adapterID -> adapterType
	void registerAdapterType(string, string);
	//map<string, queryAdapterCfgStru> m_queryAdapterCfg;  // adapterID -> config
	//map<string, quoteAdapterCfgStru> m_quoteAdapterCfg;  // adapterID -> config

	boost::unordered_map<string, adapterBase*> m_adapters; // adapterID -> adapterBase*
	//boost::unordered_map<string, quoteAdapterBase*> m_quoteAdapters; // adapterID -> quoteAdapterBase*
	//boost::unordered_map<string, traderAdapterBase*> m_tradeAdapters; // adapterID-> tradeAdapterBase*
	//map<string, adapter_status> m_adapterStatus; //adapterID->adapter status
	//boost::mutex m_adapterStatusLock;

private:
	//void loadAdapterConfig();
	//void createTradeAdapter(string adapterID, string tradeFront, string broker, string user,
	//	string pwd, string userproductID, string authenticateCode, athenathreadpoolPtr tp);
	//void createQuoteAdapter(string adapterID, string mdFront, string broker, string user, string pwd);
	//void deleteTradeAdapter(string adapterID, bool reCreate);
	//void deleteQuoteAdapter(string adapterID, bool reCreate);

public:
	void onAdapterLogin(string adapterID);
	void onAdapterLogout(string adapterID);
	void onFrontDisconnected(string adapterID, string adapterType);
	void deleteAdapter(string adapterID, string adapterType, bool reCreate);

	//行情
private:
	// quoteAdapterID -> list of <exchange, code>
	//map <string, list<pair<string, string> > >  m_delayedSubscribeInstruments;  
	//boost::mutex m_delayedSubscribeInstrumentsLock;
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
	void onRespCtpCancel(string adapterID, CThostFtdcInputOrderActionField *pInputOrderAction, 
		CThostFtdcRspInfoField *pRspInfo);

private:
	map<enum_adapterType, map<enum_order_type, char> > m_orderTypeMap;
	map<enum_adapterType, map<enum_order_dir_type, char> > m_orderDirMap;
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
