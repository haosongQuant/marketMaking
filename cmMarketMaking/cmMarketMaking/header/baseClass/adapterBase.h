#pragma once
#include <string>
using namespace std;

enum adapter_status
{
	ADAPTER_STATUS_NEW,
	ADAPTER_STATUS_CONNECTING,
	ADAPTER_STATUS_LOGIN,
	ADAPTER_STATUS_LOGOUT,
	ADAPTER_STATUS_DISCONNECT,
	ADAPTER_STATUS_TONEW,
	ADAPTER_STATUS_NA,
};

class adapterBase
{
public:
	string m_adapterID;
	adapter_status m_status;
public:
	adapterBase():m_status(ADAPTER_STATUS_NEW){};
	adapterBase(string adapterID) :m_adapterID(adapterID), m_status(ADAPTER_STATUS_NEW){};
	virtual void destroyAdapter(){};
};

class traderAdapterBase :public adapterBase
{
public:
	virtual int OrderInsert(string code, //合约代码
		char   dir,    //交易方向
		string kp,    //开平仓
		string price,  //价格
		int    vol,    //数量
		string exchange,  //交易所
		char   priceType //报单类型
		){
		return 0;
	};
	virtual int OrderInsert(string instrument, char priceType, char dir,
		char ComOffsetFlag, char ComHedgeFlag, double price,
		int volume, char tmCondition, char volCondition, int minVol, char contiCondition,
		double stopPrz, char forceCloseReason){
		return 0;
	};

	virtual int OrderInsert(string instrument, string exchange, char orderType, char dir,
		char positionEffect, char ComHedgeFlag, double price, unsigned int volume){
		return 0;
	};

	virtual int cancelOrder(int key) = 0;

	virtual void destroyAdapter(){};
};

class quoteAdapterBase :public adapterBase
{
public:
	virtual void Subscribe(string instIdList, string exchange) = 0;
	virtual void UnSubscribe(string instIdList, string exchange) = 0;
};

class queryAdapterBase : public adapterBase
{

};
