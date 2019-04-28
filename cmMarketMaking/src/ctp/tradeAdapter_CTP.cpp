#include <iostream>
#include <time.h>
#include "ctp/tradeAdapter_CTP.h"
#include "baseClass/Utils.h"
#include "glog\logging.h"

using namespace std;

tradeAdapterCTP::tradeAdapterCTP(string adapterID, char * tradeFront, char * broker, char * user, char * pwd,
	athenathreadpoolPtr tp)// :m_onLogin(NULL)
	:m_threadpool(tp), m_lag_Timer(tp->getDispatcher()), m_qryOrder_Timer(tp->getDispatcher())
{
	m_adapterID = adapterID;

	m_pUserApi = CThostFtdcTraderApi::CreateFtdcTraderApi();
	m_pUserApi->RegisterSpi(this);         // 注册事件类
	m_pUserApi->SubscribePublicTopic(THOST_TERT_QUICK);			  // 注册公有流
	m_pUserApi->SubscribePrivateTopic(THOST_TERT_QUICK);			  // 注册私有流
	m_pUserApi->RegisterFront(tradeFront);							  // 注册交易前置地址

	memset(&m_loginField, 0, sizeof(m_loginField));
	strncpy(m_loginField.BrokerID, broker, sizeof(m_loginField.BrokerID));
	strncpy(m_loginField.UserID, user, sizeof(m_loginField.UserID));
	strncpy(m_loginField.Password, pwd, sizeof(m_loginField.Password));

	m_needAuthenticate = false;
	m_qryingOrder = false;
	m_cancelQryTimer = false;
};


tradeAdapterCTP::tradeAdapterCTP(string adapterID, char* tradeFront, char* broker, char* user, char* pwd,
	char * userproductID, char * authenticateCode, athenathreadpoolPtr tp)
	:m_threadpool(tp), m_lag_Timer(tp->getDispatcher()), m_qryOrder_Timer(tp->getDispatcher())
{
	m_adapterID = adapterID;

	m_pUserApi = CThostFtdcTraderApi::CreateFtdcTraderApi();
	m_pUserApi->RegisterSpi(this);         // 注册事件类
	m_pUserApi->SubscribePublicTopic(THOST_TERT_QUICK);			  // 注册公有流
	m_pUserApi->SubscribePrivateTopic(THOST_TERT_QUICK);			  // 注册私有流
	m_pUserApi->RegisterFront(tradeFront);							  // 注册交易前置地址

	memset(&m_loginField, 0, sizeof(m_loginField));
	strncpy(m_loginField.BrokerID, broker, sizeof(m_loginField.BrokerID));
	strncpy(m_loginField.UserID, user, sizeof(m_loginField.UserID));
	strncpy(m_loginField.Password, pwd, sizeof(m_loginField.Password));

	m_needAuthenticate = true;
	memset(&m_authenticateField, 0, sizeof(m_authenticateField));
	strncpy(m_authenticateField.BrokerID, broker, sizeof(m_authenticateField.BrokerID));
	strncpy(m_authenticateField.UserID, user, sizeof(m_authenticateField.UserID));
	strncpy(m_authenticateField.UserProductInfo, userproductID, sizeof(m_authenticateField.UserProductInfo));
	strncpy(m_authenticateField.AuthCode, authenticateCode, sizeof(m_authenticateField.AuthCode));

	m_qryingOrder = false;
	m_cancelQryTimer = false;
};

void tradeAdapterCTP::destroyAdapter()
{
	m_status = ADAPTER_STATUS_DISCONNECT;
	try{
		m_pUserApi->Release();
	}
	catch (exception& e)
	{
		LOG(INFO)  << e.what() << endl;
	}
};

int tradeAdapterCTP::init()
{
	m_pUserApi->Init();
	m_status = ADAPTER_STATUS_CONNECTING;
	return 0;
};

void tradeAdapterCTP::OnFrontConnected()
{
	LOG(WARNING)  << m_adapterID << ": ctp trade connected!" << endl;
	if (m_needAuthenticate)
	{
		int ret = m_pUserApi->ReqAuthenticate(&m_authenticateField, ++m_requestId);
		LOG(INFO)  << m_adapterID << ":  req | send Authenticate ... " << ((ret == 0) ? "succ" : "fail") << endl;
	}
	else
		login();
};

void tradeAdapterCTP::OnRspAuthenticate(CThostFtdcRspAuthenticateField *pRspAuthenticateField,
	CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	if (isErrorRespInfo(pRspInfo))
		LOG(INFO)  << m_adapterID << ": Authenticate error | ErrorID: " << pRspInfo->ErrorID <<
		", ErrorMsg: " << pRspInfo->ErrorMsg << endl;
	else
	{
		LOG(INFO)  << m_adapterID << ": Authenticate succ | broker: " << pRspAuthenticateField->BrokerID <<
			", user: " << pRspAuthenticateField->UserID <<
			", productInfo: " << pRspAuthenticateField->UserProductInfo << endl;
		login();
	}
};

