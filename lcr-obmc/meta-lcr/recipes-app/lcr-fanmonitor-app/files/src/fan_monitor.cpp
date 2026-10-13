#include "../include/fan_monitor.hpp"

std::string getService(sdbusplus::bus_t& bus, const std::string& intf,
    const std::string& path)
{
    phosphor::logging::log<phosphor::logging::level::INFO>("INFO: getService enter ");
    auto mapperCall =
    bus.new_method_call("xyz.openbmc_project.ObjectMapper",
            "/xyz/openbmc_project/object_mapper",
            "xyz.openbmc_project.ObjectMapper", "GetObject");

    mapperCall.append(path);
    mapperCall.append(std::vector<std::string>({intf}));

    auto mapperResponseMsg = bus.call(mapperCall);

    if (mapperResponseMsg.is_method_error())
    {
        phosphor::logging::log<phosphor::logging::level::INFO>("INFO: getService error in the mapper call");
        throw std::runtime_error("ERROR in mapper call");
    }

    std::map<std::string, std::vector<std::string>> mapperResponse;
    mapperResponseMsg.read(mapperResponse);

    if (mapperResponse.begin() == mapperResponse.end())
    {
        phosphor::logging::log<phosphor::logging::level::INFO>("INFO: getService error in the mapper response");
        throw std::runtime_error("ERROR in reading the mapper response");
    }

    return mapperResponse.begin()->first;
}

nlohmann::json fan_monitor::readJson(std::string path) {
    std::ifstream f(path);

    nlohmann::json read;

    try {
        f >> read;
    } catch (const nlohmann::json::parse_error& e) {
        std::cerr << "JSON parse error: " << e.what() << std::endl;
    }
    
    return read;
}

int fan_monitor::read_controller_val(std::string path) {
    std::ifstream f(path);

    int read;

    try {
        f >> read;
    } catch (...) {
        phosphor::logging::log<phosphor::logging::level::INFO>(("fan_monitor failed to read file " + path).c_str());    
    }
    
    return read;
}

void fan_monitor::fan_enable(std::string fen_path) {
    std::ofstream writingfile(fen_path);
    writingfile << 1;
}

void fan_monitor::pwmflag_enable(std::string pen_path) {
    std::ofstream writingfile(pen_path);
    writingfile << 1;
}

namespace fs = std::filesystem;

bool isValidDirectory(const std::string& directory) {
    // std::string path = "/sys/class/hwmon/" + directory + "/";
    if (!fs::is_directory(directory)) {
        return false;
    };
    for (const auto& entry : fs::directory_iterator(directory)) {
        if (entry.is_regular_file()) {
            std::string filePath = entry.path().string();

            if (entry.path().filename() == "name") {
                std::ifstream file(filePath);
                if (file.is_open()) {
                    std::string content;
                    std::getline(file, content);  // read one line
                
                    // remove any trailing '\r' or '\n' just in case
                    content.erase(std::remove(content.begin(), content.end(), '\n'), content.end());
                    content.erase(std::remove(content.begin(), content.end(), '\r'), content.end());
                    //dbgFile << "[" << currentTimestamp() << "] " << "LCR:TEMP: Content of name file: " << content << std::endl;
                    if (content == "max31790") {
                        return true;
                    }
                } else {
                    ;
                }
            }
        }
    }
    return false;
}

std::vector<std::string> fan_monitor::explore_dir() 
{
    std::vector<std::string> directories;
    std::string path = "/sys/class/hwmon/";
    for (const auto& entry : fs::directory_iterator(path)) {

        if (isValidDirectory(entry.path().string())) {

            directories.push_back(entry.path().string());
        }
    }
    return directories;
}

std::optional<std::tuple<int, int>> fan_monitor::get_hwmon_i2c_bus_addr(const std::string& hwmon_dir)
{
    fs::path device_link = fs::path(hwmon_dir) / "device";

    if (!fs::exists(device_link) || !fs::is_symlink(device_link)) {
        return std::nullopt;
    }

    std::error_code ec;
    fs::path resolved = fs::canonical(device_link, ec);
    if (ec) {
        return std::nullopt;
    }

    // Last component is always bus-addr for an I2C client
    std::string leaf = resolved.filename().string();   // example 1-002f
    auto dash = leaf.find('-');
    if (dash == std::string::npos) {
        return std::nullopt;
    }

    try {
        int bus  = std::stoi(leaf.substr(0, dash));
        int addr = std::stoi(leaf.substr(dash + 1), nullptr, 16); // hex to int
        phosphor::logging::log<phosphor::logging::level::INFO>(("fan_monitor bus found " + std::to_string(bus) + " and address " + std::to_string(addr)).c_str());
        return std::make_tuple(bus, addr);
    } catch (...) {
        return std::nullopt;
    }
}

