#include "../include/lcr_gpio_mon.hpp"

using json = nlohmann::json;

json readJson(std::string path) {
    std::ifstream f(path);

    json read;

    try {
        f >> read;
    } catch (const nlohmann::json::parse_error& e) {
        std::cerr << "JSON parse error: " << e.what() << std::endl;
    }
    
    return read;
}

bool gpio_chip_exists(const std::string& chip_path) {
    try {
        gpiod::chip chip(chip_path);
        return true;
    } catch (const std::system_error& e) {
        if (e.code().value() == ENOENT ||    // No such file
            e.code().value() == ENODEV ||     // No such device
            e.code().value() == EACCES) {     // Permission denied
            return false;
        }
        throw;
    }
}

GpioMonitor::GpioMonitor(std::shared_ptr<sdbusplus::asio::connection> conn_ref,
        sdbusplus::asio::object_server& server_ref)
        : conn(conn_ref), obj_server(server_ref), timer(conn->get_io_context())
{

    obj_server.add_manager("/xyz/openbmc_project/sensors");
    phosphor::logging::log<phosphor::logging::level::INFO>("GpioMonitor initialization");
    
    configJson = readJson("/usr/share/lcr_configs/bus_devices.json");

    ts = 0.1;
    build_gpio_list();
    initallpins();
    
    expose_gpios();

};

GpioMonitor::~GpioMonitor() {;};

gpio_info GpioMonitor::populate_info(bool input, bool interrupt, int chip, int line, int status, std::string name) {
    gpio_info populated_gpio;
    populated_gpio.input = input;
    populated_gpio.interrupt = interrupt;
    populated_gpio.chip = chip;
    populated_gpio.line = line;
    populated_gpio.status = status;
    populated_gpio.name = name;

    return populated_gpio;
}

gpiod::chip GpioMonitor::find_chip_by_label(const std::string& want)
{
    phosphor::logging::log<phosphor::logging::level::INFO>("find_chip_by_label enter");
    for (auto& c : gpiod::make_chip_iter())
    {
        phosphor::logging::log<phosphor::logging::level::INFO>(("find_chip_by_label on chip " + c.name() + " with label " + c.label()).c_str());
        if (c.label() == want)
            return c;
    }
    gpiod::chip empty_chip;
    return empty_chip;
}