void tradeAdapterCTP::OnFrontDisconnected(int nReason)
{
	LOG(INFO)  << m_adapterID << ": trade adapterCTP disconnected!" << endl;
	if (m_status != ADAPTER_STATUS_DISCONNECT && m_OnFrontDisconnected)
	{
		m_OnFrontDisconnected(m_adapterID);
	}
	m_status = ADAPTER_STATUS_DISCONNECT;
};

void tradeAdapterCTP::OnHeartBeatWarning(int nTimeLapse)
{
	LOG(INFO)  << m_adapterID << ": heartbeat warning: " << nTimeLapse << "s." << endl;
};

int tradeAdapterCTP::login()
{
	int ret = m_pUserApi->ReqUserLogin(&m_loginField, ++m_requestId);
	LOG(INFO)  << m_adapterID << ":  req | send login ... " << ((ret == 0) ? "succ" : "fail") << endl;
	return ret;
};

void tradeAdapterCTP::OnRspUserLogin(CThostFtdcRspUserLoginField *pRspUserLogin,
	CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	if (isErrorRespInfo(pRspInfo))
		LOG(INFO)  << m_adapterID << ": trade login error | ErrorID: " << pRspInfo->ErrorID << ", ErrorMsg: " << pRspInfo->ErrorMsg
		<< ", user: " << m_loginField.UserID << ", pwd:" << m_loginField.Password << ", broker: " << m_loginField.BrokerID<<endl;
	else
	{
		LOG(INFO)  << m_adapterID<<": trade login succ!" << endl;

		// 保存会话参数    
		//m_frontId = pRspUserLogin->FrontID;
		//m_sessionId = pRspUserLogin->SessionID;

		time_t t;
		tm* local;
		t = time(NULL);
		local = localtime(&t);
		memset(m_orderRef, 0, sizeof(m_orderRef));
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
		sprintf(m_orderRef, "00%02d%02d%02d0000", hourSeq, local->tm_min, local->tm_sec);

		m_status = ADAPTER_STATUS_LOGIN;
		m_lag_Timer.expires_from_now(boost::posix_time::milliseconds(3000));
		m_lag_Timer.async_wait(boost::bind(&tradeAdapterCTP::queryTradingAccount, this));
	}
};

void tradeAdapterCTP::OnRspUserLogout(CThostFtdcUserLogoutField *pUserLogout, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	LOG(INFO)  << "tradeAdapterCTP logout!" << endl;

	m_status = ADAPTER_STATUS_LOGOUT;
	if (m_OnUserLogout != NULL)
	{
		m_OnUserLogout(m_adapterID);
	}
}

int tradeAdapterCTP::queryTradingAccount()
{
	CThostFtdcQryTradingAccountField qryTradingAccountField;
	memset(&qryTradingAccountField, 0, sizeof(qryTradingAccountField));
	strncpy(qryTradingAccountField.BrokerID, m_loginField.BrokerID, sizeof(qryTradingAccountField.BrokerID));
	strncpy(qryTradingAccountField.InvestorID, m_loginField.UserID, sizeof(qryTradingAccountField.InvestorID));
	int ret = m_pUserApi->ReqQryTradingAccount(&qryTradingAccountField, ++m_requestId);
	LOG(INFO)  << m_adapterID << ":  req | query trading account ... " << ((ret == 0) ? "succ" : "fail") << endl;
	return ret;
};

