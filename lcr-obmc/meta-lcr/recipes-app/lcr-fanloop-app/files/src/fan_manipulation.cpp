#include "../include/fan_manipulation.hpp"


fan_loop::fan_loop(std::shared_ptr<sdbusplus::asio::connection> conn_ref, 
                    sdbusplus::asio::object_server& server_ref,
                    boost::asio::io_context& io) : 
                    conn(conn_ref), 
                    obj_server(server_ref),
                    tick_timer(io),
                    retry_timer(io)  
{
    phosphor::logging::log<phosphor::logging::level::INFO>("fan_loop init enter");
    running = false;
    control_count = 0;
    pwm_retries = 0;
    period_ = std::chrono::milliseconds(1000);
    controller = std::make_unique<pid_control::ec::pid_info_t>();

    phosphor::logging::log<phosphor::logging::level::INFO>("fan_loop initialize_temps");
    initialize_temps();

    phosphor::logging::log<phosphor::logging::level::INFO>("fan_loop initialize_controller");
    initialize_controller();

    get_fan_pwms();
    
    phosphor::logging::log<phosphor::logging::level::INFO>("fan_loop init exit");
}

fan_loop::~fan_loop()
{
    ;
}

void fan_loop::schedule_temp_retry()
{
    retry_timer.expires_after(std::chrono::seconds(1));
    retry_timer.async_wait([this](const boost::system::error_code& ec) {
        if (!ec) {
            initialize_temps();
        }
    });
}

void fan_loop::arm_tick()
{
    int recheck_flag_ = 1;
    
    tick_timer.expires_after(period_);
    tick_timer.async_wait([this, &recheck_flag_](const boost::system::error_code& ec) {
        if (ec) {
            return;
        }

        if (!running && pwm_retries >= 20) {
            return;
        } else if (!running && pwm_retries < 20) {
            phosphor::logging::log<phosphor::logging::level::INFO>("arm_tick Early check");
            initialize_temps();
            get_fan_pwms();
            pwm_retries++;
        }

        if (buserrors < 3 && generrors < 3) {
            if (!running) {running=true;}
            read_temperatures();
            control_loop();
            if ((recheck_flag_ % 10) == 0) {
                phosphor::logging::log<phosphor::logging::level::INFO>(("arm_tick Late check, recheck_flag_: " + std::to_string(recheck_flag_)).c_str());
                get_fan_pwms();
                recheck_flag_ = 0;
            }
            recheck_flag_ = recheck_flag_ + 1;
        } else {
            running = false;
        }
        if (buserrors < 3 && generrors < 3) {
            arm_tick();
        }
    });
}

void fan_loop::initialize_temps() 
{
    if (init_inflight_) {
        return;
    }
    init_inflight_ = true;

    // phosphor::logging::log<phosphor::logging::level::INFO>("initialize_temps enter");
    std::string sense_interface = "xyz.openbmc_project.Sensor.Value";
    std::string stub = "/xyz/openbmc_project/sensors/temperature";

    // phosphor::logging::log<phosphor::logging::level::INFO>("initialize_temps get subtree");

    phosphor::interface::util::async_get_subtree(
        conn, stub, 0, {sense_interface},
        [this, sense_interface](const SubTree& tree) {
            // phosphor::logging::log<phosphor::logging::level::INFO>("initialize_temps callback enter");

            if (tree.empty()) {
                phosphor::logging::log<phosphor::logging::level::INFO>("initialize_temps tree empty");
                init_inflight_ = false;
                schedule_temp_retry();
                return;
            }

            auto items = std::make_shared<std::vector<std::pair<std::string, std::string>>>();
            for (const auto& [p, s_map] : tree) {
                if (!s_map.empty()) {
                    items->emplace_back(p, s_map.begin()->first);
                }
            }
            if (items->empty()) {
                phosphor::logging::log<phosphor::logging::level::INFO>("initialize_temps items empty");
                init_inflight_ = false;
                schedule_temp_retry();
                return;
            }

            auto idx = std::make_shared<size_t>(0);
            auto check_next = std::make_shared<std::function<void()>>();

            // phosphor::logging::log<phosphor::logging::level::INFO>("initialize_temps begin check_next");
            *check_next = [this, items, idx, sense_interface, check_next]() {
                if (*idx >= items->size()) {
                    temps_ready_ = !temps.empty();
                    init_inflight_ = false;
                    // phosphor::logging::log<phosphor::logging::level::INFO>(("initialize_temps done, count=" + std::to_string(temps.size())).c_str());
                    if (temps_ready_) {
                        arm_tick();
                    } else {
                        phosphor::logging::log<phosphor::logging::level::INFO>("initialize_temps temps not ready");
                        schedule_temp_retry();
                    }
                    return;
                }

                auto [path, service] = (*items)[*idx];
                ++(*idx);

                // phosphor::logging::log<phosphor::logging::level::INFO>(("initialize_temps path: " + path).c_str());
                
                std::string name = path.substr(path.rfind('/') + 1);
                if (std::find(temp_names.begin(), temp_names.end(), name) == temp_names.end()) {
                    auto inst = std::make_unique<temperature_sensor>();
                    inst->path = path;
                    inst->service = service;
                    inst->name = name;
                    temp_names.push_back(name);
                    temps.push_back(std::move(inst));
                }

                phosphor::interface::util::async_get_dbus_property<double>(
                    conn, service, path, sense_interface, "Value",
                    [this, path, check_next](const boost::system::error_code& ec, double val) {
                        // phosphor::logging::log<phosphor::logging::level::INFO>(("initialize_temps value: " + path + " = " + std::to_string(val)).c_str());
                        (*check_next)();   // always continue
                    }
                );
            };

            (*check_next)();
        }
    );
}

