#include "baseClass\Utils.h"
#include "infrastructure.h"

int infrastructure::insertOrder(string adapterID, string instrument, string exchange, 
	enum_order_type orderType, enum_order_dir_type dir, enum_position_effect_type positionEffect, 
	enum_hedge_flag hedgeflag, double price, unsigned int volume,
	boost::function<void(orderRtnPtr)> orderRtnhandler, boost::function<void(tradeRtnPtr)> tradeRtnhandler)
{
	//todo: develop virtual counter
	enum_position_effect_type poEffect;
	if (positionEffect == POSITION_EFFECT_CLOSE && exchange == "SHFE")
		poEffect = POSITION_EFFECT_CLOSE_TODAY;
	else
		poEffect = positionEffect;
	switch (m_adapterTypeMap[adapterID])
	{
	case ADAPTER_CTP_TRADE:
	{
		tradeAdapterCTP * pTradeAdapter = (tradeAdapterCTP *)m_adapters[adapterID];
		int orderRef = pTradeAdapter->OrderInsert(instrument, exchange,
			m_orderTypeMap[ADAPTER_CTP_TRADE][orderType],
			m_orderDirMap[ADAPTER_CTP_TRADE][dir],
			m_positinEffectMap[ADAPTER_CTP_TRADE][poEffect],
			m_hedgeFlagMap[ADAPTER_CTP_TRADE][hedgeflag],
			price, volume, 
			THOST_FTDC_TC_GFD, //当日有效
			THOST_FTDC_VC_AV, //任何数量
			1, THOST_FTDC_CC_Immediately, //立即成交
			0.0, THOST_FTDC_FCC_NotForceClose //非强平
			);
		if (orderRef != -1) //下单成功
		{
			m_orderRtnHandlers[adapterID][orderRef] = orderRtnhandler;
			m_tradeRtnHandlers[adapterID][orderRef] = tradeRtnhandler;
			//cout << "infra: order sent succ." << endl;
			return orderRef;
		}
		break;
	}
	case ADAPTER_TAP_TRADE:
	{
		tradeAdapter_TAP * pTradeAdapter = (tradeAdapter_TAP *)m_adapters[adapterID];
		int orderRef = pTradeAdapter->OrderInsert(instrument, exchange,
			m_orderTypeMap[ADAPTER_TAP_TRADE][orderType],
			m_orderDirMap[ADAPTER_TAP_TRADE][dir],
			m_positinEffectMap[ADAPTER_TAP_TRADE][positionEffect],
			price, volume);
		if (orderRef != -1) //下单成功
		{
			m_orderRtnHandlers[adapterID][orderRef] = orderRtnhandler;
			m_tradeRtnHandlers[adapterID][orderRef] = tradeRtnhandler;
			cout << "infra: order sent succ." << endl;
			return orderRef;
		}
		break;
	}
	}
	return -1;
};

int infrastructure::cancelOrder(string adapterID, int orderRef, boost::function<void(cancelRtnPtr)> cancelRtnhandler)
{
	int cancelRtnCd = 0;
	switch (m_adapterTypeMap[adapterID])
	{
	case ADAPTER_CTP_TRADE:
	{
		tradeAdapterCTP * pTradeAdapter = (tradeAdapterCTP *)m_adapters[adapterID];
		cancelRtnCd = pTradeAdapter->cancelOrder(orderRef);
		if (cancelRtnCd > 0)
			m_cancelRtnHandlers[adapterID][cancelRtnCd] = cancelRtnhandler;
		break;
	}
	case ADAPTER_TAP_TRADE:
	{
		tradeAdapter_TAP * pTradeAdapter = (tradeAdapter_TAP *)m_adapters[adapterID];
		return pTradeAdapter->cancelOrder(orderRef);
		break;
	}
	}
	return cancelRtnCd;
};

void infrastructure::onRespCtpCancel(string adapterID, CThostFtdcInputOrderActionField *pInputOrderAction,
	CThostFtdcRspInfoField *pRspInfo)
{
	cancelRtnPtr cancelPtr = cancelRtnPtr(new cancelRtn_struct());
	cancelPtr->m_cancelOrderRef = pInputOrderAction->OrderActionRef;
	cancelPtr->m_originOrderRef = atoi(pInputOrderAction->OrderRef);
	cancelPtr->m_isCancelSucc = pRspInfo->ErrorID == 0 ? true : false;
	auto iter1 = m_cancelRtnHandlers.find(adapterID);
	if (iter1 != m_cancelRtnHandlers.end())
	{
		auto iter2 = iter1->second.find(cancelPtr->m_cancelOrderRef);
		if (iter2 != iter1->second.end())
			m_tradeTP->getDispatcher().post(bind((m_cancelRtnHandlers[adapterID][cancelPtr->m_cancelOrderRef]),
				cancelPtr));
	}
};


void infrastructure::onRtnCTPOrderActionErr(string adapterID, CThostFtdcOrderActionField *pOrderAction,
	CThostFtdcRspInfoField *pRspInfo)
{
	cancelRtnPtr cancelPtr = cancelRtnPtr(new cancelRtn_struct());
	cancelPtr->m_cancelOrderRef = pOrderAction->OrderActionRef;
	cancelPtr->m_originOrderRef = atoi(pOrderAction->OrderRef);
	cancelPtr->m_isCancelSucc = pRspInfo->ErrorID == 0 ? true : false;
	switch (pRspInfo->ErrorID)
	{
	case 26:
	{
		cancelPtr->m_cancelOrderRc = CANCEL_RC_TRADED_OR_CANCELED;
		break;
	}
	default:
	{
		cancelPtr->m_cancelOrderRc = CANCEL_RC_UNDEFINED;
		break;
	}
	}

	auto iter1 = m_cancelRtnHandlers.find(adapterID);
	if (iter1 != m_cancelRtnHandlers.end())
	{
		auto iter2 = iter1->second.find(cancelPtr->m_cancelOrderRef);
		if (iter2 != iter1->second.end())
			m_tradeTP->getDispatcher().post(bind((m_cancelRtnHandlers[adapterID][cancelPtr->m_cancelOrderRef]),
			cancelPtr));
	}
};

