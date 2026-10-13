#include "../include/ADC_manipulation.hpp"


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

nlohmann::json ADC_sensor::readJson(std::string path) 
{
    std::ifstream f(path);

    nlohmann::json read;

    try {
        f >> read;
    } catch (const nlohmann::json::parse_error& e) {
        std::cerr << "JSON parse error: " << e.what() << std::endl;
    }
    
    return read;
}

namespace fs = std::filesystem;

ADC_sensor::ADC_sensor(std::shared_ptr<sdbusplus::asio::connection> conn_ref,sdbusplus::asio::object_server& server_ref) : 
    conn(conn_ref), obj_server(server_ref), timer(conn->get_io_context())
{
    first = true;
    obj_server.add_manager("/xyz/openbmc_project/sensors");
    configJson = readJson("/usr/share/lcr_configs/bus_devices.json");
    SOSA = configJson["SOSA"];
    if (SOSA) {
        phosphor::logging::log<phosphor::logging::level::INFO>("SOSA Chassis");
    } else {
        phosphor::logging::log<phosphor::logging::level::INFO>("Legacy Chassis");
    }

    for (auto& adc_dir : configJson["adcsensors"].items()) {
        phosphor::logging::log<phosphor::logging::level::INFO>(("ADC_sensor entry " + adc_dir.key()).c_str());

        for (const auto& entry : fs::directory_iterator("/sys/bus/iio/devices")) {
            // Resolve symlinks, ".", and ".." to get the absolute canonical path
            std::error_code ec;
            fs::path canonical_path = fs::canonical(entry.path(), ec);
            if (ec) {
                continue;
            }

            if (canonical_path.string().find(adc_dir.key()) != std::string::npos && canonical_path.string().find("trigger") == std::string::npos) {
                phosphor::logging::log<phosphor::logging::level::INFO>(("ADC_sensor found path of adc " + entry.path().string()).c_str());
                phosphor::logging::log<phosphor::logging::level::INFO>(("ADC_sensor canonical path of adc " + canonical_path.string()).c_str());

                for (auto& rail : adc_dir.value().items()) {
                    if (rail.key() != "bus" && rail.key() != "address") {
                        std::string dir = configJson["adcsensors"][adc_dir.key()][rail.key()]["dir"].get<std::string>();
                        std::string path = entry.path().string() + "/" + dir;
                        double targ = configJson["adcsensors"][adc_dir.key()][rail.key()]["target"].get<double>();
                        if (configJson["adcsensors"][adc_dir.key()][rail.key()]["present"]) {
                            sensorItems[rail.key()] = std::make_unique<ADC_element>(rail.key(), path, targ);
                            sensorItems[rail.key()]->init = true;
                            sensorItems[rail.key()]->type           = configJson["adcsensors"][adc_dir.key()][rail.key()]["type"].get<std::string>();
                            sensorItems[rail.key()]->highthresh     = configJson["adcsensors"][adc_dir.key()][rail.key()]["thresholds"]["critmax"].get<double>();
                            sensorItems[rail.key()]->lowthresh      = configJson["adcsensors"][adc_dir.key()][rail.key()]["thresholds"]["critmin"].get<double>();
                            sensorItems[rail.key()]->warnhighthresh = configJson["adcsensors"][adc_dir.key()][rail.key()]["thresholds"]["warnmax"].get<double>();
                            sensorItems[rail.key()]->warnlowthresh  = configJson["adcsensors"][adc_dir.key()][rail.key()]["thresholds"]["warnmin"].get<double>();
                            sensorItems[rail.key()]->parentName     = adc_dir.key();
                            if (configJson["adcsensors"][adc_dir.key()][rail.key()].contains("nominal")) {
                                sensorItems[rail.key()]->nominal = configJson["adcsensors"][adc_dir.key()][rail.key()]["nominal"].get<double>();
                                sensorItems[rail.key()]->cmax = configJson["adcsensors"][adc_dir.key()][rail.key()]["max"].get<double>();
                            } else {
                                sensorItems[rail.key()]->nominal = -1.0;
                            }
                        }
                    }
                }
            }
        }
    }

    calculate_cs();
    expose_readings();
    
}

