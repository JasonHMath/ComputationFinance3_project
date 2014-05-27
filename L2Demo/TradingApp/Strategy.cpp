#include "Strategy.h"


Strategy::Strategy(const string& symbol,
	const string& maturityMonthYear,
	const string& account,
     const shared_ptr<Write2Txt>& W1)
	: symbol_(symbol),
	maturityMonthYear_(maturityMonthYear),
	account_(account),
	W1_(W1)
{ }

void Strategy::OnInit(Simple & simple)
{
	// Subscribing to market data at startup
	simple.SendMarketDataSubscription(symbol_, maturityMonthYear_);
}

void Strategy::OnOrderFill(Simple & simple, SimpleSide side, double qty, double px)
{
	std::cout << std::endl << "RECEIVED FILL: side=" << side << ", price=" << px << ", qty=" << qty << std::endl;

	// You may want to update your position inventories

}

void Strategy::OnOrderReject(Simple & simple, SimpleSide side, double qty)
{
	// The order was rejected -- you may want to update your working quantity
	
}


void Strategy::OnBestBidUpdate(Simple & simple, double qty, double px)
{
	std::cout << "MarketDataUpdate: BID " << px << " / " << qty << std::endl;

	//Simple example: sell if the bid is higher than a cerain value
	time_t timer;

	time(&timer);

	W1_->Write_txt_file(timer, "BID", qty, px);
		
}

void Strategy::OnBestOfferUpdate(Simple & simple, double qty, double px)
{
	std::cout << "MarketDataUpdate: OFFER " << px << " / " << qty << std::endl;

	time_t timer;

	time(&timer);

	W1_->Write_txt_file(timer, "OFFER", qty, px);
	//Simple example: sell if the bid is higher than a cerain value
		
}

void Strategy::OnLastTradeUpdate(Simple & simple, double qty, double px)
{
	std::cout << "MarketDataUpdate: Last Trade " << px << " / " << qty << std::endl;

	//You can have make a trading decision based on the last trade 

	time_t timer;
	
	time(&timer);

	W1_->Write_txt_file(timer, "Last Trade", qty, px);


}