void infrastructure::queryOrder(string adapterID, int orderRef)
{
	switch (m_adapterTypeMap[adapterID])
	{
	case ADAPTER_CTP_TRADE:
	{
		tradeAdapterCTP * pTradeAdapter = (tradeAdapterCTP *)m_adapters[adapterID];
		pTradeAdapter->queryOrder(orderRef);
		break;
	}
	case ADAPTER_TAP_TRADE:
	{
		break;
	}
	}
	return;
};

void infrastructure::queryOrder(string adapterID)
{
	switch (m_adapterTypeMap[adapterID])
	{
	case ADAPTER_CTP_TRADE:
	{
		tradeAdapterCTP * pTradeAdapter = (tradeAdapterCTP *)m_adapters[adapterID];
		pTradeAdapter->queryOrder();
		break;
	}
	case ADAPTER_TAP_TRADE:
	{
		break;
	}
	}
	return;
};
void infrastructure::onRtnCtpOrder(string adapterID, CThostFtdcOrderFieldPtr pOrder)
{
	orderRtnPtr orderPtr = orderRtnPtr(new orderRtn_struct());
	int orderRef = atoi(pOrder->OrderRef);
	orderPtr->m_orderRef = orderRef;
	orderPtr->m_direction = m_orderDirMapRev[ADAPTER_CTP_TRADE][pOrder->Direction];
	orderPtr->m_InstrumentID = string(pOrder->InstrumentID);
	orderPtr->m_orderStatus = m_orderStatusMapRev[ADAPTER_CTP_TRADE][pOrder->OrderStatus];
	orderPtr->m_price = pOrder->LimitPrice;
	orderPtr->m_statusMsg = string(pOrder->StatusMsg);
	orderPtr->m_volumeTotal = pOrder->VolumeTotal;
	orderPtr->m_VolumeTotalOriginal = pOrder->VolumeTotalOriginal;
	orderPtr->m_volumeTraded = pOrder->VolumeTraded;
	orderPtr->m_ZCETotalTradedVolume = pOrder->ZCETotalTradedVolume;
	orderPtr->m_tradingDay = string(pOrder->TradingDay);
	auto iter1 = m_orderRtnHandlers.find(adapterID);
	if (iter1 != m_orderRtnHandlers.end())
	{
		auto iter2 = iter1->second.find(orderRef);
		if (iter2 != iter1->second.end())
			m_tradeTP->getDispatcher().post(bind((m_orderRtnHandlers[adapterID][orderRef]), orderPtr));
		else if (broadcastOrder)
			broadcastOrder(orderPtr);
	}
};

void infrastructure::onRtnCtpTrade(string adapterID, CThostFtdcTradeField *pTrade)
{
	int orderRef = atoi(pTrade->OrderRef);

	tradeRtnPtr tradePtr = tradeRtnPtr(new tradeRtn_struct());
	tradePtr->m_instId = string(pTrade->InstrumentID);
	tradePtr->m_tradeId = string(pTrade->TradeID);
	tradePtr->m_exchange = string(pTrade->ExchangeID);
	tradePtr->m_orderDir = m_orderDirMapRev[ADAPTER_CTP_TRADE][pTrade->Direction];
	if (THOST_FTDC_OF_Open == pTrade->OffsetFlag)
		tradePtr->m_positionEffectTyp = m_positinEffectMapRev[ADAPTER_CTP_TRADE][THOST_FTDC_OFEN_Open];
	else if (THOST_FTDC_OF_Close == pTrade->OffsetFlag)
		tradePtr->m_positionEffectTyp = m_positinEffectMapRev[ADAPTER_CTP_TRADE][THOST_FTDC_OFEN_Close];
	tradePtr->m_orderRef = orderRef;
	tradePtr->m_price = pTrade->Price;
	tradePtr->m_volume = pTrade->Volume;
	tradePtr->m_tradeDate = string(pTrade->TradeDate);

	auto iter = m_tradeRtnHandlers.find(adapterID);
	if (iter != m_tradeRtnHandlers.end())
	{
		auto iter2 = iter->second.find(orderRef);
		if (iter2 != iter->second.end())
			m_tradeTP->getDispatcher().post(bind((m_tradeRtnHandlers[adapterID][orderRef]), tradePtr));
	}
};

void infrastructure::onRtnTapOrder(string adapterID, TapAPIOrderInfoNotice *pOrder)
{
	orderRtnPtr orderPtr = orderRtnPtr(new orderRtn_struct());
	int orderRef = pOrder->OrderInfo->RefInt;
	auto iter1 = m_orderRtnHandlers.find(adapterID);
	if (iter1 != m_orderRtnHandlers.end())
	{
		auto iter2 = iter1->second.find(orderRef);
		if (iter2 != iter1->second.end())
			m_tradeTP->getDispatcher().post(bind((m_orderRtnHandlers[adapterID][orderRef]), orderPtr));
	}
};
void infrastructure::onRtnTapTrade(string adapterID, TapAPIFillInfo *pTrade)
{
	tradeRtnPtr tradePtr = tradeRtnPtr(new tradeRtn_struct());
	//int orderRef = atoi(pTrade->ref);
	//auto iter = m_tradeRtnHandlers.find(orderRef);
	//if (iter != m_tradeRtnHandlers.end())
	//	m_tradeRtnHandlers[orderRef](tradePtr);
};