
#include "../include/lcr_network_config.hpp"

int main(int argc, char* argv[])
{
    
    network_config_manager inst;
    if (argc > 1) {
        inst.process_argument(argc-1, argv);
    } else {
        inst.display_options();
    }
    
    return 0;
}