fan_monitor::fan_monitor(std::shared_ptr<sdbusplus::asio::connection> conn_ref,sdbusplus::asio::object_server& server_ref) : 
                        conn(conn_ref), obj_server(server_ref), timer(conn->get_io_context())                         
{

    obj_server.add_manager("/xyz/openbmc_project/sensors");
    phosphor::logging::log<phosphor::logging::level::INFO>("fan_monitor init enter");

    configJson = readJson("/usr/share/lcr_configs/bus_devices.json");

    std::unordered_map<std::string, std::tuple<int, int>> bus_addr;
    for (auto& fan_dir : configJson["fancontrollers"].items()) {
        phosphor::logging::log<phosphor::logging::level::INFO>(("fan_monitor directory " + fan_dir.key()).c_str());
        bus_addr[fan_dir.key()] = std::make_tuple(configJson["fancontrollers"][fan_dir.key()]["bus"].get<int>(), configJson["fancontrollers"][fan_dir.key()]["address"].get<int>());
    }

    std::vector<std::string> directories = explore_dir();
    for (std::string d : directories) {
        auto dir_bus_addr = get_hwmon_i2c_bus_addr(d);
        if (!dir_bus_addr) {
            phosphor::logging::log<phosphor::logging::level::WARNING>(
                ("Could not determine I2C bus/addr for " + d).c_str());
            continue;
        }
    
        for (auto const& [name, i2c_tuple] : bus_addr) {
            if (i2c_tuple == dir_bus_addr) {
                phosphor::logging::log<phosphor::logging::level::INFO>(("fan_monitor directory name " + name + " address exists in json").c_str());
                for (auto& fan_inst : configJson["fancontrollers"][name].items()) {
                    if (fan_inst.key() != "bus" && fan_inst.key() != "address" && fan_inst.key() != "pwm_bitmax") {
                        int num = configJson["fancontrollers"][name][fan_inst.key()]["dir"].get<int>();
                        std::string pwm_path =    d + "/pwm" + std::to_string(num);
                        std::string pen_path =    d + "/pwm" + std::to_string(num) + "_enable";
                        std::string fen_path =    d + "/fan" + std::to_string(num) + "_enable";
                        std::string tach_path =   d + "/fan" + std::to_string(num) + "_input";
                        std::string fault_path =  d + "/fan" + std::to_string(num) + "_fault";
                        std::string target_path = d + "/fan" + std::to_string(num) + "_target";
                        int pwm_init =     read_controller_val(pwm_path);
                        int tach_init =    read_controller_val(tach_path);
                        int pwm_enable =   read_controller_val(pen_path);
                        int tach_enable =  read_controller_val(fen_path);
                        int pwm_bitmax =   configJson["fancontrollers"][name]["pwm_bitmax"].get<int>();
                        std::string fan_name = fan_inst.key();
                        fan_enable(fen_path);
                        if (fs::exists(pwm_path)) {
                            fanItems[fan_name] = std::make_unique<fan_element>(pwm_path, tach_path, pen_path, fen_path, fault_path, target_path, 
                                                                                fan_name, pwm_init, tach_init, num, pwm_enable, tach_enable, pwm_bitmax, true);
                            fanItems[fan_name]->highthresh     = configJson["fancontrollers"][name][fan_name]["thresholds"]["critmax"].get<int>();
                            fanItems[fan_name]->lowthresh      = configJson["fancontrollers"][name][fan_name]["thresholds"]["critmin"].get<int>();
                            fanItems[fan_name]->warnhighthresh = configJson["fancontrollers"][name][fan_name]["thresholds"]["warnmax"].get<int>();
                            fanItems[fan_name]->warnlowthresh  = configJson["fancontrollers"][name][fan_name]["thresholds"]["warnmin"].get<int>();
                            fanItems[fan_name]->parentName = name;
                            pwmflag_enable(pen_path);
                        } else {
                            phosphor::logging::log<phosphor::logging::level::INFO>(("fan_monitor pwm path : " + pwm_path + " does not exist").c_str());
                        } 
                    }
                }
            } else {
                phosphor::logging::log<phosphor::logging::level::INFO>(("fan_monitor directory name " + name + " DOES NOT exist in json").c_str());
            }
        }
    }

    expose_readings();
    phosphor::logging::log<phosphor::logging::level::INFO>("fan_monitor init exit");
}    

fan_monitor::~fan_monitor()
{
    ;
}

bool fan_monitor::writeConfigJson() {

    std::ofstream outJSON("/usr/share/lcr_configs/bus_devices.json");
    if (!outJSON.is_open()) {
        std::cerr << "Failed to open file for writing!\n";
        return false;
    }

    outJSON << std::setw(4) << configJson << std::endl; 
    outJSON.close();
    return true;
}

