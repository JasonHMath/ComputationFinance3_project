#ifndef ID_HELPER_H
#define ID_HELPER_H

#include <string>

using std::string;

/// Creates unique IDs for orders and market data subscriptions
class IdHelper
{
public:
	IdHelper();
	~IdHelper();

	string GetNextOrderId();

	string GetCurrentOrderId();
	
	string GetNextMDRequestId();
	
	void WriteOrderIdToFile();
	
	int ReadOrderIdFromFile();

private:
	IdHelper(const IdHelper&) = delete;
	IdHelper& operator=(const IdHelper&) = delete;

private:
	int orderId_;
	int mdRequestId_;
};

#endif