#include "Simple.h"
#include "Strategy.h"

Simple::Simple(Strategy & strategy)
	: strategy_(strategy)
{ }

Simple::~Simple()
{
	std::cout << "Shutting down..." << std::endl;
	
	if(initiator_) initiator_->stop();
	delete initiator_;
	delete logFactory_;
	delete messageStoreFactory_;
	delete sessionSettings_;
}

/// Establish FIX connections and do any other setup.
void Simple::Init(const std::string & configFile)
{
	// Boilerplate quickfix setup:
	sessionSettings_ = new FIX::SessionSettings(configFile);
	messageStoreFactory_ = new FIX::FileStoreFactory(*sessionSettings_);
	logFactory_ = new FIX::FileLogFactory(*sessionSettings_);
	initiator_ = new FIX::SocketInitiator(*this, *messageStoreFactory_, *sessionSettings_, *logFactory_);
	initiator_->start();
	
	// Make sure all Sessions are logged on before we tell our Strategy it is OK to start:
	for(int i = 0; i < 10; ++i)
	{
		FIX::Session* marketDataSession = FIX::Session::lookupSession(mdSessionId_);
		FIX::Session* orderSession = FIX::Session::lookupSession(orderSessionId_);
		if(marketDataSession && orderSession)
		{
			if(marketDataSession->isLoggedOn() && orderSession->isLoggedOn())
			{
				strategy_.OnInit(*this);
				return;
			}
		}
		std::cout << "[init] Waiting for all FIX Sessions to logon..." << std::endl;
		FIX::process_sleep(1);
	}

	throw std::runtime_error("[init] Fatal error: timed out waiting for all FIX Sessions to logon!");
}

void Simple::SendMarketDataSubscription(const std::string & symbol, const std::string & maturityMonthYear)
{
	// We want the latest snapshot, plus updates, for top of the book:
	FIX42::MarketDataRequest msg;
	msg.set(FIX::MDReqID(idHelper_.GetNextMDRequestId()));
	msg.set(FIX::SubscriptionRequestType(FIX::SubscriptionRequestType_SNAPSHOT_PLUS_UPDATES));
	msg.set(FIX::MarketDepth(1));
	msg.set(FIX::MDUpdateType(FIX::MDUpdateType_INCREMENTAL_REFRESH));
	msg.set(FIX::AggregatedBook(FIX::AggregatedBook_YES));

	// We want best bid, best offer, and last trade notifications
	FIX42::MarketDataRequest::NoMDEntryTypes marketDataEntryGroup;
	marketDataEntryGroup.set(FIX::MDEntryType(FIX::MDEntryType_BID));
	msg.addGroup(marketDataEntryGroup);
	marketDataEntryGroup.set(FIX::MDEntryType(FIX::MDEntryType_OFFER));
	msg.addGroup(marketDataEntryGroup);
	marketDataEntryGroup.set(FIX::MDEntryType(FIX::MDEntryType_TRADE));
	msg.addGroup(marketDataEntryGroup);

	// Repeating group for the instrument to which we are subscribing:
	FIX42::MarketDataRequest::NoRelatedSym symbolGroup;
	symbolGroup.set(FIX::Symbol(symbol));
	symbolGroup.set(FIX::MaturityMonthYear(maturityMonthYear));
	symbolGroup.set(FIX::SecurityExchange("CME"));
	symbolGroup.set(FIX::SecurityType("FUT"));
	msg.addGroup(symbolGroup);
	
	
	FIX::Session::sendToTarget(msg, mdSessionId_);
}

