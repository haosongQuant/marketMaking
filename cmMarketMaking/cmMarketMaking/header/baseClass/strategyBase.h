#pragma once
#include <string>
#include <boost/shared_ptr.hpp>
#include "baseClass/orderBase.h"
using namespace std;

enum enum_strategy_interrupt_result
{
	STRATEGY_INTERRUPT_BREAKING,
	STRATEGY_INTERRUPT_WAIT_CALLBACK,
	STRATEGY_INTERRUPT_FAIL,
};

class strategyBase
{
public:
	string m_strateID;
public:
	strategyBase(){};
	strategyBase(string strategyID) :m_strateID(strategyID){};
	virtual void startStrategy() = 0;
	virtual void stopStrategy() = 0;

	virtual enum_strategy_interrupt_result tryInterrupt(boost::function<void()> pauseHandler){ return STRATEGY_INTERRUPT_FAIL; };
	virtual void interrupt(boost::function<void()> pauseHandler){};
	virtual bool pause(boost::function<void()> pauseHandler){ return true; };
	virtual void resume(){};

	virtual void registerOrder(orderRtnPtr pOrder){};
};


class tradeGroupBuffer
{
public:
	int m_Id;
	list<int> m_orderIdList;
	double m_start_milliSec;
	double m_end_milliSec;
	tradeGroupBuffer() :m_start_milliSec(0.0), m_end_milliSec(0.0){};
};
typedef boost::shared_ptr<tradeGroupBuffer> tradeGroupBufferPtr;

enum enum_strategy_type
{
	STRATEGY_cmMM01,
	STRATEGY_cmSpec01,
	STRATEGY_ERROR,
};
