#pragma once

#include <iostream>
#include "infrastructure.h"
#include "threadpool\threadpool.h"
#include "baseClass/strategyBase.h"

using namespace std;

class cmMM01 : public strategyBase
{
private:
	string m_strategyId;
	string m_strategyTyp;
	string m_productId;
	string m_exchange;
	string m_quoteAdapterID;
	string m_tradeAdapterID;
	double m_tickSize;
	double m_miniOrderSpread;

	infrastructure* m_infra;

	athenathreadpoolPtr m_quoteTP;
	athenathreadpoolPtr m_tradeTP;

public:
	cmMM01(string strategyId, string strategyTyp, string productId, string exchange, 
		string quoteAdapterID, string tradeAdapterID, double tickSize, double miniOrderSpread,
		athenathreadpoolPtr quoteTP, athenathreadpoolPtr tradeTP, infrastructure* infra);
	~cmMM01();
	virtual void startStrategy();
	virtual void stopAdapter();

public:

	void onRtnMD(futuresMDPtr pFuturesMD);
};