#include "../include/chassis_control.hpp"

namespace fs = std::filesystem;

nlohmann::json readJson(std::string path) {
    std::ifstream f(path);

    nlohmann::json read;

    try {
        f >> read;
    } catch (const nlohmann::json::parse_error& e) {
        std::cerr << "JSON parse error: " << e.what() << std::endl;
    }

    return read;
}

Chassis_Exposer::Chassis_Exposer(std::shared_ptr<sdbusplus::asio::connection> conn_ref,
    sdbusplus::asio::object_server& server_ref)
    : conn(conn_ref), obj_server(server_ref), timer(conn->get_io_context())
{

    obj_server.add_manager("/xyz/openbmc_project/control/");
    chassis_fru_config = readJson("/usr/share/chassis_FRU_dir/chassis_FRU_information.json");

    BridgeDeviceAddress = static_cast<uint8_t>(chassis_fru_config["ChassisCapabilities"]["BridgeDeviceAddress"]);
    FRUDeviceAddress    = static_cast<uint8_t>(chassis_fru_config["ChassisCapabilities"]["FRUDeviceAddress"]);
    SDRDeviceAddress    = static_cast<uint8_t>(chassis_fru_config["ChassisCapabilities"]["SDRDeviceAddress"]);
    SELDeviceAddress    = static_cast<uint8_t>(chassis_fru_config["ChassisCapabilities"]["SELDeviceAddress"]);
    SMDeviceAddress     = static_cast<uint8_t>(chassis_fru_config["ChassisCapabilities"]["SMDeviceAddress"]);
    CapabilitiesFlags   = static_cast<uint8_t>(chassis_fru_config["ChassisCapabilities"]["CapabilitiesFlags"]);

    ChassisFrontPanelLockoutEnabled = chassis_fru_config["ChassisCapabilities"]["ChassisFrontPanelLockoutEnabled"];
    ChassisIntrusionEnabled         = chassis_fru_config["ChassisCapabilities"]["ChassisIntrusionEnabled"];
    ChassisNMIEnabled               = chassis_fru_config["ChassisCapabilities"]["ChassisNMIEnabled"];
    ChassisPowerInterlockEnabled    = chassis_fru_config["ChassisCapabilities"]["ChassisPowerInterlockEnabled"];
    
    RestorePolicy                   = chassis_fru_config["ChassisCapabilities"]["powerRestorePolicy"];
}

Chassis_Exposer::~Chassis_Exposer() 
{
    
}

void Chassis_Exposer::expose_to_DBus() 
{

    // Path for the sensor
    std::string path_caps = "/xyz/openbmc_project/control/chassis0/chassiscapabilities";
    value_iface = obj_server.add_unique_interface(path_caps, "xyz.openbmc_project.Control.ChassisCapabilities");
    std::string path_power = "/xyz/openbmc_project/Chassis/Control/Power0";
    power_iface = obj_server.add_unique_interface(path_power, "xyz.openbmc_project.Chassis.Control.Power");
    std::string path_restorepolicy = "/xyz/openbmc_project/control/host0/power_restore_policy";
    restorepolicy_iface = obj_server.add_unique_interface(path_power, "xyz.openbmc_project.Chassis.Control.Power.RestorePolicy");

    phosphor::logging::log<phosphor::logging::level::INFO>(("attempting to expose " + path_caps).c_str());

    value_iface->register_property_r<uint8_t>(
        "BridgeDeviceAddress",
        sdbusplus::vtable::property_::emits_change,
        [this](const uint8_t&) -> uint8_t { return BridgeDeviceAddress; }
    );
    value_iface->register_property_r<uint8_t>(
        "FRUDeviceAddress",
        sdbusplus::vtable::property_::emits_change,
        [this](const uint8_t&) -> uint8_t { return FRUDeviceAddress; }
    );
    value_iface->register_property_r<uint8_t>(
        "SDRDeviceAddress",
        sdbusplus::vtable::property_::emits_change,
        [this](const uint8_t&) -> uint8_t { return SDRDeviceAddress; }
    );
    value_iface->register_property_r<uint8_t>(
        "SELDeviceAddress",
        sdbusplus::vtable::property_::emits_change,
        [this](const uint8_t&) -> uint8_t { return SELDeviceAddress; }
    );
    value_iface->register_property_r<uint8_t>(
        "SMDeviceAddress",
        sdbusplus::vtable::property_::emits_change,
        [this](const uint8_t&) -> uint8_t { return SMDeviceAddress; }
    );
    value_iface->register_property_r<uint8_t>(
        "CapabilitiesFlags",
        sdbusplus::vtable::property_::emits_change,
        [this](const uint8_t&) -> uint8_t { return CapabilitiesFlags; }
    );
    value_iface->register_property_r<bool>(
        "ChassisFrontPanelLockoutEnabled",
        sdbusplus::vtable::property_::emits_change,
        [this](const bool&) -> bool { return ChassisFrontPanelLockoutEnabled; }
    );
    value_iface->register_property_r<bool>(
        "ChassisIntrusionEnabled",
        sdbusplus::vtable::property_::emits_change,
        [this](const bool&) -> bool { return ChassisIntrusionEnabled; }
    );
    value_iface->register_property_r<bool>(
        "ChassisNMIEnabled",
        sdbusplus::vtable::property_::emits_change,
        [this](const bool&) -> bool { return ChassisNMIEnabled; }
    );
    value_iface->register_property_r<bool>(
        "ChassisPowerInterlockEnabled",
        sdbusplus::vtable::property_::emits_change,
        [this](const bool&) -> bool { return ChassisPowerInterlockEnabled; }
    );

    //PFail needs to be further developed. Rule 5.1.4-1
    power_iface->register_property_r<bool>(
        "PFail",
        sdbusplus::vtable::property_::emits_change,
        [this](const bool&) -> bool { return false; }
    );

    restorepolicy_iface->register_property_r<uint8_t>(
        "PowerRestorePolicy",
        sdbusplus::vtable::property_::emits_change,
        [this](const uint8_t&) -> uint8_t { return RestorePolicy; }
    );

    value_iface->initialize();
    power_iface->initialize();
    restorepolicy_iface->initialize();
    phosphor::logging::log<phosphor::logging::level::INFO>("expose_reading exit");
}
