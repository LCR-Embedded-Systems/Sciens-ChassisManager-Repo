#include "../include/lcr_network_config.hpp"

void network_config_manager::display_Address_elements() 
{
    std::cout << "Elements of Address:" << std::endl;
    std::cout << "\tAddress" << std::endl;
    std::cout << "\tPeer" << std::endl;
    std::cout << "\tBroadcast" << std::endl;
    std::cout << "\tLabel" << std::endl;
    std::cout << "\tPreferredLifetime" << std::endl;
    std::cout << "\tScope" << std::endl;
    std::cout << "\tRouteMetric" << std::endl;
    std::cout << "\tHomeAddress" << std::endl;
    std::cout << "\tDuplicateAddressDetection" << std::endl;
    std::cout << "\tAddPrefixRoute" << std::endl;
}


int network_config_manager::set_Address_element(std::string element, std::string value)
{
    return AddressMap[element](value);
}

//helper function for passing section and passthrough element/value to set_element util and error handling
void network_config_manager::handle_Address_set(std::string element, std::string value)
{
    if (set_element("Address", element, value) == -1)
    {
        display_Address_elements();
    }
}

/*
 * helper functions for passing element/value to set_element util
 */

int network_config_manager::set_Address_Address(std::string value)           {handle_Address_set("Address",                   value);return 0;} 
int network_config_manager::set_Peer(std::string value)                      {handle_Address_set("Peer",                      value);return 0;} 
int network_config_manager::set_Broadcast(std::string value)                 {handle_Address_set("Broadcast",                 value);return 0;} 
int network_config_manager::set_Label(std::string value)                     {handle_Address_set("Label",                     value);return 0;} 
int network_config_manager::set_PreferredLifetime(std::string value)         {handle_Address_set("PreferredLifetime",         value);return 0;} 
int network_config_manager::set_Scope_Address(std::string value)             {handle_Address_set("Scope",                     value);return 0;} 
int network_config_manager::set_RouteMetric(std::string value)               {handle_Address_set("RouteMetric",               value);return 0;} 
int network_config_manager::set_HomeAddress(std::string value)               {handle_Address_set("HomeAddress",               value);return 0;} 
int network_config_manager::set_DuplicateAddressDetection(std::string value) {handle_Address_set("DuplicateAddressDetection", value);return 0;} 
int network_config_manager::set_AddPrefixRoute(std::string value)            {handle_Address_set("AddPrefixRoute",            value);return 0;} 

