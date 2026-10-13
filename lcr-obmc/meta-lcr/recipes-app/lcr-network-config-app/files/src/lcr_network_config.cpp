#include "../include/lcr_network_config.hpp"

network_config_manager::network_config_manager()
{
    MatchMap["name"]                        = [this](std::string value) { return set_Name(std::move(value)); };
    MatchMap["type"]                        = [this](std::string value) { return set_Type_Match(std::move(value)); };
    MatchMap["macaddress"]                  = [this](std::string value) { return set_MACAddress_Match(std::move(value)); };
    MatchMap["permanentmacaddress"]         = [this](std::string value) { return set_PermanentMACAddress(std::move(value)); };
    MatchMap["path"]                        = [this](std::string value) { return set_Path(std::move(value)); };
    MatchMap["driver"]                      = [this](std::string value) { return set_Driver(std::move(value)); };
    MatchMap["kind"]                        = [this](std::string value) { return set_Kind(std::move(value)); };
    MatchMap["property"]                    = [this](std::string value) { return set_Property(std::move(value)); };
    MatchMap["kernelcommandline"]           = [this](std::string value) { return set_KernelCommandLine(std::move(value)); };
    MatchMap["virtualization"]              = [this](std::string value) { return set_Virtualization(std::move(value)); };

    LinkMap["macaddress"]                   = [this](std::string value) { return set_MACAddress_Link(std::move(value)); };
    LinkMap["unmanaged"]                    = [this](std::string value) { return set_Unmanaged(std::move(value)); };
    LinkMap["mtubytes"]                     = [this](std::string value) { return set_MTUBytes(std::move(value)); };
    LinkMap["configurewithoutcarrier"]      = [this](std::string value) { return set_ConfigureWithoutCarrier(std::move(value)); };

    NetworkMap["dhcp"]                      = [this](std::string value) { return set_DHCP(std::move(value)); };
    NetworkMap["linklocaladdressing"]       = [this](std::string value) { return set_LinkLocalAddressing(std::move(value)); };
    NetworkMap["ipv6acceptra"]              = [this](std::string value) { return set_IPv6AcceptRA(std::move(value)); };
    NetworkMap["address"]                   = [this](std::string value) { return set_Address_Network(std::move(value)); };
    NetworkMap["gateway"]                   = [this](std::string value) { return set_Gateway_Network(std::move(value)); };
    NetworkMap["dns"]                       = [this](std::string value) { return set_DNS(std::move(value)); };
    NetworkMap["ntp"]                       = [this](std::string value) { return set_NTP(std::move(value)); };
    NetworkMap["domains"]                   = [this](std::string value) { return set_Domains(std::move(value)); };
    NetworkMap["ipv6sendra"]                = [this](std::string value) { return set_IPv6SendRA(std::move(value)); };
    NetworkMap["ipforward"]                 = [this](std::string value) { return set_IPForward(std::move(value)); };
    NetworkMap["ipmasquerade"]              = [this](std::string value) { return set_IPMasquerade(std::move(value)); };
    NetworkMap["vlan"]                      = [this](std::string value) { return set_VLAN(std::move(value)); };
    NetworkMap["bridge"]                    = [this](std::string value) { return set_Bridge(std::move(value)); };
    NetworkMap["bond"]                      = [this](std::string value) { return set_Bond(std::move(value)); };
    NetworkMap["lldp"]                      = [this](std::string value) { return set_LLDP(std::move(value)); };
    NetworkMap["emitlldp"]                  = [this](std::string value) { return set_EmitLLDP(std::move(value)); };

    NeighborMap["address"]                  = [this](std::string value) { return set_Address_Neighbor(std::move(value)); };
    NeighborMap["linklayeraddress"]         = [this](std::string value) { return set_LinkLayerAddress(std::move(value)); };

    AddressMap["address"]                   = [this](std::string value) { return set_Address_Address(std::move(value)); };
    AddressMap["peer"]                      = [this](std::string value) { return set_Peer(std::move(value)); };
    AddressMap["broadcast"]                 = [this](std::string value) { return set_Broadcast(std::move(value)); };
    AddressMap["label"]                     = [this](std::string value) { return set_Label(std::move(value)); };
    AddressMap["preferredlifetime"]         = [this](std::string value) { return set_PreferredLifetime(std::move(value)); };
    AddressMap["scope"]                     = [this](std::string value) { return set_Scope_Address(std::move(value)); };
    AddressMap["routemetric"]               = [this](std::string value) { return set_RouteMetric(std::move(value)); };
    AddressMap["homeaddress"]               = [this](std::string value) { return set_HomeAddress(std::move(value)); };
    AddressMap["duplicateaddressdetection"] = [this](std::string value) { return set_DuplicateAddressDetection(std::move(value)); };
    AddressMap["addprefixroute"]            = [this](std::string value) { return set_AddPrefixRoute(std::move(value)); };

    DHCPMap["clientidentifier"]             = [this](std::string value) { return set_ClientIdentifier(std::move(value)); };
    DHCPMap["usedns"]                       = [this](std::string value) { return set_UseDNS_DHCP(std::move(value)); };
    DHCPMap["usedomains"]                   = [this](std::string value) { return set_UseDomains_DHCP(std::move(value)); };
    DHCPMap["usentp"]                       = [this](std::string value) { return set_UseNTP(std::move(value)); };
    DHCPMap["usemtu"]                       = [this](std::string value) { return set_UseMTU(std::move(value)); };
    DHCPMap["usehostname"]                  = [this](std::string value) { return set_UseHostname(std::move(value)); };
    DHCPMap["sendhostname"]                 = [this](std::string value) { return set_SendHostname(std::move(value)); };
    DHCPMap["useroutes"]                    = [this](std::string value) { return set_UseRoutes(std::move(value)); };
    DHCPMap["usegateway"]                   = [this](std::string value) { return set_UseGateway_DHCP(std::move(value)); };
    DHCPMap["routemetric"]                  = [this](std::string value) { return set_RouteMetric_DHCP(std::move(value)); };
    DHCPMap["anonymize"]                    = [this](std::string value) { return set_Anonymize(std::move(value)); };
    DHCPMap["rapidcommit"]                  = [this](std::string value) { return set_RapidCommit(std::move(value)); };
    DHCPMap["duidtype"]                     = [this](std::string value) { return set_DUIDType(std::move(value)); };
    DHCPMap["iaid"]                         = [this](std::string value) { return set_IAID(std::move(value)); };
    DHCPMap["usetimezone"]                  = [this](std::string value) { return set_UseTimezone(std::move(value)); };

    RouteMap["gateway"]                     = [this](std::string value) { return set_Gateway_Route(std::move(value)); };
    RouteMap["destination"]                 = [this](std::string value) { return set_Destination(std::move(value)); };
    RouteMap["source"]                      = [this](std::string value) { return set_Source(std::move(value)); };
    RouteMap["metric"]                      = [this](std::string value) { return set_Metric(std::move(value)); };
    RouteMap["table"]                       = [this](std::string value) { return set_Table(std::move(value)); };
    RouteMap["preferredsource"]             = [this](std::string value) { return set_PreferredSource(std::move(value)); };
    RouteMap["scope"]                       = [this](std::string value) { return set_Scope_Route(std::move(value)); };
    RouteMap["type"]                        = [this](std::string value) { return set_Type_Route(std::move(value)); };
    RouteMap["gatewayonlink"]               = [this](std::string value) { return set_GatewayOnLink(std::move(value)); };

    IPv6AcceptRAMap["dhcpv6client"]         = [this](std::string value) { return set_DHCPv6Client(std::move(value)); };
    IPv6AcceptRAMap["usedns"]               = [this](std::string value) { return set_UseDNS_IPv6(std::move(value)); };
    IPv6AcceptRAMap["usedomains"]           = [this](std::string value) { return set_UseDomains_IPv6(std::move(value)); };
    IPv6AcceptRAMap["useautonomousprefix"]  = [this](std::string value) { return set_UseAutonomousPrefix(std::move(value)); };
    IPv6AcceptRAMap["useonlinkprefix"]      = [this](std::string value) { return set_UseOnLinkPrefix(std::move(value)); };
    IPv6AcceptRAMap["usegateway"]           = [this](std::string value) { return set_UseGateway_IPv6(std::move(value)); };
    IPv6AcceptRAMap["routemetric"]          = [this](std::string value) { return set_RouteMetric_IPv6(std::move(value)); };
    IPv6AcceptRAMap["token"]                = [this](std::string value) { return set_Token(std::move(value)); };

}