ADC_sensor::~ADC_sensor()
{
    ;
}

void ADC_sensor::calculate_cs() 
{
    for (const auto& [key, ADC] : sensorItems) {
        if (ADC->nominal >= 0 && ADC->cmax != 0) {
            ADC->scale_coeff = ADC->V_target / ((ADC->nominal / ADC->cmax) * 4095);
            phosphor::logging::log<phosphor::logging::level::INFO>(("calculate_cs calculated (with nominal): " + std::to_string(ADC->scale_coeff)).c_str());
        } else {
            ADC->scale_coeff = ADC->V_target / 2047;
            phosphor::logging::log<phosphor::logging::level::INFO>(("calculate_cs calculated (without nominal): " + std::to_string(ADC->scale_coeff)).c_str());
        }
    }
}

bool ADC_sensor::writeConfigJson() {

    std::ofstream outJSON("/usr/share/lcr_configs/bus_devices.json");
    if (!outJSON.is_open()) {
        std::cerr << "Failed to open file for writing!\n";
        return false;
    }

    outJSON << std::setw(4) << configJson << std::endl; 
    outJSON.close();
    return true;
}

int ADC_sensor::expose_readings() 
{

    for (const auto& [key, ADC] : sensorItems) {

        // Path for the sensor
        std::string path = "/xyz/openbmc_project/sensors/" + ADC->type + "/" + ADC->name;

        std::unique_ptr<sdbusplus::asio::dbus_interface> value_iface      = obj_server.add_unique_interface(path, "xyz.openbmc_project.Sensor.Value");
        std::unique_ptr<sdbusplus::asio::dbus_interface> assoc_iface      = obj_server.add_unique_interface(path, "xyz.openbmc_project.Association.Definitions");
        std::unique_ptr<sdbusplus::asio::dbus_interface> crit_thres_iface = obj_server.add_unique_interface(path, "xyz.openbmc_project.Sensor.Threshold.Critical");
        std::unique_ptr<sdbusplus::asio::dbus_interface> warn_thres_iface = obj_server.add_unique_interface(path, "xyz.openbmc_project.Sensor.Threshold.Warning");

        phosphor::logging::log<phosphor::logging::level::INFO>(("attempting to expose " + path).c_str());

        value_iface->register_property_r<double>(
            "Value",
            sdbusplus::vtable::property_::emits_change,
            [this, name = ADC->name](const double&) -> double {
                // Getter: Retrieve from map
                auto it = sensorItems.find(name);
                if (it != sensorItems.end()) {
                    return (it->second->V_current);
                }
                return -253.0;  // Fallback
            }
        );

        // Register other required properties for Sensor.Value with defaults
        value_iface->register_property_r<double>(
            "MaxValue",
            sdbusplus::vtable::property_::const_,
            [](const auto&) { return 24.0; }
        );
        value_iface->register_property_r<double>(
            "MinValue",
            sdbusplus::vtable::property_::const_,
            [](const auto&) { return -24.0; }
        );

        if (ADC->type == "voltage") {
            value_iface->register_property_r<std::string>(
                "Unit",
                sdbusplus::vtable::property_::const_,
                [](const auto&) { return std::string("xyz.openbmc_project.Sensor.Value.Unit.Volts"); }
            );
        } else if (ADC->type == "current") {
            value_iface->register_property_r<std::string>(
                "Unit",
                sdbusplus::vtable::property_::const_,
                [](const auto&) { return std::string("xyz.openbmc_project.Sensor.Value.Unit.Amperage"); }
            );
        }

        value_iface->register_property_r<int64_t>(
            "Scale",
            sdbusplus::vtable::property_::const_,
            [](const auto&) { return int64_t(0); }
        );

        crit_thres_iface->register_property_rw<double>(
            "CriticalHigh",
            sdbusplus::vtable::property_::emits_change,
            [this, name = ADC->name](const double& requested_value, double& current_value) -> bool {
                // Setter: Validate/update stored status
                auto it = sensorItems.find(name);
                if (it == sensorItems.end()) {
                    return false;
                }
        
                if (requested_value == current_value) {
                    // No change - don't emit signal
                    return true;
                }
        
                // Update internal D-Bus value and map
                current_value = requested_value;
                it->second->highthresh = requested_value;
        
                configJson["adcsensors"][it->second->parentName][name]["thresholds"]["critmax"] = requested_value;
                return writeConfigJson();
            },
            [this, name = ADC->name](const double&) -> double {
                auto it = sensorItems.find(name);
                if (it != sensorItems.end()) {
                    return (it->second->highthresh);
                }
                return 253.0;  // Fallback
            }
        );
    
        crit_thres_iface->register_property_rw<double>(
            "CriticalLow",
            sdbusplus::vtable::property_::emits_change,
            [this, name = ADC->name](const double& requested_value, double& current_value) -> bool {
                // Setter: Validate/update stored status
                auto it = sensorItems.find(name);
                if (it == sensorItems.end()) {
                    return false;
                }
        
                if (requested_value == current_value) {
                    // No change - don't emit signal
                    return true;
                }
        
                // Update internal D-Bus value and map
                current_value = requested_value;
                it->second->lowthresh = requested_value;
        
                configJson["adcsensors"][it->second->parentName][name]["thresholds"]["critmin"] = requested_value;
                return writeConfigJson();
            },
            [this, name = ADC->name](const double&) -> double {
                auto it = sensorItems.find(name);
                if (it != sensorItems.end()) {
                    return (it->second->lowthresh);
                }
                return -253.0;  // Fallback
            }
        );


        crit_thres_iface->register_property_r<bool>(
            "CriticalAlarmHigh",
            sdbusplus::vtable::property_::emits_change,  // Changed to emits_change
            [this, name = ADC->name](const auto&) {
                auto it = sensorItems.find(name);
                if (it != sensorItems.end()) {
                    double cur_val = std::abs(it->second->V_current);
                    return cur_val > std::abs(it->second->highthresh);
                }
                return false;
            }
        );
        crit_thres_iface->register_property_r<bool>(
            "CriticalAlarmLow",
            sdbusplus::vtable::property_::emits_change,  // Changed to emits_change
            [this, name = ADC->name](const auto&) {
                auto it = sensorItems.find(name);
                if (it != sensorItems.end()) {
                    double cur_val = std::abs(it->second->V_current);
                    return cur_val < std::abs(it->second->lowthresh);
                }
                return false;
            }
        );

        warn_thres_iface->register_property_rw<double>(
            "WarningHigh",
            sdbusplus::vtable::property_::emits_change,
            [this, name = ADC->name](const double& requested_value, double& current_value) -> bool {
                // Setter: Validate/update stored status
                auto it = sensorItems.find(name);
                if (it == sensorItems.end()) {
                    return false;
                }
        
                if (requested_value == current_value) {
                    // No change - don't emit signal
                    return true;
                }
        
                // Update internal D-Bus value and map
                current_value = requested_value;
                it->second->warnhighthresh = requested_value;
        
                configJson["adcsensors"][it->second->parentName][name]["thresholds"]["warnmax"] = requested_value;
                return writeConfigJson();
            },
            [this, name = ADC->name](const double&) -> double {
                auto it = sensorItems.find(name);
                if (it != sensorItems.end()) {
                    return (it->second->warnhighthresh);
                }
                return 253.0;  // Fallback
            }
        );
    
        warn_thres_iface->register_property_rw<double>(
            "WarningLow",
            sdbusplus::vtable::property_::emits_change,
            [this, name = ADC->name](const double& requested_value, double& current_value) -> bool {
                // Setter: Validate/update stored status
                auto it = sensorItems.find(name);
                if (it == sensorItems.end()) {
                    return false;
                }
        
                if (requested_value == current_value) {
                    // No change - don't emit signal
                    return true;
                }
        
                // Update internal D-Bus value and map
                current_value = requested_value;
                it->second->warnlowthresh = requested_value;
        
                configJson["adcsensors"][it->second->parentName][name]["thresholds"]["warnmin"] = requested_value;
                return writeConfigJson();
            },
            [this, name = ADC->name](const double&) -> double {
                auto it = sensorItems.find(name);
                if (it != sensorItems.end()) {
                    return (it->second->warnlowthresh);
                }
                return -253.0;  // Fallback
            }
        );

        warn_thres_iface->register_property_r<bool>(
            "WarningAlarmHigh",
            sdbusplus::vtable::property_::emits_change,  // Changed to emits_change
            [this, name = ADC->name](const auto&) {
                auto it = sensorItems.find(name);
                if (it != sensorItems.end()) {
                    double cur_val = std::abs(it->second->V_current);
                    return cur_val > std::abs(it->second->warnhighthresh);
                }
                return false;
            }
        );
        warn_thres_iface->register_property_r<bool>(
            "WarningAlarmLow",
            sdbusplus::vtable::property_::emits_change,  // Changed to emits_change
            [this, name = ADC->name](const auto&) {
                auto it = sensorItems.find(name);
                if (it != sensorItems.end()) {
                    double cur_val = std::abs(it->second->V_current);
                    return cur_val < std::abs(it->second->warnlowthresh);
                }
                return false;
            }
        );

        assoc_iface->register_property_r<std::vector<std::tuple<std::string, std::string, std::string>>>(
            "Associations",
            sdbusplus::vtable::property_::const_,
            [](const auto&) {
                return std::vector<std::tuple<std::string, std::string, std::string>>{
                    {"chassis", "all_sensors", "/xyz/openbmc_project/inventory/system/chassis/LCR_ChM"}
                };
            }
        );

        value_iface->initialize();
        crit_thres_iface->initialize();
        warn_thres_iface->initialize();
        assoc_iface->initialize();

        m_value_iface[ADC->name] = std::move(value_iface);
        m_crit_thres_iface[ADC->name] = std::move(crit_thres_iface);
        m_warn_thres_iface[ADC->name] = std::move(warn_thres_iface);
        m_assoc_iface[ADC->name] = std::move(assoc_iface);
        
    }
    phosphor::logging::log<phosphor::logging::level::INFO>("expose_reading exit");
    return 0;

};

