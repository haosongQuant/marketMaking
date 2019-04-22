#include "strategyEngine.h"
#include "baseClass/Utils.h"

strategyEngine::strategyEngine(Json::Value  config, infrastructure* infra):
m_config(config), m_infra(infra)
{
	m_quoteTP = athenathreadpoolPtr(new threadpool(4));
	m_tradeTP = athenathreadpoolPtr(new threadpool(6));
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
				strategyConfig["orderQty"].asDouble(),
				strategyConfig["volMulti"].asInt(),
				m_quoteTP, m_tradeTP, m_infra, strategyConfig);
			m_strategies[strategyId] = pCmMM01;
			break;
		}
		}
	}
}

void strategyEngine::commandProcess()
{
	//¸ñÊ½£ºstart|stop strategyName(mm_ZC905)
	char commandLine[100];
	memset(commandLine, 0, sizeof(commandLine));
	cin.getline(commandLine, sizeof(commandLine)-1);
	string command = string(commandLine);
	if ("exit" == command)
		return;
	else if (athenaUtils::Rtrim(command) == "")
	{
		commandProcess();
		return;
	}
	else
	{
		vector<string> commandEle;
		athenaUtils::Split(command, " ", commandEle);
		if (commandEle[0] == "start")
		{
			switch (m_strategyTypeMap[commandEle[1]])
			{
			case STRATEGY_cmMM01:
			{
				cmMM01* pStrategy = (cmMM01*)m_strategies[commandEle[1]];
				pStrategy->startStrategy();
				break;
			}
			}
		}
		else if (commandEle[0] == "stop")
		{
			switch (m_strategyTypeMap[commandEle[1]])
			{
			case STRATEGY_cmMM01:
			{
				cmMM01* pStrategy = (cmMM01*)m_strategies[commandEle[1]];
				pStrategy->pauseMM(boost::bind(&IpauseStrategy::plainVanilla, &m_pauseInterface));
				break;
			}
			}
		}
		else if (commandEle[0] == "resume")
		{
			switch (m_strategyTypeMap[commandEle[1]])
			{
			case STRATEGY_cmMM01:
			{
				cmMM01* pStrategy = (cmMM01*)m_strategies[commandEle[1]];
				pStrategy->resumeMM();
				break;
			}
			}
		}
		else
		{
			cout << "strategyEngine: unrecognized command | " << commandEle[0] << endl;
		}
	}
	commandProcess();
};