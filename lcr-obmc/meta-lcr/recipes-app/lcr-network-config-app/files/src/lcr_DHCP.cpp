#include "../include/lcr_network_config.hpp"

void network_config_manager::display_DHCP_elements() 
{
    std::cout << "Elements of DHCP:" << std::endl;
    std::cout << "\tClientIdentifier" << std::endl;
    std::cout << "\tUseDNS" << std::endl;
    std::cout << "\tUseDomains" << std::endl;
    std::cout << "\tUseNTP" << std::endl;
    std::cout << "\tUseMTU" << std::endl;
    std::cout << "\tUseHostname" << std::endl;
    std::cout << "\tSendHostname" << std::endl;
    std::cout << "\tUseRoutes" << std::endl;
    std::cout << "\tUseGateway" << std::endl;
    std::cout << "\tRouteMetric" << std::endl;
    std::cout << "\tAnonymize" << std::endl;
    std::cout << "\tRapidCommit" << std::endl;
    std::cout << "\tDUIDType" << std::endl;
    std::cout << "\tIAID" << std::endl;
    std::cout << "\tUseTimezone" << std::endl;
}

int network_config_manager::set_DHCP_element(std::string element, std::string value)
{
    return DHCPMap[element](value);
}

//helper function for passing section and passthrough element/value to set_element util and error handling
void network_config_manager::handle_DHCP_set(std::string element, std::string value)
{
    if (set_element("DHCP", element, value) == -1)
    {
        display_DHCP_elements();
    }
}

/*
 * helper functions for passing element/value to set_element util
 */

int network_config_manager::set_ClientIdentifier(std::string value) {handle_DHCP_set("ClientIdentifier", value);return 0;}
int network_config_manager::set_UseDNS_DHCP(std::string value)      {handle_DHCP_set("UseDNS",           value);return 0;}
int network_config_manager::set_UseDomains_DHCP(std::string value)  {handle_DHCP_set("UseDomains",       value);return 0;}
int network_config_manager::set_UseNTP(std::string value)           {handle_DHCP_set("UseNTP",           value);return 0;}
int network_config_manager::set_UseMTU(std::string value)           {handle_DHCP_set("UseMTU",           value);return 0;}
int network_config_manager::set_UseHostname(std::string value)      {handle_DHCP_set("UseHostname",      value);return 0;}
int network_config_manager::set_SendHostname(std::string value)     {handle_DHCP_set("SendHostname",     value);return 0;}
int network_config_manager::set_UseRoutes(std::string value)        {handle_DHCP_set("UseRoutes",        value);return 0;}
int network_config_manager::set_UseGateway_DHCP(std::string value)  {handle_DHCP_set("UseGateway",       value);return 0;}
int network_config_manager::set_RouteMetric_DHCP(std::string value) {handle_DHCP_set("RouteMetric",      value);return 0;}
int network_config_manager::set_Anonymize(std::string value)        {handle_DHCP_set("Anonymize",        value);return 0;}
int network_config_manager::set_RapidCommit(std::string value)      {handle_DHCP_set("RapidCommit",      value);return 0;}
int network_config_manager::set_DUIDType(std::string value)         {handle_DHCP_set("DUIDType",         value);return 0;}
int network_config_manager::set_IAID(std::string value)             {handle_DHCP_set("IAID",             value);return 0;}
int network_config_manager::set_UseTimezone(std::string value)      {handle_DHCP_set("UseTimezone",      value);return 0;}

