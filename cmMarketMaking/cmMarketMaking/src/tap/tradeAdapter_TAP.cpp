#include <iostream>
#include <time.h>
#include "tap/tradeAdapter_TAP.h"
#include "tap/TapAPIError.h"

using namespace std;

tradeAdapter_TAP::tradeAdapter_TAP(string adapterID, TAPIAUTHCODE authCode, TAPISTR_300 keyOpLogPath, \
	TAPICHAR * ip_address, TAPIUINT16 port, char * user, char * pwd, athenathreadpoolPtr tp)
	:m_threadpool(tp), m_lag_Timer(tp->getDispatcher()), m_adapterID(adapterID), m_isApiReady(false)
{
	//取得API的版本信息
	cout << GetTapTradeAPIVersion() << endl;

	//创建API实例
	TAPIINT32 iResult = TAPIERROR_SUCCEED;
	memset(&m_stAppInfo, 0, sizeof(m_stAppInfo));
	strncpy(m_stAppInfo.AuthCode, authCode, sizeof(m_stAppInfo.AuthCode));
	strncpy(m_stAppInfo.KeyOperationLogPath, keyOpLogPath, sizeof(m_stAppInfo.KeyOperationLogPath));
	m_pApi = CreateTapTradeAPI(&m_stAppInfo, iResult);
	if (NULL == m_pApi){
		cout << m_adapterID << ": create trade API obj fail, error code: " << iResult << endl;
		return;
	}

	//设定ITapTradeAPINotify的实现类，用于异步消息的接收
	m_pApi->SetAPINotify(this);

	//设定服务器IP、端口
	TAPIINT32 iErr = m_pApi->SetHostAddress(ip_address, port);
	if (TAPIERROR_SUCCEED != iErr) {
		cout << "SetHostAddress Error:" << iErr << endl;
		return;
	}

	//登录信息
	TapAPITradeLoginAuth stLoginAuth;
	memset(&m_stLoginAuth, 0, sizeof(m_stLoginAuth));
	strncpy(m_stLoginAuth.UserNo, user, sizeof(m_stLoginAuth.UserNo));
	strncpy(m_stLoginAuth.Password, pwd, sizeof(m_stLoginAuth.Password));
	m_stLoginAuth.ISModifyPassword = APIYNFLAG_NO;
	m_stLoginAuth.ISDDA = APIYNFLAG_NO;
};

tradeAdapter_TAP::~tradeAdapter_TAP()
{
	destroyAdapter();
};

void tradeAdapter_TAP::destroyAdapter()
{
	try{
		FreeTapTradeAPI(m_pApi);
	}
	catch (exception& e)
	{
		cout << e.what() << endl;
	}
};

int tradeAdapter_TAP::init()
{
	login();

	time_t t;
	tm* local;
	t = time(NULL);
	local = localtime(&t);
	int hourSeq;
	if (21 <= local->tm_hour && local->tm_hour <= 23)
		hourSeq = local->tm_hour - 21;
	else if (0 <= local->tm_hour && local->tm_hour <= 2)
		hourSeq = local->tm_hour + 3;
	else if (9 <= local->tm_hour && local->tm_hour <= 11)
		hourSeq = local->tm_hour - 3;
	else if (13 <= local->tm_hour && local->tm_hour <= 15)
		hourSeq = local->tm_hour - 4;
	else
		hourSeq = local->tm_hour;

	memset(m_orderRef, 0, sizeof(m_orderRef));
	sprintf(m_orderRef, "00%02d%02d%02d0000", hourSeq, local->tm_min, local->tm_sec);

	return 0;
};

int tradeAdapter_TAP::login()
{
	TAPIINT32 iErr = m_pApi->Login(&m_stLoginAuth);
	if (TAPIERROR_SUCCEED != iErr) {
		cout << m_adapterID << "tap trade Api login Error:" << iErr << endl;
		return -1;
	}
	return 0;
};

void tradeAdapter_TAP::OnConnect()
{
	cout << endl << m_adapterID << " tap_trade connected!" << endl;
};

void tradeAdapter_TAP::OnDisconnect(TAPIINT32 reasonCode)
{
	cout << m_adapterID << " tap_trade disconnected!" << endl;
	if (m_OnFrontDisconnected)
		m_OnFrontDisconnected(m_adapterID, "trade");
};

