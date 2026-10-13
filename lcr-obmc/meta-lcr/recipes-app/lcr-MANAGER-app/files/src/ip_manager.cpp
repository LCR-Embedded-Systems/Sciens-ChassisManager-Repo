#include "../include/global.hpp"
#include "../include/ip_manager.hpp"

namespace fs = std::filesystem;

ip_Manager::ip_Manager(std::shared_ptr<sdbusplus::asio::connection> conn_ref,
    sdbusplus::asio::object_server& server_ref)
    : conn(conn_ref), obj_server(server_ref), timer(conn->get_io_context())
{
    temp_dest_end0 = "/var/persistent/temp_end0.network";
    temp_dest_end1 = "/var/persistent/temp_end1.network";
    end0_source_file = "/lib/systemd/network/60-phosphor-networkd-end0.network";
    end1_source_file = "/lib/systemd/network/60-phosphor-networkd-end1.network";
    fs::path target_dir = "/var/persistent";
    end0_destination = target_dir / end0_source_file.filename();
    end1_destination = target_dir / end1_source_file.filename();
    retrieve_network_configuration();
    obj_server.add_manager("/xyz/openbmc_project/network_configuration");
    expose_ip();
    reloadNetworkConfig();
}

ip_Manager::~ip_Manager()
{
    ;
}

void ip_Manager::retrieve_network_configuration()
{
    if (!fs::exists(end0_destination)) {
        fs::copy_file(end0_source_file, end0_destination, fs::copy_options::overwrite_existing);    
    }
    if (!fs::exists(end1_destination)) {
        fs::copy_file(end1_source_file, end1_destination, fs::copy_options::overwrite_existing);    
    }
    
    fs::copy_file(end0_destination, end0_source_file, fs::copy_options::overwrite_existing);
    fs::copy_file(end1_destination, end1_source_file, fs::copy_options::overwrite_existing);

    std::ifstream end0_file(end0_source_file);
    std::ifstream end1_file(end1_source_file);

    std::string line;
    
    while (std::getline(end0_file, line)) {
        phosphor::logging::log<phosphor::logging::level::INFO>(("ip_Manager: retrieve_network_configuration end0 next line: " + line).c_str());
        if (line.find("Address") != std::string::npos) {
            size_t pos = line.find('=');
            if (pos != std::string::npos) {
                std::string address_prefix = line.substr(pos + 1);
                size_t slash_pos = address_prefix.find('/');
                if (slash_pos != std::string::npos) {
                    end0_ip_address = address_prefix.substr(0, slash_pos);
                    end0_prefix = address_prefix.substr(slash_pos + 1);
                    phosphor::logging::log<phosphor::logging::level::INFO>(("ip_Manager: retrieve_network_configuration end0 ip: " + end0_ip_address).c_str());
                    phosphor::logging::log<phosphor::logging::level::INFO>(("ip_Manager: retrieve_network_configuration end0 prefix: " + end0_prefix).c_str());
                }
            }
        }
        if (line.find("Gateway") != std::string::npos) {
            size_t pos = line.find('=');
            if (pos != std::string::npos) {
                end0_gateway = line.substr(pos + 1);
                phosphor::logging::log<phosphor::logging::level::INFO>(("ip_Manager: retrieve_network_configuration end0 gateway: " + end0_gateway).c_str());
            }
        }
        if (line.find("DHCP") != std::string::npos) {
            size_t pos = line.find('=');
            if (pos != std::string::npos) {
                end0_dhcp = !(line.substr(pos + 1) == "no");
            }
        }
    }
    end0_file.close();
    while (std::getline(end1_file, line)) {
        phosphor::logging::log<phosphor::logging::level::INFO>(("ip_Manager: retrieve_network_configuration end1 next line: " + line).c_str());
        if (line.find("Address") != std::string::npos) {
            size_t pos = line.find('=');
            if (pos != std::string::npos) {
                std::string address_prefix = line.substr(pos + 1);
                size_t slash_pos = address_prefix.find('/');
                if (slash_pos != std::string::npos) {
                    end1_ip_address = address_prefix.substr(0, slash_pos);
                    end1_prefix = address_prefix.substr(slash_pos + 1);
                    phosphor::logging::log<phosphor::logging::level::INFO>(("ip_Manager: retrieve_network_configuration end1 ip: " + end1_ip_address).c_str());
                    phosphor::logging::log<phosphor::logging::level::INFO>(("ip_Manager: retrieve_network_configuration end1 prefix: " + end1_prefix).c_str());
                }
            }
        }
        if (line.find("Gateway") != std::string::npos) {
            size_t pos = line.find('=');
            if (pos != std::string::npos) {
                end1_gateway = line.substr(pos + 1);
                phosphor::logging::log<phosphor::logging::level::INFO>(("ip_Manager: retrieve_network_configuration end1 gateway: " + end1_gateway).c_str());
            }
        }
        if (line.find("DHCP") != std::string::npos) {
            size_t pos = line.find('=');
            if (pos != std::string::npos) {
                end1_dhcp = !(line.substr(pos + 1) == "no");
            }
        }
    }
    end1_file.close();

}