void Simple::SendMarketOrder(const std::string & symbol, const std::string & maturityMonthYear, const std::string & account, SimpleSide side, int qty)
{
	FIX42::NewOrderSingle msg;
	
	if(SimpleSide::BUY == side) msg.set(FIX::Side(FIX::Side_BUY));
	else msg.set(FIX::Side(FIX::Side_SELL));

	msg.set(FIX::Symbol(symbol));
	msg.set(FIX::MaturityMonthYear(maturityMonthYear));
	msg.set(FIX::Account(account));
	msg.set(FIX::OrderQty(qty));	
	msg.set(FIX::ClOrdID(idHelper_.GetNextOrderId()));
	
	msg.set(FIX::OrdType(FIX::OrdType_MARKET));
	msg.set(FIX::TimeInForce(FIX::TimeInForce_DAY));
	msg.set(FIX::SecurityExchange("CME"));
	msg.set(FIX::SecurityType("FUT"));
	msg.set(FIX::CustomerOrFirm(0));
	msg.set(FIX::Rule80A('A'));

	FIX::Session::sendToTarget(msg, orderSessionId_);
}

void Simple::onMessage(const FIX42::ExecutionReport& msg, const FIX::SessionID&)
{
	FIX::ExecType execType;
	FIX::Symbol symbol;
	FIX::MaturityMonthYear maturityMonthYear;
	FIX::Side side;

	// See what kind of execution report this is:
	msg.get(execType);
	if (FIX::ExecType_FILL == execType.getValue() || FIX::ExecType_PARTIAL_FILL == execType.getValue())
	{
		FIX::LastShares lastQty;
		FIX::LastPx lastPx;
		msg.get(symbol);
		msg.get(maturityMonthYear);
		msg.get(side);
		msg.get(lastQty);
		msg.get(lastPx);

		// Let our Strategy know about the fill:
		if (FIX::Side_BUY == side)
		{
			strategy_.OnOrderFill(*this, SimpleSide::BUY, lastQty.getValue(), lastPx.getValue());
		}
		else if (FIX::Side_SELL == side)
		{
			strategy_.OnOrderFill(*this, SimpleSide::SELL, lastQty.getValue(), lastPx.getValue());
		}
	}
	else if (FIX::ExecType_REJECTED == execType.getValue())
	{
		FIX::OrderQty orderQty;
		msg.get(symbol);
		msg.get(maturityMonthYear);
		msg.get(side);
		msg.get(orderQty);

		// Let our Strategy know about the reject
		if (FIX::Side_BUY == side)
		{
			strategy_.OnOrderReject(*this, SimpleSide::BUY, orderQty.getValue());
		}
		else if (FIX::Side_SELL == side)
		{
			strategy_.OnOrderReject(*this, SimpleSide::SELL, orderQty.getValue());
		}

		std::cout << std::endl << "RECEIVED REJECT: " << msg << std::endl;
	}
	else if (FIX::ExecType_NEW == execType.getValue())
	{
		// Our order was accepted (but has not yet been filled)
		// You may want to change your book keeping to indicate the order is ack'd by the exchange
	}
	else
	{
		std::cout << "Not sure what to do with ExecutionReport with ExecType=" << execType << ": " << msg << std::endl;
	}
}


void Simple::onMessage(const FIX42::MarketDataSnapshotFullRefresh& msg, const FIX::SessionID&)
{

	FIX::NoMDEntries noMDEntries;
	msg.get(noMDEntries);
	for (int i = 1; i <= noMDEntries; ++i)
	{
		FIX42::MarketDataSnapshotFullRefresh::NoMDEntries group;
		FIX::MDEntryType type;
		FIX::MDEntryPx px;
		FIX::MDEntrySize qty;

		msg.getGroup(i, group);
		group.get(type);
		group.get(px);
		group.get(qty);

		if (FIX::MDEntryType_BID == type.getValue())
		{
			strategy_.OnBestBidUpdate(*this, qty.getValue(), px.getValue());
		}
		else if (FIX::MDEntryType_OFFER == type.getValue())
		{
			strategy_.OnBestOfferUpdate(*this, qty.getValue(), px.getValue());
		}
		else if (FIX::MDEntryType_TRADE == type.getValue())
		{
			strategy_.OnLastTradeUpdate(*this, qty.getValue(), px.getValue());
		}
		else
		{
			std::cout << "Unknown MDEntryType: " << type << std::endl;
		}
	}
}