void GpioMonitor::build_gpio_list() {

    phosphor::logging::log<phosphor::logging::level::INFO>("populating gpio list");

    sysreset = populate_info(false, false, 1, 0, 0, std::string("sysreset"));
    nvmro = populate_info(true, false, 2, 0, 0, std::string("nvmro"));
    gdiscrete = populate_info(false, false, 3, 0, 0, std::string("gdiscrete"));
    psenable = populate_info(false, false, 3, 3, 0, std::string("psenable"));
    ps1inh = populate_info(false, false, 3, 2, 0, std::string("ps1inh"));
    ps1fail = populate_info(true, false, 4, 0, 0, std::string("ps1fail"));
    ps2inh = populate_info(false, false, 3, 1, 0, std::string("ps2inh"));
    ps2fail = populate_info(true, false, 4, 1, 0, std::string("ps2fail"));

    available_gpios[nvmro.name] = nvmro;
    available_gpios[sysreset.name] = sysreset;
    available_gpios[ps1fail.name] = ps1fail;
    available_gpios[ps2fail.name] = ps2fail;
    available_gpios[gdiscrete.name] = gdiscrete;
    available_gpios[psenable.name] = psenable;
    available_gpios[ps1inh.name] = ps1inh;
    available_gpios[ps2inh.name] = ps2inh;

    chassis_signals[nvmro.name] = nvmro;
    chassis_signals[sysreset.name] = sysreset;
    chassis_signals[ps1fail.name] = ps1fail;
    chassis_signals[ps2fail.name] = ps2fail;
    chassis_signals[gdiscrete.name] = gdiscrete;
    chassis_signals[psenable.name] = psenable;
    chassis_signals[ps1inh.name] = ps1inh;
    chassis_signals[ps2inh.name] = ps2inh;

    for (auto& gpio_label : configJson["gpioexpanders"].items()) {
        phosphor::logging::log<phosphor::logging::level::INFO>(("gpio chip " + gpio_label.key() + " checking interrupt...").c_str());
        bool interrupt = configJson["gpioexpanders"][gpio_label.key()]["interrupt"].get<bool>();
        if (interrupt) {
            phosphor::logging::log<phosphor::logging::level::INFO>(("gpio chip " + gpio_label.key() + " interrupt is true, getting interrupt pin").c_str());
            interrupt_pins.push_back(configJson["gpioexpanders"][gpio_label.key()]["interrupt_pin"].get<std::string>());
        }
        gpiod::chip gpiochip = find_chip_by_label(gpio_label.key());
        if (!gpiochip) {
            continue;
        }
        std::string gpiochipname = gpiochip.name();
        phosphor::logging::log<phosphor::logging::level::INFO>(("gpiochipname " + gpiochipname + " found").c_str());
        char c_chip = gpiochip.name().back();
        std::string s_chip(1, c_chip);
        int chip = stoi(s_chip);
        if (gpio_chip_exists("/dev/" + gpiochipname)) {
            
            for (auto& gpio : gpio_label.value().items()) {
                if (gpio.key() != "bus" && gpio.key() != "address" && gpio.key() != "interrupt" && gpio.key() != "interrupt_pin") {
                    int line = configJson["gpioexpanders"][gpio_label.key()][gpio.key()].get<int>();
                    phosphor::logging::log<phosphor::logging::level::INFO>(("gpio chip " + gpio_label.key() + " being populated in list").c_str());
                    available_gpios[gpio.key()] = populate_info(false, interrupt, chip, line, 0, gpio.key());
                }
            }
        } else {
            phosphor::logging::log<phosphor::logging::level::INFO>(("gpio chip " + gpio_label.key() + " is not present.").c_str());
        }
    }

    std::unordered_map<int, std::string> output_names = {
        {8,  configJson["nativeGPIOs"]["8"].get<std::string>()},
        {9,  configJson["nativeGPIOs"]["9"].get<std::string>()},
        {10, configJson["nativeGPIOs"]["10"].get<std::string>()},
        {11, configJson["nativeGPIOs"]["11"].get<std::string>()},
        {12, configJson["nativeGPIOs"]["12"].get<std::string>()},
        {13, configJson["nativeGPIOs"]["13"].get<std::string>()},
        {14, configJson["nativeGPIOs"]["14"].get<std::string>()},
        {15, configJson["nativeGPIOs"]["15"].get<std::string>()}
    };

    std::unordered_map<int, std::string> input_names = {
        {0, configJson["nativeGPIOs"]["0"].get<std::string>()},
        {1, configJson["nativeGPIOs"]["1"].get<std::string>()},
        {2, configJson["nativeGPIOs"]["2"].get<std::string>()},
        {3, configJson["nativeGPIOs"]["3"].get<std::string>()},
        {4, configJson["nativeGPIOs"]["4"].get<std::string>()},
        {5, configJson["nativeGPIOs"]["5"].get<std::string>()},
        {6, configJson["nativeGPIOs"]["6"].get<std::string>()},
        {7, configJson["nativeGPIOs"]["7"].get<std::string>()}
    };

    for (const auto& [line, name] : output_names) {
        gpio_info output_gpio = populate_info(false, false, 0, line, 0, name);
        available_gpios[output_gpio.name] = output_gpio;
    }

    for (const auto& [line, name] : input_names) {
        phosphor::logging::log<phosphor::logging::level::INFO>(("input gpio " + name).c_str());
        if (std::find(interrupt_pins.begin(), interrupt_pins.end(), name) == interrupt_pins.end()) {
            gpio_info input_gpio = populate_info(true, false, 0, line, 0, name);
            available_gpios[input_gpio.name] = input_gpio;
            phosphor::logging::log<phosphor::logging::level::INFO>(("input gpio " + name + " populated").c_str());
        }
    }
}

bool GpioMonitor::readgpio(gpio_info* target_gpio)
{
    // phosphor::logging::log<phosphor::logging::level::INFO>("readgpio enter");
    bool ischassissig = false;
    for (auto& pair : chassis_signals) {
        if (pair.second.name == target_gpio->name) {
            ischassissig = true;
            break;
        }
    }
    if (target_gpio->input && (!ischassissig || (target_gpio->name == std::string("nvmro")) || (target_gpio->name == std::string("ps1fail"))
     || (target_gpio->name == std::string("ps2fail")) || (target_gpio->name == configJson["onSwitchGPIO"]["name"]))) {
        try {

            if (target_gpio == nullptr) {
                phosphor::logging::log<phosphor::logging::level::INFO>("readgpio target gpio does not exist");
                return false;
            }
    
            bool ret_check = false;
    
            // Open the GPIO chip
            gpiod::chip chip("gpiochip" + std::to_string(target_gpio->chip));
            
            // Get the line
            gpiod::line line = chip.get_line(target_gpio->line);
            
            // Request line as output
            line.request({"readgpio", gpiod::line_request::DIRECTION_INPUT, 0});
            // phosphor::logging::log<phosphor::logging::level::INFO>("readgpio attempting to get value");
            // Read the current value
            int value = line.get_value();
            
            if (value != target_gpio->status) {
                ret_check = true;
            }
            // Update status
            // phosphor::logging::log<phosphor::logging::level::INFO>("readgpio updating the status");
            target_gpio->status = value;
            line.release();
            // phosphor::logging::log<phosphor::logging::level::INFO>("readgpio exit");
            return ret_check;
            
        } catch (const std::exception& e) {
            phosphor::logging::log<phosphor::logging::level::INFO>("readgpio error, exit");
            return false;
        }
    } else {
        return false;
    }
    
    // phosphor::logging::log<phosphor::logging::level::INFO>("readgpio exit");
}

