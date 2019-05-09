#include<io.h>
#include <time.h>
#include <iostream>
#include <string.h>
#include <vector>
#include <fstream>
#include <sstream>
#include "baseClass/UTC.h"
#include "baseClass/Utils.h"
#include <map>

using namespace std;
using namespace athenaUTC;

map<string, double> contractMultiplier = {
	{ "ZC911", 100 }, 
	{ "ZC907", 100 }, 
	{ "TA911", 5 },
	{ "TA907", 5 }
};

map<string, double> fullTradeTime = {
	{ "mm_ZC911", 6.25 },
	{ "mm_ZC907", 6.25 },
	{ "mm_TA911", 6.25 },
	{ "mm_TA907", 6.25 }
};

vector<string> files;
map<string, map<string, double> > lastPriceDic; //tradingDate -> instrument -> lastprice
map<string, map<string, double> > validTimeDic; //tradingDate -> strategyId -> validTime

//tradingDate -> strategyID -> instrument -> list <vol, price> 
map<string, map<string, map<string, list<pair<int, double> > > > > tradeDic; 

//tradingDate -> strategyID -> instrument -> list <vol, price> 
map<string, map<string, map<string, list<pair<int, double> > > > > specTradeDic;

void getLogPath(string path)
{
	//文件句柄  
	long   hFile = 0;
	//文件信息  
	struct _finddata_t fileinfo;
	string p;
	if ((hFile = _findfirst(p.assign(path).append("\\*").c_str(), &fileinfo)) != -1)
	{
		do
		{
			//如果不是目录
			if (!(fileinfo.attrib &  _A_SUBDIR) && strncmp(fileinfo.name, "log_info", 8) == 0)
			{
				files.push_back(p.assign(path).append("\\").append(fileinfo.name));
			}
		} while (_findnext(hFile, &fileinfo) == 0);
	}
}

void processQuote(string lineStr)
{
	string tradingDt;
	string instrument;
	double lastprice;
	vector<string> lineElements;
	athenaUtils::Split(lineStr, ",", lineElements);
	for (auto ele : lineElements)
	{
		vector<string> wordElements;
		athenaUtils::Split(ele, ":", wordElements);
		if (wordElements[0] == "tradingDate")
		{
			tradingDt = wordElements[1];
			continue;
		}
		if (wordElements[0] == "instrument")
		{
			instrument = wordElements[1];
			continue;
		}
		if (wordElements[0] == "lastPrice")
		{
			lastprice = atof((const char*)(wordElements[1].c_str()));
			continue;
		}
	}
	lastPriceDic[tradingDt][instrument] = lastprice;
	return;
}

void processValidTime(string lineStr)
{
	string tradingDt;
	string strategyId;
	double validTime;
	vector<string> lineElements;
	athenaUtils::Split(lineStr, ",", lineElements);
	strategyId = lineElements[1];
	tradingDt = lineElements[3];
	validTime = atof((const char*)(lineElements[5].c_str()));
	auto iter = validTimeDic[tradingDt].find(strategyId);
	if (iter == validTimeDic[tradingDt].end())
		validTimeDic[tradingDt][strategyId] += 0.0;
	validTimeDic[tradingDt][strategyId] += validTime;
	return;
}

void processTrade(string lineStr)
{
//I0429 22:46:50.005864  2440 cmmm01.cpp:305] ,mm_TA911,trade_rtn, orderRef:121205667, tradeDate:20190429, InstrumentID:TA911, Direction:0, Price:5712, volume:10

	string tradingDt;
	string instrument;
	string strategyId;
	int    direction;
	double price;
	int    volume;
	vector<string> lineElements;
	athenaUtils::Split(lineStr, ",", lineElements);
	strategyId = lineElements[1];
	for (auto ele : lineElements)
	{
		vector<string> wordElements;
		athenaUtils::Split(ele, ":", wordElements);
		if (wordElements[0] == "tradeDate")
		{
			tradingDt = wordElements[1];
			continue;
		}
		if (wordElements[0] == "InstrumentID")
		{
			instrument = wordElements[1];
			continue;
		}
		if (wordElements[0] == "Direction")
		{
			direction = atoi((const char*)(wordElements[1].c_str()));
			continue;
		}
		if (wordElements[0] == "Price")
		{
			price = atof((const char*)(wordElements[1].c_str()));
			continue;
		}
		if (wordElements[0] == "volume")
		{
			volume = atoi((const char*)(wordElements[1].c_str()));
			continue;
		}
	}
	
	tradeDic[tradingDt][strategyId][instrument].push_back(make_pair((direction == 0 ? (volume * -1): volume), price));

	return;
}

