#include "strategy/cmMM01.h"

cmMM01::cmMM01(string strategyId, string strategyTyp, string productId, string exchange,
	string quoteAdapterID, string tradeAdapterID, double tickSize, double miniOrderSpread,
	athenathreadpoolPtr quoteTP, athenathreadpoolPtr tradeTP, infrastructure* infra)
	:m_strategyId(strategyId), m_strategyTyp(strategyTyp), m_productId(productId), m_exchange(exchange),
	m_quoteAdapterID(quoteAdapterID), m_tradeAdapterID(tradeAdapterID), m_tickSize(tickSize),
	m_miniOrderSpread(miniOrderSpread), m_quoteTP(quoteTP), m_tradeTP(tradeTP), m_infra(infra)
{
};

void cmMM01::startStrategy(){
	m_infra->subscribeFutures(m_quoteAdapterID, m_exchange, m_productId, bind(&cmMM01::onRtnMD, this, _1));
};


void cmMM01::onRtnMD(futuresMDPtr pFuturesMD)
{

};