void tradeAdapterCTP::OnRspQryTradingAccount(CThostFtdcTradingAccountField *pTradingAccount,
	CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	if (pTradingAccount)
	{
		//LOG(INFO)  << "经纪公司代码	:" << pTradingAccount->BrokerID << endl
		LOG(INFO)  << "投资者帐号:" << pTradingAccount->AccountID << ", "
			//	<< "上次质押金额:" << pTradingAccount->PreMortgage << endl
			//	<< "上次信用额度:" << pTradingAccount->PreCredit << endl
			//	<< "上次存款额:" << pTradingAccount->PreDeposit << endl
			//	<< "上次结算准备金:" << pTradingAccount->PreBalance << endl
			//	<< "上次占用的保证金:" << pTradingAccount->PreMargin << endl
			//	<< "利息基数:" << pTradingAccount->InterestBase << endl
			//	<< "利息收入:" << pTradingAccount->Interest << endl
			//	<< "入金金额:" << pTradingAccount->Deposit << endl
			//	<< "出金金额:" << pTradingAccount->Withdraw << endl
			//	<< "冻结的保证金:" << pTradingAccount->FrozenMargin << endl
			//	<< "冻结的资金:" << pTradingAccount->FrozenCash << endl
			//	<< "冻结的手续费:" << pTradingAccount->FrozenCommission << endl
			//	<< "当前保证金总额:" << pTradingAccount->CurrMargin << endl
			//	<< "资金差额:" << pTradingAccount->CashIn << endl
			//	<< "手续费:" << pTradingAccount->Commission << endl
			//	<< "平仓盈亏:" << pTradingAccount->CloseProfit << endl
			//	<< "持仓盈亏:" << pTradingAccount->PositionProfit << endl
			//	<< "期货结算准备金:" << pTradingAccount->Balance << endl
			<< "可用资金: " << pTradingAccount->Available << endl;
		//	<< "可取资金:" << pTradingAccount->WithdrawQuota << endl
		//	<< "基本准备金:" << pTradingAccount->Reserve << endl
		//	<< "交易日:" << pTradingAccount->TradingDay << endl
		//	<< "结算编号:" << pTradingAccount->SettlementID << endl
		//	<< "信用额度:" << pTradingAccount->Credit << endl
		//	<< "质押金额:" << pTradingAccount->Mortgage << endl
		//	<< "交易所保证金:" << pTradingAccount->ExchangeMargin << endl
		//	<< "投资者交割保证金:" << pTradingAccount->DeliveryMargin << endl
		//	<< "交易所交割保证金:" << pTradingAccount->ExchangeDeliveryMargin << endl
		//	<< "保底期货结算准备金:" << pTradingAccount->ReserveBalance << endl
		//	<< "币种代码:" << pTradingAccount->CurrencyID << endl
		//	<< "上次货币质入金额:" << pTradingAccount->PreFundMortgageIn << endl
		//	<< "上次货币质出金额:" << pTradingAccount->PreFundMortgageOut << endl
		//	<< "货币质入金额:" << pTradingAccount->FundMortgageIn << endl
		//	<< "货币质出金额:" << pTradingAccount->FundMortgageOut << endl
		//	<< "货币质押余额:" << pTradingAccount->FundMortgageAvailable << endl
		//	<< "可质押货币金额:" << pTradingAccount->MortgageableFund << endl
		//	<< "特殊产品占用保证金:" << pTradingAccount->SpecProductMargin << endl
		//	<< "特殊产品冻结保证金:" << pTradingAccount->SpecProductFrozenMargin << endl
		//	<< "特殊产品手续费:" << pTradingAccount->SpecProductCommission << endl
		//	<< "特殊产品冻结手续费:" << pTradingAccount->SpecProductFrozenCommission << endl
		//	<< "特殊产品持仓盈亏:" << pTradingAccount->SpecProductPositionProfit << endl
		//	<< "特殊产品平仓盈亏:" << pTradingAccount->SpecProductCloseProfit << endl
		//	<< "根据持仓盈亏算法计算的特殊产品持仓盈亏:" << pTradingAccount->SpecProductPositionProfitByAlg << endl
		//	<< "特殊产品交易所保证金:" << pTradingAccount->SpecProductExchangeMargin << endl;

		if (bIsLast)
		{
			LOG(INFO)  << m_adapterID << ": query trading account done." << endl;
			m_lag_Timer.expires_from_now(boost::posix_time::milliseconds(3000));
			m_lag_Timer.async_wait(boost::bind(&tradeAdapterCTP::queryInvestorPosition, this));
		}
	}
	else
	{
		LOG(INFO)  << "resp | query cash fail ";
		if (pRspInfo == nullptr)
			LOG(INFO)  << endl;
		else
			LOG(INFO)  << " ErrorID: " << pRspInfo->ErrorID << ", ErrorMsg : " << pRspInfo->ErrorMsg << endl;
	}
};

int tradeAdapterCTP::queryInvestorPosition()
{
	CThostFtdcQryInvestorPositionField qryInvestorPositionField;
	memset(&qryInvestorPositionField, 0, sizeof(qryInvestorPositionField));
	strncpy(qryInvestorPositionField.BrokerID, m_loginField.BrokerID, sizeof(qryInvestorPositionField.BrokerID));
	strncpy(qryInvestorPositionField.InvestorID, m_loginField.UserID, sizeof(qryInvestorPositionField.InvestorID));
	//LOG(INFO)  << qryInvestorPositionField.BrokerID << endl
	//	<< qryInvestorPositionField.InvestorID << endl
	//	<< qryInvestorPositionField.InstrumentID << endl;
	int ret = m_pUserApi->ReqQryInvestorPosition(&qryInvestorPositionField, ++m_requestId);
	LOG(INFO)  << m_adapterID << ":  req | query position ... " << ((ret == 0) ? "succ" : "fail") << ", ret = " << ret << endl;
	return ret;
};