void processSpecTrade(string lineStr)
{
	string tradingDt;
	string instrument;
	string strategyId;
	int    direction;
	double price;
	int    volume;
	vector<string> lineElements;
	athenaUtils::Split(lineStr, ",", lineElements);
	strategyId = lineElements[1];
	for (auto ele : lineElements)
	{
		vector<string> wordElements;
		athenaUtils::Split(ele, ":", wordElements);
		if (wordElements[0] == "tradeDate")
		{
			tradingDt = wordElements[1];
			continue;
		}
		if (wordElements[0] == "InstrumentID")
		{
			instrument = wordElements[1];
			continue;
		}
		if (wordElements[0] == "Direction")
		{
			direction = atoi((const char*)(wordElements[1].c_str()));
			continue;
		}
		if (wordElements[0] == "Price")
		{
			price = atof((const char*)(wordElements[1].c_str()));
			continue;
		}
		if (wordElements[0] == "volume")
		{
			volume = atoi((const char*)(wordElements[1].c_str()));
			continue;
		}
	}

	specTradeDic[tradingDt][strategyId][instrument].push_back(make_pair((direction == 0 ? (volume * -1) : volume), price));

	return;
}

void scanFiles()
{
	for (auto filename : files)
	{
		cout << "processing " << filename << endl;

		ifstream inFile(filename, ios::in);
		string lineStr;
		while (getline(inFile, lineStr))
		{
			string::size_type idx;
			idx = lineStr.find("行情");
			if (idx != string::npos)//行情
			{
				processQuote(lineStr);
				continue;
			}

			idx = lineStr.find("validTime");
			if (idx != string::npos)//行情
			{
				processValidTime(lineStr);
				continue;
			}

			idx = lineStr.find("trade_rtn");
			if (idx != string::npos) //成交
			{
				processTrade(lineStr);
				continue;
			}

			idx = lineStr.find("spec_tradeRtn");
			if (idx != string::npos) //成交
			{
				processSpecTrade(lineStr);
				continue;
			}
		}
	}
}

