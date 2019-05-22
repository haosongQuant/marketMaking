#include "baseClass\Utils.h"
#include "infrastructure.h"
#include "glog\logging.h"

void infrastructure::registerFuturesQuoteHandler(string adapterID, string exchange, string instList,
	boost::function<void(futuresMDPtr)> handler)
{
	char instrumentList[1024 * 20];
	memset(instrumentList, 0, sizeof(instrumentList));
	strncpy(instrumentList, instList.c_str(), sizeof(instrumentList));
	vector<string> instVec;
	athenaUtils::Split(instrumentList, ",", instVec, "-");
	{
		boost::mutex::scoped_lock lock1(m_futuresMDHandlerLock);
		for each (auto instrument in instVec)
			m_futuresMDHandler[adapterID][instrument].push_back(handler);
	}
};

void infrastructure::subscribeFutures(string adapterID, string exchange, string instList, 
	boost::function<void(futuresMDPtr)> handler)
{
	registerFuturesQuoteHandler(adapterID, exchange, instList, handler);

	switch (m_adapterTypeMap[adapterID])
	{
	case ADAPTER_CTP_QUOTE:
	{
		quoteAdapter_CTP * pQuoteAdapter = (quoteAdapter_CTP *)m_adapters[adapterID];
		pQuoteAdapter->Subscribe(instList, exchange);
		break;
	}
	case ADAPTER_TAP_QUOTE:
	{
		char instrumentList[1024 * 20];
		memset(instrumentList, 0, sizeof(instrumentList));
		strncpy(instrumentList, instList.c_str(), sizeof(instrumentList));
		vector<string> instVec;
		athenaUtils::Split(instrumentList, ",", instVec, "-");

		quoteAdapter_TAP * pQuoteAdapter = (quoteAdapter_TAP *)m_adapters[adapterID];
		for each (auto instrument in instVec)
			pQuoteAdapter->Subscribe(instrument, exchange);
		break;
	}
	}
};

void infrastructure::onRtnCtpQuote(string adapterID, CThostFtdcDepthMarketDataField* dataptr)
{
	if (m_productCategory[string(dataptr->ExchangeID)][string(dataptr->InstrumentID)]
		== CATE_CM_FUTURES)
	{
		futuresMD_struct*  detail = new futuresMD_struct();
		memset(detail, 0, sizeof(futuresMD_struct));
		futuresMDPtr quote = futuresMDPtr(detail);
		{
			strncpy(quote->TradingDay, dataptr->TradingDay, sizeof(quote->TradingDay));
			strncpy(quote->InstrumentID, dataptr->InstrumentID, sizeof(quote->InstrumentID));
			strncpy(quote->ExchangeID, dataptr->ExchangeID, sizeof(quote->ExchangeID));
			quote->LastPrice = dataptr->LastPrice;
			//quote->PreSettlementPrice = dataptr->PreSettlementPrice;
			//quote->PreClosePrice = dataptr->PreClosePrice;
			//quote->PreOpenInterest = dataptr->PreOpenInterest;
			//quote->OpenPrice = dataptr->OpenPrice;
			//quote->HighestPrice = dataptr->HighestPrice;
			//quote->LowestPrice = dataptr->LowestPrice;
			quote->Volume = dataptr->Volume;
			quote->Turnover = dataptr->Turnover;
			quote->OpenInterest = dataptr->OpenInterest;
			//quote->ClosePrice = dataptr->ClosePrice;
			//quote->SettlementPrice = dataptr->SettlementPrice;
			quote->UpperLimitPrice = dataptr->UpperLimitPrice;
			quote->LowerLimitPrice = dataptr->LowerLimitPrice;
			//quote->PreDelta = dataptr->PreDelta;
			//quote->CurrDelta = dataptr->CurrDelta;
			strncpy(quote->UpdateTime, dataptr->UpdateTime, sizeof(quote->UpdateTime));
			quote->UpdateMillisec = dataptr->UpdateMillisec;
			quote->bidprice[0] = dataptr->BidPrice1;
			quote->bidprice[1] = dataptr->BidPrice2;
			quote->bidprice[2] = dataptr->BidPrice3;
			quote->bidprice[3] = dataptr->BidPrice4;
			quote->bidprice[4] = dataptr->BidPrice5;
			quote->bidvol[0] = dataptr->BidVolume1;
			quote->bidvol[1] = dataptr->BidVolume2;
			quote->bidvol[2] = dataptr->BidVolume3;
			quote->bidvol[3] = dataptr->BidVolume4;
			quote->bidvol[4] = dataptr->BidVolume5;
			quote->askprice[0] = dataptr->AskPrice1;
			quote->askprice[1] = dataptr->AskPrice2;
			quote->askprice[2] = dataptr->AskPrice3;
			quote->askprice[3] = dataptr->AskPrice4;
			quote->askprice[4] = dataptr->AskPrice5;
			quote->askvol[0] = dataptr->AskVolume1;
			quote->askvol[1] = dataptr->AskVolume2;
			quote->askvol[2] = dataptr->AskVolume3;
			quote->askvol[3] = dataptr->AskVolume4;
			quote->askvol[4] = dataptr->AskVolume5;
			quote->AveragePrice = dataptr->AveragePrice;
		}
		//m_quoteTP->getDispatcher().post(bind(&infrastructure::onFuturesTick, this, adapterID, quote));
		onFuturesTick(adapterID, quote);
	}
	else
		LOG(WARNING) << "quote category not recognized! instrument: " << dataptr->InstrumentID << endl;
	return;
}

