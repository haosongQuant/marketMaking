#include <iostream>
//#include <boost/bind.hpp>
#include "json/configloader.h"
#include "threadpool/threadpool.h"
#include "infrastructure.h"
//#include "tap/quoteAdapter_TAP.h"
//#include "tap\tradeAdapter_TAP.h"

using namespace std;

int main()
{
	auto global_config = loadconfig(".\\resource\\config.json");

	cout << global_config << endl;

	return 0;
}
