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
#include <xyz/openbmc_project/Sensor/Threshold/Critical/server.hpp>
#include <xyz/openbmc_project/Sensor/Threshold/Warning/server.hpp>
#include <xyz/openbmc_project/ObjectMapper/server.hpp>

using ValueIface = sdbusplus::xyz::openbmc_project::Sensor::server::Value;
using AssociationIface = sdbusplus::xyz::openbmc_project::Association::server::Definitions;
using ObjectManagerIface = sdbusplus::server::manager_t;
using ThresholdIface = sdbusplus::xyz::openbmc_project::Sensor::Threshold::server::Critical;


class Chassis_Exposer {
    public: 
        Chassis_Exposer(std::shared_ptr<sdbusplus::asio::connection> conn_ref, sdbusplus::asio::object_server& server_ref);
        ~Chassis_Exposer();
        void expose_to_DBus();
        
    private:
        std::shared_ptr<sdbusplus::asio::connection> conn;
        sdbusplus::asio::object_server& obj_server;
        boost::asio::steady_timer timer;

        nlohmann::json chassis_fru_config;

        uint8_t BridgeDeviceAddress;
        uint8_t FRUDeviceAddress;
        uint8_t SDRDeviceAddress;
        uint8_t SELDeviceAddress;
        uint8_t SMDeviceAddress;
        uint8_t CapabilitiesFlags;

        bool ChassisFrontPanelLockoutEnabled;
        bool ChassisIntrusionEnabled;
        bool ChassisNMIEnabled;
        bool ChassisPowerInterlockEnabled;

        uint8_t RestorePolicy;

        std::unique_ptr<sdbusplus::asio::dbus_interface> value_iface;
        std::unique_ptr<sdbusplus::asio::dbus_interface> power_iface;
        std::unique_ptr<sdbusplus::asio::dbus_interface> restorepolicy_iface;

};