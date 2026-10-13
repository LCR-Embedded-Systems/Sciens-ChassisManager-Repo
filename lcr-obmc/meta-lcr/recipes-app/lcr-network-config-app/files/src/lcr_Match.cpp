#include "../include/lcr_network_config.hpp"

void network_config_manager::display_Match_elements() 
{
    std::cout << "Elements of Match:" << std::endl;
    std::cout << "\tName" << std::endl;
    std::cout << "\tType" << std::endl;
    std::cout << "\tMACAddress" << std::endl;
    std::cout << "\tPermanentMACAddress" << std::endl;
    std::cout << "\tPath" << std::endl;
    std::cout << "\tDriver" << std::endl;
    std::cout << "\tKind" << std::endl;
    std::cout << "\tProperty" << std::endl;
    std::cout << "\tKernelCommandLine" << std::endl;
    std::cout << "\tVirtualization" << std::endl;
}

int network_config_manager::set_Match_element(std::string element, std::string value)
{
    return MatchMap[element](value);
}

//helper function for passing section and passthrough element/value to set_element util and error handling
void network_config_manager::handle_Match_set(std::string element, std::string value)
{
    if (set_element("Match", element, value) == -1)
    {
        display_Match_elements();
    }
}

/*
 * helper functions for passing element/value to set_element util
 */

int network_config_manager::set_Name(std::string value)                {handle_Match_set("Name",                value);return 0;}
int network_config_manager::set_Type_Match(std::string value)          {handle_Match_set("Type",                value);return 0;}
int network_config_manager::set_MACAddress_Match(std::string value)    {handle_Match_set("MACAddress",          value);return 0;}
int network_config_manager::set_PermanentMACAddress(std::string value) {handle_Match_set("PermanentMACAddress", value);return 0;}
int network_config_manager::set_Path(std::string value)                {handle_Match_set("Path",                value);return 0;}
int network_config_manager::set_Driver(std::string value)              {handle_Match_set("Driver",              value);return 0;}
int network_config_manager::set_Kind(std::string value)                {handle_Match_set("Kind",                value);return 0;}
int network_config_manager::set_Property(std::string value)            {handle_Match_set("Property",            value);return 0;}
int network_config_manager::set_KernelCommandLine(std::string value)   {handle_Match_set("KernelCommandLine",   value);return 0;}
int network_config_manager::set_Virtualization(std::string value)      {handle_Match_set("Virtualization",      value);return 0;}