void infrastructure::onRtnTapQuote(string adapterID, TapAPIQuoteWhole* dataptr)
{
	if (dataptr->Contract.Commodity.CommodityType == TAPI_COMMODITY_TYPE_FUTURES)
	{
		futuresMD_struct*  detail = new futuresMD_struct();
		memset(detail, 0, sizeof(futuresMD_struct));
		futuresMDPtr quote = futuresMDPtr(detail);
		{
			strncpy(quote->TradingDay, dataptr->DateTimeStamp, 4);
			strncpy(quote->TradingDay + 4, dataptr->DateTimeStamp + 5, 2);
			strncpy(quote->TradingDay + 6, dataptr->DateTimeStamp + 8, 2);
			strncpy(quote->InstrumentID, dataptr->Contract.Commodity.CommodityNo, strlen(dataptr->Contract.Commodity.CommodityNo));
			strncpy(quote->InstrumentID + strlen(dataptr->Contract.Commodity.CommodityNo), dataptr->Contract.ContractNo1, strlen(dataptr->Contract.ContractNo1));
			strncpy(quote->ExchangeID, dataptr->Contract.Commodity.ExchangeNo, sizeof(quote->ExchangeID));
			quote->LastPrice = dataptr->QLastPrice;
			//quote->PreSettlementPrice = dataptr->PreSettlementPrice;
			//quote->PreClosePrice = dataptr->PreClosePrice;
			//quote->PreOpenInterest = dataptr->PreOpenInterest;
			//quote->OpenPrice = dataptr->OpenPrice;
			//quote->HighestPrice = dataptr->HighestPrice;
			//quote->LowestPrice = dataptr->LowestPrice;
			quote->Volume = dataptr->QLastQty;
			quote->Turnover = dataptr->QTurnoverRate;
			quote->OpenInterest = dataptr->QPositionQty;
			//quote->ClosePrice = dataptr->ClosePrice;
			//quote->SettlementPrice = dataptr->SettlementPrice;
			//quote->UpperLimitPrice = dataptr->UpperLimitPrice;
			//quote->LowerLimitPrice = dataptr->LowerLimitPrice;
			//quote->PreDelta = dataptr->PreDelta;
			//quote->CurrDelta = dataptr->CurrDelta;
			strncpy(quote->UpdateTime, dataptr->DateTimeStamp+11, 8);
			quote->UpdateMillisec = atoi(dataptr->DateTimeStamp+20);
			memcpy(quote->bidprice, dataptr->QBidPrice, 10 * sizeof(double));
			quote->bidvol[0] = dataptr->QBidQty[0];
			quote->bidvol[1] = dataptr->QBidQty[1];
			quote->bidvol[2] = dataptr->QBidQty[2];
			quote->bidvol[3] = dataptr->QBidQty[3];
			quote->bidvol[4] = dataptr->QBidQty[4];
			quote->bidvol[5] = dataptr->QBidQty[5];
			quote->bidvol[6] = dataptr->QBidQty[6];
			quote->bidvol[7] = dataptr->QBidQty[7];
			quote->bidvol[8] = dataptr->QBidQty[8];
			quote->bidvol[9] = dataptr->QBidQty[9];
			memcpy(quote->askprice, dataptr->QAskPrice, 10 * sizeof(double));
			quote->askvol[0] = dataptr->QAskQty[0];
			quote->askvol[1] = dataptr->QAskQty[1];
			quote->askvol[2] = dataptr->QAskQty[2];
			quote->askvol[3] = dataptr->QAskQty[3];
			quote->askvol[4] = dataptr->QAskQty[4];
			quote->askvol[5] = dataptr->QAskQty[5];
			quote->askvol[6] = dataptr->QAskQty[6];
			quote->askvol[7] = dataptr->QAskQty[7];
			quote->askvol[8] = dataptr->QAskQty[8];
			quote->askvol[9] = dataptr->QAskQty[9];
			quote->AveragePrice = dataptr->QAveragePrice;
		}

		m_quoteTP->getDispatcher().post(bind(&infrastructure::onFuturesTick, this, adapterID, quote));
	}
	return;
};

void infrastructure::onFuturesTick(string adapterID, futuresMDPtr pQuote)
{
	boost::mutex::scoped_lock lock(m_futuresMDHandlerLock);
	auto iter0 = m_futuresMDHandler.find(adapterID);
	if (iter0 != m_futuresMDHandler.end())
	{
		auto iter2 = iter0->second.find(string(pQuote->InstrumentID));
		if (iter2 != iter0->second.end())
		{
			for each(auto item in iter2->second)
			{
				m_quoteTP->getDispatcher().post(bind((item), pQuote));
			}
			return;
		}
	}
	LOG(WARNING) << "infra warning: "<< string(pQuote->InstrumentID) << " no handler registered!" << endl;
	return;
};

void infrastructure::onRtnCtpInstruments(string adapterID, CThostFtdcInstrumentField* inst)
{
	if (inst->ProductClass == THOST_FTDC_PC_Futures)
		m_productCategory[string(inst->ExchangeID)][string(inst->InstrumentID)] = CATE_CM_FUTURES;
	else if (inst->ProductClass == THOST_FTDC_PC_Options)
		m_productCategory[string(inst->ExchangeID)][string(inst->InstrumentID)] = CATE_CM_OPTIONS;
}
