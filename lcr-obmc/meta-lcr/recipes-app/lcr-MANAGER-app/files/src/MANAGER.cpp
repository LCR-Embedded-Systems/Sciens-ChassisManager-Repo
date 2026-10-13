#include "../include/MANAGER.hpp"

std::ofstream boot_dbgfile("boot_lcr.dbg.log", std::ios::app);
std::ofstream srstatus_file("/usr/share/sr_status", std::ios::out | std::ios::trunc);
std::ofstream system_logs("system_logs.log", std::ios::app);

const size_t MAX_SIZE = 256 * 1024;  // 256 kB in bytes

typedef std::tuple<int, uint8_t, uint8_t, uint8_t, uint8_t, std::vector<uint8_t>> ipmb_response_return_type_t;

/*
* sendIPMBRequest (repurposed from vita 4611 implementation in phosphor-ipmi-host)
* This function sends a IPMI request over i2c bus
* using the d-bus method sendRequest provided by ipmbbridge
* Channel :- Channel initialized by ipmbbridged based on its
        JSON script

* netfn , lun , cmd are copied in the IPMI request on the i2c bus
* data (optional depends on above) is copied to IPMI request
*
* If the response fails (or timesout ) nullopt is returned. 
* caller can optionally look at the cc value that is returned
* as an output parameter
*/
std::optional<ipmb_response_return_type_t> sendIPMBRequest(
    [[maybe_unused]] const uint8_t channel, //either 0 or 4, 4 -> ipmb b, 0 -> ipmb a
    const uint8_t addr,
    const uint8_t netfn, 
    const uint8_t lun, 
    const uint8_t cmd, 
    std::vector<uint8_t> &data,
    uint8_t *ipmi_response_cc, /* out */
    bool verbose
)
{
    
    if (verbose) {
        std::ostringstream oss;
        oss << "Sending IPMB request with the following data:\n"
            << "\tchannel: " << static_cast<int>(channel) << "\n"
            << "\taddress: " << static_cast<int>(addr) << "\n"
            << "\tnetfn: "   << static_cast<int>(netfn) << "\n"
            << "\tlun: "     << static_cast<int>(lun) << "\n"
            << "\tcmd: "     << static_cast<int>(cmd) << "\n"
            << "\tdata: ";

        for (const auto& byte : data) {
            oss << "0x" << std::hex << std::uppercase << static_cast<int>(byte) << " ";
        }
        oss << std::dec << std::nouppercase;

        if (channel == 4) {
            oss << "\nLCR:IPMID: using ipmb b";
        } else if (channel == 0) {
            oss << "\nLCR:IPMID: using ipmb a";
        }

        // Log the full detailed message
        phosphor::logging::log<phosphor::logging::level::INFO>(
            oss.str().c_str()
        );
    }

    static int ipmb_flip = 0;
    bool ipmb_a;
    bool ipmb_b;
    
    try {
        std::ifstream file("/usr/share/ipmb_a_enable");
        if (file.is_open()) {
            int flag_value = 0;
            if (file >> flag_value) {
                if (flag_value == 1) {
                    ipmb_a = true;  
                } else {
                    ipmb_a = false;
                }
            }
            file.close();
        }
    } catch (const std::exception& e) {
        phosphor::logging::log<phosphor::logging::level::ERR>(
            "Failed to read ipmb_a_enable file",
            phosphor::logging::entry("ERROR=%s", e.what()));
    }

    try {
        std::ifstream file("/usr/share/ipmb_b_enable");
        if (file.is_open()) {
            int flag_value = 0;
            if (file >> flag_value) {
                if (flag_value == 1) {
                    ipmb_b = true;  
                } else {
                    ipmb_b = false;
                }
            }
            file.close();
        }
    } catch (const std::exception& e) {
        phosphor::logging::log<phosphor::logging::level::ERR>(
            "Failed to read ipmb_b_enable file",
            phosphor::logging::entry("ERROR=%s", e.what()));
    }

    uint8_t channel_sender = 0x00;

    try
    {
        auto bus = sdbusplus::bus::new_default();
        // Prepare the DBus method call
        auto method = bus.new_method_call(
            "xyz.openbmc_project.Ipmi.Channel.Ipmb",       // Service
            "/xyz/openbmc_project/Ipmi/Channel/Ipmb",      // Object path
            "org.openbmc.Ipmb",                            // Interface
            "sendRequest");                                // Method

        // Append parameters
        method.append(channel_sender, addr, netfn, lun, cmd, data);

        // Call the method synchronously
        constexpr uint64_t T3_TIMEOUT_US = 250'000;
        auto reply = bus.call(method, T3_TIMEOUT_US);

        bool retrysuccess = false;

        int ipmb_flip_retry = ipmb_flip ^ 1;
        //Rule 5.1.1-9, retry with timeout every time until
        if (reply.is_method_error())
        {
            for (int retries = 0; retries < 5; retries++) {
                /*
                Rule 5.1.1-6 When IPMB-B is implemented, a VITA 46.11 compliant ChMC that is unable to successfully 
                transmit an IPMI message across one of its System IPMB interfaces shall attempt to resend the 
                unsuccessfully transmitted IPMI message over its alternate IPMB interface.
                */
                phosphor::logging::log<phosphor::logging::level::INFO>("LCR:IPMID: retry ipmb message with alternate ipmb per Rule 5.1.1-6");
                auto method_retry = bus.new_method_call(
                        "xyz.openbmc_project.Ipmi.Channel.Ipmb",       // Service
                        "/xyz/openbmc_project/Ipmi/Channel/Ipmb",      // Object path
                        "org.openbmc.Ipmb",                            // Interface
                        "sendRequest");                                // Method

                // Append parameters
                
                channel_sender = static_cast<uint8_t>(ipmb_flip_retry * 4);
                method_retry.append(channel_sender, addr, netfn, lun, cmd, data);

                ipmb_flip_retry ^= 1;
                reply = bus.call(method_retry, T3_TIMEOUT_US);

                if (!reply.is_method_error()) {
                    retrysuccess = true;
                    break;
                }
            }
            if (!retrysuccess) { return std::nullopt; }
        }

        std::tuple<int, uint8_t, uint8_t, uint8_t, uint8_t, std::vector<uint8_t>> result;
        uint8_t cc;

        reply.read(result);
        cc = std::get<4>(result);

        if (!cc) {
            // success
            *ipmi_response_cc = cc;
            return result;
        }
        else {
            *ipmi_response_cc = cc;
            return std::nullopt;
        }
    }
    catch (const std::exception& e)
    {
        *ipmi_response_cc = 0xFF; /* TODO revisit this later */
        return std::nullopt;
    }

    /* We should never come here since the try catch should catch everything */
    *ipmi_response_cc = 0xFF;
    return std::nullopt;
}