void tradeAdapterCTP::OnRspQryInvestorPosition(CThostFtdcInvestorPositionField *pInvestorPosition,
	CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	if (pInvestorPosition)
	{
		/*LOG(INFO) << "合约代码: " << pInvestorPosition->InstrumentID << endl
		<< "经纪公司代码: " << pInvestorPosition->BrokerID << endl
		<< "投资者代码: " << pInvestorPosition->InvestorID << endl
		<< "持仓多空方向: " << pInvestorPosition->PosiDirection << endl
		<< "投机套保标志: " << pInvestorPosition->HedgeFlag << endl
		<< "持仓日期: " << pInvestorPosition->PositionDate << endl
		<< "上日持仓: " << pInvestorPosition->YdPosition << endl
		<< "今日持仓: " << pInvestorPosition->Position << endl
		<< "多头冻结: " << pInvestorPosition->LongFrozen << endl
		<< "空头冻结: " << pInvestorPosition->ShortFrozen << endl
		<< "开仓冻结金额: " << pInvestorPosition->LongFrozenAmount << endl
		<< "开仓冻结金额: " << pInvestorPosition->ShortFrozenAmount << endl
		<< "开仓量: " << pInvestorPosition->OpenVolume << endl
		<< "平仓量: " << pInvestorPosition->CloseVolume << endl
		<< "开仓金额: " << pInvestorPosition->OpenAmount << endl
		<< "平仓金额: " << pInvestorPosition->CloseAmount << endl
		<< "持仓成本: " << pInvestorPosition->PositionCost << endl
		<< "上次占用的保证金: " << pInvestorPosition->PreMargin << endl
		<< "占用的保证金: " << pInvestorPosition->UseMargin << endl
		<< "冻结的保证金: " << pInvestorPosition->FrozenMargin << endl
		<< "冻结的资金: " << pInvestorPosition->FrozenCash << endl
		<< "冻结的手续费: " << pInvestorPosition->FrozenCommission << endl
		<< "资金差额: " << pInvestorPosition->CashIn << endl
		<< "手续费: " << pInvestorPosition->Commission << endl
		<< "平仓盈亏: " << pInvestorPosition->CloseProfit << endl
		<< "持仓盈亏: " << pInvestorPosition->PositionProfit << endl
		<< "上次结算价: " << pInvestorPosition->PreSettlementPrice << endl
		<< "本次结算价: " << pInvestorPosition->SettlementPrice << endl
		<< "交易日: " << pInvestorPosition->TradingDay << endl
		<< "结算编号: " << pInvestorPosition->SettlementID << endl
		<< "开仓成本: " << pInvestorPosition->OpenCost << endl
		<< "交易所保证金: " << pInvestorPosition->ExchangeMargin << endl
		<< "组合成交形成的持仓: " << pInvestorPosition->CombPosition << endl
		<< "组合多头冻结: " << pInvestorPosition->CombLongFrozen << endl
		<< "组合空头冻结: " << pInvestorPosition->CombShortFrozen << endl
		<< "逐日盯市平仓盈亏: " << pInvestorPosition->CloseProfitByDate << endl
		<< "逐笔对冲平仓盈亏: " << pInvestorPosition->CloseProfitByTrade << endl
		<< "今日持仓: " << pInvestorPosition->TodayPosition << endl
		<< "保证金率: " << pInvestorPosition->MarginRateByMoney << endl
		<< "保证金率(按手数): " << pInvestorPosition->MarginRateByVolume << endl
		<< "执行冻结: " << pInvestorPosition->StrikeFrozen << endl
		<< "执行冻结金额: " << pInvestorPosition->StrikeFrozenAmount << endl
		<< "放弃执行冻结: " << pInvestorPosition->AbandonFrozen << endl;*/
		if (m_OnInvestorPositionRtn)
			m_OnInvestorPositionRtn(pInvestorPosition);

		if (bIsLast)
		{
			LOG(INFO)  << m_adapterID << ": query investor position done." << endl;
			m_lag_Timer.expires_from_now(boost::posix_time::milliseconds(3000));
			m_lag_Timer.async_wait(boost::bind(&tradeAdapterCTP::confirmSettlementInfo, this));
		}
	}
	else
	{
		LOG(INFO)  << m_adapterID << ": resp | query position fail";
		if (pRspInfo == nullptr)
			LOG(INFO)  << ", pRspInfo is nullptr!" << endl;
		else
			LOG(INFO)  << ", ErrorID: " << pRspInfo->ErrorID << ", ErrorMsg : " << pRspInfo->ErrorMsg << endl;
		m_lag_Timer.expires_from_now(boost::posix_time::milliseconds(3000));
		m_lag_Timer.async_wait(boost::bind(&tradeAdapterCTP::confirmSettlementInfo, this));
	}
};
int tradeAdapterCTP::confirmSettlementInfo()
{
	CThostFtdcSettlementInfoConfirmField* pConfirm = new CThostFtdcSettlementInfoConfirmField();
	memset(pConfirm, 0, sizeof(CThostFtdcSettlementInfoConfirmField));
	strncpy(pConfirm->BrokerID, m_loginField.BrokerID, sizeof(pConfirm->BrokerID) - 1);
	strncpy(pConfirm->InvestorID, m_loginField.UserID, sizeof(pConfirm->InvestorID) - 1);
	m_pUserApi->ReqSettlementInfoConfirm(pConfirm, ++m_requestId);
	return 0;
};