void Simple::onMessage(const FIX42::MarketDataIncrementalRefresh& msg, const FIX::SessionID&)
{
	FIX::NoMDEntries noMDEntries;
	msg.get(noMDEntries);
	for (int i = 1; i <= noMDEntries; ++i)
	{
		FIX42::MarketDataIncrementalRefresh::NoMDEntries group;
		FIX::MDEntryType type;
		FIX::MDEntryPx px;
		FIX::MDEntrySize qty;
		FIX::MDUpdateAction action;

		msg.getGroup(i, group);
		group.get(type);
		group.get(px);
		group.get(qty);
		group.get(action);

		if (FIX::MDUpdateAction_NEW == action.getValue() || FIX::MDUpdateAction_CHANGE == action.getValue())
		{
			if (FIX::MDEntryType_BID == type.getValue())
			{
				strategy_.OnBestBidUpdate(*this, qty.getValue(), px.getValue());
			}
			else if (FIX::MDEntryType_OFFER == type.getValue())
			{
				strategy_.OnBestOfferUpdate(*this, qty.getValue(), px.getValue());
			}
			else if (FIX::MDEntryType_TRADE == type.getValue())
			{
				strategy_.OnLastTradeUpdate(*this, qty.getValue(), px.getValue());
			}
			else
			{
				std::cout << "Unknown MDEntryType: " << type << std::endl;
			}
		}
	}
}

void Simple::onMessage(const FIX42::MarketDataRequestReject& msg, const FIX::SessionID&)
{
	FIX::MDReqID reqId;
	FIX::MDReqRejReason reason;
	FIX::Text text;

	if (msg.isSetField(FIX::FIELD::MDReqID)) msg.get(reqId);

	if (msg.isSetField(FIX::FIELD::MDReqRejReason)) msg.get(reason);

	if (msg.isSetField(FIX::FIELD::Text)) msg.get(text);

	std::cout << "MarketDataRequestReject: MDReqID=" << reqId << ", reason=" << reason << ", text=" << text << std::endl;
}


//-----------------------------------------------------------------------------
// Called by QF whenever a Session is successfully logged on.
//
// We need to know which SessionID to use when sending orders vs sending a
// market data subscription.  The onLogon() callback is a good time to
// distinguish between the two Sessions.  We will use a couple of custom config
// file options to help us do that.
void Simple::onLogon(const FIX::SessionID& sessionId)
{
	// Ask QF for the SessionSettings for this Session
	const FIX::Dictionary* settings = initiator_->getSessionSettings(sessionId);

	// Grab our custom "MyMarketDataSession" parameter (if it exists) from the SessionSettings
	if(settings->has("MyMarketDataSession") && settings->getBool("MyMarketDataSession"))
	{
		mdSessionId_ = sessionId;
		std::cout << "[onLogon] " << mdSessionId_ << " (MyMarketDataSession)" << std::endl;
	}

	// Grab our custom "MyOrderSession" parameter (if it exists) from the SessionSettings
	if(settings->has("MyOrderSession") && settings->getBool("MyOrderSession"))
	{
		orderSessionId_ = sessionId;
		std::cout << "[onLogon] " << orderSessionId_ << " (MyOrderSession)" << std::endl;
	}
}


//-----------------------------------------------------------------------------
// Called by QF right before an Admin-type message is sent to the counterparty.
//
// Examples of Admin-type messages are Logon, Logout, and Heartbeat.  We never
// send these types of messages ourselves -- QF does so for us automatically.
//
// However, we may need to customize the content of an Admin-type -- e.g., our
// counterparty may require us to specify a username or password.
void Simple::toAdmin( FIX::Message& message, const FIX::SessionID& sessionId) 
{
	// First we have to figure out what the message is
	// Remember: Msgtype is in the header, not the body!
	FIX::MsgType msgType;
	message.getHeader().getField(msgType);
	
	// Tip: right-click 'FIX::MsgType_Logon' and select 'Go To Definition' to see other useful contants that QF defines for you.
	// Tip: hover your mouse cursor over 'FIX::MsgType_Logon' to see its value.
	if(FIX::MsgType_Logon == msgType)
	{
		const FIX::Dictionary* settings = initiator_->getSessionSettings(sessionId);
		if(settings->has("MyPassword"))
		{
			message.setField(FIX::RawData(settings->getString("MyPassword")));
		}
		else
		{
			std::cout << "Warning: MyPassword not found in cfg file for session " << sessionId << std::endl;
		}
	}
}


