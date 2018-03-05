#include "basic.h"
#include <fstream>

basic::interpreter* interp = nullptr;

int main(int argc, char** argv)
{
	basic::interpreter bi("session");
	bi.print_version(std::cout);
	std::cout << "\nbasic:$ ";

	std::string buff;
	std::string cmd;

	std::ofstream bas_mod("session.bas");

	while (std::getline(std::cin, buff))
	{
		cmd = buff.substr(0, buff.find_first_of(' '));
		if ((cmd == "quit"))
		{
			// bail
			break;
		}
		if (bi.eval(buff) >= 0)
		{
			// record the statement into a file
			bas_mod << buff << "\n";
		}
		std::cout << "basic:$ ";
	}

	bas_mod.close();

	// this should make correct return void
	bi.quit();

	buff = "; Output from Basic Interpreter Session";
	bi.print_module(buff);
	std::cout << buff << "\n";

	std::ofstream ofs("session.ll");
	ofs << buff << "\n";
	ofs.close();

	return 0;
}