network_config_manager::~network_config_manager()
{
    
}

std::string lowercase(std::string astring)
{
    std::transform(astring.begin(), astring.end(), astring.begin(), [](unsigned char c) {
        return std::tolower(c);
    });
    return astring;
}

void network_config_manager::process_argument(int nargs, char* args[]) 
{
    if (nargs >= 2) {
        int ret;
        std::string intfarg = lowercase(std::string(args[1]));

        if (intfarg != "end0" && intfarg != "end1") {
            std::cout << "Interface name does not exist" << std::endl;
            display_options();
            return;
        }

        source_file = "/lib/systemd/network/60-phosphor-networkd-" + intfarg + ".network";
        destination = "/var/persistent/60-phosphor-networkd-" + intfarg + ".network";
        temp_dest = "/var/persistent/temp_" + intfarg + ".network";

        std::string sectionarg = lowercase(std::string(args[2]));
        std::string elementarg = "N/A";
        std::string valuearg = "N/A";
        if (nargs > 2) {
            elementarg = lowercase(std::string(args[3]));
        }
        remove = false;
        if (nargs > 3) {
            valuearg = "";
            for (int i = 4; i < nargs + 1; i++) {
                valuearg += std::string(args[i]) + " ";
            }
            remove = (valuearg == std::string("remove"));
            std::cout << "In section: " << sectionarg << ", setting Element: " << elementarg << ", to New Value: " << valuearg << std::endl;
        }
        if (sectionarg == "match") {
            if (elementarg == "list" || elementarg == "N/A") {
                display_Match_elements();
            } else {
                ret = set_Match_element(elementarg, valuearg);
            }
        } else if (sectionarg == "link") {
            if (elementarg == "list" || elementarg == "N/A") {
                display_Link_elements();
            } else {
                ret = set_Link_element(elementarg, valuearg);
            }
        } else if (sectionarg == "network") {
            if (elementarg == "list" || elementarg == "N/A") {
                display_Network_elements();
            } else {
                ret = set_Network_element(elementarg, valuearg);
            }
        } else if (sectionarg == "address") {
            if (elementarg == "list" || elementarg == "N/A") {
                display_Address_elements();
            } else {
                ret = set_Address_element(elementarg, valuearg);
            }
        } else if (sectionarg == "route") {
            if (elementarg == "list" || elementarg == "N/A") {
                display_Route_elements();
            } else {
                ret = set_Route_element(elementarg, valuearg);
            }
        } else if (sectionarg == "neighbor") {
            if (elementarg == "list" || elementarg == "N/A") {
                display_Neighbor_elements();
            } else {
                ret = set_Neighbor_element(elementarg, valuearg);
            }
        } else if (sectionarg == "dhcp") {
            if (elementarg == "list" || elementarg == "N/A") {
                display_DHCP_elements();
            } else {
                ret = set_DHCP_element(elementarg, valuearg);
            }
        } else if (sectionarg == "ipv6acceptra") {
            if (elementarg == "list" || elementarg == "N/A") {
                display_IPv6AcceptRA_elements();
            } else {
                ret = set_IPv6AcceptRA_element(elementarg, valuearg);
            }
        } else if (sectionarg == "show") {
            showNetworkConfig();
            return;
        } else {
            std::cout << "unrecognized Section/command" << std::endl;
        }
        if (ret == 0) {
            //move temp file to .network location for phosphor-networkd
            move_network();
            //reload phosphor-networkd to take new values
            reloadNetworkConfig();
        } else {
            std::cout << "Failed to set value. Potentially due to invalid [NEW VALUE]" << std::endl;
        }
        
    } else if (lowercase(std::string(args[1])) == "help") {
        display_options();
    } else {
        std::cout << "Not enough arguments" << std::endl;
        display_options();
        std::cout << "\033[3m Use [lcr_network_config help] to display options again \033[23m" << std::endl;
    }
}

