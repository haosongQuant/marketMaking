#pragma once

enum enum_adapterType
{
	ADAPTER_CTP_TRADE,
	ADAPTER_CTP_QUOTE,
	ADAPTER_TAP_TRADE,
	ADAPTER_TAP_QUOTE,
	ADAPTER_ERROR_TYP
};

enum enum_productCategory
{
	CATE_CM_FUTURES,
	CATE_CM_OPTIONS,
};

struct futuresMD_struct
{
	char	TradingDay[9];    ///交易日
	char	InstrumentID[31]; ///合约代码
	char	ExchangeID[9];    ///交易所代码
	//char	ExchangeInstID[31];///合约在交易所的代码
	double	LastPrice; ///最新价
	/*double	PreSettlementPrice;///上次结算价
	double	PreClosePrice;///昨收盘
	double	PreOpenInterest;///昨持仓量
	double	OpenPrice;///今开盘
	double	HighestPrice;///最高价
	double	LowestPrice;///最低价*/
	double	Volume;///数量
	double	Turnover;///成交金额
	double	OpenInterest;///持仓量
	/*double	ClosePrice;///今收盘
	double	SettlementPrice;///本次结算价*/
	/*double	UpperLimitPrice;///涨停板价
	double	LowerLimitPrice;///跌停板价
	double	PreDelta;///昨虚实度
	double	CurrDelta;///今虚实度
	double	PreIOPV;///昨日基金净值
	double	IOPV;///基金净值
	double	AuctionPrice;///动态参考价格*/
	char	UpdateTime[9];///最后修改时间
	int	    UpdateMillisec;///最后修改毫秒
	double  bidprice[10];
	double  bidvol[10];
	double  bidCount[10];
	double  askprice[10];
	double  askvol[10];
	double  askCount[10];
	double	AveragePrice;///当日均价
	//char	ActionDay[9];///业务日期
	//char	TradingPhase;///交易阶段
	//char	OpenRestriction;///开仓限制
	//double	YieldToMaturity;          ///到期收益率
	//double	TradeCount;         ///成交笔数
	//double	TotalTradeVolume;   ///成交总量
	//double	TotalBidVolume;     ///委托买入总量
	//double	WeightedAvgBidPrice;      ///加权平均委买价
	//double	AltWeightedAvgBidPrice;   ///债券加权平均委买价
	//double	TotalOfferVolume;         ///委托卖出总量
	//double	WeightedAvgOfferPrice;    ///加权平均委卖价
	//double	AltWeightedAvgOfferPrice; ///债券加权平均委卖价格
	int	BidPriceLevel;        ///买价深度
	int	OfferPriceLevel;      ///卖价深度
};
typedef boost::shared_ptr<futuresMD_struct> futuresMDPtr;