//-----------------------------------------------------------------------------
// Called by QF after we call Session::sendToTarget() to send a message, right
// before the message is actually transmitted.  
//
// Examples of App-type messages that we might send are NewOrderSingle,
// OrderCancelRequest, and MarketDataRequest.
//
// The FIX Protocol guarantees in-order delivery of all messages.  For example,
// if you temporarily lose your network connection, FIX Protocol ensures that
// any messages that failed to make it to either counterparty will be
// re-transmitted.
//
// This is helpful behaviour when you are, say, receiving an ExecutionReport.  It
// is probably NOT helpful behaviour if say, you send a NewOrderSingle which gets
// re-transmitted an hour later when the network suddenly comes back up and the
// market has moved significantly!
//
// This is your chance to thwart the automatic resend behaviour if you do not
// want it.
void Simple::toApp( FIX::Message& message, const FIX::SessionID& sessionID ) throw( FIX::DoNotSend )
{
	try
	{
		FIX::PossDupFlag possDupFlag;
	
		message.getHeader().getField(possDupFlag); // Throws FIX::FieldNotFound if PossDupFlag is not present.
		
		if(true == possDupFlag.getValue())
			throw FIX::DoNotSend();                // This will prevent QF from sending the message.
	}
	catch(FIX::FieldNotFound &)
	{ }

	std::cout << std::endl << "OUT: " << message << std::endl;
}

//-----------------------------------------------------------------------------
// Called by QF when an App-type message is received from the counterparty.
//
// Examples of Application-type messages are ExecutionReport and CancelReject.
// We could just write all of our code for handling these message right here in
// this callback.
//
// However, we would probably end up with a really, really long function if we
// did that.  Instead, we usually just call the QF crack() function here, which
// calls the proper onMessage() callback for whatever MsgType we just received.
void Simple::fromApp( const FIX::Message& message, const FIX::SessionID& sessionID )
	throw( FIX::FieldNotFound, FIX::IncorrectDataFormat, FIX::IncorrectTagValue, FIX::UnsupportedMessageType )
{
	crack( message, sessionID );
}


//-----------------------------------------------------------------------------
// Called by QF when an Admin-type message is received from the counterparty.
//
// Examples of Admin-type messages are Logon, Logout, and Heartbeat.
//
// We almost never want or need to do anything in this callback.  QF handles
// these types of messages for us automatically.
void Simple::fromAdmin( const FIX::Message&, const FIX::SessionID& ) 
throw( FIX::FieldNotFound, FIX::IncorrectDataFormat, FIX::IncorrectTagValue, FIX::RejectLogon )
{}


//-----------------------------------------------------------------------------
// Called by QF when a Session is created (but before it is logged on).
// We do not usually do anything here.
void Simple::onCreate( const FIX::SessionID& sessionId)
{ }


//-----------------------------------------------------------------------------
// Called by QF when a Session is either logged out or suddenly disconnected.
// We can use this to notify our application of a lost connection.
void Simple::onLogout( const FIX::SessionID& sessionId )
{
	std::cout << "[onLogout] " << sessionId << std::endl;
}


/// Useful for printing.
std::ostream & operator<<(std::ostream & out, const SimpleSide & side)
{
	switch(side)
	{
	case BUY:
		out << "BUY";
		return out;
	case SELL:
		out << "SELL";
		return out;
	default:
		throw std::invalid_argument("Unknown side");
	}
}