void fan_monitor::expose_readings() {
    phosphor::logging::log<phosphor::logging::level::INFO>("expose_readings enter");
    for (const auto& [key, fan] : fanItems) {
        std::string pwm_control_path = "/xyz/openbmc_project/control/fanpwm/" + fan->name;
        std::string pwm_sense_path =   "/xyz/openbmc_project/sensors/fan_pwm/" + fan->name;
        std::string tach_sense_path =  "/xyz/openbmc_project/sensors/fan_tach/"   + fan->name;

        std::unique_ptr<sdbusplus::asio::dbus_interface> pwm_control_iface =      obj_server.add_unique_interface(pwm_control_path, "xyz.openbmc_project.Control.FanPwm");
        std::unique_ptr<sdbusplus::asio::dbus_interface> pwm_sense_iface =        obj_server.add_unique_interface(pwm_sense_path, "xyz.openbmc_project.Sensor.Value");
        std::unique_ptr<sdbusplus::asio::dbus_interface> pwm_assoc_iface =        obj_server.add_unique_interface(pwm_sense_path, "xyz.openbmc_project.Association.Definitions");
        std::unique_ptr<sdbusplus::asio::dbus_interface> tach_assoc_iface =       obj_server.add_unique_interface(tach_sense_path, "xyz.openbmc_project.Association.Definitions");
        std::unique_ptr<sdbusplus::asio::dbus_interface> tach_crit_thresh_iface = obj_server.add_unique_interface(tach_sense_path, "xyz.openbmc_project.Sensor.Threshold.Critical");
        std::unique_ptr<sdbusplus::asio::dbus_interface> tach_warn_thresh_iface = obj_server.add_unique_interface(tach_sense_path, "xyz.openbmc_project.Sensor.Threshold.Warning");
        std::unique_ptr<sdbusplus::asio::dbus_interface> tach_sense_iface =       obj_server.add_unique_interface(tach_sense_path, "xyz.openbmc_project.Sensor.Value");
        std::unique_ptr<sdbusplus::asio::dbus_interface> tach_avail_iface =       obj_server.add_unique_interface(tach_sense_path, "xyz.openbmc_project.State.Decorator.Availability");
        std::unique_ptr<sdbusplus::asio::dbus_interface> tach_oper_iface =        obj_server.add_unique_interface(tach_sense_path, "xyz.openbmc_project.State.Decorator.OperationalStatus");

        pwm_control_iface->register_property_rw<uint64_t>(
            "Target",
            sdbusplus::vtable::property_::emits_change,
            [this, name = fan->name](const uint64_t& requested_value, uint64_t& current_value) -> bool {
                // Setter: Validate/update stored status
                auto it = fanItems.find(name);
                if (it == fanItems.end()) {
                    return false;
                }
        
                if (requested_value == current_value) {
                    // No change - don't emit signal
                    return true;
                }
        
                // Update internal D-Bus value and map
                current_value = requested_value;
                it->second->pwm = requested_value;
        
    
                // Success with change - emit signal
                return true;
            },
            [this, name = fan->name](const uint64_t&) -> uint64_t {
                // Getter: Retrieve from map
                auto it = fanItems.find(name);
                if (it != fanItems.end()) {
                    // phosphor::logging::log<phosphor::logging::level::INFO>(
                        // ("Internal value of " + name + " is currently " + std::to_string(it->second->pwm)).c_str()
                    // );
                    return it->second->pwm;
                }
                return 0;  // Fallback
            }
        );

        pwm_sense_iface->register_property_r<double>(
            "Value",
            sdbusplus::vtable::property_::emits_change,
            [this, name = fan->name](const double&) -> double {
                // Getter: Retrieve from map
                auto it = fanItems.find(name);
                if (it != fanItems.end()) {
                    return ((double)((double)(it->second->pwm) / it->second->pwm_bitmax) * 100.0);
                }
                return -1;  // Fallback
            }
        );

        pwm_sense_iface->register_property_r<int>(
            "PWM_Value",
            sdbusplus::vtable::property_::emits_change,
            [this, name = fan->name](const int&) -> int {
                // Getter: Retrieve from map
                auto it = fanItems.find(name);
                if (it != fanItems.end()) {
                    return ((int)it->second->pwm);
                }
                return -1;  // Fallback
            }
        );

        pwm_sense_iface->register_property_r<int>(
            "PWM_Bitmax",
            sdbusplus::vtable::property_::emits_change,
            [this, name = fan->name](const int&) -> int {
                // Getter: Retrieve from map
                auto it = fanItems.find(name);
                if (it != fanItems.end()) {
                    return ((int)it->second->pwm_bitmax);
                }
                return -1;  // Fallback
            }
        );

        pwm_sense_iface->register_property_r<std::string>(
            "Unit",
            sdbusplus::vtable::property_::const_,
            [](const auto&) { return std::string("xyz.openbmc_project.Sensor.Value.Unit.Percent"); }
        );

        pwm_assoc_iface->register_property_r<std::vector<std::tuple<std::string, std::string, std::string>>>(
            "Associations",
            sdbusplus::vtable::property_::const_,
            [](const auto&) {
                return std::vector<std::tuple<std::string, std::string, std::string>>{
                    {"chassis", "all_sensors", "/xyz/openbmc_project/inventory/system/chassis/LCR_ChM"}
                };
            }
        );

        tach_assoc_iface->register_property_r<std::vector<std::tuple<std::string, std::string, std::string>>>(
            "Associations",
            sdbusplus::vtable::property_::const_,
            [](const auto&) {
                return std::vector<std::tuple<std::string, std::string, std::string>>{
                    {"chassis", "all_sensors", "/xyz/openbmc_project/inventory/system/chassis/LCR_ChM"}
                };
            }
        );

        tach_crit_thresh_iface->register_property_rw<int>(
            "CriticalHigh",
            sdbusplus::vtable::property_::emits_change,
            [this, name = fan->name](const int& requested_value, int& current_value) -> bool {
                // Setter: Validate/update stored status
                auto it = fanItems.find(name);
                if (it == fanItems.end()) {
                    return false;
                }
        
                if (requested_value == current_value) {
                    // No change - don't emit signal
                    return true;
                }
        
                // Update internal D-Bus value and map
                current_value = requested_value;
                it->second->highthresh = requested_value;
        
                configJson["fancontrollers"][it->second->parentName][name]["thresholds"]["critmax"] = requested_value;
                writeConfigJson();

                // Success with change - emit signal
                return true;
            },
            [this, name = fan->name](const int&) -> int {
                auto it = fanItems.find(name);
                if (it != fanItems.end()) {
                    return it->second->highthresh;
                }
                return 10000;  // Fallback
            }
        );
    
        tach_crit_thresh_iface->register_property_rw<int>(
            "CriticalLow",
            sdbusplus::vtable::property_::emits_change,
            [this, name = fan->name](const int& requested_value, int& current_value) -> bool {
                // Setter: Validate/update stored status
                auto it = fanItems.find(name);
                if (it == fanItems.end()) {
                    return false;
                }
        
                if (requested_value == current_value) {
                    // No change - don't emit signal
                    return true;
                }
        
                // Update internal D-Bus value and map
                current_value = requested_value;
                it->second->lowthresh = requested_value;
        
                configJson["fancontrollers"][it->second->parentName][name]["thresholds"]["critmin"] = requested_value;
                writeConfigJson();
        
    
                // Success with change - emit signal
                return true;
            },
            [this, name = fan->name](const int&) -> int {
                auto it = fanItems.find(name);
                if (it != fanItems.end()) {
                    return it->second->lowthresh;
                }
                return -5000;  // Fallback
            }
        );

        tach_crit_thresh_iface->register_property_r<bool>(
            "CriticalAlarmHigh",
            sdbusplus::vtable::property_::emits_change,  // Changed to emits_change
            [this, name = fan->name](const auto&) {
                auto it = fanItems.find(name);
                if (it != fanItems.end()) {
                    int cur_val = it->second->tach;
                    return cur_val > it->second->highthresh;
                }
                return false;
            }
        );
        tach_crit_thresh_iface->register_property_r<bool>(
            "CriticalAlarmLow",
            sdbusplus::vtable::property_::emits_change,  // Changed to emits_change
            [this, name = fan->name](const auto&) {
                auto it = fanItems.find(name);
                if (it != fanItems.end()) {
                    int cur_val = it->second->tach;
                    return cur_val <= it->second->lowthresh;
                }
                return false;
            }
        );
        // xyz.openbmc_project.Sensor.Value.Unit.RPMS

        tach_warn_thresh_iface->register_property_rw<int>(
            "WarningHigh",
            sdbusplus::vtable::property_::emits_change,
            [this, name = fan->name](const int& requested_value, int& current_value) -> bool {
                // Setter: Validate/update stored status
                auto it = fanItems.find(name);
                if (it == fanItems.end()) {
                    return false;
                }
        
                if (requested_value == current_value) {
                    // No change - don't emit signal
                    return true;
                }
        
                // Update internal D-Bus value and map
                current_value = requested_value;
                it->second->warnhighthresh = requested_value;
        
                configJson["fancontrollers"][it->second->parentName][name]["thresholds"]["warmmax"] = requested_value;
                writeConfigJson();
        
                // Success with change - emit signal
                return true;
            },
            [this, name = fan->name](const int&) -> int {
                auto it = fanItems.find(name);
                if (it != fanItems.end()) {
                    return it->second->warnhighthresh;
                }
                return 10000;  // Fallback
            }
        );
    
        tach_warn_thresh_iface->register_property_rw<int>(
            "WarningLow",
            sdbusplus::vtable::property_::emits_change,
            [this, name = fan->name](const int& requested_value, int& current_value) -> bool {
                // Setter: Validate/update stored status
                auto it = fanItems.find(name);
                if (it == fanItems.end()) {
                    return false;
                }
        
                if (requested_value == current_value) {
                    // No change - don't emit signal
                    return true;
                }
        
                // Update internal D-Bus value and map
                current_value = requested_value;
                it->second->warnlowthresh = requested_value;
        
                configJson["fancontrollers"][it->second->parentName][name]["thresholds"]["warnmin"] = requested_value;
                writeConfigJson();
        
                // Success with change - emit signal
                return true;
            },
            [this, name = fan->name](const int&) -> int {
                auto it = fanItems.find(name);
                if (it != fanItems.end()) {
                    return it->second->warnlowthresh;
                }
                return -5000;  // Fallback
            }
        );

        tach_warn_thresh_iface->register_property_r<bool>(
            "WarningAlarmHigh",
            sdbusplus::vtable::property_::emits_change,  // Changed to emits_change
            [this, name = fan->name](const auto&) {
                auto it = fanItems.find(name);
                if (it != fanItems.end()) {
                    int cur_val = it->second->tach;
                    return cur_val > it->second->warnhighthresh;
                }
                return false;
            }
        );
        tach_warn_thresh_iface->register_property_r<bool>(
            "WarningAlarmLow",
            sdbusplus::vtable::property_::emits_change,  // Changed to emits_change
            [this, name = fan->name](const auto&) {
                auto it = fanItems.find(name);
                if (it != fanItems.end()) {
                    int cur_val = it->second->tach;
                    return cur_val <= it->second->warnlowthresh;
                }
                return false;
            }
        );

        // tach_sense_iface
        tach_sense_iface->register_property_r<int>(
            "Value",
            sdbusplus::vtable::property_::emits_change,
            [this, name = fan->name](const int&) -> int {
                // Getter: Retrieve from map
                auto it = fanItems.find(name);
                if (it != fanItems.end()) {
                    // phosphor::logging::log<phosphor::logging::level::INFO>(
                        // ("Internal tach value of " + name + " is currently " + std::to_string(it->second->tach)).c_str()
                    // );
                    return it->second->tach;
                }
                return -1;  // Fallback
            }
        );

        tach_sense_iface->register_property_r<std::string>(
            "Unit",
            sdbusplus::vtable::property_::const_,
            [](const auto&) { return std::string("xyz.openbmc_project.Sensor.Value.Unit.RPMS"); }
        );

        tach_avail_iface->register_property_r<bool>(
            "Available",
            sdbusplus::vtable::property_::const_,
            [](const auto&) { return true; }
        );

        tach_oper_iface->register_property_r<bool>(
            "Functional",
            sdbusplus::vtable::property_::const_,
            [](const auto&) { return true; }
        );

        pwm_control_iface->initialize();
        pwm_sense_iface->initialize();
        pwm_assoc_iface->initialize();
        tach_assoc_iface->initialize();
        tach_crit_thresh_iface->initialize();
        tach_warn_thresh_iface->initialize();
        tach_sense_iface->initialize();
        tach_avail_iface->initialize();
        tach_oper_iface->initialize();

        m_pwm_control_iface[fan->name]      = std::move(pwm_control_iface);
        m_pwm_sense_iface[fan->name]        = std::move(pwm_sense_iface);
        m_pwm_assoc_iface[fan->name]        = std::move(pwm_assoc_iface);
        m_tach_assoc_iface[fan->name]       = std::move(tach_assoc_iface);
        m_tach_crit_thresh_iface[fan->name] = std::move(tach_crit_thresh_iface);
        m_tach_warn_thresh_iface[fan->name] = std::move(tach_warn_thresh_iface);
        m_tach_sense_iface[fan->name]       = std::move(tach_sense_iface);
        m_tach_avail_iface[fan->name]       = std::move(tach_avail_iface);
        m_tach_oper_iface[fan->name]        = std::move(tach_oper_iface);

    }
    phosphor::logging::log<phosphor::logging::level::INFO>("expose_readings exit");
}