void tradeAdapter_TAP::OnRspLogin(TAPIINT32 errorCode, const TapAPITradeLoginRspInfo *loginRspInfo)
{
	if (TAPIERROR_SUCCEED == errorCode) {
		cout << m_adapterID << " login succ，wait for API init ..." << endl;
	}
	else {
		cout << m_adapterID << " login fail，error code: " << errorCode << endl;
	}
};
void tradeAdapter_TAP::OnAPIReady()
{
	cout << m_adapterID << ": API init succ." << endl;
	m_isApiReady = true;
	if (m_OnUserLogin != NULL)
	{
		m_OnUserLogin(m_adapterID);
	}
};
/*
int tradeAdapter_TAP::queryTradingAccount()
{
	CThostFtdcQryTradingAccountField qryTradingAccountField;
	memset(&qryTradingAccountField, 0, sizeof(qryTradingAccountField));
	strncpy(qryTradingAccountField.BrokerID, m_loginField.BrokerID, sizeof(qryTradingAccountField.BrokerID));
	strncpy(qryTradingAccountField.InvestorID, m_loginField.UserID, sizeof(qryTradingAccountField.InvestorID));
	int ret = m_pUserApi->ReqQryTradingAccount(&qryTradingAccountField, ++m_requestId);
	cerr << " req | query cash ... " << ((ret == 0) ? "succ" : "fail") << endl;
	return ret;
};

int tradeAdapter_TAP::queryInvestorPosition()
{
	CThostFtdcQryInvestorPositionField qryInvestorPositionField;
	memset(&qryInvestorPositionField, 0, sizeof(qryInvestorPositionField));
	strncpy(qryInvestorPositionField.BrokerID, m_loginField.BrokerID, sizeof(qryInvestorPositionField.BrokerID));
	strncpy(qryInvestorPositionField.InvestorID, m_loginField.UserID, sizeof(qryInvestorPositionField.InvestorID));
	cout << qryInvestorPositionField.BrokerID << endl
		<< qryInvestorPositionField.InvestorID << endl
		<< qryInvestorPositionField.InstrumentID << endl;
	int ret = m_pUserApi->ReqQryInvestorPosition(&qryInvestorPositionField, ++m_requestId);
	cerr << " req | query position ... " << ((ret == 0) ? "succ" : "fail") << ", ret = " << ret << endl;
	return ret;
};

int tradeAdapter_TAP::queryAllInstrument()
{
	CThostFtdcQryInstrumentField qryInstrument;
	memset(&qryInstrument, 0, sizeof(CThostFtdcQryInstrumentField));
	int ret = m_pUserApi->ReqQryInstrument(&qryInstrument, ++m_requestId);
	cerr << " req | query all instruments ... " << ((ret == 0) ? "succ" : "fail") << endl;
	return ret;
};
*/


void tradeAdapter_TAP::splitInstId(string instId, char* commodity, char* contract)
{
	unsigned int i;
	for (i = 0; i < instId.length(); i++)
		if (isdigit(instId[i]))
			break;
	if (i > 0)
		strncpy(commodity, instId.c_str(), i);
	if (i < instId.length() - 1)
		strncpy(contract, instId.c_str() + i, instId.length() - i);
};

int tradeAdapter_TAP::OrderInsert(string instrument, string exchange, char orderType, char dir,
	char positionEffect, double price, unsigned int volume)
{
	TapAPINewOrder stNewOrder;
	memset(&stNewOrder, 0, sizeof(stNewOrder));

	TAPISTR_10 commodityNo;
	TAPISTR_10 ContractNo;
	memset(commodityNo, 0, sizeof(commodityNo));
	memset(ContractNo, 0, sizeof(ContractNo));
	
	strncpy(stNewOrder.AccountNo, m_stLoginAuth.UserNo, sizeof(stNewOrder.AccountNo));
	strncpy(stNewOrder.ExchangeNo, exchange.c_str(), sizeof(stNewOrder.ExchangeNo));
	stNewOrder.CommodityType = TAPI_COMMODITY_TYPE_FUTURES; // 默认为期货单

	splitInstId(instrument, commodityNo, ContractNo);
	strncpy(stNewOrder.CommodityNo, commodityNo, sizeof(stNewOrder.CommodityNo));
	strncpy(stNewOrder.ContractNo, ContractNo, sizeof(stNewOrder.ContractNo));

	//strncpy(stNewOrder.StrikePrice, "");
	stNewOrder.CallOrPutFlag = TAPI_CALLPUT_FLAG_NONE;
	//strncpy(stNewOrder.ContractNo2, "");
	//strncpy(stNewOrder.StrikePrice2, "");
	stNewOrder.CallOrPutFlag2 = TAPI_CALLPUT_FLAG_NONE;

	stNewOrder.OrderType = orderType;
	stNewOrder.OrderSource = TAPI_ORDER_SOURCE_ESUNNY_API; //报单来源
	stNewOrder.TimeInForce = TAPI_ORDER_TIMEINFORCE_GFD;   //当日有效
	//strncpy(stNewOrder.ExpireTime, "");
	stNewOrder.IsRiskOrder = APIYNFLAG_NO;
	stNewOrder.OrderSide = dir;
	stNewOrder.PositionEffect = positionEffect;
	stNewOrder.PositionEffect2 = TAPI_PositionEffect_NONE;

	//strncpy(stNewOrder.InquiryNo, ""); //?
	stNewOrder.HedgeFlag = TAPI_HEDGEFLAG_T; //没有做市商类型

	stNewOrder.OrderPrice = price;
	//stNewOrder.OrderPrice2;
	//stNewOrder.StopPrice;
	stNewOrder.OrderQty = volume;
	//stNewOrder.OrderMinQty;
	//stNewOrder.MinClipSize;
	//stNewOrder.MaxClipSize;

	int nextOrderRef = atoi(m_orderRef) + 1;
	sprintf(m_orderRef, "%012d", nextOrderRef);
	stNewOrder.RefInt = nextOrderRef;
	strncpy(stNewOrder.RefString, m_orderRef, sizeof(m_orderRef));
	stNewOrder.TacticsType = TAPI_TACTICS_TYPE_NONE;
	stNewOrder.TriggerCondition = TAPI_TRIGGER_CONDITION_NONE;
	stNewOrder.TriggerPriceType = TAPI_TRIGGER_PRICE_NONE;
	stNewOrder.AddOneIsValid = APIYNFLAG_NO;
	//stNewOrder.OrderQty2;
	stNewOrder.HedgeFlag2 = TAPI_HEDGEFLAG_NONE;
	stNewOrder.MarketLevel = TAPI_MARKET_LEVEL_0;
	stNewOrder.FutureAutoCloseFlag = APIYNFLAG_NO; // V9.0.2.0 20150520

	TAPIUINT32 m_uiSessionID = 0;
	TAPIINT32 iErr = m_pApi->InsertOrder(&m_uiSessionID, &stNewOrder);
	if (TAPIERROR_SUCCEED != iErr) {
		cout << m_adapterID <<  " insertOrder Error:" << iErr << endl;
		return -1;
	}
	return nextOrderRef;
}

