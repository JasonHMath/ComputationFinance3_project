# include "Write2Txt.h"

Write2Txt::Write2Txt(string s1) :s1_(s1)
{
	fstream outfile;

}

void Write2Txt::Write_txt_file(time_t time, string simple, double qty, double px){

	outfile.open(s1_, std::ios_base::app);
	outfile << time << ' ' << simple << ' ' << qty << ' ' << px << endl;
	outfile.close();

}