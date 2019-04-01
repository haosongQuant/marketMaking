#pragma once
#include <iostream>
#include <map>
#include "ctp/ThostFtdcTraderApi.h"
#include "threadpool/threadpool.h"
#include <boost/shared_ptr.hpp>
#include <boost/smart_ptr/detail/spinlock.hpp>
#include <boost\function.hpp>
#include <boost\asio.hpp>
#include <boost\bind.hpp>
#include "baseClass/adapterBase.h"

using namespace std;

typedef boost::shared_ptr<CThostFtdcOrderField> CThostFtdcOrderFieldPtr;

class  tradeAdapterCTP : public traderAdapterBase, public CThostFtdcTraderSpi
{

private:
	int m_requestId = 0;
	int m_frontId;    //前置编号
	int m_sessionId;    //会话编号
	char m_orderRef[13];
	map<int, CThostFtdcOrderFieldPtr> m_ref2order;
	boost::detail::spinlock m_ref2order_lock;
	CThostFtdcTraderApi* m_pUserApi;
	CThostFtdcReqUserLoginField m_loginField;
	bool m_needAuthenticate;
	CThostFtdcReqAuthenticateField m_authenticateField;

public:
	tradeAdapterCTP(string adapterID, char* tradeFront, char* broker, char* user, char* pwd, 
		athenathreadpoolPtr tp);
	tradeAdapterCTP(string adapterID, char* tradeFront, char* broker, char* user, char* pwd,
		char * userproductID, char * authenticateCode, athenathreadpoolPtr tp);

	virtual void destroyAdapter();

	int init();
	int login();

	int queryTradingAccount();//查询资金
	int queryInvestorPosition();//查询持仓
	int queryAllInstrument();//查询全部合约

	//下单
	virtual int OrderInsert(string instrument, char priceType, char dir,
		char ComOffsetFlag, char ComHedgeFlag, double price,
		int volume, char tmCondition, char volCondition, int minVol, char contiCondition,
		double stopPrz, char forceCloseReason);
	//撤单
	virtual void cancelOrder(int orderRef);

private:
	athenathreadpoolPtr m_threadpool;
	boost::asio::deadline_timer m_lag_Timer;

public:
	boost::function<void(string adapterID)> m_OnUserLogin;
	boost::function<void(string adapterID)> m_OnUserLogout;
	boost::function<void(string adapterID, string adapterType)> m_OnFrontDisconnected;
	boost::function<void(string, CThostFtdcOrderField*)> m_OnOrderRtn;
	boost::function<void(string, CThostFtdcTradeField*)> m_OnTradeRtn;
	boost::function<void(string, CThostFtdcInstrumentField*)> m_OnInstrumentsRtn;
	boost::function<void(CThostFtdcInvestorPositionField*)> m_OnInvestorPositionRtn;

private:
	bool isErrorRespInfo(CThostFtdcRspInfoField *pRspInfo);

public:
	///当客户端与交易后台建立起通信连接时（还未登录前），该方法被调用。
	virtual void OnFrontConnected();

	///当客户端与交易后台通信连接断开时，该方法被调用。当发生这个情况后，API会自动重新连接，客户端可不做处理。
	///@param nReason 错误原因
	///        0x1001 网络读失败
	///        0x1002 网络写失败
	///        0x2001 接收心跳超时
	///        0x2002 发送心跳失败
	///        0x2003 收到错误报文
	virtual void OnFrontDisconnected(int nReason);

	///心跳超时警告。当长时间未收到报文时，该方法被调用。
	///@param nTimeLapse 距离上次接收报文的时间
	virtual void OnHeartBeatWarning(int nTimeLapse);

	///客户端认证响应
	virtual void OnRspAuthenticate(CThostFtdcRspAuthenticateField *pRspAuthenticateField, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);

	///登录请求响应
	virtual void OnRspUserLogin(CThostFtdcRspUserLoginField *pRspUserLogin, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);

	///登出请求响应
	virtual void OnRspUserLogout(CThostFtdcUserLogoutField *pUserLogout, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);

	///请求查询资金账户响应
	virtual void OnRspQryTradingAccount(CThostFtdcTradingAccountField *pTradingAccount, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);
	///请求查询投资者持仓响应
	virtual void OnRspQryInvestorPosition(CThostFtdcInvestorPositionField *pInvestorPosition, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);
	///请求查询合约响应
	virtual void OnRspQryInstrument(CThostFtdcInstrumentField *pInstrument, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);

	///报单录入请求响应
	virtual void OnRspOrderInsert(CThostFtdcInputOrderField *pInputOrder, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);
	///报单录入错误回报
	virtual void OnErrRtnOrderInsert(CThostFtdcInputOrderField *pInputOrder, CThostFtdcRspInfoField *pRspInfo);

	///报单通知
	virtual void OnRtnOrder(CThostFtdcOrderField *pOrder);

	///成交通知
	virtual void OnRtnTrade(CThostFtdcTradeField *pTrade);

	///报单操作请求响应
	virtual void OnRspOrderAction(CThostFtdcInputOrderActionField *pInputOrderAction, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);
	///报单操作错误回报
	virtual void OnErrRtnOrderAction(CThostFtdcOrderActionField *pOrderAction, CThostFtdcRspInfoField *pRspInfo);

};