void tradeAdapter_TAP::OnRspOrderAction(TAPIUINT32 sessionID, TAPIUINT32 errorCode, const TapAPIOrderActionRsp *info)
{
	if (TAPIERROR_SUCCEED != errorCode){
		cout << m_adapterID << " resp | order action fail, errorCode: " << errorCode << endl;
	}
	else
		cout << m_adapterID << "resp | order action succ, contract: " << info->OrderInfo->CommodityNo << info->OrderInfo->ContractNo << ", action: "<< info->ActionType
			<< ", orderDir: " << info->OrderInfo->OrderSide << ", Effect: " << info->OrderInfo->PositionEffect << ", price: " << info->OrderInfo->OrderPrice << ", quantity: " << info->OrderInfo->OrderQty << endl;
};

void tradeAdapter_TAP::OnRtnOrder(const TapAPIOrderInfoNotice *info)
{
	orderRefInfoPtr pOrderRef = orderRefInfoPtr(new orderRefInfo());
	pOrderRef->RefInt = info->OrderInfo->RefInt;
	strncpy(pOrderRef->RefString, info->OrderInfo->RefString, sizeof(pOrderRef->RefString));
	pOrderRef->ServerFlag = info->OrderInfo->ServerFlag;
	strncpy(pOrderRef->OrderNo, info->OrderInfo->OrderNo, sizeof(pOrderRef->OrderNo));
	{
		boost::detail::spinlock l(m_ref2order_lock);
		m_ref2order[pOrderRef->RefInt] = pOrderRef;
	}
	if (m_OnOrderRtn)
		m_OnOrderRtn(m_adapterID, (TapAPIOrderInfoNotice *)info);
	/*
	cerr << " 回报 | 报单已提交...序号:" << pOrder->BrokerOrderSeq
	<< ", OrderStatus:" << pOrder->OrderStatus
	<< ", CombHedgeFlag:" << pOrder->CombHedgeFlag
	<< ", CombOffsetFlag:" << pOrder->CombOffsetFlag
	<< ", Direction:" << pOrder->Direction
	<< ", InstrumentID:" << pOrder->InstrumentID
	<< ", LimitPrice:" << opOrderrder->LimitPrice
	<< ", MinVolume:" << pOrder->MinVolume
	<< ", OrderPriceType:" << pOrder->OrderPriceType
	<< ", StatusMsg:" << pOrder->StatusMsg
	<< ", orderRef:" << pOrder->OrderRef
	<< endl;*/
};

void tradeAdapter_TAP::OnRtnFill(const TapAPIFillInfo *info)
{

	//cerr << " tradeAdapter_TAP | 报单已成交...成交编号:" << pTrade->TradeID << endl;

	if (m_OnTradeRtn)
		m_OnTradeRtn(m_adapterID, (TapAPIFillInfo *)info);
};

int tradeAdapter_TAP::cancelOrder(int orderRef)
{
	auto iter = m_ref2order.find(orderRef);
	if (iter == m_ref2order.end())
	{
		cerr << m_adapterID <<  " cancel order fail | orderRef " << orderRef << " not found in adapter." << endl;
		return -1;
	}
	TAPIUINT32 sessionID;
	TapAPIOrderCancelReq orderRefInfo;

	orderRefInfo.RefInt = iter->second->RefInt;
	strncpy(orderRefInfo.RefString, iter->second->RefString, sizeof(orderRefInfo.RefString));
	orderRefInfo.ServerFlag = iter->second->ServerFlag;
	strncpy(orderRefInfo.OrderNo, iter->second->OrderNo, sizeof(orderRefInfo.OrderNo));

	TAPIINT32 iErr = m_pApi->CancelOrder(&sessionID, &orderRefInfo);
	if (TAPIERROR_SUCCEED != iErr)
		cout << m_adapterID << " req CanceOrder Error: " << iErr << endl;
	else
		cout << m_adapterID << " req CancelOrder succ." << endl;
	return 0; //todo：设计返回id
};