void ADC_sensor::update_readings() 
{

    // phosphor::logging::log<phosphor::logging::level::INFO>("update_reading enter");
    if (sensorItems.empty()) {
        schedule_update();  // Nothing to do; schedule next
        return;
    }

    // Chain async reads over map (use vector of keys for ordering if needed; here assuming order doesn't matter)
    std::vector<std::string> keys;
    for (const auto& kv : sensorItems) { keys.push_back(kv.first); }

    auto idx = std::make_shared<size_t>(0);
    auto check_next_ptr = std::make_shared<std::function<void()>>();
    // phosphor::logging::log<phosphor::logging::level::INFO>("starting to check temperatures and update bus");
    *check_next_ptr = [this, keys, idx, check_next_ptr]() mutable {
        if (*idx >= keys.size()) {
            // All done; schedule next update
            schedule_update();
            // phosphor::logging::log<phosphor::logging::level::INFO>("update_reading exit");
            return;
        }
        const std::string& adc_name = keys[*idx];
        ++(*idx);
        // phosphor::logging::log<phosphor::logging::level::INFO>("update_reading starting the async_readtemp");
        async_readadc(adc_name, [this, check_next_ptr, adc_name](double old_value, double new_value) {
            // phosphor::logging::log<phosphor::logging::level::INFO>("update_reading entering the async bus update");
            async_update_bus(adc_name, old_value, new_value);
            (*check_next_ptr)();  // Proceed to next adc sensor
        });
        // phosphor::logging::log<phosphor::logging::level::INFO>("update_reading after the async_readtemp");
    };
    // phosphor::logging::log<phosphor::logging::level::INFO>("update_reading moving onto the next pointer");
    (*check_next_ptr)();  // Start chain

    // phosphor::logging::log<phosphor::logging::level::INFO>("update_reading exit");
}