void GpioMonitor::initallpins() {
    phosphor::logging::log<phosphor::logging::level::INFO>("initallpins enter");

    for (auto& [name, gpio] : available_gpios) {
        bool gpiostatus = readgpio(&gpio);
    }
}

void GpioMonitor::expose_gpios() {
    
    phosphor::logging::log<phosphor::logging::level::INFO>("expose_gpios enter");

    for (const auto& [name, gpio] : available_gpios) {
    
        expose_gpio(gpio);
    
    }

    if (!powerswitch_init) {
        expose_unpopulated_powerswitch();
    } 
    if (!intrusion_init) {
        expose_unpopulated_intrusion();
    } 
    if (!resetbutton_init) {
        expose_unpopulated_resetbutton();
    }
    phosphor::logging::log<phosphor::logging::level::INFO>("expose_gpios exit");
}

void GpioMonitor::expose_powerswitch_gpio(const gpio_info& target_gpio) 
{
    phosphor::logging::log<phosphor::logging::level::INFO>("expose_powerswitch_gpio enter");
    std::string path = "/xyz/openbmc_project/Chassis/Buttons/Power0";
    powerswitch_iface = obj_server.add_unique_interface(path, "xyz.openbmc_project.Chassis.Buttons.Power");
    powerswitch_iface->register_property_r<bool>(
        "Enabled",
        sdbusplus::vtable::property_::emits_change,
        [this, name = target_gpio.name](const bool&) -> bool {
            // Getter: Retrieve from map
            auto it = available_gpios.find(name);
            if (it != available_gpios.end()) {
                return (it->second.status == 1);
            }
            return false;  // Fallback
        }
    );
    powerswitch_iface->initialize();
    powerswitch_init = true;
}

void GpioMonitor::expose_resetbutton_gpio(const gpio_info& target_gpio) 
{
    phosphor::logging::log<phosphor::logging::level::INFO>("expose_resetbutton_gpio enter");
    std::string path = "/xyz/openbmc_project/Chassis/Buttons/Reset0";
    resetbutton_iface = obj_server.add_unique_interface(path, "xyz.openbmc_project.Chassis.Buttons.Reset");
    resetbutton_iface->register_property_r<bool>(
        "Enabled",
        sdbusplus::vtable::property_::emits_change,
        [this, name = target_gpio.name](const bool&) -> bool {
            // Getter: Retrieve from map
            auto it = available_gpios.find(name);
            if (it != available_gpios.end()) {
                return (it->second.status == 1);
            }
            return false;  // Fallback
        }
    );
    resetbutton_iface->initialize();
    resetbutton_init = true;
}

void GpioMonitor::expose_intrusion_gpio(const gpio_info& target_gpio) 
{
    phosphor::logging::log<phosphor::logging::level::INFO>("expose_intrusion_gpio enter");
    std::string path = "/xyz/openbmc_project/Chassis/Intrusion";
    intrusion_iface = obj_server.add_unique_interface(path, "xyz.openbmc_project.Chassis.Intrusion");
    intrusion_iface->register_property_r<bool>(
        "Status",
        sdbusplus::vtable::property_::emits_change,
        [this, name = target_gpio.name](const bool&) -> bool {
            // Getter: Retrieve from map
            auto it = available_gpios.find(name);
            if (it != available_gpios.end()) {
                return (it->second.status == 1);
            }
            return false;  // Fallback
        }
    );
    intrusion_iface->initialize();
    intrusion_init = true;
}


