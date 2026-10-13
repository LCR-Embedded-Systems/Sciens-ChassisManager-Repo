#include "../include/lcr_network_config.hpp"

void network_config_manager::display_IPv6AcceptRA_elements() 
{
    std::cout << "Elements of IPv6AcceptRA:" << std::endl;
    std::cout << "\tDHCPv6Client" << std::endl;
    std::cout << "\tUseDNS" << std::endl;
    std::cout << "\tUseDomains" << std::endl;
    std::cout << "\tUseAutonomousPrefix" << std::endl;
    std::cout << "\tUseOnLinkPrefix" << std::endl;
    std::cout << "\tUseGateway" << std::endl;
    std::cout << "\tRouteMetric" << std::endl;
    std::cout << "\tToken" << std::endl;
}

int network_config_manager::set_IPv6AcceptRA_element(std::string element, std::string value)
{
    return IPv6AcceptRAMap[element](value);
}

//helper function for passing section and passthrough element/value to set_element util and error handling
void network_config_manager::handle_IPv6AcceptRA_set(std::string element, std::string value)
{
    if (set_element("IPv6AcceptRA", element, value) == -1)
    {
        display_IPv6AcceptRA_elements();
    }
}

/*
 * helper functions for passing element/value to set_element util
 */

int network_config_manager::set_DHCPv6Client(std::string value)        {handle_IPv6AcceptRA_set("DHCPv6Client",        value);return 0;}
int network_config_manager::set_UseDNS_IPv6(std::string value)         {handle_IPv6AcceptRA_set("UseDNS",              value);return 0;}
int network_config_manager::set_UseDomains_IPv6(std::string value)     {handle_IPv6AcceptRA_set("UseDomains",          value);return 0;}
int network_config_manager::set_UseAutonomousPrefix(std::string value) {handle_IPv6AcceptRA_set("UseAutonomousPrefix", value);return 0;}
int network_config_manager::set_UseOnLinkPrefix(std::string value)     {handle_IPv6AcceptRA_set("UseOnLinkPrefix",     value);return 0;}
int network_config_manager::set_UseGateway_IPv6(std::string value)     {handle_IPv6AcceptRA_set("UseGateway",          value);return 0;}
int network_config_manager::set_RouteMetric_IPv6(std::string value)    {handle_IPv6AcceptRA_set("RouteMetric",         value);return 0;}
int network_config_manager::set_Token(std::string value)               {handle_IPv6AcceptRA_set("Token",               value);return 0;}

