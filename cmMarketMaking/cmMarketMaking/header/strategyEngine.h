#pragma once
#include <iostream>
#include "infrastructure.h"
#include "threadpool\threadpool.h"
#include "json\json.h"
#include "glog\logging.h"
#include "baseClass/orderBase.h"
#include "strategy\cmMM01.h"
#include "strategy\cmMM02.h"
#include "strategy\cmSpec01.h"
#include "strategy\cmTestOrder01.h"

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

public:
	void onBroadcastOrder(orderRtnPtr orderPtr);
};