void fan_monitor::update_readings() {
    // phosphor::logging::log<phosphor::logging::level::INFO>("update_readings enter");
    if (fanItems.empty()) {
        schedule_update();  // Nothing to do; schedule next
        return;
    }

    // Chain async reads over map
    std::vector<std::string> keys;
    for (const auto& kv : fanItems) { keys.push_back(kv.first); }

    auto idx = std::make_shared<size_t>(0);
    auto check_next_ptr = std::make_shared<std::function<void()>>();
    *check_next_ptr = [this, keys, idx, check_next_ptr]() mutable {
        if (*idx >= keys.size()) {
            // All done; schedule next update
            schedule_update();
            // phosphor::logging::log<phosphor::logging::level::INFO>("update_readings exit");
            return;
        }
        const std::string& fan_name = keys[*idx];
        ++(*idx);

        auto remaining = std::make_shared<std::atomic<int>>(2);  // one for PWM, one for tach

        auto fan_done = [this, remaining, check_next_ptr, fan_name]() {
            if (--(*remaining) == 0) {
                (*check_next_ptr)();
            }
        };

        async_update_pwm(fan_name, [this, fan_done, fan_name](uint64_t old_value, uint64_t new_value) {
            async_update_bus_pwm(fan_name, old_value, new_value);
            fan_done();
        });

        async_readtach(fan_name, [this, fan_done, fan_name](int old_value, int new_value) {
            async_update_bus_tach(fan_name, old_value, new_value);
            fan_done();
        });
        
        // phosphor::logging::log<phosphor::logging::level::INFO>("update_readings after the async_readtach");
    };
    // phosphor::logging::log<phosphor::logging::level::INFO>("update_readings moving onto the next pointer");
    (*check_next_ptr)();  // Start chain

    // phosphor::logging::log<phosphor::logging::level::INFO>("update_readings exit");
}

