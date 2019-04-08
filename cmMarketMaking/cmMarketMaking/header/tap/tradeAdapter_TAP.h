#pragma once
#include <iostream>
#include <map>
#include "tap/TapTradeAPI.h"
#include "threadpool/threadpool.h"
#include <boost/shared_ptr.hpp>
#include <boost/smart_ptr/detail/spinlock.hpp>
#include <boost\function.hpp>
#include <boost\asio.hpp>
#include <boost\bind.hpp>
#include "baseClass/adapterBase.h"

using namespace std;

struct orderRefInfo
{
	TAPIINT32					RefInt;							///< 整型参考值
	TAPISTR_50					RefString;						///< 字符串参考值
	TAPICHAR					ServerFlag;						///< 服务器标识
	TAPISTR_20					OrderNo;						///< 委托编码
};

typedef boost::shared_ptr<orderRefInfo> orderRefInfoPtr;

class  tradeAdapter_TAP : public traderAdapterBase, public ITapTradeAPINotify
{

private:
	string m_adapterID;
	int    m_requestId = 0;
	bool   m_isApiReady;
	ITapTradeAPI*         m_pApi;
	TapAPIApplicationInfo m_stAppInfo;
	TapAPITradeLoginAuth  m_stLoginAuth;

	char m_orderRef[13];
	map<int, orderRefInfoPtr> m_ref2order;
	boost::detail::spinlock m_ref2order_lock;

public:
	tradeAdapter_TAP(string adapterID, TAPIAUTHCODE authCode, TAPISTR_300 keyOpLogPath, \
		TAPICHAR * ip_address, TAPIUINT16 port, char * user, char * pwd,  athenathreadpoolPtr tp);
	~tradeAdapter_TAP();
	virtual void destroyAdapter();

	int init();
	int login();
	bool isAdapterReady(){ return m_isApiReady; };

	//int queryTradingAccount();//查询资金
	//int queryInvestorPosition();//查询持仓
	//int queryAllInstrument();//查询全部合约

	//下单
	virtual int OrderInsert(string instrument, string exchange, char orderType, char dir,
		char positionEffect, double price, unsigned int volume);
	//撤单
	virtual int cancelOrder(int orderRef);
private:
	void splitInstId(string instId, char* commodity, char* contract);

private:
	athenathreadpoolPtr m_threadpool;
	athena_lag_timer    m_lag_Timer;

public:
	boost::function<void(string adapterID)> m_OnUserLogin;
	boost::function<void(string adapterID, string adapterType)> m_OnFrontDisconnected;
	boost::function<void(string, TapAPIOrderInfoNotice *)> m_OnOrderRtn;
	boost::function<void(string, TapAPIFillInfo *)> m_OnTradeRtn;
	//boost::function<void(string, CThostFtdcInstrumentField*)> m_OnInstrumentsRtn;
	//boost::function<void(CThostFtdcInvestorPositionField*)> m_OnInvestorPositionRtn;


public:

	/**
	* @brief 连接成功回调通知
	* @ingroup G_T_Login
	*/
	virtual void TAP_CDECL OnConnect();
	/**
	* @brief	系统登录过程回调。
	* @details	此函数为Login()登录函数的回调，调用Login()成功后建立了链路连接，然后API将向服务器发送登录认证信息，
	*			登录期间的数据发送情况和登录的回馈信息传递到此回调函数中。
	* @param[in] errorCode 返回错误码,0表示成功。
	* @param[in] loginRspInfo 登陆应答信息，如果errorCode!=0，则loginRspInfo=NULL。
	* @attention	该回调返回成功，说明用户登录成功。但是不代表API准备完毕。
	* @ingroup G_T_Login
	*/
	virtual void TAP_CDECL OnRspLogin(TAPIINT32 errorCode, const TapAPITradeLoginRspInfo *loginRspInfo);
	/**
	* @brief	通知用户API准备就绪。
	* @details	只有用户回调收到此就绪通知时才能进行后续的各种行情数据查询操作。\n
	*			此回调函数是API能否正常工作的标志。
	* @attention 就绪后才可以进行后续正常操作
	* @ingroup G_T_Login
	*/
	virtual void TAP_CDECL OnAPIReady();
	/**
	* @brief	API和服务失去连接的回调
	* @details	在API使用过程中主动或者被动与服务器服务失去连接后都会触发此回调通知用户与服务器的连接已经断开。
	* @param[in] reasonCode 断开原因代码。
	* @ingroup G_T_Disconnect
	*/
	virtual void TAP_CDECL OnDisconnect(TAPIINT32 reasonCode);
	/**
	* @brief 通知用户密码修改结果
	* @param[in] sessionID 修改密码的会话ID,和ChangePassword返回的会话ID对应。
	* @param[in] errorCode 返回错误码，0表示成功。
	* @ingroup G_T_UserInfo
	*/
	virtual void TAP_CDECL OnRspChangePassword(TAPIUINT32 sessionID, TAPIINT32 errorCode){};
	/**
	* @brief 设置用户预留信息反馈
	* @param[in] sessionID 设置用户预留信息的会话ID
	* @param[in] errorCode 返回错误码，0表示成功。
	* @param[in] info 指向返回的信息结构体。当errorCode不为0时，info为空。
	* @attention 不要修改和删除info所指示的数据；函数调用结束，参数不再有效。
	* @note 该接口暂未实现
	* @ingroup G_T_UserInfo
	*/
	virtual void TAP_CDECL OnRspSetReservedInfo(TAPIUINT32 sessionID, TAPIINT32 errorCode, const TAPISTR_50 info){};
	/**
	* @brief	返回用户信息
	* @details	此回调接口向用户返回查询的资金账号的详细信息。用户有必要将得到的账号编号保存起来，然后在后续的函数调用中使用。
	* @param[in] sessionID 请求的会话ID；
	* @param[in] errorCode 错误码。0 表示成功。
	* @param[in] isLast 标示是否是最后一批数据；
	* @param[in] info 指向返回的信息结构体。当errorCode不为0时，info为空。
	* @attention 不要修改和删除info所指示的数据；函数调用结束，参数不再有效。
	* @ingroup G_T_AccountInfo
	*/
	virtual void TAP_CDECL OnRspQryAccount(TAPIUINT32 sessionID, TAPIUINT32 errorCode, TAPIYNFLAG isLast, const TapAPIAccountInfo *info){};
	/**
	* @brief 返回资金账户的资金信息
	* @param[in] sessionID 请求的会话ID；
	* @param[in] errorCode 错误码。0 表示成功。
	* @param[in] isLast 	标示是否是最后一批数据；
	* @param[in] info		指向返回的信息结构体。当errorCode不为0时，info为空。
	* @attention 不要修改和删除info所指示的数据；函数调用结束，参数不再有效。
	* @ingroup G_T_AccountDetails
	*/
	virtual void TAP_CDECL OnRspQryFund(TAPIUINT32 sessionID, TAPIINT32 errorCode, TAPIYNFLAG isLast, const TapAPIFundData *info){};
	/**
	* @brief	用户资金变化通知
	* @details	用户的委托成交后会引起资金数据的变化，因此需要向用户实时反馈。
	* @param[in] info		指向返回的信息结构体。当errorCode不为0时，info为空。
	* @note 如果不关注此项内容，可以设定Login时的NoticeIgnoreFlag以屏蔽。
	* @attention 不要修改和删除info所指示的数据；函数调用结束，参数不再有效。
	* @ingroup G_T_AccountDetails
	*/
	virtual void TAP_CDECL OnRtnFund(const TapAPIFundData *info){};
	/**
	* @brief 返回系统中的交易所信息
	* @param[in] sessionID 请求的会话ID；
	* @param[in] errorCode 错误码。0 表示成功。
	* @param[in] isLast 	标示是否是最后一批数据；
	* @param[in] info		指向返回的信息结构体。当errorCode不为0时，info为空。
	* @attention 不要修改和删除info所指示的数据；函数调用结束，参数不再有效。
	* @ingroup G_T_TradeSystem
	*/
	virtual void TAP_CDECL OnRspQryExchange(TAPIUINT32 sessionID, TAPIINT32 errorCode, TAPIYNFLAG isLast, const TapAPIExchangeInfo *info){};
	/**
	* @brief	返回系统中品种信息
	* @details	此回调接口用于向用户返回得到的所有品种信息。
	* @param[in] sessionID 请求的会话ID，和GetAllCommodities()函数返回对应；
	* @param[in] errorCode 错误码。0 表示成功。
	* @param[in] isLast 	标示是否是最后一批数据；
	* @param[in] info		指向返回的信息结构体。当errorCode不为0时，info为空。
	* @attention 不要修改和删除info所指示的数据；函数调用结束，参数不再有效。
	* @ingroup G_T_Commodity
	*/
	virtual void TAP_CDECL OnRspQryCommodity(TAPIUINT32 sessionID, TAPIINT32 errorCode, TAPIYNFLAG isLast, const TapAPICommodityInfo *info){};
	/**
	* @brief 返回系统中合约信息
	* @param[in] sessionID 请求的会话ID；
	* @param[in] errorCode 错误码。0 表示成功。
	* @param[in] isLast 	标示是否是最后一批数据；
	* @param[in] info		指向返回的信息结构体。当errorCode不为0时，info为空。
	* @attention 不要修改和删除info所指示的数据；函数调用结束，参数不再有效。
	* @ingroup G_T_Contract
	*/
	virtual void TAP_CDECL OnRspQryContract(TAPIUINT32 sessionID, TAPIINT32 errorCode, TAPIYNFLAG isLast, const TapAPITradeContractInfo *info){};
	/**
	* @brief	返回新增合约信息
	* @details	向用户推送新的合约。主要用来处理在交易时间段中服务器添加了新合约时，向用户发送这个合约的信息。
	* @param[in] info		指向返回的信息结构体。当errorCode不为0时，info为空。
	* @attention 不要修改和删除info所指示的数据；函数调用结束，参数不再有效。
	* @ingroup G_T_Contract
	*/
	virtual void TAP_CDECL OnRtnContract(const TapAPITradeContractInfo *info){};
	/**
	* @brief 返回新委托。新下的或者其他地方下的推送过来的。
	* @details	服务器接收到客户下的委托内容后就会保存起来等待触发，同时向用户回馈一个
	*			新委托信息说明服务器正确处理了用户的请求，返回的信息中包含了全部的委托信息，
	*			同时有一个用来标示此委托的委托号。
	* @param[in] info 指向返回的信息结构体。当errorCode不为0时，info为空。
	* @note 如果不关注此项内容，可以设定Login时的NoticeIgnoreFlag以屏蔽。
	* @attention 不要修改和删除info所指示的数据；函数调用结束，参数不再有效。
	* @ingroup G_T_TradeActions
	*/
	virtual void TAP_CDECL OnRtnOrder(const TapAPIOrderInfoNotice *info);
	/**
	* @brief	返回对报单的主动操作结果
	* @details	如下单，撤单等操作的结果。
	* @param[in] sessionID 请求的会话ID；
	* @param[in] errorCode 错误码。0 表示成功。
	* @param[in] info 报单的具体信息。当errorCode不为0时，info为空。
	* @attention 不要修改和删除info所指示的数据；函数调用结束，参数不再有效。
	* @note 该接口目前没有用到，所有操作结果通过OnRtnOrder返回
	* @ingroup G_T_TradeActions
	*/
	virtual void TAP_CDECL OnRspOrderAction(TAPIUINT32 sessionID, TAPIUINT32 errorCode, const TapAPIOrderActionRsp *info);
	/**
	* @brief	返回查询的委托信息
	* @details	返回用户查询的委托的具体信息。
	* @param[in] sessionID 请求的会话ID；
	* @param[in] errorCode 错误码。0 表示成功。
	* @param[in] isLast 标示是否是最后一批数据；
	* @param[in] info 指向返回的信息结构体。当errorCode不为0时，info为空。
	* @attention 不要修改和删除info所指示的数据；函数调用结束，参数不再有效。
	* @ingroup G_T_TradeInfo
	*/
	virtual void TAP_CDECL OnRspQryOrder(TAPIUINT32 sessionID, TAPIINT32 errorCode, TAPIYNFLAG isLast, const TapAPIOrderInfo *info){};
	/**
	* @brief 返回查询的委托变化流程信息
	* @param[in] sessionID 请求的会话ID；
	* @param[in] errorCode 错误码，当errorCode==0时，info指向返回的委托变化流程结构体，不然为NULL；
	* @param[in] isLast 标示是否是最后一批数据；
	* @param[in] info 返回的委托变化流程指针。
	* @attention 不要修改和删除info所指示的数据；函数调用结束，参数不再有效。
	* @ingroup G_T_TradeInfo
	*/
	virtual void TAP_CDECL OnRspQryOrderProcess(TAPIUINT32 sessionID, TAPIINT32 errorCode, TAPIYNFLAG isLast, const TapAPIOrderInfo *info){};
	/**
	* @brief 返回查询的成交信息
	* @param[in] sessionID 请求的会话ID；
	* @param[in] errorCode 错误码。0 表示成功。
	* @param[in] isLast 	标示是否是最后一批数据；
	* @param[in] info		指向返回的信息结构体。当errorCode不为0时，info为空。
	* @attention 不要修改和删除info所指示的数据；函数调用结束，参数不再有效。
	* @ingroup G_T_TradeInfo
	*/
	virtual void TAP_CDECL OnRspQryFill(TAPIUINT32 sessionID, TAPIINT32 errorCode, TAPIYNFLAG isLast, const TapAPIFillInfo *info){};
	/**
	* @brief	推送来的成交信息
	* @details	用户的委托成交后将向用户推送成交信息。
	* @param[in] info		指向返回的信息结构体。当errorCode不为0时，info为空。
	* @note 如果不关注此项内容，可以设定Login时的NoticeIgnoreFlag以屏蔽。
	* @attention 不要修改和删除info所指示的数据；函数调用结束，参数不再有效。
	* @ingroup G_T_TradeActions
	*/
	virtual void TAP_CDECL OnRtnFill(const TapAPIFillInfo *info);
	/**
	* @brief 返回查询的持仓
	* @param[in] sessionID 请求的会话ID；
	* @param[in] errorCode 错误码。0 表示成功。
	* @param[in] isLast 	标示是否是最后一批数据；
	* @param[in] info		指向返回的信息结构体。当errorCode不为0时，info为空。
	* @attention 不要修改和删除info所指示的数据；函数调用结束，参数不再有效。
	* @ingroup G_T_TradeInfo
	*/
	virtual void TAP_CDECL OnRspQryPosition(TAPIUINT32 sessionID, TAPIINT32 errorCode, TAPIYNFLAG isLast, const TapAPIPositionInfo *info){};
	/**
	* @brief 持仓变化推送通知
	* @param[in] info		指向返回的信息结构体。当errorCode不为0时，info为空。
	* @note 如果不关注此项内容，可以设定Login时的NoticeIgnoreFlag以屏蔽。
	* @attention 不要修改和删除info所指示的数据；函数调用结束，参数不再有效。
	* @ingroup G_T_TradeActions
	*/
	virtual void TAP_CDECL OnRtnPosition(const TapAPIPositionInfo *info){};
	/**
	* @brief 返回查询的平仓
	* @param[in] sessionID 请求的会话ID；
	* @param[in] errorCode 错误码。0 表示成功。
	* @param[in] isLast 	标示是否是最后一批数据；
	* @param[in] info		指向返回的信息结构体。当errorCode不为0时，info为空。
	* @attention 不要修改和删除info所指示的数据；函数调用结束，参数不再有效。
	* @ingroup G_T_TradeInfo
	*/
	virtual void TAP_CDECL OnRspQryClose(TAPIUINT32 sessionID, TAPIINT32 errorCode, TAPIYNFLAG isLast, const TapAPICloseInfo *info){};
	/**
	* @brief 平仓数据变化推送
	* @param[in] info		指向返回的信息结构体。当errorCode不为0时，info为空。
	* @note 如果不关注此项内容，可以设定Login时的NoticeIgnoreFlag以屏蔽。
	* @attention 不要修改和删除info所指示的数据；函数调用结束，参数不再有效。
	* @ingroup G_T_TradeActions
	*/
	virtual void TAP_CDECL OnRtnClose(const TapAPICloseInfo *info){};
	/**
	* @brief 持仓盈亏通知
	* @param[in] info		指向返回的信息结构体。当errorCode不为0时，info为空。
	* @note 如果不关注此项内容，可以设定Login时的NoticeIgnoreFlag以屏蔽。
	* @attention 不要修改和删除info所指示的数据；函数调用结束，参数不再有效。
	* @ingroup G_T_TradeActions
	*/
	virtual void TAP_CDECL OnRtnPositionProfit(const TapAPIPositionProfitNotice *info){};
	/**
	* @brief 深度行情查询应答
	* @param[in] sessionID 请求的会话ID；
	* @param[in] errorCode 错误码。0 表示成功。
	* @param[in] isLast 	标示是否是最后一批数据；
	* @param[in] info	  指向返回的深度行情信息结构体。当errorCode不为0时，info为空。
	* @attention 不要修改和删除info所指示的数据；函数调用结束，参数不再有效。
	* @ingroup G_T_DeepQuote
	*/
	virtual void TAP_CDECL OnRspQryDeepQuote(TAPIUINT32 sessionID, TAPIINT32 errorCode, TAPIYNFLAG isLast, const TapAPIDeepQuoteQryRsp *info){};
	/**
	* @brief 交易所时间状态信息查询应答
	* @param[in] sessionID 请求的会话ID；
	* @param[in] errorCode 错误码。0 表示成功。
	* @param[in] isLast 	标示是否是最后一批数据
	* @param[in] info		指向返回的信息结构体。当errorCode不为0时，info为空。
	* @attention  不要修改和删除info所指示的数据；函数调用结束，参数不再有效。
	* @ingroup G_T_TradeSystem
	*/
	virtual void TAP_CDECL OnRspQryExchangeStateInfo(TAPIUINT32 sessionID, TAPIINT32 errorCode, TAPIYNFLAG isLast, const TapAPIExchangeStateInfo * info){};
	/**
	* @brief 交易所时间状态信息通知
	* @param[in] info		指向返回的信息结构体。当errorCode不为0时，info为空。
	* @attention  不要修改和删除info所指示的数据；函数调用结束，参数不再有效。
	* @ingroup G_T_TradeSystem
	*/
	virtual void TAP_CDECL OnRtnExchangeStateInfo(const TapAPIExchangeStateInfoNotice * info){};
	/**
	* @brief 询价通知
	* @param[in] info		指向返回的信息结构体。当errorCode不为0时，info为空。
	* @attention 不要修改和删除info所指示的数据；函数调用结束，参数不再有效。
	* @ingroup G_T_TradeActions
	*/
	virtual void TAP_CDECL OnRtnReqQuoteNotice(const TapAPIReqQuoteNotice *info){};