void network_config_manager::display_options()
{
    std::cout << "Usage:" << std::endl;
    std::cout << "\tlcr_network_config [INTF] [SECTION] [ELEMENT] [NEW VALUE]" << std::endl;
    std::cout << "\tINTF: Interfaces" << std::endl;
    std::cout << "\t\tend0" << std::endl;
    std::cout << "\t\tend1" << std::endl;
    std::cout << "\tSECTION: Section to be modified" << std::endl;
    std::cout << "\t\tMatch" << std::endl;
    std::cout << "\t\tLink" << std::endl;
    std::cout << "\t\tNetwork" << std::endl;
    std::cout << "\t\tAddress" << std::endl;
    std::cout << "\t\tRoute" << std::endl;
    std::cout << "\t\tNeighbor" << std::endl;
    std::cout << "\t\tDHCP" << std::endl;
    std::cout << "\t\tIPv6AcceptRA" << std::endl;
    std::cout << "\t\tShow (No further arguments, shows the current network settings of the selected interface)" << std::endl;
    std::cout << "\tELEMENT: Element of the section to be modified" << std::endl;
    std::cout << "\t\tSelect section to see applicable elements" << std::endl;
    std::cout << "\tNEW VALUE: New value that [ELEMENT] will be set to." << std::endl;
    std::cout << "\t\t(New values are currently not checked. Incompatible values of elements will cause the Phosphor Network Manager to fail)" << std::endl;
    std::cout << "\tTo remove an element, the [NEW VALUE] argument is to be set to \"remove\"" << std::endl;
    std::cout << std::endl;
}

