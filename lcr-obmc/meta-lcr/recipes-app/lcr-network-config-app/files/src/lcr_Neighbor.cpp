#include "../include/lcr_network_config.hpp"



void network_config_manager::display_Neighbor_elements()
{
    std::cout << "Elements of Neighbor:" << std::endl;
    std::cout << "\tAddress" << std::endl;
    std::cout << "\tLinkLayerAddress" << std::endl;
}


int network_config_manager::set_Neighbor_element(std::string element, std::string value)
{
    return NeighborMap[element](value);
}

//helper function for passing section and passthrough element/value to set_element util and error handling
void network_config_manager::handle_Neighbor_set(std::string element, std::string value)
{
    if (set_element("Neighbor", element, value) == -1)
    {
        display_Neighbor_elements();
    }
}

/*
 * helper functions for passing element/value to set_element util
 */
int network_config_manager::set_Address_Neighbor(std::string value) { handle_Network_set("Address",          value);return 0;}
int network_config_manager::set_LinkLayerAddress(std::string value) { handle_Network_set("LinkLayerAddress", value);return 0;}