void ADC_sensor::async_readadc(const std::string& adc_name, std::function<void(double, double)> callback) 
{
    // phosphor::logging::log<phosphor::logging::level::INFO>("async_readadc enter");
    boost::asio::post(timer.get_executor(), [this, adc_name, callback]() {
        // phosphor::logging::log<phosphor::logging::level::INFO>("async_readadc entering callback");
        double new_value = -29.0;  // Error value
        try {
            // phosphor::logging::log<phosphor::logging::level::INFO>(("async_readadc callback trying to find sensor item at name " + adc_name).c_str());
            auto it = sensorItems.find(adc_name);
            if (it == sensorItems.end()) {
                // phosphor::logging::log<phosphor::logging::level::INFO>("async_readadc callback failed to find sensor item");
                throw std::runtime_error("adc not found");
            }
            // phosphor::logging::log<phosphor::logging::level::INFO>("async_readadc callback trying to find sensor item");
            ADC_element* adc_ptr = it->second.get();  // Pointer to modifiable sensor element in map

            double old_value = (adc_ptr->V_current);
            if (adc_ptr->init) {
                old_value = adc_ptr->V_target;
                adc_ptr->init = false;
            }

            // phosphor::logging::log<phosphor::logging::level::INFO>("async_readadc attempting to get file");
            std::ifstream readingfile(adc_ptr->path);

            if (!readingfile) {
                std::cerr << "Failed to open file in " << adc_ptr->path << "\n";
                callback(-29.0, -29.0);
            }
        
            readingfile >> adc_ptr->raw_reading;
            // phosphor::logging::log<phosphor::logging::level::INFO>(("async_readadc raw_reading: " + std::to_string(adc_ptr->raw_reading)).c_str());

            adc_ptr->filter_vec.push_back(adc_ptr->raw_reading);
            if (adc_ptr->filter_vec.size() > 100) {
                adc_ptr->filter_vec.erase(adc_ptr->filter_vec.begin());
            }

            double sum = std::accumulate(adc_ptr->filter_vec.begin(), adc_ptr->filter_vec.end(), 0.0);
            // phosphor::logging::log<phosphor::logging::level::INFO>(("async_readadc sum: " + std::to_string(sum)).c_str());
            double average = sum / adc_ptr->filter_vec.size();
            // phosphor::logging::log<phosphor::logging::level::INFO>(("async_readadc average: " + std::to_string(average)).c_str());

            // adc_ptr->V_current = adc_ptr->scale_coeff * adc_ptr->raw_reading;
            adc_ptr->V_current = adc_ptr->scale_coeff * average;
            
            // phosphor::logging::log<phosphor::logging::level::INFO>(("async_readadc calculated: " + std::to_string(adc_ptr->V_current)).c_str());
            new_value = adc_ptr->V_current;  // Get updated status
            
            // phosphor::logging::log<phosphor::logging::level::INFO>("async_readadc callback");
            callback(old_value, new_value);
        } catch (const std::exception& e) {
            // On error, don't update cur_reading (already not updated), log, and callback with error value
            double error_value = -29.0;
            double old_value = -29.0; 
            phosphor::logging::log<phosphor::logging::level::ERR>(("Failed to read sensor " + adc_name + ": " + e.what()).c_str());
            callback(old_value, error_value);
        }
    });
    // phosphor::logging::log<phosphor::logging::level::INFO>("async_readadc exit");
}