	/**
	* @brief 上手信息查询应答
	* @param[in] sessionID 请求的会话ID；
	* @param[in] errorCode 错误码。0 表示成功。
	* @param[in] isLast 	标示是否是最后一批数据
	* @param[in] info		指向返回的信息结构体。当errorCode不为0时，info为空。
	* @attention  不要修改和删除info所指示的数据；函数调用结束，参数不再有效。
	* @ingroup G_T_UpperChannelInfo
	*/
	virtual void TAP_CDECL OnRspUpperChannelInfo(TAPIUINT32 sessionID, TAPIINT32 errorCode, TAPIYNFLAG isLast, const TapAPIUpperChannelInfo * info){};
	/**
	* @brief 客户最终费率应答
	* @details   保证金比例计算方式：手数*每手乘数*计算参数*价格\n
	*             保证金定额计算方式：手数*计算参数\n
	*             手续费绝对方式计算方式：手数*按手数计算参数+手数*每手乘数*价格*按金额计算参数
	* @param[in] sessionID 请求的会话ID；
	* @param[in] errorCode 错误码。0 表示成功。
	* @param[in] isLast 	标示是否是最后一批数据
	* @param[in] info		指向返回的信息结构体。当errorCode不为0时，info为空。
	* @attention  不要修改和删除info所指示的数据；函数调用结束，参数不再有效。
	* @ingroup G_T_AccountRentInfo
	*/
	virtual void TAP_CDECL OnRspAccountRentInfo(TAPIUINT32 sessionID, TAPIINT32 errorCode, TAPIYNFLAG isLast, const TapAPIAccountRentInfo * info){};
};
