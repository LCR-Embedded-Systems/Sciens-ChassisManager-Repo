#include "global.hpp"
#include "pid.hpp"

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

using temp_name = std::string;

struct temperature_sensor {
    std::string path;
    std::string service;
    temp_name name;
    double value;
    double crit_high;
    double crit_low;
};

struct fan {
    std::string path;
    std::string service;
    std::string name;
    uint64_t pwm;
    int pwm_bitmax;
    int tach;
};

//fan loop controller definition
//TODO: create handling for both linear and PID controller schemas

class fan_loop
{

    public:

        fan_loop(std::shared_ptr<sdbusplus::asio::connection> conn_ref, sdbusplus::asio::object_server& server_ref, boost::asio::io_context& io);
        ~fan_loop();

        int buserrors = 0;
        int generrors = 0;

        int read_temperatures();
        int get_fan_pwms();
        void initialize_temps();

        void initialize_controller();

        nlohmann::json readJson(std::string path);

        int control_loop();

        double get_ts();

        double getcalcval();
        
        void schedule_temp_retry();
        void arm_tick();

    private:

        bool linear;
        bool running;

        double ts;

        int numfans;
        
        std::shared_ptr<sdbusplus::asio::connection> conn;
        sdbusplus::asio::object_server& obj_server;
        boost::asio::steady_timer tick_timer;
        boost::asio::steady_timer retry_timer;
        std::chrono::milliseconds period_{1000};
        bool temps_ready_{false};
        bool init_inflight_{false};

        std::vector<std::unique_ptr<temperature_sensor>> temps;
        std::vector<std::unique_ptr<fan>> fans;
        std::vector<std::string> pwm_paths;
        std::vector<std::string> temp_names;

        std::map<temp_name, fan> temp_fan_map;

        std::unique_ptr<pid_control::ec::pid_info_t> controller;

        std::map<std::string, std::unique_ptr<sdbusplus::server::object_t<ValueIface, AssociationIface, ThresholdIface>>> fan_controllers;

        std::unique_ptr<ObjectManagerIface> object_manager;

        int control_count;

        int pwm_retries;

};
