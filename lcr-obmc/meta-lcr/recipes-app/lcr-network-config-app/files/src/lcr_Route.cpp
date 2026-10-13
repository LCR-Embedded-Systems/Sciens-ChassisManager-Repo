#include "../include/lcr_network_config.hpp"

void network_config_manager::display_Route_elements() 
{
    std::cout << "Elements of Route:" << std::endl;
    std::cout << "\tGateway" << std::endl;
    std::cout << "\tDestination" << std::endl;
    std::cout << "\tSource" << std::endl;
    std::cout << "\tMetric" << std::endl;
    std::cout << "\tTable" << std::endl;
    std::cout << "\tPreferredSource" << std::endl;
    std::cout << "\tScope" << std::endl;
    std::cout << "\tType" << std::endl;
    std::cout << "\tGatewayOnLink" << std::endl;
}

int network_config_manager::set_Route_element(std::string element, std::string value)
{
    return RouteMap[element](value);
}

//helper function for passing section and passthrough element/value to set_element util and error handling
void network_config_manager::handle_Route_set(std::string element, std::string value)
{
    if (set_element("Route", element, value) == -1)
    {
        display_Route_elements();
    }
}

int network_config_manager::set_Gateway_Route(std::string value)   { handle_Route_set("Gateway",         value); return 0;}
int network_config_manager::set_Destination(std::string value)     { handle_Route_set("Destination",     value); return 0;}
int network_config_manager::set_Source(std::string value)          { handle_Route_set("Source",          value); return 0;}
int network_config_manager::set_Metric(std::string value)          { handle_Route_set("Metric",          value); return 0;}
int network_config_manager::set_Table(std::string value)           { handle_Route_set("Table",           value); return 0;}
int network_config_manager::set_PreferredSource(std::string value) { handle_Route_set("PreferredSource", value); return 0;}
int network_config_manager::set_Scope_Route(std::string value)     { handle_Route_set("Scope",           value); return 0;}
int network_config_manager::set_Type_Route(std::string value)      { handle_Route_set("Type",            value); return 0;}
int network_config_manager::set_GatewayOnLink(std::string value)   { handle_Route_set("GatewayOnLink",   value); return 0;}


