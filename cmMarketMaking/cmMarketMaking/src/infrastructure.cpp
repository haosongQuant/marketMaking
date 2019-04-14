#include <time.h>
#include <fstream>
#include "infrastructure.h"


infrastructure::infrastructure(Json::Value config) :
m_config(config)
{
	m_quoteTP = athenathreadpoolPtr(new threadpool(4));
	m_tradeTP = athenathreadpoolPtr(new threadpool(6));
};

void infrastructure::init()
{
	cout << "initing infra..." << endl;
	time_t t;
	tm* local;
	t = time(NULL);
	local = localtime(&t);
	char currDate[9];
	memset(currDate, 0, sizeof(currDate));
	sprintf(currDate, "%04d%02d%02d", local->tm_year + 1900, local->tm_mon + 1, local->tm_mday);
	m_date = string(currDate);

	//loadAdapterConfig();
	genOrderParmMap();
	initAdapters();
};


void infrastructure::registerAdapterType(string adapterID, string adapterType)
{
	if ("tradeCtp" == adapterType)
		m_adapterTypeMap[adapterID] = ADAPTER_CTP_TRADE;
	else if ("quoteCtp" == adapterType)
		m_adapterTypeMap[adapterID] = ADAPTER_CTP_QUOTE;
	else if ("tradeTAP" == adapterType)
		m_adapterTypeMap[adapterID] = ADAPTER_TAP_TRADE;
	else if ("quoteTAP" == adapterType)
		m_adapterTypeMap[adapterID] = ADAPTER_TAP_QUOTE;
	else
		m_adapterTypeMap[adapterID] = ADAPTER_ERROR_TYP;
};
void infrastructure::initAdapters()
{

	int adapterNum = m_config["adapters"].size();
	for (int i = 0; i < adapterNum; ++i)
	{
		Json::Value adapterConfig = m_config["adapters"][i];
		string adapterID = adapterConfig["adapterID"].asString();
		string adapterType = adapterConfig["adapterType"].asString();
		registerAdapterType(adapterID, adapterType);

		cout << "initing " << adapterID << " ..." << endl;

		switch (m_adapterTypeMap[adapterID])
		{
		case ADAPTER_CTP_TRADE:
		{
			bool auth = adapterConfig["Authenticate"].asBool();
			tradeAdapterCTP * pTradeAdapter;
			if (auth)
				pTradeAdapter = new tradeAdapterCTP(adapterID,
				(char *)(adapterConfig["frontIP"].asString().c_str()),
				(char *)(adapterConfig["broker"].asString().c_str()),
				(char *)(adapterConfig["user"].asString().c_str()),
				(char *)(adapterConfig["pwd"].asString().c_str()),
				(char *)(adapterConfig["productID"].asString().c_str()),
				(char *)(adapterConfig["authCode"].asString().c_str()),
				m_tradeTP);
			else
				pTradeAdapter = new tradeAdapterCTP(adapterID,
				(char *)(adapterConfig["frontIP"].asString().c_str()),
				(char *)(adapterConfig["broker"].asString().c_str()),
				(char *)(adapterConfig["user"].asString().c_str()),
				(char *)(adapterConfig["pwd"].asString().c_str()),
				m_tradeTP);
			//pTradeAdapter->m_OnUserLogin = bind(&infrastructure::onAdapterLogin, this, _1);
			//pTradeAdapter->m_OnUserLogout = bind(&infrastructure::onAdapterLogout, this, _1);
			//pTradeAdapter->m_OnFrontDisconnected = bind(&infrastructure::onFrontDisconnected, this, _1, _2);
			pTradeAdapter->m_OnInstrumentsRtn = bind(&infrastructure::onRtnCtpInstruments, this, _1, _2);
			pTradeAdapter->m_OnOrderRtn = bind(&infrastructure::onRtnCtpOrder, this, _1, _2);
			pTradeAdapter->m_OnTradeRtn = bind(&infrastructure::onRtnCtpTrade, this, _1, _2);
			pTradeAdapter->m_onRspCancel = bind(&infrastructure::onRespCtpCancel, this, _1, _2, _3);
			pTradeAdapter->init();
			//m_tradeAdapters[adapterID] = pTradeAdapter;
			m_adapters[adapterID] = pTradeAdapter;
			break;
		}
		case ADAPTER_CTP_QUOTE:
		{
			quoteAdapter_CTP * pQuoteAdapter = new quoteAdapter_CTP(adapterID,
				(char *)(adapterConfig["frontIP"].asString().c_str()),
				(char *)(adapterConfig["broker"].asString().c_str()),
				(char *)(adapterConfig["user"].asString().c_str()),
				(char *)(adapterConfig["pwd"].asString().c_str()));
			pQuoteAdapter->m_onRtnMarketData = bind(&infrastructure::onRtnCtpQuote, this, _1, _2);
			//pQuoteAdapter->m_OnUserLogin = bind(&infrastructure::onAdapterLogin, this, _1);
			//pQuoteAdapter->m_OnFrontDisconnected = bind(&infrastructure::onFrontDisconnected, this, _1, _2);
			//pQuoteAdapter->m_OnUserLogout = bind(&infrastructure::onAdapterLogout, this, _1);
			pQuoteAdapter->init();
			//m_quoteAdapters[adapterID] = pQuoteAdapter;
			m_adapters[adapterID] = pQuoteAdapter;
			break;
		}
		case ADAPTER_TAP_TRADE:
		{
			tradeAdapter_TAP * pTradeAdapter = new tradeAdapter_TAP(adapterID,
				(char *)(adapterConfig["AuthCode"].asString().c_str()),
				(char *)(adapterConfig["LogPath"].asString().c_str()),
				(char *)(adapterConfig["IP"].asString().c_str()),
				adapterConfig["port"].asUInt(),
				(char *)(adapterConfig["user"].asString().c_str()),
				(char *)(adapterConfig["pwd"].asString().c_str()),
				m_tradeTP);
			pTradeAdapter->m_OnOrderRtn = bind(&infrastructure::onRtnTapOrder, this, _1, _2);
			pTradeAdapter->m_OnTradeRtn = bind(&infrastructure::onRtnTapTrade, this, _1, _2);
			pTradeAdapter->init();
			m_adapters[adapterID] = pTradeAdapter;
			break;
		}
		case ADAPTER_TAP_QUOTE:
		{
			quoteAdapter_TAP * pQuoteAdapter = new quoteAdapter_TAP(adapterID,
				(char *)(adapterConfig["AuthCode"].asString().c_str()),
				(char *)(adapterConfig["LogPath"].asString().c_str()),
				(char *)(adapterConfig["IP"].asString().c_str()),
				adapterConfig["port"].asUInt(),
				(char *)(adapterConfig["user"].asString().c_str()),
				(char *)(adapterConfig["pwd"].asString().c_str()));
			pQuoteAdapter->m_onRtnMarketData = bind(&infrastructure::onRtnTapQuote, this, _1, _2);
			pQuoteAdapter->init();
			m_adapters[adapterID] = pQuoteAdapter;
			break;
		}
		}
	}
}

