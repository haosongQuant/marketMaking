#include <time.h>
#include <fstream>
#include "infrastructure.h"


infrastructure::infrastructure(Json::Value config) :
m_config(config)
{
	m_quoteTP = athenathreadpoolPtr(new threadpool(4));
	m_tradeTP = athenathreadpoolPtr(new threadpool(4));
};

void infrastructure::init()
{
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


//
//void infrastructure::loadAdapterConfig()
//{
//	int adapterNum = m_config["tradeAdapters"].size();
//	for (int i = 0; i < adapterNum; ++i)
//	{
//		string adapterID;
//		tradeAdapterCfgStru tradeAdapter;
//		Json::Value adapterConfig = m_config["tradeAdapters"][i];
//		adapterID = adapterConfig["adapterID"].asString();
//		tradeAdapter.m_adapterID = adapterID;
//		tradeAdapter.m_frontIP = adapterConfig["frontIP"].asString();
//		tradeAdapter.m_broker = adapterConfig["broker"].asString();
//		tradeAdapter.m_user = adapterConfig["user"].asString();
//		tradeAdapter.m_pwd = adapterConfig["pwd"].asString();
//		tradeAdapter.m_Authenticate = adapterConfig["Authenticate"].asBool();
//		tradeAdapter.m_productID = adapterConfig["productID"].asString();
//		tradeAdapter.m_authCode = adapterConfig["authCode"].asString();
//		m_tradeAdapterCfg[adapterID] = tradeAdapter;
//		Json::Value openConfig = adapterConfig["openTime"];
//		loadOpenTime(adapterID, "trade", openConfig);
//	}
//	adapterNum = m_config["queryAdapters"].size();
//	for (int i = 0; i < adapterNum; ++i)
//	{
//		string adapterID;
//		queryAdapterCfgStru queryAdapter;
//		Json::Value adapterConfig = m_config["queryAdapters"][i];
//		adapterID = adapterConfig["adapterID"].asString();
//		queryAdapter.m_adapterID = adapterID;
//		queryAdapter.m_frontIP = adapterConfig["frontIP"].asString();
//		queryAdapter.m_broker = adapterConfig["broker"].asString();
//		queryAdapter.m_user = adapterConfig["user"].asString();
//		queryAdapter.m_pwd = adapterConfig["pwd"].asString();
//		queryAdapter.m_productID = adapterConfig["productID"].asString();
//		queryAdapter.m_authCode = adapterConfig["authCode"].asString();
//		m_queryAdapterCfg[adapterID] = queryAdapter;
//		Json::Value openConfig = adapterConfig["openTime"];
//		loadOpenTime(adapterID, "query", openConfig);
//	}
//	adapterNum = m_config["quoteAdapters"].size();
//	for (int i = 0; i < adapterNum; ++i)
//	{
//		string adapterID;
//		quoteAdapterCfgStru quoteAdapter;
//		Json::Value adapterConfig = m_config["quoteAdapters"][i];
//		adapterID = adapterConfig["adapterID"].asString();
//		quoteAdapter.m_adapterID = adapterID;
//		quoteAdapter.m_frontIP = adapterConfig["frontIP"].asString();
//		quoteAdapter.m_broker = adapterConfig["broker"].asString();
//		quoteAdapter.m_user = adapterConfig["user"].asString();
//		quoteAdapter.m_pwd = adapterConfig["pwd"].asString();
//		m_quoteAdapterCfg[adapterID] = quoteAdapter;
//		Json::Value openConfig = adapterConfig["openTime"];
//		loadOpenTime(adapterID, "quote", openConfig);
//	}
//};