// Check if D-Bus system is ready
bool isDBusReady() 
{
    sd_bus *bus = NULL;
    sd_bus_error error = SD_BUS_ERROR_NULL;
    sd_bus_message *msg = NULL;
    
    int r = sd_bus_open_system(&bus);
    if (r < 0) {
        return false;
    }
    
    // Try a simple ping to systemd
    r = sd_bus_call_method(bus,
                          "org.freedesktop.systemd1",
                          "/org/freedesktop/systemd1",
                          "org.freedesktop.systemd1.Manager",
                          "ListUnits",
                          &error,
                          &msg,
                          "");
    
    sd_bus_error_free(&error);
    if (msg) sd_bus_message_unref(msg);
    sd_bus_unref(bus);
    
    return r >= 0;
}

// Helper function to check if a systemd service is active
bool isServiceActive(const std::string& serviceName) 
{
    const int max_retries = 5;
    const int base_delay_ms = 1000;  // Longer base delay
    const int max_delay_ms = 5000;
    
    // First, wait for D-Bus to be ready
    int dbus_wait_attempts = 0;
    const int max_dbus_wait = 10;
    
    while (!isDBusReady() && dbus_wait_attempts < max_dbus_wait) {
        phosphor::logging::log<phosphor::logging::level::INFO>("isServiceActive: Waiting for DBus to become ready");
        std::this_thread::sleep_for(std::chrono::milliseconds(3000));
        dbus_wait_attempts++;
    }
    
    if (dbus_wait_attempts >= max_dbus_wait) {
        phosphor::logging::log<phosphor::logging::level::INFO>("isServiceActive: D-Bus did not become ready, proceeding anyway");
    }
    
    for (int attempt = 1; attempt <= max_retries; ++attempt) {
        sd_bus *bus = NULL;
        sd_bus_error error = SD_BUS_ERROR_NULL;
        sd_bus_message *msg = NULL;
        char *state = NULL;
        std::string unit_path;
        int r;

        try {
            // Connect to the system bus
            r = sd_bus_open_system(&bus);
            if (r < 0) {
                throw std::runtime_error("Failed to connect to system bus: " + 
                    std::string(strerror(-r)));
            }

            // Set a timeout on the bus connection
            sd_bus_set_method_call_timeout(bus, 5000000);  // 5 second timeout

            r = sd_bus_call_method(bus,
                                  "org.freedesktop.systemd1",
                                  "/org/freedesktop/systemd1",
                                  "org.freedesktop.systemd1.Manager",
                                  "LoadUnit",
                                  &error,
                                  &msg,
                                  "s",
                                  serviceName.c_str());
            
            if (r < 0) {
                throw std::runtime_error("Failed to get unit: " + 
                    std::string(error.message ? error.message : strerror(-r)));
            }

            const char *path;
            r = sd_bus_message_read(msg, "o", &path);
            if (r < 0) {
                throw std::runtime_error("Failed to read unit path: " + 
                    std::string(strerror(-r)));
            }

            unit_path = std::string(path);
            sd_bus_message_unref(msg);
            msg = NULL;

            // Get the ActiveState property
            r = sd_bus_get_property_string(bus,
                                           "org.freedesktop.systemd1",
                                           unit_path.c_str(),
                                           "org.freedesktop.systemd1.Unit",
                                           "ActiveState",
                                           &error,
                                           &state);
            
            if (r < 0) {
                throw std::runtime_error("Failed to get ActiveState: " + 
                    std::string(error.message ? error.message : strerror(-r)));
            }

            bool isActive = (strcmp(state, "active") == 0);
            free(state);
            
            // Cleanup on success
            sd_bus_error_free(&error);
            if (msg) sd_bus_message_unref(msg);
            sd_bus_unref(bus);
            
            return isActive;
            
        } catch (const std::exception& e) {
            // Log error
            phosphor::logging::log<phosphor::logging::level::INFO>("isServiceActive: Error checking service");
            boot_dbgfile << "[" << currentTimestamp() << "] " 
                        << "Attempt " << attempt << " error checking service " 
                        << serviceName << ": " << e.what() << std::endl;
            
            // Cleanup on failure
            sd_bus_error_free(&error);
            if (msg) sd_bus_message_unref(msg);
            if (bus) sd_bus_unref(bus);
            if (state) free(state);
            
            if (attempt < max_retries) {
                // Progressive backoff with cap
                int delay = std::min(base_delay_ms * attempt, max_delay_ms);
                std::this_thread::sleep_for(std::chrono::milliseconds(delay));
            }
        }
    }
    
    phosphor::logging::log<phosphor::logging::level::INFO>("isServiceActive: Failed to check service");
    
    return false;
}

void recover_log_offsets(int fd, size_t mtd_size, size_t erase_size,
                         off_t& oldest_offset, off_t& write_offset)
{
    oldest_offset = 0;
    write_offset = 0;

    std::vector<std::pair<off_t, off_t>> dirty_regions;

    for (off_t block = 0; block < static_cast<off_t>(mtd_size); block += erase_size) {
        std::vector<uint8_t> buf(erase_size);
        if (lseek(fd, block, SEEK_SET) < 0) break;
        ssize_t rb = read(fd, buf.data(), erase_size);
        if (rb != static_cast<ssize_t>(erase_size)) break;

        off_t first = -1, last = -1;
        for (size_t i = 0; i < erase_size; ++i) {
            if (buf[i] != 0xFF) {
                if (first == -1) first = i;
                last = i;
            }
        }
        if (first != -1) {
            off_t rstart = block + first;
            off_t rend   = block + last + 1;

            if (!dirty_regions.empty()) {
                auto& prev = dirty_regions.back();
                if (rstart <= prev.second) {
                    prev.second = std::max(prev.second, rend);
                } else {
                    dirty_regions.emplace_back(rstart, rend);
                }
            } else {
                dirty_regions.emplace_back(rstart, rend);
            }
        }
    }

    if (dirty_regions.empty()) {
        oldest_offset = 0;
        write_offset = 0;
        return;
    }

    if (dirty_regions.size() == 1) {
        oldest_offset = dirty_regions[0].first;
        write_offset  = dirty_regions[0].second;
    } else if (dirty_regions.size() == 2) {
        boot_dbgfile << "[" << currentTimestamp() << "] LCR:BOOT: dirty_regions is 2" << std::endl;
        phosphor::logging::log<phosphor::logging::level::INFO>("recover_log_offsets: dirty_regions is 2");
        auto& r_low  = dirty_regions[0];
        auto& r_high = dirty_regions[1];

        if (r_low.first < erase_size && r_high.second > mtd_size - erase_size) {
            oldest_offset = r_high.first;

            int fd2 = open("/dev/mtd4", O_RDONLY);
            if (fd2 >= 0) {
                size_t low_len = r_low.second - r_low.first;
                std::vector<uint8_t> buf(low_len);
                lseek(fd2, r_low.first, SEEK_SET);
                read(fd2, buf.data(), low_len);
                close(fd2);

                off_t best = r_low.first;   // at minimum the start of low region

                // Walk the buffer looking for valid lines: [....]\n
                size_t pos = 0;
                while (pos < low_len) {
                    // find next '['
                    auto it = std::find(buf.begin() + pos, buf.end(), '[');
                    if (it == buf.end()) break;
                    size_t start = it - buf.begin();

                    // find next '\n' after it
                    auto nl_it = std::find(buf.begin() + start, buf.end(), '\n');
                    if (nl_it == buf.end()) break;
                    size_t nl = nl_it - buf.begin();

                    size_t line_len = nl - start;
                    if (line_len > 0 && line_len < 1024 && buf[start] == '[') {
                        // valid line - advance best past its '\n'
                        best = r_low.first + nl + 1;
                    }
                    pos = nl + 1;
                }
                write_offset = best;
                phosphor::logging::log<phosphor::logging::level::INFO>(("recover_log_offsets: best write_offset: " + std::to_string(write_offset)).c_str());
            } else {
                write_offset = r_low.second;   // fallback
                phosphor::logging::log<phosphor::logging::level::INFO>(("recover_log_offsets: r_low.second write_offset: " + std::to_string(write_offset)).c_str());
            }
        } else {
            phosphor::logging::log<phosphor::logging::level::INFO>("recover_log_offsets: unexpected region layout, starting fresh");
            oldest_offset = 0;
            write_offset = 0;
        }
    }
}