void ip_Manager::expose_ip()
{
    phosphor::logging::log<phosphor::logging::level::INFO>("ip_Manager: expose_ip enter");
    std::string path_end0 = "/xyz/openbmc_project/network_configuration/end0";
    value_iface_end0 = obj_server.add_unique_interface(path_end0, "xyz.openbmc_project.network_configuration");
    std::string path_end1 = "/xyz/openbmc_project/network_configuration/end1";
    value_iface_end1 = obj_server.add_unique_interface(path_end1, "xyz.openbmc_project.network_configuration");

    phosphor::logging::log<phosphor::logging::level::INFO>("ip_Manager: expose_ip end0 registering Address");
    value_iface_end0->register_property_rw<std::string>(
        "Address",
        sdbusplus::vtable::property_::emits_change,
        [this](const std::string& requested_value, std::string& value) -> bool {
            if (requested_value == value) {
                return false;
            }
    
            if (check_IP_Address(requested_value)) {
                end0_ip_address = requested_value;
                move_IP_Address_Prefix_end0();
            } else {
                return false;
            }

            return true;
        },
        [this](const std::string&) -> std::string {
            return end0_ip_address;
        }
    );
    phosphor::logging::log<phosphor::logging::level::INFO>("ip_Manager: expose_ip end0 registering Prefix");
    value_iface_end0->register_property_rw<std::string>(
        "Prefix",
        sdbusplus::vtable::property_::emits_change,
        [this](const std::string& requested_value, std::string& value) -> bool {
            if (requested_value == value) {
                return false;
            }
    
            end0_prefix = requested_value;
            move_IP_Address_Prefix_end0();

            return true;
        },
        [this](const std::string&) -> std::string {
            return end0_prefix;
        }
    );
    phosphor::logging::log<phosphor::logging::level::INFO>("ip_Manager: expose_ip end0 registering Gateway");
    value_iface_end0->register_property_rw<std::string>(
        "Gateway",
        sdbusplus::vtable::property_::emits_change,
        [this](const std::string& requested_value, std::string& value) -> bool {
            if (requested_value == value) {
                return false;
            }
    
            end0_gateway = requested_value;
            move_gateway_end0();

            return true;
        },
        [this](const std::string&) -> std::string {
            return end0_gateway;
        }
    );
    phosphor::logging::log<phosphor::logging::level::INFO>("ip_Manager: expose_ip end0 registering DHCP");
    value_iface_end0->register_property_rw<bool>(
        "DHCP",
        sdbusplus::vtable::property_::emits_change,
        [this](const bool& requested_value, bool& value) -> bool {
            if (requested_value == value) {
                phosphor::logging::log<phosphor::logging::level::INFO>("ip_Manager: expose_ip DHCP value is the same");
            }
    
            end0_dhcp = requested_value;
            move_dhcp_end0();

            return true;
        },
        [this](const bool&) -> bool {
            return end0_dhcp;
        }
    );


    phosphor::logging::log<phosphor::logging::level::INFO>("ip_Manager: expose_ip end1 registering Address");
    value_iface_end1->register_property_rw<std::string>(
        "Address",
        sdbusplus::vtable::property_::emits_change,
        [this](const std::string& requested_value, std::string& value) -> bool {
            if (requested_value == value) {
                return false;
            }
    
            end1_ip_address = requested_value;
            move_IP_Address_Prefix_end1();

            return true;
        },
        [this](const std::string&) -> std::string {
            return end1_ip_address;
        }
    );
    phosphor::logging::log<phosphor::logging::level::INFO>("ip_Manager: expose_ip end1 registering Prefix");
    value_iface_end1->register_property_rw<std::string>(
        "Prefix",
        sdbusplus::vtable::property_::emits_change,
        [this](const std::string& requested_value, std::string& value) -> bool {
            if (requested_value == value) {
                return false;
            }
    
            end1_prefix = requested_value;
            move_IP_Address_Prefix_end1();

            return true;
        },
        [this](const std::string&) -> std::string {
            return end1_prefix;
        }
    );
    phosphor::logging::log<phosphor::logging::level::INFO>("ip_Manager: expose_ip end1 registering Gateway");
    value_iface_end1->register_property_rw<std::string>(
        "Gateway",
        sdbusplus::vtable::property_::emits_change,
        [this](const std::string& requested_value, std::string& value) -> bool {
            if (requested_value == value) {
                return false;
            }
    
            end1_gateway = requested_value;
            move_gateway_end1();

            return true;
        },
        [this](const std::string&) -> std::string {
            return end1_gateway;
        }
    );
    phosphor::logging::log<phosphor::logging::level::INFO>("ip_Manager: expose_ip end1 registering DHCP");
    value_iface_end1->register_property_rw<bool>(
        "DHCP",
        sdbusplus::vtable::property_::emits_change,
        [this](const bool& requested_value, bool& value) -> bool {
            if (requested_value == value) {
                phosphor::logging::log<phosphor::logging::level::INFO>("ip_Manager: expose_ip DHCP value is the same");
            }
    
            end1_dhcp = requested_value;
            move_dhcp_end1();

            return true;
        },
        [this](const bool&) -> bool {
            return end1_dhcp;
        }
    );

    phosphor::logging::log<phosphor::logging::level::INFO>("ip_Manager: expose_ip initializing interface");
    value_iface_end0->initialize();
    value_iface_end1->initialize();
    phosphor::logging::log<phosphor::logging::level::INFO>("ip_Manager: expose_ip exit");
}

