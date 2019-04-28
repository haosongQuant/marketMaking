#include <iostream>
#include "baseClass\UTC.h"
#include "glog/initLog.h"
#include "json/configloader.h"
#include "threadpool/threadpool.h"
#include "infrastructure.h"
#include "strategyEngine.h"

using namespace std;
using namespace athenaUTC;

int main()
{
	UTC::Init();

	initLog("D://mm_log//", //日志文件位置
		    1,              // 屏幕输出级别: GLOG_INFO = 0, GLOG_WARNING = 1, GLOG_ERROR = 2, GLOG_FATAL = 3
		    100);           //日志文件大小
			
	auto global_config = loadconfig(".\\resource\\config.json");

	infrastructure* pInfra = new infrastructure(global_config);
	pInfra->init();

	strategyEngine* pStrategy = new strategyEngine(global_config, pInfra);
	pStrategy->init();
	pStrategy->commandProcess();

	return 0;
}
