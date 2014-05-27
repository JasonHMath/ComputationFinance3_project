#ifndef Write2Txt_H
#define Write2Txt_H

#include "Simple.h"
#include <iostream>
#include <fstream>		// To read from or write to a file
#include <ctime>

using std::time_t;
using std::string;
using std::ofstream;
using std::endl;
using std::cerr;
using std::fstream;

class Write2Txt
{
public:
	Write2Txt(string s1);

	void Write_txt_file(time_t time, string simple, double qty, double px);

private:
	ofstream outfile;
	string s1_;

};

#endif