void fan_monitor::async_readtach(const std::string& fan_name, std::function<void(int, int)> callback) {
    boost::asio::post(timer.get_executor(), [this, fan_name, callback]() {
        // phosphor::logging::log<phosphor::logging::level::INFO>("async_readtach entering callback");
        int new_value = 0;  // Error value
        try {
            // phosphor::logging::log<phosphor::logging::level::INFO>(("async_readtach callback trying to find sensor item at name " + fan_name).c_str());
            auto it = fanItems.find(fan_name);
            if (it == fanItems.end()) {
                // phosphor::logging::log<phosphor::logging::level::INFO>("async_readtach callback failed to find sensor item");
                throw std::runtime_error("tach not found");
            }
            // phosphor::logging::log<phosphor::logging::level::INFO>("async_readtach callback trying to find sensor item");
            fan_element* fan_ptr = it->second.get();  // Pointer to modifiable sensor element in map

            int old_value = (fan_ptr->tach);

            // phosphor::logging::log<phosphor::logging::level::INFO>("async_readtach attempting to get file");
            std::ifstream readingfile(fan_ptr->tach_path);

            if (!readingfile) {
                std::cerr << "Failed to open file in " << fan_ptr->tach_path << "\n";
                callback(0, 0);
            }
            // phosphor::logging::log<phosphor::logging::level::INFO>("async_readtach getting current reading");
        
            readingfile >> fan_ptr->tach;

            // phosphor::logging::log<phosphor::logging::level::INFO>("async_readtach calculating");
            new_value = fan_ptr->tach;  // Get updated status
            
            // phosphor::logging::log<phosphor::logging::level::INFO>("async_readtach callback");
            callback(old_value, new_value);
        } catch (const std::exception& e) {
            // On error, don't update cur_reading (already not updated), log, and callback with error value
            int error_value = 0;
            int old_value = 0;  // Or fetch actual old if needed; using error for consistency
            phosphor::logging::log<phosphor::logging::level::ERR>(("Failed to read sensor " + fan_name + ": " + e.what()).c_str());
            callback(old_value, error_value);
        }
    });
}