void fan_loop::initialize_controller() 
{

    nlohmann::json data;
    controller->initialized=true;
    try {
        data = readJson("/usr/share/pid/pid.json");

        linear = data["linear"].get<bool>();
        ts = data["ts"].get<double>();

        controller->ts = ts;
        controller->proportionalCoeff = data["proportionalCoeff"].get<double>();
        controller->integralCoeff = data["integralCoeff"].get<double>();
        controller->derivativeCoeff = data["derivativeCoeff"].get<double>();
        controller->feedFwdOffset = data["feedFwdOffset"].get<double>();
        controller->feedFwdGain = data["feedFwdGain"].get<double>();

        controller->integralLimit.min = data["integralLimit"]["min"].get<double>();
        controller->integralLimit.max = data["integralLimit"]["max"].get<double>();

        controller->outLim.min = data["outLim"]["min"].get<double>();
        controller->outLim.max = data["outLim"]["max"].get<double>();

        controller->slewNeg = data["slewNeg"].get<double>();
        controller->slewPos = data["slewPos"].get<double>();
        
        controller->positiveHysteresis = data["positiveHysteresis"].get<double>();
        controller->negativeHysteresis = data["negativeHysteresis"].get<double>();

    } catch (...) { // Catch-all block for any unhandled exception
        phosphor::logging::log<phosphor::logging::level::INFO>("initialize_controller json read failed");
        return;
    }
}

nlohmann::json fan_loop::readJson(std::string path) 
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

int fan_loop::read_temperatures() 
{
    // phosphor::logging::log<phosphor::logging::level::INFO>("read_temperatures enter");
    std::string sense_interface = "xyz.openbmc_project.Sensor.Value";
    std::string property = "Value";
    for (const auto& tempinst : temps) {
        
        std::string path    = tempinst->path;
        std::string service = tempinst->service;

        phosphor::interface::util::async_get_dbus_property<double>(
            conn, service, path, sense_interface, "Value",
            [this, path](const boost::system::error_code& ec, double val) {
                for (const auto& t : temps) {
                    if (t->path == path) {
                        t->value = val;
                        // phosphor::logging::log<phosphor::logging::level::INFO>(("read_temperatures value: " + path + " = " + std::to_string(val)).c_str());
                        break;
                    }
                }
            }
        );
        phosphor::interface::util::async_get_dbus_property<double>(
            conn, service, path,
            "xyz.openbmc_project.Sensor.Threshold.Critical", "CriticalHigh",
            [this, path](const boost::system::error_code& ec, double val) {
                for (const auto& t : temps) {
                    if (t->path == path) {
                        t->crit_high = val;
                        break;
                    }
                }
            }
        );
        phosphor::interface::util::async_get_dbus_property<double>(
            conn, service, path,
            "xyz.openbmc_project.Sensor.Threshold.Critical", "CriticalLow",
            [this, path](const boost::system::error_code& ec, double val) {
                for (const auto& t : temps) {
                    if (t->path == path) {
                        t->crit_low = val;
                        break;
                    }
                }
            }
        );
    }
    // phosphor::logging::log<phosphor::logging::level::INFO>("read_temperatures exit");
    return 0;
}

