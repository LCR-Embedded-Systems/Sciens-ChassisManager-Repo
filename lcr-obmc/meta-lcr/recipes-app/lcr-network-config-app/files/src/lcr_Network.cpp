#include "../include/lcr_network_config.hpp"

void network_config_manager::display_Network_elements() 
{
    std::cout << "Elements of Network:" << std::endl;
    std::cout << "\tDHCP" << std::endl;
    std::cout << "\tLinkLocalAddressing" << std::endl;
    std::cout << "\tIPv6AcceptRA" << std::endl;
    std::cout << "\tAddress" << std::endl;
    std::cout << "\tGateway" << std::endl;
    std::cout << "\tDNS" << std::endl;
    std::cout << "\tNTP" << std::endl;
    std::cout << "\tDomains" << std::endl;
    std::cout << "\tIPv6SendRA" << std::endl;
    std::cout << "\tIPForward" << std::endl;
    std::cout << "\tIPMasquerade" << std::endl;
    std::cout << "\tVLAN" << std::endl;
    std::cout << "\tBridge" << std::endl;
    std::cout << "\tBond" << std::endl;
    std::cout << "\tLLDP" << std::endl;
    std::cout << "\tEmitLLDP" << std::endl;
}



int network_config_manager::set_Network_element(std::string element, std::string value)
{
    return NetworkMap[element](value);
}

//helper function for passing section and passthrough element/value to set_element util and error handling
void network_config_manager::handle_Network_set(std::string element, std::string value)
{
    if (set_element("Network", element, value) == -1)
    {
        display_Network_elements();
    }
}

/*
 * helper functions for passing element/value to set_element util
 */
int network_config_manager::set_DHCP(std::string value)                { handle_Network_set("DHCP",                value);return 0;}
int network_config_manager::set_LinkLocalAddressing(std::string value) { handle_Network_set("LinkLocalAddressing", value);return 0;}
int network_config_manager::set_IPv6AcceptRA(std::string value)        { handle_Network_set("IPv6AcceptRA",        value);return 0;}

int network_config_manager::set_Address_Network(std::string value)     
{ 
    bool prefixpresent=false;
    std::vector<std::string> result;
    std::stringstream ss(value);
    std::string token;
    while (std::getline(ss, token, '.')) {
        result.push_back(token);
    }
    for (const auto& str : result) {
        if (str.find("/") == std::string::npos) {
            try {
                std::cout << str << std::endl;
                [[maybe_unused]] int ph = std::stoi(str);
            }
            catch (const std::invalid_argument& e) {
                std::cerr << "Error: Invalid argument (not a number).\n";
                return -1;
            } 
        } else {
            std::stringstream sssub(str);
            while (std::getline(sssub, token, '/')) {
                try {
                    std::cout << token << std::endl;
                    [[maybe_unused]] int phss = std::stoi(token);
                    prefixpresent = true;
                }
                catch (const std::invalid_argument& e) {
                    std::cout << std::stoi(token) << std::endl;
                    std::cerr << "Error: Invalid argument (not a number).\n";
                    return -1;
                } 
            }
        }
    }
    if (prefixpresent) {
        handle_Network_set("Address",             value);
        return 0;
    }
    
    return -1;
}

int network_config_manager::set_Gateway_Network(std::string value)     { handle_Network_set("Gateway",      value);return 0;}
int network_config_manager::set_DNS(std::string value)                 { handle_Network_set("DNS",          value);return 0;}
int network_config_manager::set_NTP(std::string value)                 { handle_Network_set("NTP",          value);return 0;}
int network_config_manager::set_Domains(std::string value)             { handle_Network_set("Domains",      value);return 0;}
int network_config_manager::set_IPv6SendRA(std::string value)          { handle_Network_set("IPv6SendRA",   value);return 0;}
int network_config_manager::set_IPForward(std::string value)           { handle_Network_set("IPForward",    value);return 0;}
int network_config_manager::set_IPMasquerade(std::string value)        { handle_Network_set("IPMasquerade", value);return 0;}
int network_config_manager::set_VLAN(std::string value)                { handle_Network_set("VLAN",         value);return 0;}
int network_config_manager::set_Bridge(std::string value)              { handle_Network_set("Bridge",       value);return 0;}
int network_config_manager::set_Bond(std::string value)                { handle_Network_set("Bond",         value);return 0;}
int network_config_manager::set_LLDP(std::string value)                { handle_Network_set("LLDP",         value);return 0;}
int network_config_manager::set_EmitLLDP(std::string value)            { handle_Network_set("EmitLLDP",     value);return 0;}
