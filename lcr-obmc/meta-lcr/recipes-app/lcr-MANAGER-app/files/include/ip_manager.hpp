
#include "../include/global.hpp"

class ip_Manager
{

    public:

        ip_Manager(std::shared_ptr<sdbusplus::asio::connection> conn_ref, sdbusplus::asio::object_server& server_ref);
        ~ip_Manager();
        void reloadNetworkConfig();
        void move_IP_Address_Prefix_end0();
        void move_IP_Address_Prefix_end1();
        bool check_IP_Address(std::string new_address);
        void move_gateway_end0();
        void move_dhcp_end0();
        void move_gateway_end1();
        void move_dhcp_end1();
        void expose_ip();
        void retrieve_network_configuration();
        void retrieve_from_file(std::ifstream networkfile);

        void move_end0_reload();
        void move_end1_reload();

    private:

        std::shared_ptr<sdbusplus::asio::connection> conn;
        sdbusplus::asio::object_server& obj_server;
        boost::asio::steady_timer timer;

        std::unique_ptr<sdbusplus::asio::dbus_interface> value_iface_end0;
        std::unique_ptr<sdbusplus::asio::dbus_interface> value_iface_end1;

        std::string temp_dest_end0;
        std::string temp_dest_end1;
        fs::path end0_source_file;
        fs::path end1_source_file;
        fs::path end0_destination;
        fs::path end1_destination;

        std::string end0_ip_address;
        std::string end0_prefix;
        std::string end0_gateway;
        bool end0_dhcp;
        std::string end1_ip_address;
        std::string end1_prefix;
        std::string end1_gateway;
        bool end1_dhcp;

};
