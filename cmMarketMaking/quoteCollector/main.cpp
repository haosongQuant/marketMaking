#include "ctp/quoteAdapter_CTP.h"
#include "glog/initLog.h"
#include "json/configloader.h"
#include <iostream>
#include <list>

using namespace std;

char mdFront[] = "tcp://180.168.146.187:10031";
char broker[] = "9999";
char user[] = "140108";
char pwd[] = "1qazxc";
quoteAdapter_CTP * pQuoteAdapter;

list<pair<string, string> > instrumentList;

void onRtnCtpQuote(string, CThostFtdcDepthMarketDataField *pDepthMarketData){
	LOG(INFO) << ",QuoteRecord," << pDepthMarketData->TradingDay
		<< "," << pDepthMarketData->UpdateTime
		<< "," << pDepthMarketData->UpdateMillisec
		<< "," << pDepthMarketData->InstrumentID
		<< "," << pDepthMarketData->LastPrice
		<< "," << pDepthMarketData->AskPrice1
		<< "," << pDepthMarketData->AskVolume1
		<< "," << pDepthMarketData->BidPrice1
		<< "," << pDepthMarketData->BidVolume1
		<< "," << pDepthMarketData->OpenPrice
		<< "," << pDepthMarketData->OpenInterest
		<< "," << pDepthMarketData->Volume
		<< "," << pDepthMarketData->PreClosePrice
		<< "," << pDepthMarketData->PreSettlementPrice
		<< "," << pDepthMarketData->SettlementPrice
		<< ","<<pDepthMarketData->UpperLimitPrice
		<< ","<<pDepthMarketData->LowerLimitPrice<< endl;
}

void onQuoteLogin(string adapterID)
{
	for (auto item : instrumentList)
		pQuoteAdapter->Subscribe(item.first, item.second);
}

void main(){

	initLog("D://quote_log//", //日志文件位置
		0,              // 屏幕输出级别: GLOG_INFO = 0, GLOG_WARNING = 1, GLOG_ERROR = 2, GLOG_FATAL = 3
		100);           //日志文件大小

	instrumentList.push_back(make_pair("ZC911", "CZCE"));
	instrumentList.push_back(make_pair("ZC907", "CZCE"));

	string adapterID = "quoteAdapterCTP";
	pQuoteAdapter = new quoteAdapter_CTP(adapterID, mdFront, broker, user, pwd);
	pQuoteAdapter->m_OnUserLogin = bind(onQuoteLogin, _1);
	pQuoteAdapter->m_onRtnMarketData = bind(onRtnCtpQuote, _1, _2);
	pQuoteAdapter->init();
	
	char cmd[100];
	memset(cmd, 0, sizeof(cmd));
	cin >> cmd;
	if (strcmp(cmd, "exit") == 0)
		exit(0);
}