nlohmann::json Manager::readJson(std::string path) 
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

Manager::Manager(bool simflag) : sim(simflag)
{
    phosphor::logging::log<phosphor::logging::level::INFO>("Manager init enter");
    srstatus_file.seekp(0);
    srstatus_file << 0 << std::endl;
    srstatus_file.flush();
    move_config();
    fs::path end0_source_file = "/lib/systemd/network/60-phosphor-networkd-end0.network";
    fs::path end1_source_file = "/lib/systemd/network/60-phosphor-networkd-end1.network";
    fs::path target_dir = "/var/persistent";
    fs::path end0_destination = target_dir / end0_source_file.filename();
    fs::path end1_destination = target_dir / end1_source_file.filename();
    
    if (!fs::exists(end0_destination)) {
        fs::copy_file(end0_source_file, end0_destination, fs::copy_options::overwrite_existing);    
    }
    if (!fs::exists(end1_destination)) {
        fs::copy_file(end1_source_file, end1_destination, fs::copy_options::overwrite_existing);    
    }
    
    fs::copy_file(end0_destination, end0_source_file, fs::copy_options::overwrite_existing);
    fs::copy_file(end1_destination, end1_source_file, fs::copy_options::overwrite_existing);

    phosphor::logging::log<phosphor::logging::level::INFO>("Manager init reading json");
    configJson = readJson("/usr/share/lcr_configs/bus_devices.json");

    ADCpaths paths;

    std::string stub = "/sys/bus/iio/devices/";
    

    phosphor::logging::log<phosphor::logging::level::INFO>("Manager init parsing adcs");
    for (auto& adc_dir : configJson["adcsensors"].items()) {
        boot_dbgfile << "[" << currentTimestamp() << "] " << adc_dir.key() << " bus: " << configJson["adcsensors"][adc_dir.key()]["bus"] << std::endl;
        if (configJson["adcsensors"][adc_dir.key()]["bus"] == 0 && configJson["adcsensors"][adc_dir.key()]["address"] == 0) { // bus and address is 0, this is the vpx bus
            
            for (const auto& entry : fs::directory_iterator("/sys/bus/iio/devices")) {
                // Resolve symlinks, ".", and ".." to get the absolute canonical path
                std::error_code ec;
                fs::path canonical_path = fs::canonical(entry.path(), ec);
                if (ec) {
                    continue;
                }

                if (canonical_path.string().find(adc_dir.key()) != std::string::npos && canonical_path.string().find("trigger") == std::string::npos) {
                    stub = entry.path().string();
                    directory = adc_dir.key();
                    phosphor::logging::log<phosphor::logging::level::INFO>(("Manager found path of xadc " + entry.path().string()).c_str());
                    phosphor::logging::log<phosphor::logging::level::INFO>(("Manager canonical path of xadc " + canonical_path.string()).c_str());
                }
            }

            // if (!std::filesystem::is_directory(stub + adc_dir.key())) {
            //     stub += "iio:device0";
            //     directory = adc_dir.key();
            //     phosphor::logging::log<phosphor::logging::level::INFO>(("MANAGER: " + adc_dir.key() + " does not exist, using default").c_str());
            //     break;
            // }
            // stub += adc_dir.key();
            // directory = adc_dir.key();
            // phosphor::logging::log<phosphor::logging::level::INFO>(("MANAGER: " + adc_dir.key() + " bus and address are 0").c_str());
            
        }
    }

    paths.V12path     = stub + "/in_voltage9_raw";
    paths.V3_3path    = stub + "/in_voltage10_raw";
    paths.V5path      = stub + "/in_voltage11_raw";
    paths.V3_3Auxpath = stub + "/in_voltage12_raw";
    paths.Vp12Auxpath = stub + "/in_voltage13_raw";
    paths.Vn12Auxpath = stub + "/in_voltage14_raw";

    fancontrol = configJson["fancontrol"];
    SOSA = configJson["SOSA"];
    resetLow = configJson["ResetActiveLow"];

    temp_service_status = false;
    voltage_service_status = false;
    fan_controller_service_status = false;
    fan_monitor_service_status = false;
    mandatory_sensor_service_status = false;
    gpio_service_status = false;
    ipmi_service_status = false;

    phosphor::logging::log<phosphor::logging::level::INFO>("Manager init initializing logs and writing");
    init_system_logs();

    write_log_to_file("-----------------------------------------------------------------------------------------------------------------------");
    write_log_to_file("CHASSIS MANAGER BOOTUP");
    write_log_to_file("Chassis name detected: " + configJson["name"].get<std::string>());
    write_log_to_file("FPGA has finished boot. Kernel booting has begun. These logs are for the BOOT SEQUENCE AND MANAGER application.");
    write_log_to_file("This application manages the System Reset and PS inhibit signals.");
    write_log_to_file("Once voltages are nominal and services are active, the system reset is released, allowing the cards to boot.");
    write_log_to_file("-----------------------------------------------------------------------------------------------------------------------");

    if (!SOSA) {
        ADCs.push_back(std::make_unique<ADC_element>("VS1",          paths.V12path,     12.0,  configJson["adcsensors"][directory.c_str()]["VS1"]["present"]));
        ADCs.push_back(std::make_unique<ADC_element>("VS2",          paths.V3_3path,    3.3,   configJson["adcsensors"][directory.c_str()]["VS2"]["present"]));
        ADCs.push_back(std::make_unique<ADC_element>("VS3",          paths.V5path,      5.0,   configJson["adcsensors"][directory.c_str()]["VS3"]["present"]));
        ADCs.push_back(std::make_unique<ADC_element>("3_3auxV_Rail", paths.V3_3Auxpath, 3.3,   configJson["adcsensors"][directory.c_str()]["3_3auxV_Rail"]["present"]));
        ADCs.push_back(std::make_unique<ADC_element>("12pauxV_Rail", paths.Vp12Auxpath, 12.0,  configJson["adcsensors"][directory.c_str()]["12pauxV_Rail"]["present"]));
        ADCs.push_back(std::make_unique<ADC_element>("12nauxV_Rail", paths.Vn12Auxpath, -12.0, configJson["adcsensors"][directory.c_str()]["12nauxV_Rail"]["present"]));
    } else {
        ADCs.push_back(std::make_unique<ADC_element>("VS1",          paths.V12path,     12.0,  configJson["adcsensors"][directory.c_str()]["VS1"]["present"]));
        ADCs.push_back(std::make_unique<ADC_element>("3_3auxV_Rail", paths.V3_3Auxpath, 3.3,   configJson["adcsensors"][directory.c_str()]["3_3auxV_Rail"]["present"]));
    }


    phosphor::logging::log<phosphor::logging::level::INFO>("Manager init calculate cs");
    calculate_cs();

    sysreset.input=false;
    sysreset.chip=1;
    sysreset.line=0;
    sysreset.status=0;
    sysreset.dbusflag=false;
    sysreset.name="sysreset";

    ps_enable.input=false;
    ps_enable.chip=3;
    ps_enable.line=3;
    ps_enable.status=0;
    ps_enable.dbusflag=false;
    ps_enable.name="psenable";

    ps_inhibit1.input=false;
    ps_inhibit1.chip=3;
    ps_inhibit1.line=2;
    ps_inhibit1.status=0;
    ps_inhibit1.dbusflag=false;
    ps_inhibit1.name="ps1inh";

    ps_inhibit2.input=false;
    ps_inhibit2.chip=3;
    ps_inhibit2.line=1;
    ps_inhibit2.status=0;
    ps_inhibit2.dbusflag=false;
    ps_inhibit2.name="ps2inh";

    if (find_chip_by_label(configJson["onSwitchGPIO"]["label"]) >= 0 && configJson["onSwitchGPIO"]["chip"].get<int>() >= 0) {
        switch_gpio.input=true;
        switch_gpio.chip=find_chip_by_label(configJson["onSwitchGPIO"]["label"]);
        switch_gpio.line=configJson["onSwitchGPIO"]["line"];
        switch_gpio.status=0;
        switch_gpio.name="switch_gpio";
        phosphor::logging::log<phosphor::logging::level::INFO>(("init_system_logs power switch found, chip: " + std::to_string(switch_gpio.chip) + " and line: " + std::to_string(switch_gpio.line)).c_str());
    }  else {
        switch_gpio.input=false;
        switch_gpio.chip=-1;
        switch_gpio.line=-1;
        switch_gpio.status=0;
        switch_gpio.name="switch_gpio";
        phosphor::logging::log<phosphor::logging::level::INFO>("init_system_logs did not find power switch");
    }

    alarm_gpio.input=false;
    alarm_gpio.chip=0;
    alarm_gpio.line=11;
    alarm_gpio.status=0;
    alarm_gpio.name="alarm_gpio";
    
    phosphor::logging::log<phosphor::logging::level::INFO>("Manager init exit");
}