///投资者结算结果确认响应
void tradeAdapterCTP::OnRspSettlementInfoConfirm(CThostFtdcSettlementInfoConfirmField *pSettlementInfoConfirm, 
	CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	if (pSettlementInfoConfirm)
	{
		if (bIsLast)
		{
			LOG(INFO) << m_adapterID << ": resp | confirm Settlement info succ." << endl;
			LOG(WARNING) << "---------- " << m_adapterID << " init done ----------" << endl;
			if (m_OnUserLogin != NULL)
				m_OnUserLogin(m_adapterID);

			//m_lag_Timer.expires_from_now(boost::posix_time::milliseconds(3000));
			//m_lag_Timer.async_wait(boost::bind(&tradeAdapterCTP::queryAllInstrument, this));
		}
	}
	else
	{
		LOG(INFO) << m_adapterID << ": resp | confirm Settlement info fail";
		if (pRspInfo == nullptr)
			LOG(INFO) << ", pRspInfo is nullptr!" << endl;
		else
			LOG(INFO) << ", ErrorID: " << pRspInfo->ErrorID << ", ErrorMsg : " << pRspInfo->ErrorMsg << endl;
		m_lag_Timer.expires_from_now(boost::posix_time::milliseconds(3000));
		m_lag_Timer.async_wait(boost::bind(&tradeAdapterCTP::queryAllInstrument, this));
	}
};

int tradeAdapterCTP::queryAllInstrument()
{
	CThostFtdcQryInstrumentField qryInstrument;
	memset(&qryInstrument, 0, sizeof(CThostFtdcQryInstrumentField));
	int ret = m_pUserApi->ReqQryInstrument(&qryInstrument, ++m_requestId);
	LOG(INFO)  << m_adapterID << ":  req | query all instruments ... " << ((ret == 0) ? "succ" : "fail") << endl;
	return ret;
};

void tradeAdapterCTP::OnRspQryInstrument(CThostFtdcInstrumentField *pInstrument, CThostFtdcRspInfoField *pRspInfo,
	int nRequestID, bool bIsLast)
{
	if (pInstrument != nullptr)
	{
		if (m_OnInstrumentsRtn)
			m_OnInstrumentsRtn(m_adapterID, pInstrument);
		if (bIsLast)
		{
			LOG(INFO)  << m_adapterID << ": resp | query instrument done." << endl;
			LOG(WARNING)  << "---------- " << m_adapterID << " init done ----------" << endl;
			if (m_OnUserLogin != NULL)
			{
				m_OnUserLogin(m_adapterID);
			}
		}
		//LOG(INFO)  << "合约代码" << pInstrument->InstrumentID << endl
		//<< "交易所代码" << pInstrument->ExchangeID << endl
		//<< "合约名称" << pInstrument->InstrumentName << endl
		//<< "合约在交易所的代码" << pInstrument->ExchangeInstID << endl
		//<< "产品代码" << pInstrument->ProductID << endl
		//<< "产品类型" << pInstrument->ProductClass << endl
		//<< "交割年份" << pInstrument->DeliveryYear << endl
		//<< "交割月" << pInstrument->DeliveryMonth << endl
		//<< "市价单最大下单量" << pInstrument->MaxMarketOrderVolume << endl
		//<< "市价单最小下单量" << pInstrument->MinMarketOrderVolume << endl
		//<< "限价单最大下单量" << pInstrument->MaxLimitOrderVolume << endl
		//<< "限价单最小下单量" << pInstrument->MinLimitOrderVolume << endl
		//<< "合约数量乘数" << pInstrument->VolumeMultiple << endl
		//<< "最小变动价位" << pInstrument->PriceTick << endl
		//<< "创建日" << pInstrument->CreateDate << endl
		//<< "上市日" << pInstrument->OpenDate << endl
		//<< "到期日" << pInstrument->ExpireDate << endl
		//<< "开始交割日" << pInstrument->StartDelivDate << endl
		//<< "结束交割日" << pInstrument->EndDelivDate << endl
		//<< "合约生命周期状态" << pInstrument->InstLifePhase << endl
		//<< "当前是否交易" << pInstrument->IsTrading << endl
		//<< "持仓类型" << pInstrument->PositionType << endl
		//<< "持仓日期类型" << pInstrument->PositionDateType << endl
		//<< "多头保证金率" << pInstrument->LongMarginRatio << endl
		//<< "空头保证金率" << pInstrument->ShortMarginRatio << endl
		//<< "是否使用大额单边保证金算法" << pInstrument->MaxMarginSideAlgorithm << endl
		//<< "基础商品代码" << pInstrument->UnderlyingInstrID << endl
		//<< "执行价" << pInstrument->StrikePrice << endl
		//<< "期权类型" << pInstrument->OptionsType << endl
		//<< "合约基础商品乘数" << pInstrument->UnderlyingMultiple << endl
		//<< "组合类型" << pInstrument->CombinationType << endl;
		return;
	}
	else
	{
		LOG(INFO)  << m_adapterID << ": resp | query instrument fail ";
		if (pRspInfo == nullptr)
			LOG(INFO)  << endl;
		else
			LOG(INFO)  << m_adapterID << ": ErrorID: " << pRspInfo->ErrorID << ", ErrorMsg : " << pRspInfo->ErrorMsg << endl;
	}
}