int fan_loop::get_fan_pwms() 
{

    int default_bitmax = 255;
    // phosphor::logging::log<phosphor::logging::level::INFO>("get_fan_pwms enter");

    std::string pwmstub = "/xyz/openbmc_project/control/fanpwm";
    std::string pwm_sensestub = "/xyz/openbmc_project/sensors/fan_pwm";
    std::string pwm_interface = "xyz.openbmc_project.Control.FanPwm";
    std::string pwm_senseinterface = "xyz.openbmc_project.Sensor.Value";
    try {
        phosphor::interface::util::async_get_subtree(
            conn, pwmstub, 0, {pwm_interface},
            [this, pwm_interface, pwm_sensestub, pwm_senseinterface, default_bitmax](const SubTree& tree) {

                if (tree.empty()) {
                    phosphor::logging::log<phosphor::logging::level::INFO>("get_fan_pwms: empty subtree");
                    return;
                } else {
                    phosphor::logging::log<phosphor::logging::level::INFO>("get_fan_pwms: fans found, tree not empty.");
                }

                for (const auto& [path, smap] : tree) {
                    if (smap.empty()) {
                        continue;
                    }
                    if (std::find(pwm_paths.begin(), pwm_paths.end(), path) !=
                        pwm_paths.end()) {
                        continue;
                    }

                    const std::string service = smap.begin()->first;

                    pwm_paths.push_back(path);
                    auto inst = std::make_unique<fan>();
                    inst->path = path;
                    inst->service = service;
                    size_t pos = path.rfind('/');
                    std::string tname = path.substr(pos + 1);
                    inst->name = tname;
                    fans.push_back(std::move(inst));
                    numfans++;

                    fan* raw = fans.back().get();
                    phosphor::interface::util::async_get_dbus_property<uint64_t>(
                        conn, service, path, pwm_interface, "Target",
                        [path, raw](const boost::system::error_code& ec, uint64_t val) {
                            if (ec)
                            {
                                phosphor::logging::log<phosphor::logging::level::ERR>(("get_fan_pwms Target: " + path +" failed: " + ec.message()).c_str());
                                return;
                            }
                            raw->pwm = val;
                        });

                    std::string pwm_sense_path = pwm_sensestub + "/" + tname;

                    phosphor::interface::util::async_get_dbus_property<int>(
                        conn, service, pwm_sense_path, pwm_senseinterface, "PWM_Bitmax",
                        [pwm_sense_path, raw, default_bitmax](const boost::system::error_code& ec, int val) {
                            if (ec)
                            {
                                phosphor::logging::log<phosphor::logging::level::ERR>(("get_fan_pwms PWM_Bitmax: " + pwm_sense_path +" failed: " + ec.message()).c_str());
                                raw->pwm_bitmax = default_bitmax;
                                return;
                            }
                            raw->pwm_bitmax = val;
                        }
                    );
                }
                buserrors = 0;
                generrors = 0;
            }
        );
    } catch (const sdbusplus::exception::SdBusError& e) {
        buserrors++;
        phosphor::logging::log<phosphor::logging::level::INFO>(("get_fan_pwms sdbus failure num " + std::to_string(buserrors)).c_str());
        phosphor::logging::log<phosphor::logging::level::INFO>(("get_fan_pwms sdbus failure " + std::string(e.what())).c_str());
    } catch (const std::exception& e) {
        generrors++;
        phosphor::logging::log<phosphor::logging::level::INFO>(("get_fan_pwms general failure num " + std::to_string(generrors)).c_str());
        phosphor::logging::log<phosphor::logging::level::INFO>(("get_fan_pwms general failure " + std::string(e.what())).c_str());
    } catch (...) {
        phosphor::logging::log<phosphor::logging::level::INFO>("get_fan_pwms unknown failure");
    }

    for (const auto& fan : fans) {
        fan->pwm = 0;
    }

    return 0;
}

