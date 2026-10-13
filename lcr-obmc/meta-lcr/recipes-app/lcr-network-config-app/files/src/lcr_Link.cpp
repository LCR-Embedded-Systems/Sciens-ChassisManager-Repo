#include "../include/lcr_network_config.hpp"

void network_config_manager::display_Link_elements() 
{
    std::cout << "Elements of Link:" << std::endl;
    std::cout << "\tMACAddress" << std::endl;
    std::cout << "\tUnmanaged" << std::endl;
    std::cout << "\tMTUBytes" << std::endl;
    std::cout << "\tConfigureWithoutCarrier" << std::endl;
}

int network_config_manager::set_Link_element(std::string element, std::string value)
{
    return LinkMap[element](value);
}

//helper function for passing section and passthrough element/value to set_element util and error handling
void network_config_manager::handle_Link_set(std::string element, std::string value)
{
    if (set_element("Link", element, value) == -1)
    {
        display_Link_elements();
    }
}

/*
 * helper functions for passing element/value to set_element util
 */

int network_config_manager::set_MACAddress_Link(std::string value)         {handle_Link_set("MACAddress",               value);return 0;}
int network_config_manager::set_Unmanaged(std::string value)               {handle_Link_set("Unmanaged",                value);return 0;}
int network_config_manager::set_MTUBytes(std::string value)                {handle_Link_set("MTUBytes",                 value);return 0;}
int network_config_manager::set_ConfigureWithoutCarrier(std::string value) {handle_Link_set("ConfigureWithoutCarrier", value);return 0;}