void infrastructure::genOrderParmMap()
{
	//委托类型: 限价单、市价单
	m_orderTypeMap[ADAPTER_CTP_TRADE][ORDER_TYPE_MARKET] = THOST_FTDC_OPT_AnyPrice;
	m_orderTypeMap[ADAPTER_CTP_TRADE][ORDER_TYPE_LIMIT] = THOST_FTDC_OPT_LimitPrice;
	m_orderTypeMap[ADAPTER_TAP_TRADE][ORDER_TYPE_MARKET] = TAPI_ORDER_TYPE_MARKET;
	m_orderTypeMap[ADAPTER_TAP_TRADE][ORDER_TYPE_LIMIT] = TAPI_ORDER_TYPE_LIMIT;

	//委托方向: 买、卖
	m_orderDirMap[ADAPTER_CTP_TRADE][ORDER_DIR_BUY] = THOST_FTDC_D_Buy;
	m_orderDirMap[ADAPTER_CTP_TRADE][ORDER_DIR_SELL] = THOST_FTDC_D_Sell;
	m_orderDirMap[ADAPTER_TAP_TRADE][ORDER_DIR_BUY] = TAPI_SIDE_BUY;
	m_orderDirMap[ADAPTER_TAP_TRADE][ORDER_DIR_SELL] = TAPI_SIDE_SELL;
	m_orderDirMapRev[ADAPTER_CTP_TRADE][THOST_FTDC_D_Buy] = ORDER_DIR_BUY;
	m_orderDirMapRev[ADAPTER_CTP_TRADE][THOST_FTDC_D_Sell] = ORDER_DIR_SELL;

	//开平标志
	m_positinEffectMap[ADAPTER_CTP_TRADE][POSITION_EFFECT_OPEN] = THOST_FTDC_OFEN_Open;
	m_positinEffectMap[ADAPTER_CTP_TRADE][POSITION_EFFECT_CLOSE] = THOST_FTDC_OFEN_Close;
	m_positinEffectMap[ADAPTER_TAP_TRADE][POSITION_EFFECT_OPEN] = TAPI_PositionEffect_OPEN;
	m_positinEffectMap[ADAPTER_TAP_TRADE][POSITION_EFFECT_CLOSE] = TAPI_PositionEffect_COVER;

	//报单状态
	m_orderStatusMapRev[ADAPTER_CTP_TRADE][THOST_FTDC_OST_AllTraded] = ORDER_STATUS_AllTraded;
	m_orderStatusMapRev[ADAPTER_CTP_TRADE][THOST_FTDC_OST_PartTradedQueueing] = ORDER_STATUS_PartTradedQueueing;
	m_orderStatusMapRev[ADAPTER_CTP_TRADE][THOST_FTDC_OST_PartTradedNotQueueing] = ORDER_STATUS_PartTradedNotQueueing;
	m_orderStatusMapRev[ADAPTER_CTP_TRADE][THOST_FTDC_OST_NoTradeQueueing] = ORDER_STATUS_NoTradeQueueing;
	m_orderStatusMapRev[ADAPTER_CTP_TRADE][THOST_FTDC_OST_NoTradeNotQueueing] = ORDER_STATUS_NoTradeNotQueueing;
	m_orderStatusMapRev[ADAPTER_CTP_TRADE][THOST_FTDC_OST_Canceled] = ORDER_STATUS_Canceled;
	m_orderStatusMapRev[ADAPTER_CTP_TRADE][THOST_FTDC_OST_Unknown] = ORDER_STATUS_Unknown;
	m_orderStatusMapRev[ADAPTER_CTP_TRADE][THOST_FTDC_OST_NotTouched] = ORDER_STATUS_NotTouched;
	m_orderStatusMapRev[ADAPTER_CTP_TRADE][THOST_FTDC_OST_Touched] = ORDER_STATUS_Touched;

	//投机套保标志
	m_hedgeFlagMap[ADAPTER_CTP_TRADE][FLAG_SPECULATION] = THOST_FTDC_HF_Speculation;
	m_hedgeFlagMap[ADAPTER_CTP_TRADE][FLAG_MARKETMAKER] = THOST_FTDC_HF_MarketMaker;

};