void fan_monitor::async_update_pwm(const std::string& fan_name, std::function<void(uint64_t, uint64_t)> callback) {
    boost::asio::post(timer.get_executor(), [this, fan_name, callback]() {
        // phosphor::logging::log<phosphor::logging::level::INFO>("async_update_pwm entering callback");
        uint64_t new_value = 0;  // Error value
        uint64_t old_value = 0;  // Error value
        try {
            // phosphor::logging::log<phosphor::logging::level::INFO>(("async_update_pwm callback trying to find sensor item at name " + fan_name).c_str());
            auto it = fanItems.find(fan_name);
            if (it == fanItems.end()) {
                // phosphor::logging::log<phosphor::logging::level::INFO>("async_update_pwm callback failed to find sensor item");
                throw std::runtime_error("pwm not found");
            }
            // phosphor::logging::log<phosphor::logging::level::INFO>("async_update_pwm callback trying to find sensor item");
            fan_element* fan_ptr = it->second.get();  // Pointer to modifiable sensor element in map

            std::ifstream readingfile(fan_ptr->pwm_path);

            if (!readingfile) {
                std::cerr << "Failed to open file in " << fan_ptr->tach_path << "\n";
                callback(0, 0);
            }

            readingfile >> old_value;

            readingfile.close();
            // phosphor::logging::log<phosphor::logging::level::INFO>("async_update_pwm attempting to get file");
            std::ofstream writingfile(fan_ptr->pwm_path);

            if (!readingfile) {
                std::cerr << "Failed to open file in " << fan_ptr->tach_path << "\n";
                callback(0, 0);
            }
            
            writingfile << fan_ptr->pwm;
            
            // phosphor::logging::log<phosphor::logging::level::INFO>("async_update_pwm callback");
            callback(old_value, new_value);
        } catch (const std::exception& e) {
            // On error, don't update cur_reading (already not updated), log, and callback with error value
            uint64_t error_value = 0;
            uint64_t old_value = 0;  
            phosphor::logging::log<phosphor::logging::level::ERR>(("Failed to read sensor " + fan_name + ": " + e.what()).c_str());
            callback(old_value, error_value);
        }
    });
}

