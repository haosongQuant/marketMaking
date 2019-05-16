#pragma once
#include <string>
#include <boost/shared_ptr.hpp>
using namespace std;


enum enum_order_type
{
	ORDER_TYPE_MARKET,
	ORDER_TYPE_LIMIT,

};

//! 开平类型
enum enum_position_effect_type
{
	//! 不分开平
	POSITION_EFFECT_NONE,
	//! 开仓
	POSITION_EFFECT_OPEN,
	//! 平仓
	POSITION_EFFECT_CLOSE,
	//! 平当日
	POSITION_EFFECT_CLOSE_TODAY,
	//! 平昨日
	POSITION_EFFECT_CLOSE_YESTERDAY
};

enum enum_order_dir_type
{
	ORDER_DIR_BUY,
	ORDER_DIR_SELL,
};

enum enum_hedge_flag
{
	///投机
	FLAG_SPECULATION,
	///套利
	FLAG_ARBITRAGE,
	///套保
	FLAG_HEDGE,
	///做市商
	FLAG_MARKETMAKER,
};

enum enum_order_error
{
	ORDER_SEND_ERROR_TO_DEFINE = -1000,
	ORDER_CANCEL_ERROR_NOT_FOUND,
	ORDER_CANCEL_ERROR_TRADED,
	ORDER_CANCEL_ERROR_SEND_FAIL,
};

enum enum_order_status
{	///全部成交,
	ORDER_STATUS_AllTraded,
	///部分成交还在队列中,
	ORDER_STATUS_PartTradedQueueing,
	///部分成交不在队列中,
	ORDER_STATUS_PartTradedNotQueueing,
	///未成交还在队列中,
	ORDER_STATUS_NoTradeQueueing,
	///未成交不在队列中,
	ORDER_STATUS_NoTradeNotQueueing,
	///撤单,
	ORDER_STATUS_Canceled,
	///未知,
	ORDER_STATUS_Unknown,
	///尚未触发,
	ORDER_STATUS_NotTouched,
	///已触发,
	ORDER_STATUS_Touched,
	///已触发,
	ORDER_STATUS_TerminatedFromCancel,
};

enum enum_cancel_order_rc
{
	///全部成交或撤销
	CANCEL_RC_TRADED_OR_CANCELED,
	CANCEL_RC_UNDEFINED,
};

struct orderRtn_struct
{
	///报单引用
	int m_orderRef;
	///合约代码
	string m_InstrumentID;
	///报单状态
	enum_order_status m_orderStatus;
	///状态信息
	string m_statusMsg;
	///买卖方向
	enum_order_dir_type	m_direction;
	///价格
	double m_price;
	///数量
	int	   m_VolumeTotalOriginal;
	///今成交数量
	int    m_volumeTraded=0;
	///剩余数量
	int    m_volumeTotal;
	///郑商所成交数量 ??什么鬼
	int	m_ZCETotalTradedVolume;
	///交易日
	string m_tradingDay;
};
typedef boost::shared_ptr<orderRtn_struct> orderRtnPtr;

struct tradeRtn_struct
{
	int m_orderRef;
	string m_exchange;
	string m_instId;
	string m_tradeId;
	//enum_order_type           m_orderTyp;
	enum_position_effect_type m_positionEffectTyp;
	enum_order_dir_type       m_orderDir;
	double m_price;
	double m_volume;
	string m_tradeDate;
};
typedef boost::shared_ptr<tradeRtn_struct> tradeRtnPtr;

struct cancelRtn_struct
{
	int m_cancelOrderRef;
	int m_originOrderRef;
	bool m_isCancelSucc;
	enum_cancel_order_rc m_cancelOrderRc = CANCEL_RC_UNDEFINED;
};
typedef boost::shared_ptr<cancelRtn_struct> cancelRtnPtr;

enum enum_holding_dir_type
{
	HOLDING_DIR_LONG,
	HOLDING_DIR_SHORT,
};
struct investorPosition_struct
{
	string m_instrument;
	int    m_position;
	enum_holding_dir_type m_holdingDirection;
	investorPosition_struct()
	{};
	investorPosition_struct(string instrument, enum_holding_dir_type holdingDirection):
		m_instrument(instrument), m_holdingDirection(holdingDirection), m_position(0)
	{};
};
typedef boost::shared_ptr<investorPosition_struct> investorPositionPtr;