int tradeAdapterCTP::OrderInsert(string instrument, string exchange, char priceType, char dir,
	char ComOffsetFlag, char ComHedgeFlag, double price,
	int volume, char tmCondition, char volCondition, int minVol, char contiCondition,
	double stopPrz, char forceCloseReason)
{
	CThostFtdcInputOrderField * pInputOrder = new CThostFtdcInputOrderField();
	memset(pInputOrder, 0, sizeof(CThostFtdcInputOrderField));
	strncpy(pInputOrder->BrokerID, m_loginField.BrokerID, sizeof(pInputOrder->BrokerID));
	strncpy(pInputOrder->InvestorID, m_loginField.UserID, sizeof(pInputOrder->InvestorID));
	strncpy(pInputOrder->UserID, m_loginField.UserID, sizeof(pInputOrder->UserID));
	int nextOrderRef = -1;
	{
		//LOG(INFO)  << m_adapterID << ": locking m_orderRefLock in OrderInsert." << endl;
		boost::mutex::scoped_lock l0(m_orderRefLock);
		nextOrderRef = updateOrderRef();
		strncpy(pInputOrder->OrderRef, m_orderRef, sizeof(pInputOrder->OrderRef) - 1);  //报单引用
		//LOG(INFO)  << m_adapterID << ": unlocking m_orderRefLock in OrderInsert." << endl;
	}
	strncpy(pInputOrder->InstrumentID, instrument.c_str(), sizeof(pInputOrder->InstrumentID) - 1);
	strncpy(pInputOrder->ExchangeID, exchange.c_str(), sizeof(pInputOrder->ExchangeID) - 1);
	pInputOrder->OrderPriceType = priceType;///报单价格条件
	pInputOrder->Direction = dir;  ///买卖方向
	pInputOrder->CombOffsetFlag[0] = ComOffsetFlag;///组合开平标志
	pInputOrder->CombHedgeFlag[0] = ComHedgeFlag;///组合投机套保标志

	pInputOrder->LimitPrice = price; ///价格
	pInputOrder->VolumeTotalOriginal = volume;///数量
	pInputOrder->TimeCondition = tmCondition;///有效期类型
	///GTD日期 //good till date 到哪一天有效
	///TThostFtdcDateType	GTDDate;
	pInputOrder->VolumeCondition = volCondition;///成交量类型
	pInputOrder->MinVolume = minVol;///最小成交量
	pInputOrder->ContingentCondition = contiCondition;///触发条件
	pInputOrder->StopPrice = stopPrz;///止损价
	pInputOrder->ForceCloseReason = forceCloseReason;///强平原因

	///TThostFtdcBoolType	IsAutoSuspend;///自动挂起标志 默认 0
	///TThostFtdcBusinessUnitType	BusinessUnit;///业务单元

	int reqId = ++m_requestId;
	pInputOrder->RequestID = reqId;///req编号
	///TThostFtdcBoolType	UserForceClose;///用户强评标志 默认 0
	///TThostFtdcBoolType  IsSwapOrder; ///互换单标志

	int ret = m_pUserApi->ReqOrderInsert(pInputOrder, reqId);

	if (ret == 0)
	{
		LOG(INFO) << m_adapterID << ": req | order insert succ, orderRef: " << nextOrderRef <<
			", inst: " << instrument<< ", price: "<< price << ", volume: "<<volume<< endl;
		return nextOrderRef;
	}
	else
	{
		LOG(INFO) << m_adapterID << ": req | order insert fail! retCode: " << ret << endl;
		return -1;
	}
}