void fan_monitor::async_update_bus_tach(const std::string& fan_name, int old_val, int new_val) {
    // phosphor::logging::log<phosphor::logging::level::INFO>("async_update_bus_tach enter");
    auto vit = m_tach_sense_iface.find(fan_name);
    auto tit = m_tach_crit_thresh_iface.find(fan_name);
    auto it = fanItems.find(fan_name);
    fan_element* fan_ptr = it->second.get();
    if (vit == m_tach_sense_iface.end() || !vit->second || tit == m_tach_crit_thresh_iface.end() || !tit->second) {
        return;  // No interfaces for this sensor
    }

    // Emit change signals (async via post to integrate with Asio loop)
    boost::asio::post(timer.get_executor(), [this, viface = vit->second.get(), tiface = tit->second.get(), fan_name, fan_ptr, old_val, new_val]() {

        // phosphor::logging::log<phosphor::logging::level::INFO>("async_update_bus_tach starting to set signals");
        if (viface) {
            // phosphor::logging::log<phosphor::logging::level::INFO>(("async_update_bus_tach attempting to update bus for " + fan_name).c_str());
            viface->signal_property("Value");  // Always signal Value change
        }

        // Extract thresholds with error handling (see mitigations below)
        int highthresh = 0.0;  // Default/fallback
        int lowthresh = 0.0;   // Default/fallback
        int warnhighthresh = 0.0;  // Default/fallback
        int warnlowthresh = 0.0;   // Default/fallback
        highthresh     = fan_ptr->highthresh;
        lowthresh      = fan_ptr->lowthresh;
        warnhighthresh = fan_ptr->warnhighthresh;
        warnlowthresh  = fan_ptr->warnlowthresh;

        // phosphor::logging::log<phosphor::logging::level::INFO>("async_update_bus finished setting signals");
        bool old_warn_alarm_high = std::abs(old_val) >= std::abs(warnhighthresh);
        bool new_warn_alarm_high = std::abs(new_val) >= std::abs(warnhighthresh);
        uint8_t event_data1 = new_warn_alarm_high ? static_cast<uint8_t>(ThresholdTypeCodes::unc_high) : static_cast<uint8_t>(ThresholdTypeCodes::unc_low);
        event_data1 = event_data1 | (1 << 6); //[7:6] reading in byte 2
        event_data1 = event_data1 | (1 << 4); //[5:4] threshold in byte 3
        if (old_warn_alarm_high != new_warn_alarm_high && tiface || (new_warn_alarm_high && fan_ptr->first && tiface)) {
            sendPlatformEvent(fan_name, new_warn_alarm_high, event_data1, 0xFF, 0xFF);
            tiface->signal_property("warningAlarmHigh");
        }

        bool old_warn_alarm_low = std::abs(old_val) <= std::abs(warnlowthresh);
        bool new_warn_alarm_low = std::abs(new_val) <= std::abs(warnlowthresh);
        event_data1 = new_warn_alarm_low ? static_cast<uint8_t>(ThresholdTypeCodes::lnc_high) : static_cast<uint8_t>(ThresholdTypeCodes::lnc_low);
        event_data1 = event_data1 | (1 << 6); //[7:6] reading in byte 2
        event_data1 = event_data1 | (1 << 4); //[5:4] threshold in byte 3
        if (old_warn_alarm_low != new_warn_alarm_low && tiface && old_val > -10000 || (new_warn_alarm_low && fan_ptr->first && tiface && old_val > -10000)) {
            phosphor::logging::log<phosphor::logging::level::ERR>(("Warning Low threshold alarm, old value causing alarm is " + std::to_string(old_val) + " and new value is " + std::to_string(new_val)).c_str());
            sendPlatformEvent(fan_name, new_warn_alarm_low, event_data1, 0xFF, 0xFF);
            tiface->signal_property("warningAlarmLow");
        }
        
        bool old_alarm_high = std::abs(old_val) >= std::abs(highthresh);
        bool new_alarm_high = std::abs(new_val) >= std::abs(highthresh);
        event_data1 = new_alarm_high ? static_cast<uint8_t>(ThresholdTypeCodes::ucr_high) : static_cast<uint8_t>(ThresholdTypeCodes::ucr_low);
        event_data1 = event_data1 | (1 << 6); //[7:6] reading in byte 2
        event_data1 = event_data1 | (1 << 4); //[5:4] threshold in byte 3
        if (old_alarm_high != new_alarm_high && tiface || (new_alarm_high && fan_ptr->first && tiface)) {
            sendPlatformEvent(fan_name, new_alarm_high, event_data1, 0xFF, 0xFF);
            tiface->signal_property("criticalAlarmHigh");
        }

        bool old_alarm_low = std::abs(old_val) <= std::abs(lowthresh);
        bool new_alarm_low = std::abs(new_val) <= std::abs(lowthresh);
        event_data1 = new_alarm_low ? static_cast<uint8_t>(ThresholdTypeCodes::lcr_high) : static_cast<uint8_t>(ThresholdTypeCodes::lcr_low);
        event_data1 = event_data1 | (1 << 6); //[7:6] reading in byte 2
        event_data1 = event_data1 | (1 << 4); //[5:4] threshold in byte 3
        if (old_alarm_low != new_alarm_low && tiface && old_val > -10000 || (new_alarm_low && fan_ptr->first && tiface && old_val > -10000)) {
            phosphor::logging::log<phosphor::logging::level::ERR>(("Critical Low threshold alarm, old value causing alarm is " + std::to_string(old_val) + " and new value is " + std::to_string(new_val)).c_str());
            sendPlatformEvent(fan_name, new_alarm_low, event_data1, 0xFF, 0xFF);
            tiface->signal_property("criticalAlarmLow");
        }

        fan_ptr->first = false;
    });
    // phosphor::logging::log<phosphor::logging::level::INFO>("async_update_bus_tach exit");
}