void network_config_manager::showNetworkConfig()
{
    std::string line;
    std::ifstream end_file(source_file);
    while (std::getline(end_file, line)) {
        std::cout << line << std::endl;
    }
}

void network_config_manager::move_network()
{
    if (std::rename(temp_dest.c_str(), destination.c_str()) != 0) {
        std::cout << "rename failed: " << strerror(errno) << std::endl;
        return;
    }

    try {
        fs::copy_file(destination, source_file, fs::copy_options::overwrite_existing);
    }
    catch (const fs::filesystem_error& e) {
        std::cout << "copy_file failed: " << e.what() << std::endl;
    }
}

int network_config_manager::set_element(std::string section, std::string element, std::string value)
{
    std::string section_parsed = "[" + section + "]";
    std::string line;
    std::string cur_section;
    std::ifstream end_file(source_file);
    std::ofstream temp_file(temp_dest);
    bool section_present = false;
    bool element_present = false;
    if (value == "N/A") {
        std::cout << "No value provided, exiting." << std::endl;
        return -1;
    }
    while (std::getline(end_file, line)) {
        if (line[0] == '[') {
            cur_section = line;
        }
        if (line == section_parsed) {
            section_present = true;
        }
        size_t pos = line.find('=');
        std::string element_cur;
        if (pos != std::string::npos) {
            element_cur = line.substr(0,element.size());
            if (element == element_cur && cur_section == section_parsed) {
                element_present = true;
                if (!remove) {
                    std::cout << "Section and Element both exist, modifying existing value." << std::endl;
                    temp_file << element_cur << "=" << value << "\n";
                } else {
                    std::cout << "Removing element." << std::endl;
                }
            } else {
                temp_file << line << "\n";    
            }
        } else {
            temp_file << line << "\n";
        }
    }
    end_file.close();

    if (!section_present) {
        std::cout << "Section not present, adding section and element." << std::endl;
        temp_file << "\n";
        temp_file << section_parsed << "\n";
        temp_file << element << "=" << value << "\n";
        temp_file.close();
    } else if (section_present && !element_present) {
        std::cout << "Section present, but element is not. Adding element to section." << std::endl;
        bool append = true;
        std::ifstream end_file(source_file);
        temp_file.close();
        std::ofstream temp_file(temp_dest, std::ios::trunc);
        while (std::getline(end_file, line)) {
            if (line[0] == '[') {
                cur_section = line;
            }
            if (cur_section == section_parsed && append) {
                temp_file << line << "\n";
                temp_file << element << "=" << value << "\n";
                append = false;
            } else {
                temp_file << line << "\n";
            }
        }
        temp_file.close();
    }

    if (temp_file.is_open()) {temp_file.close();}

    return 0;
}

void network_config_manager::reloadNetworkConfig()
{
    try
    {
        auto bus = sdbusplus::bus::new_default();
        auto method = bus.new_method_call(
            "org.freedesktop.network1",          // service
            "/org/freedesktop/network1",         // object path
            "org.freedesktop.network1.Manager",  // interface
            "Reload");                           // method (no args)

        bus.call(method);
    }
    catch (const sdbusplus::exception::SdBusError& e)
    {
        std::cout << "Error: " << e.what() << std::endl;
    }
}