//报单失败才返回
void tradeAdapterCTP::OnRspOrderInsert(CThostFtdcInputOrderField *pInputOrder, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	if (!isErrorRespInfo(pRspInfo)){
		if (pInputOrder)
			LOG(INFO)  << m_adapterID << ":resp | order insert succ, orderRef: " << pInputOrder->OrderRef << endl;
	}
	else
		LOG(INFO)  << m_adapterID << ":resp | order insert fail, ErrorID: " << pRspInfo->ErrorID << ", ErrorMsg: " << pRspInfo->ErrorMsg << endl;
};

void tradeAdapterCTP::OnErrRtnOrderInsert(CThostFtdcInputOrderField *pInputOrder, CThostFtdcRspInfoField *pRspInfo)
{
	if (!isErrorRespInfo(pRspInfo)){
		if (pInputOrder)
			LOG(INFO)  << "resp | order insert succ, orderRef: " << pInputOrder->OrderRef << endl;
	}
	else
		LOG(INFO)  << "resp | order insert fail, ErrorID: " << pRspInfo->ErrorID << ", ErrorMsg: " << pRspInfo->ErrorMsg << endl;
};

void tradeAdapterCTP::queryOrder()
{
	boost::mutex::scoped_lock lock(m_qryOrderLock);
	if (!m_qryingOrder)
	{
		CThostFtdcQryOrderField qryOrder;
		memset(&qryOrder, 0, sizeof(CThostFtdcQryOrderField));
		strncpy(qryOrder.BrokerID, m_loginField.BrokerID, sizeof(qryOrder.BrokerID) - 1);
		strncpy(qryOrder.InvestorID, m_loginField.UserID, sizeof(qryOrder.InvestorID) - 1);
		m_pUserApi->ReqQryOrder(&qryOrder, ++m_requestId);
		LOG(WARNING) << m_adapterID << ": Req | query order start ..." << endl;
		closeOrderQrySwitch();
		m_qryOrder_Timer.expires_from_now(boost::posix_time::millisec(1000 * 60 * 3)); //三分钟后打开查询
		m_qryOrder_Timer.async_wait(boost::bind(&tradeAdapterCTP::openOrderQrySwitch, this));
	}
	else
		LOG(WARNING) << m_adapterID << ": query order is in process, no more query lunched." << endl;
};

void tradeAdapterCTP::OnRspQryOrder(CThostFtdcOrderField *pOrder, CThostFtdcRspInfoField *pRspInfo, 
	int nRequestID, bool bIsLast)
{
	if (pOrder){
		CThostFtdcOrderFieldPtr orderPtr = CThostFtdcOrderFieldPtr(new CThostFtdcOrderField(*pOrder));
		int orderRef = atoi(pOrder->OrderRef);
		{
			boost::mutex::scoped_lock l(m_ref2order_lock);
			m_ref2order[orderRef] = orderPtr;
		}
		if (m_OnOrderRtn)
			m_OnOrderRtn(m_adapterID, pOrder);
	}
	if (bIsLast) //返回报单完成
	{
		boost::mutex::scoped_lock lock(m_qryOrderLock);
		openOrderQrySwitch();
		m_cancelQryTimer = true;
		m_qryOrder_Timer.cancel();
	}
};

void tradeAdapterCTP::OnRtnOrder(CThostFtdcOrderField *pOrder)
{
	CThostFtdcOrderFieldPtr orderPtr = CThostFtdcOrderFieldPtr(new CThostFtdcOrderField(*pOrder));
	int orderRef = atoi(pOrder->OrderRef);
	{
		//LOG(INFO)  << m_adapterID << ": locking m_ref2order in OnRtnOrder." << endl;
		boost::mutex::scoped_lock l(m_ref2order_lock);
		m_ref2order[orderRef] = orderPtr;
		//LOG(INFO)  << m_adapterID << ": unlocking m_ref2order in OnRtnOrder." << endl;
	}
	if (m_OnOrderRtn)
		m_OnOrderRtn(m_adapterID, pOrder);
	
	LOG(INFO) << m_adapterID << " Rsp | order Rtn: orderRef: " << pOrder->OrderRef //<< pOrder->BrokerOrderSeq
	<< ", InstrumentID:" << pOrder->InstrumentID
	<< ", Direction:" << pOrder->Direction
	<< ", LimitPrice:" << pOrder->LimitPrice
	<< ", OrderStatus:" << pOrder->OrderStatus
	<< ", StatusMsg:" << pOrder->StatusMsg
	//<< ", CombHedgeFlag:" << pOrder->CombHedgeFlag
	//<< ", CombOffsetFlag:" << pOrder->CombOffsetFlag
	//<< ", MinVolume:" << pOrder->MinVolume
	//<< ", OrderPriceType:" << pOrder->OrderPriceType
	//<< ", orderRef:" << pOrder->OrderRef
	<< endl;
};

