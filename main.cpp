#include "basic.h"
#include <fstream>

basic::interpreter* interp = nullptr;

int main(int argc, char** argv)
{
	basic::interpreter bi("interpreter_session");
	bi.print_version(std::cout);
	std::cout << "\nbasic:$ ";

	std::string buff;
	std::string cmd;

	while (std::getline(std::cin, buff))
	{
		cmd = buff.substr(0, buff.find_first_of(' '));
		if ((cmd == "quit") || (cmd == "exit"))
		{
			// bail
			break;
		}
		bi.eval(buff);
		std::cout << "basic:$ ";
	}

/*
 * Never define the following:
 * Will create a mess (multiple ret value).
 *
	llvm::IRBuilder<> builder(bi.get_current_block());
	builder.CreateRetVoid();
*/

	buff = "; Basic Interpreter Session\n";
	bi.print_module(buff);
	std::cout << buff << "\n";

	std::ofstream ofs("session.ll");
	ofs << buff << "\n";
	ofs.close();

	return 0;
}

