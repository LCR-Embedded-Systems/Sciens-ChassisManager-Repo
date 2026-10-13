#pragma once

#include "global.hpp"

#include <fstream>
#include <ostream>
#include <cstdint>
#include <string>

#include <systemd/sd-bus.h>
#include <sdbusplus/bus.hpp>
#include <sdbusplus/server/object.hpp>
#include <sdbusplus/message.hpp>
#include <sdbusplus/bus/match.hpp>
#include <xyz/openbmc_project/Sensor/Value/server.hpp>
#include <xyz/openbmc_project/Association/Definitions/server.hpp>
#include <xyz/openbmc_project/ObjectMapper/server.hpp>

class network_config_manager
{
    public:
    
        network_config_manager();
        ~network_config_manager();

        //Match elements
        int set_Name(std::string value);
        int set_Type_Match(std::string value);
        int set_MACAddress_Match(std::string value);
        int set_PermanentMACAddress(std::string value);
        int set_Path(std::string value);
        int set_Driver(std::string value);
        int set_Kind(std::string value);
        int set_Property(std::string value);
        int set_KernelCommandLine(std::string value);
        int set_Virtualization(std::string value);

        //Link elements
        int set_MACAddress_Link(std::string value);
        int set_Unmanaged(std::string value);
        int set_MTUBytes(std::string value);
        int set_ConfigureWithoutCarrier(std::string value);

        //Network elements
        int set_DHCP(std::string value);
        int set_LinkLocalAddressing(std::string value);
        int set_IPv6AcceptRA(std::string value);
        int set_Address_Network(std::string value);
        int set_Gateway_Network(std::string value);
        int set_DNS(std::string value);
        int set_NTP(std::string value);
        int set_Domains(std::string value);
        int set_IPv6SendRA(std::string value);
        int set_IPForward(std::string value);
        int set_IPMasquerade(std::string value);
        int set_VLAN(std::string value);
        int set_Bridge(std::string value);
        int set_Bond(std::string value);
        int set_LLDP(std::string value);
        int set_EmitLLDP(std::string value);

        //Address elements
        int set_Address_Address(std::string value);
        int set_Peer(std::string value);
        int set_Broadcast(std::string value);
        int set_Label(std::string value);
        int set_PreferredLifetime(std::string value);
        int set_Scope_Address(std::string value);
        int set_RouteMetric(std::string value);
        int set_HomeAddress(std::string value);
        int set_DuplicateAddressDetection(std::string value);
        int set_AddPrefixRoute(std::string value);

        //Route elements
        int set_Gateway_Route(std::string value);
        int set_Destination(std::string value);
        int set_Source(std::string value);
        int set_Metric(std::string value);
        int set_Table(std::string value);
        int set_PreferredSource(std::string value);
        int set_Scope_Route(std::string value);
        int set_Type_Route(std::string value);
        int set_GatewayOnLink(std::string value);

        //Neighbor elements
        int set_Address_Neighbor(std::string value);
        int set_LinkLayerAddress(std::string value);

        //DHCP elements
        int set_ClientIdentifier(std::string value);
        int set_UseDNS_DHCP(std::string value);
        int set_UseDomains_DHCP(std::string value);
        int set_UseNTP(std::string value);
        int set_UseMTU(std::string value);
        int set_UseHostname(std::string value);
        int set_SendHostname(std::string value);
        int set_UseRoutes(std::string value);
        int set_UseGateway_DHCP(std::string value);
        int set_RouteMetric_DHCP(std::string value);
        int set_Anonymize(std::string value);
        int set_RapidCommit(std::string value);
        int set_DUIDType(std::string value);
        int set_IAID(std::string value);
        int set_UseTimezone(std::string value);

        //IPv6AcceptRA elements
        int set_DHCPv6Client(std::string value);
        int set_UseDNS_IPv6(std::string value);
        int set_UseDomains_IPv6(std::string value);
        int set_UseAutonomousPrefix(std::string value);
        int set_UseOnLinkPrefix(std::string value);
        int set_UseGateway_IPv6(std::string value);
        int set_RouteMetric_IPv6(std::string value);
        int set_Token(std::string value);

        //General utils
        void move_network();
        void reloadNetworkConfig();
        void showNetworkConfig();
        int set_element(std::string section, std::string element, std::string value);
        void process_argument(int nargs, char* args[]);

        //display utils
        void display_options();
        void display_Match_elements();
        void display_Link_elements();
        void display_Network_elements();
        void display_Address_elements();
        void display_Route_elements();
        void display_Neighbor_elements();
        void display_DHCP_elements();
        void display_IPv6AcceptRA_elements();
        
        //element setter utils
        int set_Match_element(std::string element, std::string value);
        int set_Link_element(std::string element, std::string value);
        int set_Network_element(std::string element, std::string value);
        int set_Address_element(std::string element, std::string value);
        int set_Route_element(std::string element, std::string value);
        int set_Neighbor_element(std::string element, std::string value);
        int set_DHCP_element(std::string element, std::string value);
        int set_IPv6AcceptRA_element(std::string element, std::string value);

        //section setter helper utils
        void handle_Match_set(std::string element, std::string value);
        void handle_Link_set(std::string element, std::string value);
        void handle_Network_set(std::string element, std::string value);
        void handle_Address_set(std::string element, std::string value);
        void handle_Route_set(std::string element, std::string value);
        void handle_Neighbor_set(std::string element, std::string value);
        void handle_DHCP_set(std::string element, std::string value);
        void handle_IPv6AcceptRA_set(std::string element, std::string value);

    private:

        std::string temp_dest;
        fs::path source_file;
        fs::path destination;

        bool remove;

        std::unordered_map<std::string, std::function<int(std::string)>> MatchMap;
        std::unordered_map<std::string, std::function<int(std::string)>> LinkMap;
        std::unordered_map<std::string, std::function<int(std::string)>> NetworkMap;
        std::unordered_map<std::string, std::function<int(std::string)>> AddressMap;
        std::unordered_map<std::string, std::function<int(std::string)>> RouteMap;
        std::unordered_map<std::string, std::function<int(std::string)>> NeighborMap;
        std::unordered_map<std::string, std::function<int(std::string)>> DHCPMap;
        std::unordered_map<std::string, std::function<int(std::string)>> IPv6AcceptRAMap;

};