void tradeAdapterCTP::OnRtnTrade(CThostFtdcTradeField *pTrade)
{
	if (m_OnTradeRtn)
		m_OnTradeRtn(m_adapterID, pTrade);
	LOG(INFO) << m_adapterID<< " Rsp | trade Rtn: orderRef: " << pTrade->OrderRef //<< pOrder->BrokerOrderSeq
		<< ", tradeTime:" << pTrade->TradeTime
		<< ", InstrumentID:" << pTrade->InstrumentID
		<< ", Direction:" << pTrade->Direction
		<< ", Price:" << pTrade->Price
		<< ", volume:" << pTrade->Volume
		//<< ", CombHedgeFlag:" << pOrder->CombHedgeFlag
		//<< ", CombOffsetFlag:" << pOrder->CombOffsetFlag
		//<< ", MinVolume:" << pOrder->MinVolume
		//<< ", OrderPriceType:" << pOrder->OrderPriceType
		//<< ", orderRef:" << pOrder->OrderRef
		<< endl;
};

int tradeAdapterCTP::cancelOrder(int orderRef)
{
	if (orderRef == 0)
		cout << "debug" << endl;
	auto iter = m_ref2order.begin();
	{
		//LOG(INFO)  << m_adapterID << ": locking m_ref2order in cancelOrder." << endl;
		boost::mutex::scoped_lock l0(m_ref2order_lock);
		iter = m_ref2order.find(orderRef);
		//LOG(INFO)  << m_adapterID << ": unlocking m_ref2order in cancelOrder." << endl;
	}
	if (iter == m_ref2order.end())
	{
		LOG(WARNING) << m_adapterID << ": cancel Order fail | order return not received, orderRef: " << orderRef << endl;
		return ORDER_CANCEL_ERROR_NOT_FOUND;
	}
	CThostFtdcInputOrderActionField actionField;
	memset(&actionField, 0, sizeof(actionField));
	actionField.ActionFlag = THOST_FTDC_AF_Delete;
	actionField.FrontID = iter->second->FrontID;
	actionField.SessionID = iter->second->SessionID;
	int nextOrderRef = -1;
	{
		boost::mutex::scoped_lock l2(m_orderRefLock);
		nextOrderRef = updateOrderRef();
		actionField.OrderActionRef = nextOrderRef;
	}
	sprintf(actionField.OrderRef, "%012d", orderRef);
	strncpy(actionField.BrokerID, m_loginField.BrokerID, sizeof(actionField.BrokerID));
	strncpy(actionField.InvestorID, m_loginField.UserID, sizeof(actionField.InvestorID));
	strncpy(actionField.UserID, m_loginField.UserID, sizeof(actionField.UserID));
	strncpy(actionField.InstrumentID, iter->second->InstrumentID, sizeof(actionField.InstrumentID));
	int ret = m_pUserApi->ReqOrderAction(&actionField, ++m_requestId);
	if (ret == 0)
	{
		LOG(INFO)  << m_adapterID << ": req | cancel order succ, orderRef: " << orderRef << endl;
		return nextOrderRef;
	}
	else
	{
		LOG(INFO)  << m_adapterID << ": req | cancel order fail, orderRef: " << orderRef << endl;
		return ORDER_CANCEL_ERROR_SEND_FAIL;
	}
};

//撤单失败才会调用
void tradeAdapterCTP::OnRspOrderAction(CThostFtdcInputOrderActionField *pInputOrderAction,
	CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	/*if (!isErrorRespInfo(pRspInfo)){
		if (pInputOrderAction)
			LOG(INFO)  << m_adapterID << ":resp | send order action succ, orderRef: " << pInputOrderAction->OrderRef << ", orderActionRef: " << pInputOrderAction->OrderActionRef << endl;
	}
	else
		LOG(INFO)  << m_adapterID << ":resp | send order action fail, ErrorID: " << pRspInfo->ErrorID << ", ErrorMsg: " << pRspInfo->ErrorMsg << endl;*/
};

void tradeAdapterCTP::OnErrRtnOrderAction(CThostFtdcOrderActionField *pOrderAction, CThostFtdcRspInfoField *pRspInfo)
{
	if (m_onErrRtnOrderAction)
		m_onErrRtnOrderAction(m_adapterID, pOrderAction, pRspInfo);
	if (isErrorRespInfo(pRspInfo))
		LOG(INFO)  << m_adapterID << ":resp | send order action fail, OrderRef:" << pOrderAction->OrderRef << ", ErrorID: " << pRspInfo->ErrorID << ", ErrorMsg: " << pRspInfo->ErrorMsg << endl;
};

bool tradeAdapterCTP::isErrorRespInfo(CThostFtdcRspInfoField *pRspInfo)
{
	if (pRspInfo == nullptr || pRspInfo->ErrorID != 0)
		return true;
	return false;
};
