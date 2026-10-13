
#include "../include/lcr_mux.hpp"

int main(int argc, char* argv[])
{

    CLI cli;
    if (argc > 1) {
        cli.process_argument(argc, argv);
    } else {
        std::cout << "No channel provided. Select a channel between 0 and 15" << std::endl;
        std::cout << "\tUsage: mux [integer from 0 to 15]" << std::endl;
    }

    return 0;
}