void ADC_sensor::async_update_bus(const std::string& adc_name, double old_val, double new_val) 
{
    // phosphor::logging::log<phosphor::logging::level::INFO>("async_update_bus enter");
    auto vit = m_value_iface.find(adc_name);
    auto tit = m_crit_thres_iface.find(adc_name);
    auto it = sensorItems.find(adc_name);
    ADC_element& adc_ptr = *it->second;
    if (vit == m_value_iface.end() || !vit->second || tit == m_crit_thres_iface.end() || !tit->second) {
        return;  // No interfaces for this sensor
    }

    // Emit change signals (async via post to integrate with Asio loop)
    boost::asio::post(timer.get_executor(), [this, viface = vit->second.get(), tiface = tit->second.get(), adc_name, adc_ptr, old_val, new_val]() {

        // phosphor::logging::log<phosphor::logging::level::INFO>("async_update_bus starting to set signals");
        if (viface) {
            // phosphor::logging::log<phosphor::logging::level::INFO>(("async_update_bus attempting to update bus for " + adc_name).c_str());
            viface->signal_property("Value");  // Always signal Value change
        }

        // Extract thresholds with error handling (see mitigations below)
        double highthresh = 0.0;  // Default/fallback
        double lowthresh = 0.0;   // Default/fallback
        double warnhighthresh = 0.0;  // Default/fallback
        double warnlowthresh = 0.0;   // Default/fallback

        highthresh     = adc_ptr.highthresh;
        lowthresh      = adc_ptr.lowthresh;
        warnhighthresh = adc_ptr.warnhighthresh;
        warnlowthresh  = adc_ptr.warnlowthresh;
        // phosphor::logging::log<phosphor::logging::level::INFO>("async_update_bus finished setting signals");
        bool old_warn_alarm_high = std::abs(old_val) > std::abs(warnhighthresh);
        bool new_warn_alarm_high = std::abs(new_val) > std::abs(warnhighthresh);
        uint8_t event_data1 = new_warn_alarm_high ? static_cast<uint8_t>(ThresholdTypeCodes::unc_high) : static_cast<uint8_t>(ThresholdTypeCodes::unc_low);
        event_data1 = event_data1 | (1 << 6); //[7:6] reading in byte 2
        event_data1 = event_data1 | (1 << 4); //[5:4] threshold in byte 3
        if (first) {
            phosphor::logging::log<phosphor::logging::level::ERR>(("First is true, old value: " + std::to_string(old_val) + " and new value is " + std::to_string(new_val)).c_str());
        }

        if (old_warn_alarm_high != new_warn_alarm_high && tiface || (new_warn_alarm_high && first)) {
            phosphor::logging::log<phosphor::logging::level::ERR>(("Warning High threshold alarm, old value causing alarm is " + std::to_string(old_val) + " and new value is " + std::to_string(new_val)).c_str());
            sendPlatformEvent(adc_name, new_warn_alarm_high, event_data1, 0xFF, 0xFF);
            tiface->signal_property("warningAlarmHigh");
        }

        bool old_warn_alarm_low = std::abs(old_val) < std::abs(warnlowthresh);
        bool new_warn_alarm_low = std::abs(new_val) < std::abs(warnlowthresh);
        event_data1 = new_warn_alarm_low ? static_cast<uint8_t>(ThresholdTypeCodes::lnc_high) : static_cast<uint8_t>(ThresholdTypeCodes::lnc_low);
        event_data1 = event_data1 | (1 << 6); //[7:6] reading in byte 2
        event_data1 = event_data1 | (1 << 4); //[5:4] threshold in byte 3
        if (old_warn_alarm_low != new_warn_alarm_low && tiface && old_val != -29.0 && new_val != -29.0 && old_val > -10000 || (new_warn_alarm_low && first && tiface && old_val != -29.0 && new_val != -29.0 && old_val > -10000)) {
            phosphor::logging::log<phosphor::logging::level::ERR>(("Warning Low threshold alarm, old value causing alarm is " + std::to_string(old_val) + " and new value is " + std::to_string(new_val)).c_str());
            sendPlatformEvent(adc_name, new_warn_alarm_low, event_data1, 0xFF, 0xFF);
            tiface->signal_property("warningAlarmLow");
        }
        
        bool old_alarm_high = std::abs(old_val) > std::abs(highthresh);
        bool new_alarm_high = std::abs(new_val) > std::abs(highthresh);
        event_data1 = new_alarm_high ? static_cast<uint8_t>(ThresholdTypeCodes::ucr_high) : static_cast<uint8_t>(ThresholdTypeCodes::ucr_low);
        event_data1 = event_data1 | (1 << 6); //[7:6] reading in byte 2
        event_data1 = event_data1 | (1 << 4); //[5:4] threshold in byte 3
        if (old_alarm_high != new_alarm_high && tiface || (new_alarm_high && first)) {
            phosphor::logging::log<phosphor::logging::level::ERR>(("Critical High threshold alarm, old value causing alarm is " + std::to_string(old_val) + " and new value is " + std::to_string(new_val)).c_str());
            sendPlatformEvent(adc_name, new_alarm_high, event_data1, 0xFF, 0xFF);
            tiface->signal_property("criticalAlarmHigh");
        }

        bool old_alarm_low = std::abs(old_val) < std::abs(lowthresh);
        bool new_alarm_low = std::abs(new_val) < std::abs(lowthresh);
        event_data1 = new_alarm_low ? static_cast<uint8_t>(ThresholdTypeCodes::lcr_high) : static_cast<uint8_t>(ThresholdTypeCodes::lcr_low);
        event_data1 = event_data1 | (1 << 6); //[7:6] reading in byte 2
        event_data1 = event_data1 | (1 << 4); //[5:4] threshold in byte 3
        if (old_alarm_low != new_alarm_low && tiface && old_val != -29.0 && new_val != -29.0 && old_val > -10000 || (new_alarm_low && first && tiface && old_val != -29.0 && new_val != -29.0 && old_val > -10000)) {
            phosphor::logging::log<phosphor::logging::level::ERR>(("Critical Low threshold alarm, old value causing alarm is " + std::to_string(old_val) + " and new value is " + std::to_string(new_val)).c_str());
            sendPlatformEvent(adc_name, new_alarm_low, event_data1, 0xFF, 0xFF);
            tiface->signal_property("criticalAlarmLow");
        }

        first = false;

    });
    // phosphor::logging::log<phosphor::logging::level::INFO>("async_update_bus exit");
}

