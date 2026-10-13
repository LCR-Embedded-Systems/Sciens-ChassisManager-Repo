
#include "../include/lcr_interface.hpp"

int main(int argc, char* argv[])
{

    CLI cli;

    if (argc > 1) {
        cli.process_argument(argc-1, argv);
    } else {
        cli.displayCommands();
    }

    return 0;
}