void ip_Manager::reloadNetworkConfig()
{
    phosphor::logging::log<phosphor::logging::level::INFO>("ip_Manager: reloadNetworkConfig enter");
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
        phosphor::logging::log<phosphor::logging::level::INFO>("ip_Manager: reloadNetworkConfig failed");
    }
    phosphor::logging::log<phosphor::logging::level::INFO>("ip_Manager: reloadNetworkConfig exit");
}

bool ip_Manager::check_IP_Address(std::string new_address)
{
    //check that IP address is valid
    std::stringstream ss(new_address);
    std::string token;
    bool valid = true;
    while (std::getline(ss, token, '.')) {
        phosphor::logging::log<phosphor::logging::level::INFO>(("ip_Manager: check_IP_Address " + token).c_str());
        int ipcheck = std::stoi(token);
        if (ipcheck < 0 || ipcheck > 255) {
            valid = false;
        }
    }
    return valid;
}

void ip_Manager::move_end0_reload()
{
    phosphor::logging::log<phosphor::logging::level::INFO>("ip_Manager: move_end0_reload enter");
    if (std::rename(temp_dest_end0.c_str(), end0_destination.c_str()) != 0) {
        phosphor::logging::log<phosphor::logging::level::ERR>(
            ("ip_Manager: rename failed: " + std::string(strerror(errno))).c_str());
        return;
    }

    try {
        fs::path source = end0_destination;
        fs::path destination = end0_source_file;
        fs::copy_file(source, destination, fs::copy_options::overwrite_existing);
    }
    catch (const fs::filesystem_error& e) {
        phosphor::logging::log<phosphor::logging::level::ERR>(
            ("ip_Manager: copy_file failed: " + std::string(e.what())).c_str());
    }

    phosphor::logging::log<phosphor::logging::level::INFO>("ip_Manager: move_end0_reload reloadNetworkConfig");
    reloadNetworkConfig();
}

void ip_Manager::move_end1_reload()
{
    if (std::rename(temp_dest_end1.c_str(), end1_destination.c_str()) != 0) {
        phosphor::logging::log<phosphor::logging::level::ERR>(
            ("ip_Manager: rename failed: " + std::string(strerror(errno))).c_str());
        return;
    }

    try {
        fs::path source = end1_destination;
        fs::path destination = end1_source_file;
        fs::copy_file(source, destination, fs::copy_options::overwrite_existing);
    }
    catch (const fs::filesystem_error& e) {
        phosphor::logging::log<phosphor::logging::level::ERR>(
            ("ip_Manager: copy_file failed: " + std::string(e.what())).c_str());
    }

    reloadNetworkConfig();
}