void GpioMonitor::expose_unpopulated_powerswitch() 
{
    phosphor::logging::log<phosphor::logging::level::INFO>("expose_unpopulated_powerswitch enter");
    std::string path = "/xyz/openbmc_project/Chassis/Buttons/Power0";
    powerswitch_iface = obj_server.add_unique_interface(path, "xyz.openbmc_project.Chassis.Buttons.Power");
    powerswitch_iface->register_property_r<bool>(
        "Enabled",
        sdbusplus::vtable::property_::emits_change,
        [this](const bool&) -> bool {
            return false;
        }
    );
    powerswitch_iface->initialize();
    powerswitch_init = true;
}

void GpioMonitor::expose_unpopulated_resetbutton() 
{
    phosphor::logging::log<phosphor::logging::level::INFO>("expose_unpopulated_resetbutton enter");
    std::string path = "/xyz/openbmc_project/Chassis/Buttons/Reset0";
    resetbutton_iface = obj_server.add_unique_interface(path, "xyz.openbmc_project.Chassis.Buttons.Reset");
    resetbutton_iface->register_property_r<bool>(
        "Enabled",
        sdbusplus::vtable::property_::emits_change,
        [this](const bool&) -> bool {
            return false;
        }
    );
    resetbutton_iface->initialize();
    resetbutton_init = true;
}

void GpioMonitor::expose_unpopulated_intrusion() 
{
    phosphor::logging::log<phosphor::logging::level::INFO>("expose_unpopulated_intrusion enter");
    std::string path = "/xyz/openbmc_project/Chassis/Intrusion";
    intrusion_iface = obj_server.add_unique_interface(path, "xyz.openbmc_project.Chassis.Intrusion");
    intrusion_iface->register_property_r<bool>(
        "Status",
        sdbusplus::vtable::property_::emits_change,
        [this](const bool&) -> bool {
            return false;
        }
    );
    intrusion_iface->initialize();
    intrusion_init = true;
}

