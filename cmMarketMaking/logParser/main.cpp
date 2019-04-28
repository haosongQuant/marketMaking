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

vector<string> files;
map<string, map<string, double> > lastPriceDic; //tradingDate -> instrument -> lastprice

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
		}

	}
}

int main()
{
	getLogPath("D:\\mm_log");
	scanFiles();
}