void ip_Manager::move_IP_Address_Prefix_end0()
{ 
    std::ifstream end0_in(end0_destination);
    std::ofstream end0_out(temp_dest_end0);

    std::string line;
    while (std::getline(end0_in, line)) {
        phosphor::logging::log<phosphor::logging::level::INFO>(("ip_Manager: move_IP_Address_end0 end0 next line: " + line).c_str());
        if (line.find("Address") != std::string::npos) {
            size_t pos = line.find('=');
            if (pos != std::string::npos) {
                std::string address_prefix = line.substr(pos + 1);
                size_t slash_pos = address_prefix.find('/');
                if (slash_pos != std::string::npos) {
                    end0_out << "Address=" << end0_ip_address << "/" << end0_prefix << "\n";
                }
            }
        } else {
            end0_out << line << "\n";
        }
    }
    end0_in.close();
    end0_out.close();

    move_end0_reload();
}

void ip_Manager::move_IP_Address_Prefix_end1()
{

    std::ifstream end1_in(end1_destination);
    std::ofstream end1_out(temp_dest_end1);

    std::string line;
    while (std::getline(end1_in, line)) {
        phosphor::logging::log<phosphor::logging::level::INFO>(("ip_Manager: move_IP_Address_end1 end1 next line: " + line).c_str());
        if (line.find("Address") != std::string::npos) {
            size_t pos = line.find('=');
            if (pos != std::string::npos) {
                std::string address_prefix = line.substr(pos + 1);
                size_t slash_pos = address_prefix.find('/');
                if (slash_pos != std::string::npos) {
                    end1_out << "Address=" << end1_ip_address << "/" << end1_prefix << "\n";
                }
            }
        } else {
            end1_out << line << "\n";
        }
    }
    end1_in.close();
    end1_out.close();
    
    move_end1_reload();
}

void ip_Manager::move_gateway_end0()
{
    std::ifstream end0_in(end0_destination);
    std::ofstream end0_out(temp_dest_end0);

    std::string line;
    while (std::getline(end0_in, line)) {
        phosphor::logging::log<phosphor::logging::level::INFO>(("ip_Manager: move_gateway_end0 end0 next line: " + line).c_str());
        if (line.find("Gateway") != std::string::npos) {
            size_t pos = line.find('=');
            if (pos != std::string::npos) {
                end0_out << "Gateway=" << end0_gateway << "\n";
            }
        } else {
            end0_out << line << "\n";
        }
    }
    end0_in.close();
    end0_out.close();
    move_end0_reload();
}

void ip_Manager::move_gateway_end1()
{
    std::ifstream end1_in(end1_destination);
    std::ofstream end1_out(temp_dest_end0);

    std::string line;
    while (std::getline(end1_in, line)) {
        phosphor::logging::log<phosphor::logging::level::INFO>(("ip_Manager: move_gateway_end1 end1 next line: " + line).c_str());
        if (line.find("Gateway") != std::string::npos) {
            size_t pos = line.find('=');
            if (pos != std::string::npos) {
                end1_out << "Gateway=" << end1_gateway << "\n";
            }
        } else {
            end1_out << line << "\n";
        }
    }
    end1_in.close();
    end1_out.close();
    move_end1_reload();
}

void ip_Manager::move_dhcp_end0()
{
    std::ifstream end0_in(end0_destination);
    std::ofstream end0_out(temp_dest_end0);

    std::string line;
    while (std::getline(end0_in, line)) {
        phosphor::logging::log<phosphor::logging::level::INFO>(("ip_Manager: move_dhcp_end0 end0 next line: " + line).c_str());
        if (line.find("DHCP") != std::string::npos) {
            size_t pos = line.find('=');
            if (pos != std::string::npos) {
                end0_out << "DHCP=" << ((end0_dhcp) ? "yes" : "no") << "\n";
            }
        } else {
            end0_out << line << "\n";
        }
    }
    end0_in.close();
    end0_out.close();
    phosphor::logging::log<phosphor::logging::level::INFO>("ip_Manager: move_dhcp_end0 move end0 reload");
    move_end0_reload();
}

void ip_Manager::move_dhcp_end1()
{
    std::ifstream end1_in(end1_destination);
    std::ofstream end1_out(temp_dest_end0);

    std::string line;
    while (std::getline(end1_in, line)) {
        phosphor::logging::log<phosphor::logging::level::INFO>(("ip_Manager: move_dhcp_end1 end1 next line: " + line).c_str());
        if (line.find("DHCP") != std::string::npos) {
            size_t pos = line.find('=');
            if (pos != std::string::npos) {
                end1_out << "DHCP=" << ((end1_dhcp) ? "yes" : "no") << "\n";
            }
        } else {
            end1_out << line << "\n";
        }
    }
    end1_in.close();
    end1_out.close();
    move_end1_reload();
}