double fan_loop::getcalcval()
{
    std::vector<double> calcs;
    // calcs.reserve(temps.size());

    for (const auto& t : temps) {
        if (t->value < -100) {
            continue;
        }
        const double span = t->crit_high - t->crit_low;
        if (span <= 0.0) {
            continue;
        }
        double calc_val = pow((t->value - t->crit_low) / span, 1.6);
        calcs.push_back(calc_val);
    }

    if (calcs.empty()) {
        return 0;
    }
    return *std::max_element(calcs.begin(), calcs.end());
}

int fan_loop::control_loop() 
{

    if (fans.empty() && (pwm_retries < 20)) {
        phosphor::logging::log<phosphor::logging::level::INFO>("control_loop no fans have been discovered, attempting to discover fans again");
        get_fan_pwms();
        pwm_retries++;
    }
    if (pwm_retries >= 20) {
        running = false;
        return 1;
    }

    if (control_count == 10 || control_count == 20 || control_count == 30) {
        phosphor::logging::log<phosphor::logging::level::INFO>(("control_loop initialize temps again, count: " + std::to_string(control_count)).c_str());
        initialize_temps();
        control_count = control_count + 1;
    } else if (control_count < 30) {
        // phosphor::logging::log<phosphor::logging::level::INFO>(("control_loop count: " + std::to_string(control_count)).c_str());
        control_count = control_count + 1;
    }

    nlohmann::json data = readJson("/usr/share/pid/pid.json");

    std::string pwm_interface = "xyz.openbmc_project.Control.FanPwm";
    double calc_val = 0;
    int nTemps = 0;

    if (linear) {

        calc_val = getcalcval();
        uint64_t new_pwm;
        for (const auto& fan : fans) {
            new_pwm = calc_val * fan->pwm_bitmax;
            if (new_pwm < fan->pwm_bitmax * 0.3) {
                new_pwm = fan->pwm_bitmax * 0.3;
            } else if (new_pwm > fan->pwm_bitmax) {
                new_pwm=fan->pwm_bitmax;
            }
            if (new_pwm != fan->pwm) {
                fan->pwm = new_pwm;
                try {
                    phosphor::interface::util::async_set_dbus_property<uint64_t>(
                        conn,
                        fan->service,
                        fan->path,
                        "xyz.openbmc_project.Control.FanPwm",
                        "Target",
                        fan->pwm,
                        [path = fan->path](const boost::system::error_code& ec) {
                            if (ec) {
                                phosphor::logging::log<phosphor::logging::level::ERR>(("set PWM failed " + path).c_str());
                            }
                        }
                    );
                } catch (const sdbusplus::exception_t& e) {
                    phosphor::logging::log<phosphor::logging::level::ERR>(("control_loop setProperty failed: " + std::string(e.what()) + " path=" + fan->path).c_str());
                } catch (...) {
                    phosphor::logging::log<phosphor::logging::level::ERR>(("control_loop failed to set : " + fan->path + " to value " + std::to_string(fan->pwm)).c_str());
                }
            }
        }
    } else {
        double temp_high;
        for (const auto& tempinst : temps) {
            temp_high = (tempinst->value > temp_high) ? tempinst->value : temp_high;
        }
        double output;
        output = pid_control::ec::pid(controller.get(), temp_high, 0, nullptr);

        if (temp_high < 15) {
            output=0;
        }
        // phosphor::logging::log<phosphor::logging::level::INFO>(("control_loop output of PID: " + std::to_string(output)).c_str());
        for (const auto& fan : fans) {
            // phosphor::logging::log<phosphor::logging::level::INFO>(("control_loop linear attempting to control: " + path).c_str());
            fan->pwm = fan->pwm - (uint64_t)output;
            
            if (fan->pwm > fan->pwm_bitmax) {
                fan->pwm = fan->pwm_bitmax;
            } else if (fan->pwm < 0) {
                fan->pwm = 0;
            }

            phosphor::interface::util::async_set_dbus_property<uint64_t>(
                conn,
                fan->service,
                fan->path,
                "xyz.openbmc_project.Control.FanPwm",
                "Target",
                fan->pwm,
                [path = fan->path](const boost::system::error_code& ec) {
                    if (ec) {
                        phosphor::logging::log<phosphor::logging::level::ERR>(("set PWM failed " + path).c_str());
                    }
                }
            );
        }
        // phosphor::logging::log<phosphor::logging::level::INFO>(("control_loop new PWMs: " + std::to_string(*curPwms[0])).c_str());
    }
    return 0;
}

double fan_loop::get_ts()
{
    return ts;
}