void collectNoutputResult(string path)
{
	//tradingDate -> strategyId -> validTime
	//tradingDate -> strategyID -> instrument -> list <vol, price> 
	auto tradeDtIter = validTimeDic.begin();//tradingDate
	while (tradeDtIter != validTimeDic.end())
	{
		string outfileName = path + "\\result_" + tradeDtIter->first + ".csv";
		ofstream outFile;
		outFile.open(outfileName, ios::out);
		outFile << tradeDtIter->first << endl;
		//做市成交统计
		outFile << "strategyId,validTime,fullfillRate,buyVol,avgBuyPrz,sellVol,avgSellPrz,P&L" << endl;
		auto strategyIter = tradeDtIter->second.begin();//strategyID
		while (strategyIter != tradeDtIter->second.end())
		{
			double validtime = strategyIter->second;
			double profit = 0.0;
			int    buyVol = 0;
			double buyAmount = 0.0;
			int    sellVol = 0;
			double sellAmount = 0.0;
			auto instrumentIter = tradeDic[tradeDtIter->first][strategyIter->first].begin();
			while (instrumentIter != tradeDic[tradeDtIter->first][strategyIter->first].end())//存在成交记录
			{
				int netVolume = 0;
				auto tradeItemIter = instrumentIter->second.begin();
				while (tradeItemIter != instrumentIter->second.end())//遍历每一笔成交
				{
					netVolume += tradeItemIter->first;
					profit += tradeItemIter->first * tradeItemIter->second 
						* contractMultiplier[instrumentIter->first];
					if (tradeItemIter->first < 0)
					{
						buyVol -= tradeItemIter->first;
						buyAmount -= tradeItemIter->first * tradeItemIter->second;
					}
					else
					{
						sellVol += tradeItemIter->first;
						sellAmount += tradeItemIter->first * tradeItemIter->second;
					}
					tradeItemIter++;
				}
				if (netVolume != 0) //假设按收盘价对冲所有仓位
				{
					double lastprice=0.0;
					auto lastPriceIter = lastPriceDic[tradeDtIter->first].find(instrumentIter->first);
					if (lastPriceIter != lastPriceDic[tradeDtIter->first].end())
						lastprice = lastPriceIter->second;
					else
						lastprice = (tradeItemIter--)->second;//如果没有找到行情，以最后一笔成交价为准
					profit += (netVolume*-1) * lastprice
						* contractMultiplier[instrumentIter->first];
				}
				instrumentIter++;
			}
			outFile << strategyIter->first << ","
				<< validtime / (1000 * 60 * 60) << " h,"
				<< validtime / (fullTradeTime[strategyIter->first] * 60 * 60 * 10) << "%,"
				<< buyVol << "," << (buyAmount / buyVol) << "," 
				<< sellVol << "," << (sellAmount / sellVol) << ","
				<<profit<< endl;
			strategyIter++;
		}

		//投机成交统计 todo: debug
		//tradingDate -> strategyID -> instrument -> list <vol, price> 
		outFile << "strategyId,buyVol,avgBuyPrz,sellVol,avgSellPrz,P&L" << endl;
		auto strategyIter2 = specTradeDic[tradeDtIter->first].begin();//strategyID
		while (strategyIter2 != specTradeDic[tradeDtIter->first].end())
		{
			double profit = 0.0;
			int    buyVol = 0;
			double buyAmount = 0.0;
			int    sellVol = 0;
			double sellAmount = 0.0;
			auto instrumentIter = specTradeDic[tradeDtIter->first][strategyIter2->first].begin();
			while (instrumentIter != specTradeDic[tradeDtIter->first][strategyIter2->first].end())//存在成交记录
			{
				int netVolume = 0;
				auto tradeItemIter = instrumentIter->second.begin();
				while (tradeItemIter != instrumentIter->second.end())//遍历每一笔成交
				{
					netVolume += tradeItemIter->first;
					profit += tradeItemIter->first * tradeItemIter->second
						* contractMultiplier[instrumentIter->first];
					if (tradeItemIter->first < 0)
					{
						buyVol -= tradeItemIter->first;
						buyAmount -= tradeItemIter->first * tradeItemIter->second;
					}
					else
					{
						sellVol += tradeItemIter->first;
						sellAmount += tradeItemIter->first * tradeItemIter->second;
					}
					tradeItemIter++;
				}
				if (netVolume != 0) //假设按收盘价对冲所有仓位
				{
					double lastprice = 0.0;
					auto lastPriceIter = lastPriceDic[tradeDtIter->first].find(instrumentIter->first);
					if (lastPriceIter != lastPriceDic[tradeDtIter->first].end())
						lastprice = lastPriceIter->second;
					else
						lastprice = (tradeItemIter--)->second;//如果没有找到行情，以最后一笔成交价为准
					profit += (netVolume*-1) * lastprice
						* contractMultiplier[instrumentIter->first];
				}
				instrumentIter++;
			}
			outFile << strategyIter2->first << ","
				<< buyVol << "," << (buyAmount / buyVol) << ","
				<< sellVol << "," << (sellAmount / sellVol) << ","
				<< profit << endl;
			strategyIter2++;
		}
		outFile.close();
		tradeDtIter++;
	}
}

int main()
{
	getLogPath("D:\\mm_log");
	scanFiles();
	collectNoutputResult("D:\\mm_log");
	return 0;
}