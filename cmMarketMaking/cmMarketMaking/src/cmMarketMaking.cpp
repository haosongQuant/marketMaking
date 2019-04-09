#include <iostream>
#include "json/configloader.h"
#include "threadpool/threadpool.h"
#include "infrastructure.h"
#include "strategyEngine.h"

using namespace std;

int main()
{
	auto global_config = loadconfig(".\\resource\\config.json");

	infrastructure* pInfra = new infrastructure(global_config);
	pInfra->init();

	strategyEngine* pStrategy = new strategyEngine(global_config, pInfra);
	pStrategy->init();
	pStrategy->commandProcess();

	return 0;
}
