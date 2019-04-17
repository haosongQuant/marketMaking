#pragma once
#include <string>
#include <boost/shared_ptr.hpp>
using namespace std;

class strategyBase
{
public:
	string m_strateID;
public:
	strategyBase(){};
	strategyBase(string strategyID) :m_strateID(strategyID){};
	virtual void startStrategy() = 0;
	virtual void stopStrategy() = 0;
};


class tradeGroupBuffer
{
public:
	int m_Id;
	list<int> m_orderIdList;
};
typedef boost::shared_ptr<tradeGroupBuffer> tradeGroupBufferPtr;