Manager::~Manager()
{
    ;
}

void Manager::move_config()
{
    fs::path default_file = "/usr/share/lcr_configs-default/bus_devices.json";
    fs::path target_file  = "/usr/share/lcr_configs/bus_devices.json";
    if (fs::exists(target_file)) {

        nlohmann::json configJson_temp = readJson("/usr/share/lcr_configs/bus_devices.json");
        nlohmann::json default_configJson_temp = readJson("/usr/share/lcr_configs-default/bus_devices.json");

        if (configJson_temp["name"] == default_configJson_temp["name"]) {
            return;
        }
    }
    fs::copy_file(default_file, target_file, fs::copy_options::overwrite_existing);
}

void Manager::init_system_logs() 
{

    //read the old logfile from flash, and load that into the logfile in the filesystem.

    int fd = open("/dev/mtd4", O_RDWR | O_SYNC);
    if (fd < 0) {
        phosphor::logging::log<phosphor::logging::level::INFO>(("init_system_logs Failed to open /dev/mtd4 for init: " + std::string(strerror(errno))).c_str());
        return;
    }

    mtd_info_t mtd_info;
    if (ioctl(fd, MEMGETINFO, &mtd_info) != 0) {
        phosphor::logging::log<phosphor::logging::level::INFO>(("init_system_logs Failed to get MTD info: " + std::string(strerror(errno))).c_str());
        close(fd);
        return;
    }

    phosphor::logging::log<phosphor::logging::level::INFO>("init_system_logs test recover_log_offsets");
    boot_dbgfile << "[" << currentTimestamp() << "] " << "LCR:BOOT: test recover_log_offsets" << std::endl;
    recover_log_offsets(fd, mtd_info.size, mtd_info.erasesize, oldest_offset, offset);

    phosphor::logging::log<phosphor::logging::level::INFO>(("init_system_logs oldest_offset: "      + std::to_string(static_cast<int>(oldest_offset))).c_str());
    phosphor::logging::log<phosphor::logging::level::INFO>(("init_system_logs offset: "             + std::to_string(static_cast<int>(offset))).c_str());
    phosphor::logging::log<phosphor::logging::level::INFO>(("init_system_logs mtd_info.size: "      + std::to_string(static_cast<int>(mtd_info.size))).c_str());
    phosphor::logging::log<phosphor::logging::level::INFO>(("init_system_logs mtd_info.erasesize: " + std::to_string(static_cast<int>(mtd_info.erasesize))).c_str());
    phosphor::logging::log<phosphor::logging::level::INFO>(("init_system_logs remaining space: "    + std::to_string(static_cast<int>(mtd_info.size - offset))).c_str());

    close(fd);

    std::ifstream fileo("/dev/mtd4", std::ios::binary);
    std::ofstream outFile("system_logs.log");
    std::string lineo;
    off_t last_valid = 0;

    auto process_range = [&](off_t start, off_t end) {
        if (start >= end) return;

        fileo.clear();
        fileo.seekg(start);
        if (!fileo) {
            phosphor::logging::log<phosphor::logging::level::INFO>(("init_system_logs seekg failed at: "  + std::to_string(start)).c_str());
            return;
        }
        if (!sim) {
            while (fileo.tellg() < end && std::getline(fileo, lineo)) {
                if (!fileo) break;                    // stream went bad
    
                size_t len = lineo.length();
                if (len < 1024) {
                    if (!lineo.empty() && lineo[0] == '[') {
                        std::streampos pos = fileo.tellg();
                        if (pos != std::streampos(-1) && pos > last_valid) {
                            last_valid = static_cast<off_t>(pos);
                        }
                        outFile << lineo << '\n';
                    }
                } else {
                    phosphor::logging::log<phosphor::logging::level::INFO>("init_system_logs long line, stopping");
                    break;
                }
            }
        } 
    };

    if (offset >= oldest_offset) {
        process_range(oldest_offset, offset);
    } else {
        process_range(oldest_offset, mtd_info.size);
        process_range(0, offset);
    }

    phosphor::logging::log<phosphor::logging::level::INFO>(("init_system_logs offset at the end of init: "  + std::to_string(static_cast<int>(offset))).c_str());
}

void Manager::write_log_to_file(std::string log) {

    std::string newlog = "[" + timeSinceBoot() + "] " + "LCR: " + log + "\n";

    system_logs << "[" << timeSinceBoot() << "] " << "LCR: " << log << std::endl;

    newlog_string += newlog;

    trim_log();

}

