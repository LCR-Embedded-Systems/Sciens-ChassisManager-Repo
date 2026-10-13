#include "../include/lcr_gpio.hpp"

using json = nlohmann::json;

bool gpio_chip_exists(const std::string& chip_path) 
{
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

json readJson(std::string path) 
{
    std::ifstream f(path);

    json read;

    try {
        f >> read;
    } catch (const nlohmann::json::parse_error& e) {
        std::cerr << "JSON parse error: " << e.what() << std::endl;
    }
    
    return read;
}

CLI::CLI() : bus(sdbusplus::bus::new_default()) 
{
    build_gpio_list();
    configJson = readJson("/usr/share/lcr_configs/bus_devices.json");
};

CLI::~CLI() {;};

gpio_info CLI::populate_info(bool input, int chip, int line, int status, std::string name) 
{
    gpio_info populated_gpio;
    populated_gpio.input = input;
    populated_gpio.chip = chip;
    populated_gpio.line = line;
    populated_gpio.status = status;
    populated_gpio.name = name;

    return populated_gpio;
}

void CLI::build_gpio_list() 
{

    std::string path = "/xyz/openbmc_project/gpio";
    std::string interface = "xyz.openbmc_project.Sensor.Value";

    for (auto& gpiopath : phosphor::interface::util::getSubTreePathsRaw(bus, path, interface, 0)){
        int chip_value = phosphor::interface::util::getProperty<int>(bus, gpiopath, interface, "Chip");
        int line_value = phosphor::interface::util::getProperty<int>(bus, gpiopath, interface, "Line");
        bool input_value = phosphor::interface::util::getProperty<bool>(bus, gpiopath, interface, "Input");
        auto itr = gpiopath.rfind("/");
        std::string gpioName;
        if (itr != std::string::npos && itr < gpiopath.size())
        {
            gpioName = gpiopath.substr(1 + itr);
        }
        gpio_info newgpio = populate_info(input_value, chip_value, line_value, 0, gpioName);

        available_gpios.push_back(newgpio);
    }

    sysreset =     populate_info(false, 1, 0, 0, std::string("sysreset"));
    nvmro =        populate_info(true, 2, 0, 0, std::string("nvmro"));
    gdiscrete =    populate_info(false, 3, 0, 0, std::string("gdiscrete"));
    psenable =     populate_info(false, 3, 3, 0, std::string("psenable"));
    ps1inh =       populate_info(false, 3, 2, 0, std::string("ps1inh"));
    ps1fail =      populate_info(true, 4, 0, 0, std::string("ps1fail"));
    ps2inh =       populate_info(false, 3, 1, 0, std::string("ps2inh"));
    ps2fail =      populate_info(true, 4, 1, 0, std::string("ps2fail"));

    chassis_gpios.push_back(sysreset);
    chassis_gpios.push_back(nvmro);
    chassis_gpios.push_back(gdiscrete);
    chassis_gpios.push_back(psenable);
    chassis_gpios.push_back(ps1inh);
    chassis_gpios.push_back(ps1fail);
    chassis_gpios.push_back(ps2inh);
    chassis_gpios.push_back(ps2fail);
}

bool CLI::ischassisgpio(gpio_info* target_gpio) 
{
    for (auto& gpio : chassis_gpios) {
        if (gpio.name == target_gpio->name) {
            return true;
        }
    }
    return false;
}

gpio_info* CLI::get_gpio_by_name(const std::string& name) 
{
    for (auto& gpio : available_gpios) {
        if (gpio.name == name) {
            return &gpio;
        }
    }
    return nullptr;
}

gpio_info* CLI::get_chassis_gpio_by_name(const std::string& name) 
{
    for (auto& gpio : chassis_gpios) {
        if (gpio.name == name) {
            return &gpio;
        }
    }
    return nullptr;
}

void CLI::display_options()
{
    std::cout << "Available commands are: " << std::endl;
    std::cout << "\tlist : list all pins and their direction" << std::endl;
    std::cout << "\tget : get the status of an output pin or the reading of an input pin" << std::endl;
    std::cout << "\t\tusage : lcrpin get [PIN NAME]" << std::endl;
    std::cout << "\tset : set an output pin to either 1 or 0" << std::endl;
    std::cout << "\t\tusage : lcrpin set [PIN NAME] [1 OR 0]" << std::endl;
    std::cout << "\tall : retrieve the status of all pins, both input and output" << std::endl;
}

void CLI::display_PINs()
{
    std::cout << "Input pins:" << std::endl;

    for (auto& gpio : available_gpios) {
        if (gpio.input) {
            std::cout << "\tName: " << gpio.name << std::endl;
        }
    }

    std::cout << "Output pins:" << std::endl;

    for (auto& gpio : available_gpios) {
        if (gpio.input == false) {
            std::cout << "\tName: " << gpio.name << std::endl;
        }
    }
}

int CLI::process_argument(int nargs, char* args[]) 
{

    if (nargs > 1) {
        std::string argument = args[1];
        if (argument == "set" || argument == "Set") {
            if (nargs == 4) {
                std::string cname = args[2];
                std::string csetting = args[3];
                gpio_info* target_gpio = get_gpio_by_name(cname);
                if (target_gpio == nullptr) {
                    target_gpio = get_chassis_gpio_by_name(cname);
                }
                if (target_gpio == nullptr) {
                    std::cout << "gpio name does not exist." << std::endl;
                    return -1;
                }
                int cvalue = std::stoi(csetting);
                int getv;
                if (ischassisgpio(target_gpio)) {
                    getv = setdbusgpio(target_gpio, cvalue);
                } else {
                    getv = setgpio(target_gpio, cvalue);
                }
            } else {
                std::cout << "Incorrect number of arguments" << std::endl;
            }
        } else if (argument == "get" || argument == "Get") {
            if (nargs == 3) {
                std::string cname = args[2];
                gpio_info* target_gpio = get_gpio_by_name(cname);
                if (target_gpio == nullptr) {
                    target_gpio = get_chassis_gpio_by_name(cname);
                }
                if (target_gpio == nullptr) {
                    std::cout << "gpio name does not exist." << std::endl;
                    return -1;
                }
                int getv;
                if (ischassisgpio(target_gpio)) {
                    getv = readdbusgpio(target_gpio);
                } else {
                    getv = readgpio(target_gpio);
                }
                
                std::cout << "Pin " << target_gpio->name << " is " << getv << std::endl;
            } else {
                std::cout << "Incorrect number of arguments" << std::endl;
            }
        
        } else if (argument == "list" || argument == "List") {
            display_PINs();
        } else if (argument == "help" || argument == "Help") {
            display_options();
        } else if (argument == "all" || argument == "All") {
            readallpins();
        } else {
            std::cout << "Unknown command." << std::endl;
            display_options();
        }
    }
    return 0;
};

int CLI::setgpio(gpio_info* target_gpio, int value)
{
    try {
        
        if (target_gpio == nullptr) {
            std::cerr << "Error: GPIO is null" << std::endl;
            return -1;
        }
        
        // Check if GPIO is configured as output
        if (target_gpio->input) {
            std::cerr << "Error: GPIO '" << target_gpio->name << "' is configured as input" << std::endl;
            return -1;
        }
        
        if (value != 0 && value != 1) {
            std::cerr << "Error: Invalid value. Must be 0 or 1" << std::endl;
            return -1;
        }
        
        // Open the GPIO chip
        gpiod::chip chip("gpiochip" + std::to_string(target_gpio->chip));
        
        // Get the line
        gpiod::line line = chip.get_line(target_gpio->line);
        
        // Request line as output
        line.request({"setgpio", gpiod::line_request::DIRECTION_OUTPUT, 0}, value);
        
        // Update status
        target_gpio->status = value;
        line.release();
        std::cout << "GPIO '" << target_gpio->name << "' set to " << value << std::endl;
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return -1;
    }
}

int CLI::setdbusgpio(gpio_info* target_gpio, int value)
{
    try {
        
        if (target_gpio == nullptr) {
            std::cerr << "Error: GPIO is null" << std::endl;
            return -1;
        }
        
        // Check if GPIO is configured as output
        if (target_gpio->input) {
            std::cerr << "Error: GPIO '" << target_gpio->name << "' is configured as input" << std::endl;
            return -1;
        }
        
        if (value != 0 && value != 1) {
            std::cerr << "Error: Invalid value. Must be 0 or 1" << std::endl;
            return -1;
        }
        
        std::string name = target_gpio->name;
        
        std::string path = "/xyz/openbmc_project/gpio/" + name;
        std::string interface = "xyz.openbmc_project.Sensor.Value";
        std::string property = "Value";
        phosphor::interface::util::setProperty<int>(bus, path, interface, property, std::move(value));
        
        // Open the GPIO chip
        gpiod::chip chip("gpiochip" + std::to_string(target_gpio->chip));
        
        // Get the line
        gpiod::line line = chip.get_line(target_gpio->line);
        
        // Request line as output
        line.request({"setgpio", gpiod::line_request::DIRECTION_OUTPUT, 0}, value);

        // Update status
        target_gpio->status = value;
        line.release();
        // std::cout << "GPIO '" << target_gpio->name << "' set to " << value << " in the dbus" << std::endl;
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return -1;
    }
}

int CLI::readgpio(gpio_info* target_gpio)
{
    try {

        if (target_gpio == nullptr) {
            std::cerr << "Error: GPIO is null" << std::endl;
            return -1;
        }

        // Open the GPIO chip
        gpiod::chip chip("gpiochip" + std::to_string(target_gpio->chip));
        
        // Get the line
        gpiod::line line = chip.get_line(target_gpio->line);
        
        // Request line as output
        line.request({"readgpio", gpiod::line_request::DIRECTION_INPUT, 0});
        
        // Read the current value
        int value = line.get_value();
        
        // Update status
        target_gpio->status = value;
        line.release();

        return value;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return -1;
    }
}

int CLI::readdbusgpio(gpio_info* target_gpio) 
{
    try {

        if (target_gpio == nullptr) {
            std::cerr << "Error: GPIO is null" << std::endl;
            return -1;
        }

        std::string name = target_gpio->name;
        
        std::string path = "/xyz/openbmc_project/gpio/" + name;
        std::string interface = "xyz.openbmc_project.Sensor.Value";
        std::string property = "Value";
        int value = phosphor::interface::util::getProperty<int>(bus, path, interface, property);
        
        // Update status
        target_gpio->status = value;
        
        return value;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return -1;
    }
}

void CLI::readallpins() 
{
    std::cout << "Input pins:" << std::endl;

    for (auto& gpio : available_gpios) {
        if (gpio.input) {
            int gpiostatus = readgpio(&gpio);
            std::cout << "\tName: " << gpio.name << ", Status: " << gpiostatus << std::endl;
        }
    }

    std::cout << "Output pins:" << std::endl;

    for (auto& gpio : available_gpios) {
        if (gpio.input == false) {
            int gpiostatus = readgpio(&gpio);
            std::cout << "\tName: " << gpio.name << ", Status: " << gpiostatus << std::endl;
        }
    }

    std::cout << "Chassis signals:" << std::endl;
    for (auto& gpio : chassis_gpios) {
        int gpiostatus = readdbusgpio(&gpio);
        std::cout << "\tName: " << gpio.name << ", Status: " << gpiostatus << std::endl;
    }

}