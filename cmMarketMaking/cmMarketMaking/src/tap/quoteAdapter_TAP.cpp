#include <iostream>
#include <string.h>
#include <cctype>
#include "tap/quoteAdapter_TAP.h"
#include "tap/TapAPIError.h"

using namespace std;

//quoteAdapter_TAP::quoteAdapter_TAP() : m_pApi(NULL),m_isApiReady(false)
//{
//}

quoteAdapter_TAP::quoteAdapter_TAP(string adapterID, TAPIAUTHCODE authCode, TAPISTR_300 keyOpLogPath, \
	TAPICHAR * ip_address, TAPIUINT16 port, char * user, char * pwd) 
	: m_pApi(NULL), m_isApiReady(false), m_adapterID(adapterID)
{
	cout << GetTapQuoteAPIVersion() << endl;

	//创建API实例
	TAPIINT32 iResult = TAPIERROR_SUCCEED;
	memset(&m_stAppInfo, 0, sizeof(m_stAppInfo));
	strcpy(m_stAppInfo.AuthCode, authCode);
	strcpy(m_stAppInfo.KeyOperationLogPath, keyOpLogPath);
	m_pApi = CreateTapQuoteAPI(&m_stAppInfo, iResult);
	if (NULL == m_pApi){
		cout << m_adapterID << ": create quote API obj fail, error code: " << iResult << endl;
		return;
	}
	//设定ITapQuoteAPINotify的实现类，用于异步消息的接收
	m_pApi->SetAPINotify(this);

	//设定服务器IP、端口
	TAPIINT32 iErr = m_pApi->SetHostAddress(ip_address, port);
	if (TAPIERROR_SUCCEED != iErr) {
		cout << "SetHostAddress Error:" << iErr << endl;
		return;
	}

	//登录信息
	memset(&m_stLoginAuth, 0, sizeof(m_stLoginAuth));
	strncpy(m_stLoginAuth.UserNo, user, sizeof(m_stLoginAuth.UserNo));
	strncpy(m_stLoginAuth.Password, pwd, sizeof(m_stLoginAuth.Password));
	m_stLoginAuth.ISModifyPassword = APIYNFLAG_NO;
	m_stLoginAuth.ISDDA = APIYNFLAG_NO;
};

quoteAdapter_TAP::~quoteAdapter_TAP()
{
	destroyAdapter();
}

void quoteAdapter_TAP::destroyAdapter()
{
	try{
		FreeTapQuoteAPI(m_pApi);;
	}
	catch (exception& e)
	{
		cout << e.what() << endl;
	}
};

int quoteAdapter_TAP::init()
{
	login();
	return 0;
};
int quoteAdapter_TAP::login()
{
	TAPIINT32 iErr = m_pApi->Login(&m_stLoginAuth);
	if (TAPIERROR_SUCCEED != iErr) {
		cout << "tap quote Api login Error:" << iErr << endl;
		return -1;
	}
	else
		return 0;
};

void quoteAdapter_TAP::splitInstId(string instId, char* commodity, char* contract)
{
	unsigned int i;
	for (i = 0; i < instId.length(); i++)
		if (isdigit(instId[i]))
			break;
	if ( i > 0 )
		strncpy(commodity, instId.c_str(), i);
	if (i < instId.length() - 1)
		strncpy(contract, instId.c_str() + i, instId.length() - i);
};

void quoteAdapter_TAP::Subscribe(string instId, string exchange)
{
	//订阅行情
	TapAPIContract stContract;
	TAPISTR_10 commodityNo;
	TAPISTR_10 ContractNo;
	memset(&stContract, 0, sizeof(stContract));
	memset(commodityNo, 0, sizeof(commodityNo));
	memset(ContractNo, 0, sizeof(ContractNo));
	strncpy(stContract.Commodity.ExchangeNo, exchange.c_str(), sizeof(stContract.Commodity.ExchangeNo));
	stContract.Commodity.CommodityType = TAPI_COMMODITY_TYPE_FUTURES; // futures
	splitInstId(instId, commodityNo, ContractNo);
	strncpy(stContract.Commodity.CommodityNo, commodityNo, sizeof(stContract.Commodity.CommodityNo));
	strncpy(stContract.ContractNo1, ContractNo, sizeof(stContract.ContractNo1));
	stContract.CallOrPutFlag1 = TAPI_CALLPUT_FLAG_NONE;
	stContract.CallOrPutFlag2 = TAPI_CALLPUT_FLAG_NONE;
	TAPIUINT32 m_uiSessionID = 0;
	TAPIINT32 iErr = m_pApi->SubscribeQuote(&m_uiSessionID, &stContract);
	if (TAPIERROR_SUCCEED != iErr) {
		cout << m_adapterID << ": api subscribe quote Error:" << iErr << endl;
		return;
	}
}