void Manager::write_logs_to_flash() 
{

    if (newlog_string.empty()) return;

    int fd = open("/dev/mtd4", O_RDWR | O_SYNC);
    if (fd < 0) {
        phosphor::logging::log<phosphor::logging::level::INFO>(("write_logs_to_flash Failed to open /dev/mtd4: "  + std::string(strerror(errno))).c_str());
        return;
    }

    mtd_info_t mtd_info;
    if (ioctl(fd, MEMGETINFO, &mtd_info) != 0) {
        phosphor::logging::log<phosphor::logging::level::INFO>(("write_logs_to_flash Failed to get MTD info: "  + std::string(strerror(errno))).c_str());
        close(fd);
        return;
    }

    if (!(mtd_info.flags & MTD_WRITEABLE)) {
        close(fd);
        return;
    }

    size_t erase_size = mtd_info.erasesize;
    size_t data_size = newlog_string.size();
    off_t log_start = 0;
    off_t log_end   = mtd_info.size;

    auto calc_used = [&](off_t w, off_t o) -> size_t {
        if (w >= o) return w - o;
        return (log_end - o) + (w - log_start);
    };

    size_t used = calc_used(offset, oldest_offset);
    size_t free_space = (log_end - log_start) - used;

    // erase oldest blocks until we have room
    while (free_space < data_size && oldest_offset < log_end) {
        off_t block = oldest_offset & ~(erase_size - 1);

        erase_info_t er { .start = block, .length = erase_size };
        ioctl(fd, MEMUNLOCK, &er);           // ignore ENOTSUP
        if (ioctl(fd, MEMERASE, &er) != 0) {
            boot_dbgfile << "Erase (roll) failed at " << block << ": " << strerror(errno) << std::endl;
            phosphor::logging::log<phosphor::logging::level::INFO>(("write_logs_to_flash Erase (roll) failed at " + std::to_string(block) + ": " + std::string(strerror(errno))).c_str());
            close(fd);
            return;
        }

        oldest_offset = block + erase_size;
        if (oldest_offset >= log_end) oldest_offset = log_start;

        used = calc_used(offset, oldest_offset);
        free_space = (log_end - log_start) - used;
        phosphor::logging::log<phosphor::logging::level::INFO>("write_logs_to_flash Rolling over now...");
    }

    if (free_space < data_size) {
        phosphor::logging::log<phosphor::logging::level::INFO>("write_logs_to_flash Not enough space even after rolling");
        close(fd);
        return;
    }

    // Build write ranges
    std::vector<std::pair<off_t, size_t>> wranges;
    if (offset + data_size <= log_end) {
        wranges.emplace_back(offset, data_size);
    } else {
        size_t first = log_end - offset;
        if (first > 0) wranges.emplace_back(offset, first);
        size_t second = data_size - first;
        if (second > 0) wranges.emplace_back(log_start, second);
    }

    if (wranges.size() > 1) {
        phosphor::logging::log<phosphor::logging::level::INFO>("write_logs_to_flash WRAP WRITE");
    }

    // Prepare blocks in the write ranges (erase only if all-FF)

    for (auto& wr : wranges) {
        off_t wstart = wr.first;
        off_t wend   = wstart + wr.second - 1;
        off_t sb = wstart & ~(erase_size - 1);
        off_t eb = wend   & ~(erase_size - 1);

        bool force_erase = (wstart == 0);

        for (off_t b = sb; b <= eb; b += erase_size) {
            std::vector<uint8_t> buf(erase_size);
            lseek(fd, b, SEEK_SET);
            if (read(fd, buf.data(), erase_size) != static_cast<ssize_t>(erase_size)) continue;

            bool has_data = false;
            for (auto v : buf) if (v != 0xFF) { has_data = true; break; }
            
            if (!force_erase && has_data) {
                continue;                       // extending current block – preserve old data
            }

            erase_info_t er { .start = b, .length = erase_size };
            ioctl(fd, MEMUNLOCK, &er);
            if (ioctl(fd, MEMERASE, &er) != 0) {
                boot_dbgfile << "Erase failed for block " << b << "\n";
                phosphor::logging::log<phosphor::logging::level::INFO>(("write_logs_to_flash Erase failed for block " + std::to_string(b)).c_str());
            }
        }
    }

    bool all_writes_ok = true;

    const char* data_ptr = newlog_string.data();
    size_t data_off = 0;

    for (auto& wr : wranges) {
        lseek(fd, wr.first, SEEK_SET);
        ssize_t written = write(fd, data_ptr + data_off, wr.second);
        if (written != static_cast<ssize_t>(wr.second)) {
            all_writes_ok = false;
        }
        data_off += wr.second;
    }

    if (all_writes_ok && !wranges.empty()) {
        auto& last = wranges.back();
        offset = last.first + last.second;
        if (offset >= static_cast<off_t>(mtd_info.size)) offset = 0;
    } else if (!all_writes_ok) {
        phosphor::logging::log<phosphor::logging::level::INFO>("write_logs_to_flash NOT advancing offset because write was incomplete");
    }

    close(fd);
    if (all_writes_ok) newlog_string = "";

}

void Manager::update_last_append_offset(off_t new_offset) 
{
    offset = new_offset;
}

void Manager::trim_log() 
{

    // boot_dbgfile << "[" << currentTimestamp() << "] " << "LCR:BOOT: trimming log now..." << std::endl;

    std::string filename = "system_logs.log";

    try {
        size_t current_size = fs::file_size(filename);
        // boot_dbgfile << "[" << currentTimestamp() << "] " << "LCR:BOOT: current_size: " << current_size << std::endl;
        if (current_size <= MAX_SIZE) return;

        std::ifstream in(filename, std::ios::binary);
        if (!in) return;

        size_t to_skip = current_size - MAX_SIZE;
        size_t skipped = 0;
        char ch;
        while (in.get(ch)) {
            ++skipped;
            if (ch == '\n' && skipped >= to_skip) {
                break;  // Stop after a full line when enough bytes are skipped
            }
        }

        if (!in) return;  // Reached EOF without skipping enough (unlikely)

        // Now 'in' is positioned at the start of the first log to keep
        std::string temp_filename = filename + ".tmp";
        std::ofstream out(temp_filename, std::ios::binary);
        if (!out) return;

        out << in.rdbuf();  // Efficiently copy the remaining content

        in.close();
        out.close();

        fs::remove(filename);
        fs::rename(temp_filename, filename);
        // Close and reopen the global system_logs stream
        system_logs.close();
        system_logs.open("system_logs.log", std::ios::app);
    } catch (const fs::filesystem_error& e) {
        phosphor::logging::log<phosphor::logging::level::INFO>("trim_log Filesystem error");
    }

}

void Manager::write_heartbeat() 
{
    update_readings(1);
    std::string ADClog;
    ADClog = "System heartbeat... ";
    for (const auto& ADC : ADCs) {
        ADClog += ADC->name;
        ADClog += std::string(": ");
        ADClog += std::to_string(ADC->V_current);
        ADClog += std::string("V ");
    }
    write_log_to_file(ADClog);
}

void Manager::calculate_cs() 
{
    for (const auto& ADC : ADCs) {
        ADC->scale_coeff = (ADC->V_target / 2047);
    }
}

