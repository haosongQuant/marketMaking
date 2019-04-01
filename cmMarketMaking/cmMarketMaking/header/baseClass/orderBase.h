#pragma once
#include <string>
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

struct orderRtn_struct
{
};
typedef boost::shared_ptr<orderRtn_struct> orderRtnPtr;

struct tradeRtn_struct
{
};
typedef boost::shared_ptr<tradeRtn_struct> tradeRtnPtr;
