#pragma once
#include <iostream>
#include "infrastructure.h"
#include "threadpool\threadpool.h"
#include "json\json.h"
#include "strategy\cmMM01.h"

enum enum_strategy_type
{
	STRATEGY_cmMM01,
	STRATEGY_ERROR,
};

struct IpauseStrategy
{
	void plainVanilla(){ cout << "strategyEngine: processing pause" << endl; };
};

class strategyEngine
{
private:
	Json::Value     m_config; //用于保存config文件内容
	infrastructure* m_infra;  //用于访问infrastructure
	athenathreadpoolPtr m_quoteTP; // 处理行情线程池
	athenathreadpoolPtr m_tradeTP; // 处理交易委托线程池

public:
	strategyEngine(Json::Value  config, infrastructure* infra);
	void init();
	void commandProcess();

	IpauseStrategy m_pauseInterface;

private:
	map<string, strategyBase*>      m_strategies;
	map<string, enum_strategy_type> m_strategyTypeMap;
	void registerStrategyType(string strategyID, string strategyType);
};