bool Manager::getfancontrol()
{
    return fancontrol;
}

int Manager::find_chip_by_label(const std::string& want)
{
    for (auto& c : gpiod::make_chip_iter())
    {
        if (c.label() != want)
            continue;
        
        std::string name = c.name();
        char c_chip = name.back();
        std::string s_chip(1, c_chip);
        int chip = stoi(s_chip);
        return chip;

    }
    return -1;
}

bool Manager::wait_for_switch() 
{
    watch_services();
    if (switch_gpio.chip != -1) {
        try {
            phosphor::logging::log<phosphor::logging::level::INFO>("wait_for_switch checking chip...");
            // Open the GPIO chip
            gpiod::chip chip("gpiochip" + std::to_string(switch_gpio.chip));
    
            // Get the line
            gpiod::line line = chip.get_line(switch_gpio.line);
            
            // Request line as input
            line.request({"readgpio", gpiod::line_request::DIRECTION_INPUT, 0});
            
            // Read the current value
            int value = line.get_value();
            
            // Update status
            switch_gpio.status = value;
    
            phosphor::logging::log<phosphor::logging::level::INFO>(("wait_for_switch chip=" + std::to_string(switch_gpio.chip) + " line=" + std::to_string(switch_gpio.line) + " value=" + std::to_string(value)).c_str());
            line.release();
            
            if (configJson["onSwitchGPIO"]["activehigh"].get<bool>()) {
                if (value == 1) {
                    phosphor::logging::log<phosphor::logging::level::INFO>("wait_for_switch switch was turned on");
                    line.release();
                    write_log_to_file("Front Panel switch was turned on...");
                    return true;
                }
            } else {
                if (value == 0) {
                    phosphor::logging::log<phosphor::logging::level::INFO>("wait_for_switch switch was turned on");
                    line.release();
                    write_log_to_file("Front Panel switch was turned on...");
                    return true;
                }
            }
    
            return false;
        } catch (const std::exception& e) {
            phosphor::logging::log<phosphor::logging::level::INFO>("wait_for_switch Error in GPIO operations");
            return false;
        }
    } else {
        write_log_to_file("Front Panel switch GPIO is not present, continuing with power-on sequence...");
        return true;
    }
}

bool Manager::set_alarm_gpio() 
{
    try {
        // Open the GPIO chip
        gpiod::chip chip("gpiochip" + std::to_string(switch_gpio.chip));
        
        // Get the line
        gpiod::line line = chip.get_line(switch_gpio.line);
    
        phosphor::logging::log<phosphor::logging::level::INFO>("set_alarm_gpio setting alarm");
        // Request line as output
        line.request({"setgpio", gpiod::line_request::DIRECTION_OUTPUT, 0}, 1);
        switch_gpio.status=1;
        write_log_to_file("Setting Alarm gpio pin");
        phosphor::logging::log<phosphor::logging::level::INFO>("set_alarm_gpio exiting set alarm");
        line.release();

        return true;
    } catch (const std::exception& e) {
        phosphor::logging::log<phosphor::logging::level::INFO>("set_alarm_gpio Error in GPIO operations");
        return false;
    }
}

bool Manager::update_readings(int num) 
{

    bool bootflag = false;

    for (const auto& ADC : ADCs) {
        boot_dbgfile << "[" << currentTimestamp() << "] " << "LCR:BOOT: ADC path: " << ADC->path << std::endl;
        boot_dbgfile << "[" << currentTimestamp() << "] " << "LCR:BOOT: reading value: " << readFile(ADC->path) << std::endl;
        if (ADC->present) {
            ADC->raw_reading = std::stoi(readFile(ADC->path));
            ADC->V_current = ADC->scale_coeff * ADC->raw_reading;
            double lowthresh = configJson["adcsensors"][directory.c_str()][ADC->name]["thresholds"]["critmin"];
            double highthresh = configJson["adcsensors"][directory.c_str()][ADC->name]["thresholds"]["critmax"];
            if (std::abs(ADC->V_current) > std::abs(lowthresh) && std::abs(ADC->V_current) < std::abs(highthresh)) {
                // boot_dbgfile << "[" << currentTimestamp() << "] " << ADC->name << " is safe at reading " << ADC->V_current << std::endl;
                ADC->safe = true;
            } else {
                phosphor::logging::log<phosphor::logging::level::INFO>(("update_readings " + ADC->name + " is not safe at reading " + std::to_string(ADC->V_current)).c_str());
                if (num % 15 == 0) {
                    write_log_to_file("Voltage monitoring: " + ADC->name + " is unsafe at reading " + std::to_string(ADC->V_current));
                }
                ADC->safe = false;
                bootflag = false;
                return bootflag;
            }
        }
    }

    for (const auto& ADC : ADCs) {
        if (ADC->present) {
            if (ADC->safe == false) {
                phosphor::logging::log<phosphor::logging::level::INFO>(("update_readings " + ADC->name + " is not safe at reading " + std::to_string(ADC->V_current)).c_str());
                bootflag = false;
                return bootflag;
            }
        }
    }

    bootflag = true;
    
    return bootflag;
}

ADC_element* Manager::getSensorByName(const std::string& name) 
{
    for (const auto& ADC : ADCs) {
        if (ADC->name == name) {
            return ADC.get();
        }
    }
    return nullptr;
}

bool Manager::set_ps() 
{
    try {
        // Open the GPIO chip
        gpiod::chip chip("gpiochip" + std::to_string(ps_enable.chip));
        
        // Get the line
        gpiod::line line = chip.get_line(ps_enable.line);
    
        phosphor::logging::log<phosphor::logging::level::INFO>("set_ps setting ps");
        // Request line as output
        line.request({"setgpio", gpiod::line_request::DIRECTION_OUTPUT, 0}, 1);
        ps_enable.status=1;
        write_log_to_file("Setting Power Supply Enable pin");
        phosphor::logging::log<phosphor::logging::level::INFO>("set_ps exiting set ps");
        line.release();

        ps_enable.dbusflag=true;

        return true;
    } catch (const std::exception& e) {
        phosphor::logging::log<phosphor::logging::level::INFO>("set_ps Error in GPIO operations");
        return false;
    }
}

void Manager::set_power_state()
{
    auto bus = sdbusplus::bus::new_default();
    phosphor::interface::util::setProperty<std::string>(bus, "/xyz/openbmc_project/state/chassis0", "xyz.openbmc_project.State.Chassis", "CurrentPowerState", "xyz.openbmc_project.State.Chassis.PowerState.On");

}

bool Manager::set_ps_inh1() 
{
    try {
        // Open the GPIO chip
        gpiod::chip chip("gpiochip" + std::to_string(ps_inhibit1.chip));
        
        // Get the line
        gpiod::line line = chip.get_line(ps_inhibit1.line);
    
        phosphor::logging::log<phosphor::logging::level::INFO>("set_ps_inh1 setting ps inh1");
        // Request line as output
        line.request({"setgpio", gpiod::line_request::DIRECTION_OUTPUT, 0}, 1);
        ps_inhibit1.status=1;
        write_log_to_file("Setting Power Supply inhibit 1 pin");
        phosphor::logging::log<phosphor::logging::level::INFO>("set_ps_inh1 exiting set ps inh1");
        line.release();

        ps_inhibit1.dbusflag=true;

        return true;
    } catch (const std::exception& e) {
        phosphor::logging::log<phosphor::logging::level::INFO>("set_ps_inh1 Error in GPIO operations");
        return false;
    }
}