void GpioMonitor::expose_gpio(const gpio_info& target_gpio) 
{
    phosphor::logging::log<phosphor::logging::level::INFO>(("expose_gpio enter, name: " + target_gpio.name).c_str());
    if (target_gpio.name == configJson["onSwitchGPIO"]["name"].get<std::string>()) {
        expose_powerswitch_gpio(target_gpio);
    } else if (target_gpio.name == configJson["resetSwitchGPIO"]["name"].get<std::string>()) {
        expose_resetbutton_gpio(target_gpio);
    } else if (target_gpio.name == configJson["intrusionSensorGPIO"]["name"].get<std::string>()) {
        expose_intrusion_gpio(target_gpio);
    }

    std::string path = "/xyz/openbmc_project/gpio/" + target_gpio.name;
    std::unique_ptr<sdbusplus::asio::dbus_interface> value_iface = obj_server.add_unique_interface(path, "xyz.openbmc_project.Sensor.Value");
    std::unique_ptr<sdbusplus::asio::dbus_interface> assoc_iface = obj_server.add_unique_interface(path, "xyz.openbmc_project.Association.Definitions");

    value_iface->register_property_rw<int>(
        "Value",
        sdbusplus::vtable::property_::emits_change,
        [this, name = target_gpio.name](const int& requested_value, int& current_value) -> bool {
            // Setter: Validate/update stored status
            auto it = available_gpios.find(name);
            if (it == available_gpios.end()) {
                // Invalid GPIO - reject the set
                return false;
            }
    
            if (requested_value == current_value) {
                // No change - don't emit signal
                return true;
            }
    
            // Update internal D-Bus value and map
            current_value = requested_value;
            it->second.status = requested_value;

            // Success with change - emit signal
            return true;
        },
        [this, name = target_gpio.name](const int&) -> int {
            // Getter: Retrieve from map
            auto it = available_gpios.find(name);
            if (it != available_gpios.end()) {
                return it->second.status;
            }
            return -1;  // Fallback
        }
    );
    
    value_iface->register_property_r<int>(
        "Chip",
        sdbusplus::vtable::property_::const_,
        [this, name = target_gpio.name](const int&) -> int {
            // Getter: Retrieve from map
            auto it = available_gpios.find(name);
            if (it != available_gpios.end()) {
                return it->second.chip;
            }
            return -1;  // Fallback
        }
    );
    value_iface->register_property_r<int>(
        "Line",
        sdbusplus::vtable::property_::const_,
        [this, name = target_gpio.name](const int&) -> int {
            // Getter: Retrieve from map
            auto it = available_gpios.find(name);
            if (it != available_gpios.end()) {
                return it->second.line;
            }
            return -1;  // Fallback
        }
    );

    value_iface->register_property_r<bool>(
        "Input",
        sdbusplus::vtable::property_::const_,
        [this, name = target_gpio.name](const bool&) -> bool {
            // Getter: Retrieve from map
            auto it = available_gpios.find(name);
            if (it != available_gpios.end()) {
                return it->second.input;
            }
            return -1;  // Fallback
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
    assoc_iface->initialize();

    m_value_iface[target_gpio.name] = std::move(value_iface);
    m_assoc_iface[target_gpio.name] = std::move(assoc_iface);
    phosphor::logging::log<phosphor::logging::level::INFO>("expose_gpio exit");
}

void GpioMonitor::async_readgpio(const std::string& gpio_name, std::function<void(bool, int)> callback) {  // Pass name, callback success and new_status
    // phosphor::logging::log<phosphor::logging::level::INFO>("async_readgpio enter");
    boost::asio::post(timer.get_executor(), [this, gpio_name, callback]() {
        int new_status = -1;  // Error value
        try {
            auto it = available_gpios.find(gpio_name);
            if (it == available_gpios.end()) {
                throw std::runtime_error("GPIO not found");
            }
            gpio_info* gpio_ptr = &it->second;  // Pointer to modifiable gpio_info in map
            bool changed = readgpio(gpio_ptr);  // Call sync, which updates status and returns if changed
            new_status = gpio_ptr->status;  // Get updated status
            callback(changed, new_status);
        } catch (const std::exception& e) {
            phosphor::logging::log<phosphor::logging::level::ERR>(("Failed to read GPIO " + gpio_name).c_str());
            callback(false, new_status);
        }
    });
    // phosphor::logging::log<phosphor::logging::level::INFO>("async_readgpio exit");
}

void GpioMonitor::async_readallpins() {
    // phosphor::logging::log<phosphor::logging::level::INFO>("async_readallpins enter");

    if (available_gpios.empty()) {
        schedule_update();  // Nothing to do; schedule next
        return;
    }

    // Chain async reads over map (use vector of keys for ordering if needed; here assuming order doesn't matter)
    std::vector<std::string> keys;
    for (const auto& kv : available_gpios) { keys.push_back(kv.first); }

    auto idx = std::make_shared<size_t>(0);
    auto check_next_ptr = std::make_shared<std::function<void()>>();
    *check_next_ptr = [this, keys, idx, check_next_ptr]() mutable {
        if (*idx >= keys.size()) {
            // All done; schedule next update
            schedule_update();
            return;
        }
        const std::string& gpio_name = keys[*idx];
        ++(*idx);

        async_readgpio(gpio_name, [this, check_next_ptr, gpio_name](bool change, int new_status) {
            if (change) {
                // phosphor::logging::log<phosphor::logging::level::INFO>("async_readallpins change seen, attempting to update bus");
                async_update_bus(gpio_name, new_status);  // Emit always, or re-check
            }
            (*check_next_ptr)();  // Proceed to next GPIO
        });
    };
    (*check_next_ptr)();  // Start chain

    // phosphor::logging::log<phosphor::logging::level::INFO>("async_readallpins exit");
}

void GpioMonitor::async_update_bus(const std::string& gpio_name, bool new_status) {
    
    // phosphor::logging::log<phosphor::logging::level::INFO>("async_update_bus enter");
    auto it = m_value_iface.find(gpio_name);
    if (it == m_value_iface.end() || !it->second) {
        return;  // No interface for this GPIO
    }

    // Emit change signal (async via post to integrate with Asio loop)
    boost::asio::post(timer.get_executor(), [this, iface = it->second.get(), gpio_name, new_status]() {

        // phosphor::logging::log<phosphor::logging::level::INFO>("GPIO status changed", phosphor::logging::entry("NAME=%s", gpio_name.c_str()), phosphor::logging::entry("NEW_STATUS=%d", new_status));
        if (iface) {
            // phosphor::logging::log<phosphor::logging::level::INFO>(("async_update_bus attempting to update bus for " + gpio_name).c_str());
            iface->signal_property("Value");
        }
    });
    // phosphor::logging::log<phosphor::logging::level::INFO>("async_update_bus exit");
}

// Schedule the next periodic update
void GpioMonitor::schedule_update() {
    timer.expires_after(std::chrono::milliseconds(1000));
    timer.async_wait([this](const boost::system::error_code& ec) {
        if (ec) {
            phosphor::logging::log<phosphor::logging::level::INFO>("GpioMonitor schedule_update async wait failure. failed to schedule update");
            return;
        }
        this->async_readallpins();
    });
}

double GpioMonitor::get_ts() {
    phosphor::logging::log<phosphor::logging::level::INFO>("getting ts");
    return ts;
}
