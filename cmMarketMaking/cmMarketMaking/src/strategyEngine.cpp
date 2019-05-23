#include "strategyEngine.h"
#include "baseClass/Utils.h"

strategyEngine::strategyEngine(Json::Value  config, infrastructure* infra):
m_config(config), m_infra(infra)
{
	m_quoteTP = athenathreadpoolPtr(new threadpool(50));
	m_tradeTP = athenathreadpoolPtr(new threadpool(50));
	infra->broadcastOrder = boost::bind(&strategyEngine::onBroadcastOrder, this, _1);
};

void strategyEngine::registerStrategyType(string strategyID, string strategyType)
{
	if ("cmMM01" == strategyType)
		m_strategyTypeMap[strategyID] = STRATEGY_cmMM01;
	else if ("cmMM02" == strategyType)
			m_strategyTypeMap[strategyID] = STRATEGY_cmMM02;
	else if ("cmSpec01" == strategyType)
		m_strategyTypeMap[strategyID] = STRATEGY_cmSpec01;
	else if ("cmTestOrder01" == strategyType)
		m_strategyTypeMap[strategyID] = STRATEGY_cmTestOrder01;
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
			cmMM01 *pCmMM01 = new cmMM01(strategyId, strategyType,
				strategyConfig["productId"].asString(),
				strategyConfig["exchange"].asString(),
				strategyConfig["quote"].asString(),
				strategyConfig["trade"].asString(),
				strategyConfig["tickSize"].asDouble(),
				strategyConfig["miniOrderSpread"].asDouble(),
				strategyConfig["orderQty"].asDouble(),
				strategyConfig["volMulti"].asInt(),
				strategyConfig["holdingRequirement"].asInt(),
				m_quoteTP, m_tradeTP, m_infra, strategyConfig);
			m_strategies[strategyId] = pCmMM01;
			break;
		}
		case STRATEGY_cmMM02:
		{
			cmMM02 *pCmMM02 = new cmMM02(strategyId, strategyType,
				strategyConfig["productId"].asString(),
				strategyConfig["exchange"].asString(),
				strategyConfig["quote"].asString(),
				strategyConfig["trade"].asString(),
				strategyConfig["tickSize"].asDouble(),
				strategyConfig["miniOrderSpread"].asDouble(),
				strategyConfig["orderQty"].asDouble(),
				strategyConfig["volMulti"].asInt(),
				strategyConfig["holdingRequirement"].asInt(),
				m_quoteTP, m_tradeTP, m_infra, strategyConfig);
			m_strategies[strategyId] = pCmMM02;
			break;
		}
		case STRATEGY_cmSpec01:
		{
			cmSepc01 *pCmSpec01 = new cmSepc01(strategyId, strategyType,
				strategyConfig["productId"].asString(),
				strategyConfig["exchange"].asString(),
				strategyConfig["quote"].asString(),
				strategyConfig["trade"].asString(),
				strategyConfig["tickSize"].asDouble(),
				strategyConfig["orderQty"].asDouble(),
				strategyConfig["volMulti"].asInt(),
				m_quoteTP, m_tradeTP, m_infra, strategyConfig);
			string masterStrategy = strategyConfig["masterStrategy"].asString();
			pCmSpec01->registerMasterStrategy(m_strategies[masterStrategy], m_strategyTypeMap[masterStrategy]);
			m_strategies[strategyId] = pCmSpec01;
		}
		case STRATEGY_cmTestOrder01:
		{
			/*string strategyId, string strategyTyp, string productId, string exchange,
				string quoteAdapterID, string tradeAdapterID,
				athenathreadpoolPtr quoteTP, athenathreadpoolPtr tradeTP, infrastructure* infra,
				Json::Value config*/
			cmTestOrder01 *pCmTestOrder01 = new cmTestOrder01(strategyId, strategyType,
				strategyConfig["productId"].asString(),
				strategyConfig["exchange"].asString(),
				strategyConfig["quote"].asString(),
				strategyConfig["trade"].asString(),
				m_quoteTP, m_tradeTP, m_infra, strategyConfig
				);
			m_strategies[strategyId] = pCmTestOrder01;
		}
		}
	}
}

void strategyEngine::onBroadcastOrder(orderRtnPtr orderPtr)
{
	auto iter = m_strategies.begin();
	while (iter != m_strategies.end())
	{
		switch (m_strategyTypeMap[iter->first])
		{
		case STRATEGY_cmMM01:
		{
			((cmMM01 *)(iter->second))->registerOrder(orderPtr);
			break;
		}
		case STRATEGY_cmMM02:
		{
			((cmMM02 *)(iter->second))->registerOrder(orderPtr);
			break;
		}
		case STRATEGY_cmSpec01:
		{
			((cmSepc01 *)(iter->second))->registerOrder(orderPtr);
				break;
		}
		case STRATEGY_cmTestOrder01:
		{
			((cmTestOrder01 *)(iter->second))->registerOrder(orderPtr);
			break;
		}
		}
		iter++;
	}
};


void strategyEngine::commandProcess()
{
	//∏Ò Ω£∫start|stop strategyName(mm_ZC905)
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
		auto iter = m_strategyTypeMap.find(commandEle[1]);
		if (iter == m_strategyTypeMap.end())
		{
			LOG(WARNING) << "strategyEngine: strategy " << commandEle[1] << " not configured!" << endl;
		}
		else
		{
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
				case STRATEGY_cmMM02:
				{
					cmMM02* pStrategy = (cmMM02*)m_strategies[commandEle[1]];
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
					cmMM01 *pStrategy = (cmMM01 *)m_strategies[commandEle[1]];
					pStrategy->pause(boost::bind(&IpauseStrategy::plainVanilla, &m_pauseInterface));
					break;
				}
				case STRATEGY_cmMM02:
				{
					cmMM02 *pStrategy = (cmMM02 *)m_strategies[commandEle[1]];
					pStrategy->pause(boost::bind(&IpauseStrategy::plainVanilla, &m_pauseInterface));
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
					pStrategy->resume();
					break;
				}
				case STRATEGY_cmMM02:
				{
					cmMM02 *pStrategy = (cmMM02 *)m_strategies[commandEle[1]];
					pStrategy->resume();
					break;
				}
				}
			}
			else
			{
				cout << "strategyEngine: unrecognized command | " << commandEle[0] << endl;
			}
		}
	}
	commandProcess();
};