bool Manager::set_ps_inh2() 
{
    try {
        // Open the GPIO chip
        gpiod::chip chip("gpiochip" + std::to_string(ps_inhibit2.chip));
        
        // Get the line
        gpiod::line line = chip.get_line(ps_inhibit2.line);
        phosphor::logging::log<phosphor::logging::level::INFO>("set_ps_inh2 setting ps inh2");
        // Request line as output
        line.request({"setgpio", gpiod::line_request::DIRECTION_OUTPUT, 0}, 1);
        ps_inhibit2.status=1;
        write_log_to_file("Setting Power Supply inhibit 2 pin");
        phosphor::logging::log<phosphor::logging::level::INFO>("set_ps_inh2 exiting set ps inh2");
        line.release();

        ps_inhibit2.dbusflag=true;

        return true;
    } catch (const std::exception& e) {
        phosphor::logging::log<phosphor::logging::level::INFO>("set_ps_inh2 Error in GPIO operations");
        return false;
    }
}

bool Manager::unset_ps() 
{
    try {
        // Open the GPIO chip
        gpiod::chip chip("gpiochip" + std::to_string(ps_enable.chip));
        
        // Get the line
        gpiod::line line = chip.get_line(ps_enable.line);
    
        // Request line as output
        line.request({"setgpio", gpiod::line_request::DIRECTION_OUTPUT, 0}, 0);
        ps_enable.status=0;
        write_log_to_file("Unsetting Power Supply Enable pin");
        line.release();

        ps_enable.dbusflag=true;

        return true;
    } catch (const std::exception& e) {
        return false;
    }
}

bool Manager::get_fan_controller_status() 
{
    watch_services();
    return fan_controller_service_status;
}

bool Manager::unset_sr() 
{
    write_log_to_file("Voltage monitoring: voltages within range, driving system reset pin to begin chassis boot");
    try {
        // Open the GPIO chip
        gpiod::chip chip("gpiochip" + std::to_string(sysreset.chip));
        
        // Get the line
        gpiod::line line = chip.get_line(sysreset.line);

        // Request line as output
        if (resetLow) {
            line.request({"setgpio", gpiod::line_request::DIRECTION_OUTPUT, 0}, 1);
        } else {
            line.request({"setgpio", gpiod::line_request::DIRECTION_OUTPUT, 0}, 0);
        }
        
        sysreset.status=1;
        write_log_to_file("Setting Chassis-wide System Reset to boot cards");
        line.release();

        srstatus_file.seekp(0);
        srstatus_file << 1 << std::endl;
        srstatus_file.flush();

        sysreset.dbusflag=true;

        return true;
    } catch (const std::exception& e) {
        return false;
    }
}

bool Manager::set_sr() 
{
    write_log_to_file("Holding reset until chassis is safe to boot.");
    try {
        // Open the GPIO chip
        gpiod::chip chip("gpiochip" + std::to_string(sysreset.chip));
        
        // Get the line
        gpiod::line line = chip.get_line(sysreset.line);

        // Request line as output
        if (resetLow) {
            line.request({"setgpio", gpiod::line_request::DIRECTION_OUTPUT, 0}, 0);
        } else {
            line.request({"setgpio", gpiod::line_request::DIRECTION_OUTPUT, 0}, 1);
        }
        
        sysreset.status=1;
        line.release();

        srstatus_file.seekp(0);
        srstatus_file << 0 << std::endl;
        srstatus_file.flush();

        sysreset.dbusflag=true;

        return true;
    } catch (const std::exception& e) {
        return false;
    }
}


void Manager::sim_sr() 
{
    srstatus_file.seekp(0);
    srstatus_file << 1 << std::endl;
    srstatus_file.flush();
    sysreset.dbusflag=true;
}

bool Manager::check_temp_service()
{
    return isServiceActive("LCR_temp_sensors.service");
}

bool Manager::check_voltage_service()
{
    return isServiceActive("LCR_ADC_sensors.service");
}

bool Manager::check_fan_controller_service()
{
    return isServiceActive("LCR_fan_controller.service");
}

bool Manager::check_fan_monitor_service()
{
    return isServiceActive("LCR_fan_monitor.service");
}

bool Manager::check_mandatory_sensor_service()
{
    return isServiceActive("LCR_Mandatory_Sensors.service");
}

bool Manager::check_gpio_service()
{
    return isServiceActive("LCR_GPIO_Mon.service");    
}

bool Manager::check_ipmi_host_service()
{
    return isServiceActive("phosphor-ipmi-host.service");
}

void Manager::log_servicechange(std::string* log, bool status) 
{
    if (status == false) {
        *log += " to OFF";
    } else {
        *log += " to ON";
    }
}

void Manager::watch_services() 
{
    bool temp_temp_status = check_temp_service();
    bool temp_voltage_status = check_voltage_service();
    bool temp_fan_controller_status = check_fan_controller_service();
    bool temp_fan_monitor_status = check_fan_monitor_service();
    bool temp_mandatory_sensor_status = check_mandatory_sensor_service();
    bool temp_gpio_status = check_gpio_service();
    bool temp_ipmi_status = check_ipmi_host_service();

    ipmi_service_status = temp_ipmi_status;

    if (temp_temp_status != temp_service_status) {
        std::string log;
        log += "Temperature monitoring service status changed";
        log_servicechange(&log, temp_temp_status);
        temp_service_status = temp_temp_status;
        write_log_to_file(log);
    }
    if (temp_voltage_status != voltage_service_status) {
        std::string log;
        log += "Voltage monitoring service status changed";
        log_servicechange(&log, temp_voltage_status);
        voltage_service_status = temp_voltage_status;
        write_log_to_file(log);
    }
    if (temp_fan_controller_status != fan_controller_service_status) {
        std::string log;
        log += "Fan Controller service status changed";
        log_servicechange(&log, temp_fan_controller_status);
        fan_controller_service_status = temp_fan_controller_status;
        write_log_to_file(log);
    }
    if (temp_fan_monitor_status != fan_monitor_service_status) {
        std::string log;
        log += "Fan Monitor service status changed";
        log_servicechange(&log, temp_fan_monitor_status);
        fan_monitor_service_status = temp_fan_monitor_status;
        write_log_to_file(log);
    }
    if (temp_mandatory_sensor_status != mandatory_sensor_service_status) {
        std::string log;
        log += "Mandatory Sensors service status changed";
        log_servicechange(&log, temp_mandatory_sensor_status);
        mandatory_sensor_service_status = temp_mandatory_sensor_status;
        write_log_to_file(log);
    }
    if (temp_gpio_status != gpio_service_status) {
        std::string log;
        log += "GPIO monitoring service status changed";
        log_servicechange(&log, temp_gpio_status);
        gpio_service_status = temp_gpio_status;
        write_log_to_file(log);
    }
    if (temp_ipmi_status != ipmi_service_status) {
        std::string log;
        log += "IPMI host service status changed";
        log_servicechange(&log, temp_ipmi_status);
        ipmi_service_status = temp_ipmi_status;
        write_log_to_file(log);
    }
}

