#ifndef STRATEGY_H
#define STRATEGY_H

#include "Simple.h"
#include "Write2Txt.h"
#include <memory>
#include <time.h> 

using std::shared_ptr;

/// A basic workspace for implementing our strategy logic, receiving market data, receiving fills, etc.
class Strategy
{
public:
	/// Create a strategy that trades the given instrument for the given account.
	Strategy(const std::string & symbol, const std::string & maturityMonthYear, const std::string & account, const shared_ptr<Write2Txt>& W1);

	/// This callback is called once by Simple to let us know that it is done initializing itself.
	void OnInit(Simple & simple);

	/// This callback is called by Simple to let us know when the best bid changes.
	void OnBestBidUpdate(Simple & simple, double qty, double px);

	/// This callback is called by Simple to let us know when the best offer changes.
	void OnBestOfferUpdate(Simple & simple, double qty, double px);

	/// This callback is called by Simple whenever the trade ticker changes
	void OnLastTradeUpdate(Simple & simple, double qty, double px);

	/// This callback is called by Simple to let us know that an order was filled.
	void OnOrderFill(Simple & simple, SimpleSide side, double qty, double px);

	/// This callback is called by Simple to let us know that an order was rejected.
	void OnOrderReject(Simple & simple, SimpleSide side, double qty);

private:
	const std::string symbol_;
	const std::string maturityMonthYear_;
	const std::string account_;
	shared_ptr<Write2Txt> W1_;
	
};

#endif