void quoteAdapter_TAP::UnSubscribe(string instId, string exchange)
{
	//取消订阅行情
	TapAPIContract stContract;
	TAPISTR_10 commodityNo;
	TAPISTR_10 ContractNo;
	memset(&stContract, 0, sizeof(stContract));
	memset(commodityNo, 0, sizeof(commodityNo));
	memset(ContractNo, 0, sizeof(ContractNo));
	strncpy(stContract.Commodity.ExchangeNo, exchange.c_str(), sizeof(stContract.Commodity.ExchangeNo));
	stContract.Commodity.CommodityType = TAPI_COMMODITY_TYPE_FUTURES; // futures
	splitInstId(instId, commodityNo, ContractNo);
	strncpy(stContract.Commodity.CommodityNo, commodityNo, sizeof(stContract.Commodity.CommodityNo));
	strncpy(stContract.ContractNo1, ContractNo, sizeof(stContract.ContractNo1));
	stContract.CallOrPutFlag1 = TAPI_CALLPUT_FLAG_NONE;
	stContract.CallOrPutFlag2 = TAPI_CALLPUT_FLAG_NONE;
	TAPIUINT32 m_uiSessionID = 0;
	TAPIINT32 iErr = m_pApi->UnSubscribeQuote(&m_uiSessionID, &stContract);
	if (TAPIERROR_SUCCEED != iErr) {
		cout << m_adapterID << ": api unsubscribe quote Error:" << iErr << endl;
		return;
	}
};

void TAP_CDECL quoteAdapter_TAP::OnRspLogin(TAPIINT32 errorCode, const TapAPIQuotLoginRspInfo *info)
{
	if (TAPIERROR_SUCCEED == errorCode) {
		cout << m_adapterID << " login succ，wait for API init ..." << endl;
	}
	else {
		cout << m_adapterID << " login fail，error code: " << errorCode << endl;
	}
}

void TAP_CDECL quoteAdapter_TAP::OnAPIReady()
{
	cout << m_adapterID <<  ": API init succ." << endl;
	m_isApiReady = true;
	if (m_OnUserLogin != NULL)
	{
		m_OnUserLogin(m_adapterID);
	}
}

void TAP_CDECL quoteAdapter_TAP::OnDisconnect(TAPIINT32 reasonCode)
{
	cout << m_adapterID << ": API disconnected, reason code:" << reasonCode << endl;
	if (m_OnFrontDisconnected != NULL)
	{
		m_OnFrontDisconnected(m_adapterID, "quote");
	}
}

/*
void TAP_CDECL quoteAdapter_TAP::OnRspChangePassword(TAPIUINT32 sessionID, TAPIINT32 errorCode)
{
	cout << __FUNCTION__ << " is called." << endl;
}

void TAP_CDECL quoteAdapter_TAP::OnRspQryExchange(TAPIUINT32 sessionID, TAPIINT32 errorCode, TAPIYNFLAG isLast, const TapAPIExchangeInfo *info)
{
	cout << __FUNCTION__ << " is called." << endl;
}

void TAP_CDECL quoteAdapter_TAP::OnRspQryCommodity(TAPIUINT32 sessionID, TAPIINT32 errorCode, TAPIYNFLAG isLast, const TapAPIquoteAdapter_TAPCommodityInfo *info)
{
	cout << __FUNCTION__ << " is called." << endl;
}

void TAP_CDECL quoteAdapter_TAP::OnRspQryContract(TAPIUINT32 sessionID, TAPIINT32 errorCode, TAPIYNFLAG isLast, const TapAPIQuoteContractInfo *info)
{
	cout << __FUNCTION__ << " is called." << endl;
}


void TAP_CDECL quoteAdapter_TAP::OnRtnContract(const TapAPIquoteAdapter_TAPContractInfo *info)
{
	cout << __FUNCTION__ << " is called." << endl;
}*/

void TAP_CDECL quoteAdapter_TAP::OnRspSubscribeQuote(TAPIUINT32 sessionID, TAPIINT32 errorCode, TAPIYNFLAG isLast, const TapAPIQuoteWhole *info)
{
	if (TAPIERROR_SUCCEED == errorCode)
	{
		cout << m_adapterID << ": subscribe quote succ. ";
		if (NULL != info)
		{
			cout << info->DateTimeStamp << " "
				<< info->Contract.Commodity.ExchangeNo << " "
				<< info->Contract.Commodity.CommodityType << " "
				<< info->Contract.Commodity.CommodityNo << " "
				<< info->Contract.ContractNo1 << " "
				<< info->QLastPrice
				// ...		
				<< endl;
		}
	}
	else{
		cout << m_adapterID << ": subscribe quote fail，error Code：" << errorCode << endl;
	}
}

void TAP_CDECL quoteAdapter_TAP::OnRspUnSubscribeQuote(TAPIUINT32 sessionID, TAPIINT32 errorCode, TAPIYNFLAG isLast, const TapAPIContract *info)
{
	if (TAPIERROR_SUCCEED == errorCode)
		cout << m_adapterID << ": unsubscribe quote succ. ";
	else
		cout << m_adapterID << ": unsubscribe quote fail，error Code：" << errorCode << endl;
}

void TAP_CDECL quoteAdapter_TAP::OnRtnQuote(const TapAPIQuoteWhole *info)
{
	if (NULL != info)
	{
		cout << " quote | contract:" << info->Contract.Commodity.CommodityNo << info->Contract.ContractNo1 
			<< ", last: " << info->QLastPrice<< endl;

		if (m_onRtnMarketData != NULL)
			m_onRtnMarketData(m_adapterID, (TapAPIQuoteWhole *)info);
	}
}