void Manager::watch_gpios() 
{
    std::string interface = "xyz.openbmc_project.Sensor.Value";
    std::string property = "Value";
    while(true){
        if (gpio_service_status) {

            auto bus = sdbusplus::bus::new_default();

            int ps_value = ps_enable.status;
            int inh1_value = ps_inhibit1.status;
            int inh2_value = ps_inhibit2.status;
            int sr_value = sysreset.status;

            std::string ps_name = ps_enable.name;
            std::string ps_path = "/xyz/openbmc_project/gpio/" + ps_name;

            std::string psinh1_name = ps_inhibit1.name;
            std::string psinh1_path = "/xyz/openbmc_project/gpio/" + psinh1_name;

            std::string psinh2_name = ps_inhibit2.name;
            std::string psinh2_path = "/xyz/openbmc_project/gpio/" + psinh2_name;

            std::string sr_name = sysreset.name;
            std::string sr_path = "/xyz/openbmc_project/gpio/" + sr_name;

            try {
                if (ps_enable.dbusflag){
                    phosphor::logging::log<phosphor::logging::level::INFO>(("watch_gpios setting ps_enable to " + std::to_string(ps_value)).c_str());
                    phosphor::interface::util::setProperty<int>(bus, ps_path,     interface, property, std::move(ps_value));
                    ps_enable.dbusflag=false;
                }
                if (ps_inhibit1.dbusflag){
                    phosphor::logging::log<phosphor::logging::level::INFO>(("watch_gpios setting ps_inhibit1 to " + std::to_string(inh1_value)).c_str());
                    phosphor::interface::util::setProperty<int>(bus, psinh1_path, interface, property, std::move(inh1_value));
                    ps_inhibit1.dbusflag=false;
                }
                if (ps_inhibit2.dbusflag){
                    phosphor::logging::log<phosphor::logging::level::INFO>(("watch_gpios setting ps_inhibit2 to " + std::to_string(inh2_value)).c_str());
                    phosphor::interface::util::setProperty<int>(bus, psinh2_path, interface, property, std::move(inh2_value));
                    ps_inhibit2.dbusflag=false;
                }
            } catch (const std::exception& e) {
                phosphor::logging::log<phosphor::logging::level::INFO>(("watch_gpios Failed to set property for " + ps_name).c_str());
            }
            
            try {
                if (sysreset.dbusflag){
                    phosphor::interface::util::setProperty<int>(bus, sr_path, interface, property, std::move(sr_value));
                    sysreset.dbusflag = false;
                }
            } catch (const std::exception& e) {
                phosphor::logging::log<phosphor::logging::level::INFO>(("watch_gpios Failed to set property for " + sr_name).c_str());
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
    }
}

void Manager::watch_alarms() 
{
    
    phosphor::logging::log<phosphor::logging::level::INFO>("watch_alarms waiting for ipmi service");
    while(ipmi_service_status == false) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        watch_services();
    }

    auto bus = sdbusplus::bus::new_default();

    std::string threshold_interface = "xyz.openbmc_project.Sensor.Threshold.Critical";
    std::string value_interface = "xyz.openbmc_project.Sensor.Value";
    std::string voltage_service = "xyz.openbmc_project.ADCHwmon";
    std::string voltage_stub = "/xyz/openbmc_project/sensors/voltage";
    std::string temperature_service = "xyz.openbmc_project.temperatureHwmon";
    std::string temperature_stub = "/xyz/openbmc_project/sensors/temperature";
    std::string pwm_stub = "/xyz/openbmc_project/sensors/fan_pwm";
    std::string tach_stub = "/xyz/openbmc_project/sensors/fan_tach";

    try {
        temp_paths = phosphor::interface::util::getSubTreePathsRaw(bus, temperature_stub, threshold_interface, 0);
    } catch (...) {
        phosphor::logging::log<phosphor::logging::level::INFO>("watch_alarms no temperature paths");
    }
    
    try {
        voltage_paths = phosphor::interface::util::getSubTreePathsRaw(bus, voltage_stub, threshold_interface, 0);
    } catch (...) {
        phosphor::logging::log<phosphor::logging::level::INFO>("watch_alarms no voltage paths");
    }
    
    bool temp_alarm;
    bool voltage_alarm;
    bool fan_alarm;
    bool alarms = false;
    double voltage_reading;
    double temperature_reading;
    std::string temp_name;
    std::string voltage_name;
    while(alarms == false) {
        watch_services();
        temp_alarm = false;
        if (temp_paths.size() > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            for (auto& path : temp_paths) {
                try {
                    bool critical_low = phosphor::interface::util::getProperty<bool>(bus, path, threshold_interface, std::string("CriticalAlarmLow"));
                    bool critical_high = phosphor::interface::util::getProperty<bool>(bus, path, threshold_interface, std::string("CriticalAlarmHigh"));
                    temp_alarm = (critical_low || critical_high) ? true : false;
                    if (temp_alarm) {
                        temperature_reading = phosphor::interface::util::getProperty<double>(bus, path, value_interface, std::string("Value"));
                        temp_name = path;
                        break;
                    }
                } catch (const std::exception& e) {
                    // Continue to next path, treating this sensor as no alarm
                }
            }// Treat as no alarm if subtree fetch fails
        }
        
        voltage_alarm = false;
        if (voltage_paths.size() > 0) {
            for (auto& path : voltage_paths) {
                try {
                    bool critical_low = phosphor::interface::util::getProperty<bool>(bus, path, threshold_interface, std::string("CriticalAlarmLow"));
                    bool critical_high = phosphor::interface::util::getProperty<bool>(bus, path, threshold_interface, std::string("CriticalAlarmHigh"));
                    voltage_alarm = (critical_low || critical_high) ? true : false;
                    if (voltage_alarm) {
                        voltage_reading = phosphor::interface::util::getProperty<double>(bus, path, value_interface, std::string("Value"));
                        voltage_name = path;
                        break;
                    }
                } catch (const std::exception& e) {
                    // Continue to next path, treating this sensor as no alarm
                }
            }// Treat as no alarm if subtree fetch fails
        }
        alarms = (temp_alarm || voltage_alarm) ? true : false;
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

    }
    system_logs << "[" << timeSinceBoot() << "] " << "LCR: An alarm is found to be high" << std::endl;
    system_logs << "[" << timeSinceBoot() << "] " << "LCR: Alarm: " << std::endl;
    if (temp_alarm) {
        write_log_to_file("Temperature Alarm at path " + temp_name + " with reading " + std::to_string(temperature_reading));
    } else if (voltage_alarm) {
        write_log_to_file("Voltage Alarm at path " + voltage_name + " with reading " + std::to_string(voltage_reading));
    }
    unset_ps();
    set_alarm_gpio();
    write_log_to_file("Alarm is set, reset the chassis.");
    phosphor::logging::log<phosphor::logging::level::INFO>("watch_alarms ALARMS ARE HIGH");
    
}