// Schedule the next periodic update
void ADC_sensor::schedule_update() 
{
    // phosphor::logging::log<phosphor::logging::level::INFO>("schedule_update enter");
    timer.expires_after(std::chrono::milliseconds(10));
    timer.async_wait([this](const boost::system::error_code& ec) {
        if (ec) {
            phosphor::logging::log<phosphor::logging::level::INFO>("ADC_sensor schedule_update async wait failure. failed to schedule update");
            return;
        }
        this->update_readings();
    });
}


void ADC_sensor::sendPlatformEvent(const std::string& dirName, bool assertEvent, uint8_t eventData1, std::optional<uint8_t> eventData2, std::optional<uint8_t> eventData3)
{
    phosphor::logging::log<phosphor::logging::level::INFO>("ipmiSenPlatformEvent enter");
    [[maybe_unused]] uint16_t generatorID = 0;

    //sysgeneratorID, evmRev, sensorType, sensorNum, eventType, eventData1, eventData2, eventData3

    generatorID = (0 << 12)      // Channel
        | (0x0 << 10)             // Reserved
        | ((0 & 0x3) << 8) // Lun
        | (0x10 << 1);

    {// bit 7 of eventType: 0 = assert, 1 = deassert

        std::string sensorPath = "/voltage/" + dirName;

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