void fan_monitor::async_update_bus_pwm(const std::string& fan_name, uint64_t old_val, uint64_t new_val) {
    // phosphor::logging::log<phosphor::logging::level::INFO>("async_update_bus_pwm enter");
    auto vit = m_pwm_sense_iface.find(fan_name);
    auto cit = m_pwm_control_iface.find(fan_name);
    if (vit == m_tach_sense_iface.end() || !vit->second || cit == m_pwm_control_iface.end() || !cit->second) {
        return;  // No interfaces for this sensor
    }

    // Emit change signals (async via post to integrate with Asio loop)
    boost::asio::post(timer.get_executor(), [this, viface = vit->second.get(), ciface = cit->second.get(), fan_name, old_val, new_val]() {

        // phosphor::logging::log<phosphor::logging::level::INFO>("async_update_bus_pwm starting to set signals");
        if (viface) {
            // phosphor::logging::log<phosphor::logging::level::INFO>(("async_update_bus_pwm attempting to update bus for " + fan_name).c_str());
            viface->signal_property("Value");  // Always signal Value change
        }

        if (ciface) {
            // phosphor::logging::log<phosphor::logging::level::INFO>(("async_update_bus_pwm attempting to update bus for " + fan_name).c_str());
            ciface->signal_property("Target");  // Always signal Value change
        }

        // phosphor::logging::log<phosphor::logging::level::INFO>("async_update_bus_pwm finished setting signals");
    });
    // phosphor::logging::log<phosphor::logging::level::INFO>("async_update_bus_pwm exit");
}

void fan_monitor::schedule_update() {
    timer.expires_after(std::chrono::milliseconds(100));
    timer.async_wait([this](const boost::system::error_code& ec) {
        if (ec) {
            phosphor::logging::log<phosphor::logging::level::INFO>("fan_monitor schedule_update async wait failure. failed to schedule update");
            return;
        }
        this->update_readings();
    });
}

void fan_monitor::sendPlatformEvent(const std::string& dirName, bool assertEvent, uint8_t eventData1, std::optional<uint8_t> eventData2, std::optional<uint8_t> eventData3)
{
    phosphor::logging::log<phosphor::logging::level::INFO>("ipmiSenPlatformEvent enter");
    [[maybe_unused]] uint16_t generatorID = 0;

    //sysgeneratorID, evmRev, sensorType, sensorNum, eventType, eventData1, eventData2, eventData3

    generatorID = (0 << 12)      // Channel
        | (0x0 << 10)             // Reserved
        | ((0 & 0x3) << 8) // Lun
        | (0x10 << 1);

    {// bit 7 of eventType: 0 = assert, 1 = deassert

        std::string sensorPath = "/fan_tach/" + dirName;

        std::vector<uint8_t> eventData{eventData1};
        if (eventData2.has_value())
            eventData.push_back(*eventData2);
        if (eventData3.has_value())
            eventData.push_back(*eventData3);

        try
        {
            sdbusplus::bus_t dbus = sdbusplus::bus::new_default();
            std::string service = getService(dbus,
            "xyz.openbmc_project.Logging.IPMI",
            "/xyz/openbmc_project/Logging/IPMI");

            auto msg = dbus.new_method_call(
            service.c_str(),
            "/xyz/openbmc_project/Logging/IPMI",
            "xyz.openbmc_project.Logging.IPMI",
            "IpmiSelAdd");

            // Parameters match the exact IpmiSelAdd D-Bus method:
            // Message (string), Path (object_path), SELData (array byte), Assert (bool), GeneratorID (uint16)
            msg.append("SEL Entry", sensorPath, eventData, assertEvent, generatorID);
            phosphor::logging::log<phosphor::logging::level::INFO>("ipmiSenPlatformEvent sending dbus message");
            dbus.call(msg);
        }
        catch (const sdbusplus::exception_t& e)
        {
            phosphor::logging::log<phosphor::logging::level::ERR>(("Failed to log Platform Event to SEL: " + std::string(e.what())).c_str());
        }
    }
}
