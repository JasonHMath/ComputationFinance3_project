#ifndef SIMPLE_H
#define SIMPLE_H

#include <string>
#include <iostream>
#include <quickfix/Application.h>
#include <quickfix/MessageCracker.h>
#include <quickfix/FileLog.h>
#include <quickfix/FileStore.h>
#include <quickfix/SocketInitiator.h>
#include <quickfix/fix42/MarketDataRequest.h>
#include <quickfix/fix42/MarketDataRequestReject.h>
#include <quickfix/fix42/MarketDataSnapshotFullRefresh.h>
#include <quickfix/fix42/MarketDataIncrementalRefresh.h>
#include <quickfix/fix42/NewOrderSingle.h>
#include <quickfix/fix42/ExecutionReport.h>
#include "IdHelper.h"

class Strategy;

enum SimpleSide { BUY = '1', SELL = '2' };

/// A simple interface that allows our Strategy to subscribe to market data and send orders.
class Simple :public FIX::Application,
              public FIX::MessageCracker
{
public:
	Simple(Strategy & strategy);
	~Simple();

	/// Establish FIX connections and do any other setup.
	void Init(const std::string & configFile);

	/// Subscribe to market data updates for an instrument.
	void SendMarketDataSubscription(const std::string & symbol, const std::string & maturityMonthYear);
	
	/// Send a market order
	void SendMarketOrder(const std::string & symbol, const std::string & maturityMonthYear, const std::string & account, SimpleSide side, int qty);

private: 
	// QF callbacks
	void onMessage(const FIX42::ExecutionReport&, const FIX::SessionID&);
	void onMessage(const FIX42::MarketDataSnapshotFullRefresh&, const FIX::SessionID&);
	void onMessage(const FIX42::MarketDataIncrementalRefresh& msg, const FIX::SessionID&);
	void onMessage(const FIX42::MarketDataRequestReject&, const FIX::SessionID&);

	// More QF callbacks
	void onCreate(const FIX::SessionID&);
	void onLogon(const FIX::SessionID&);
	void onLogout(const FIX::SessionID&);
	void toAdmin(FIX::Message&, const FIX::SessionID&);
	void toApp(FIX::Message&, const FIX::SessionID&) throw(FIX::DoNotSend);
	void fromAdmin(const FIX::Message&, const FIX::SessionID&) throw(FIX::FieldNotFound, FIX::IncorrectDataFormat, FIX::IncorrectTagValue, FIX::RejectLogon);
	void fromApp(const FIX::Message& message, const FIX::SessionID& sessionID) throw(FIX::FieldNotFound, FIX::IncorrectDataFormat, FIX::IncorrectTagValue, FIX::UnsupportedMessageType);
	
	Strategy& strategy_;
	IdHelper idHelper_;

	FIX::SessionID mdSessionId_;
	FIX::SessionID orderSessionId_;
	FIX::MessageStoreFactory* messageStoreFactory_;
	FIX::FileLogFactory* logFactory_;
	FIX::SessionSettings* sessionSettings_;
	FIX::SocketInitiator* initiator_;
};

//Useful for printing.
std::ostream & operator<<(std::ostream & out, const SimpleSide & side);

#endif