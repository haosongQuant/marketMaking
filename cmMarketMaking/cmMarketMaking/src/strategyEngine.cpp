#include "strategyEngine.h"

strategyEngine::strategyEngine(Json::Value  config, infrastructure* infra):
m_config(config), m_infra(infra)
{
	m_quoteTP = athenathreadpoolPtr(new threadpool(2));
	m_tradeTP = athenathreadpoolPtr(new threadpool(2));
};

void strategyEngine::registerStrategyType(string strategyID, string strategyType)
{
	if ("cmMM01" == strategyType)
		m_strategyTypeMap[strategyID] = STRATEGY_cmMM01;
	else
		m_strategyTypeMap[strategyID] = STRATEGY_ERROR;
};

void strategyEngine::init()
{
	int strategyNum = m_config["strategy"].size();
	for (int i = 0; i < strategyNum; ++i)
	{
		Json::Value strategyConfig = m_config["strategy"][i];
		string strategyId = strategyConfig["strategyID"].asString();
		string strategyType = strategyConfig["strategyTyp"].asString();
		registerStrategyType(strategyId, strategyType);
		switch (m_strategyTypeMap[strategyId])
		{
		case STRATEGY_cmMM01:
		{
			cmMM01* pCmMM01 = new cmMM01(strategyId, strategyType,
				strategyConfig["productId"].asString(),
				strategyConfig["exchange"].asString(),
				strategyConfig["quote"].asString(),
				strategyConfig["trade"].asString(),
				strategyConfig["tickSize"].asDouble(),
				strategyConfig["miniOrderSpread"].asDouble(),
				m_quoteTP, m_tradeTP, m_infra);
			m_strategies[strategyId] = pCmMM01;
			break;
		}
		}
	}
}

void strategyEngine::commandProcess()
{
	string command;
	cin >> command;
};