#pragma once
#include <string>
using namespace std;

class strategyBase
{
public:
	string m_strateID;
public:
	strategyBase(){};
	strategyBase(string strategyID) :m_strateID(strategyID){};
	virtual void startStrategy() = 0;
	virtual void stopAdapter() = 0;
};
