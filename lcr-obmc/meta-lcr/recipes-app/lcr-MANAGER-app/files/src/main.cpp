#include "../include/MANAGER.hpp"

#ifdef SIMULATION
static constexpr bool is_simulation = true;
#else
static constexpr bool is_simulation = false;
#endif

using namespace std;

/*

GPI0 is used for the front panel switch
Drive ps inhibit 1 based on that. Always follow that signal
Reflect whatever is done on ps inhibit 2

*/

int main()
{

    boost::asio::io_context io;
    auto bus = std::make_shared<sdbusplus::asio::connection>(io);

    bus->request_name("xyz.openbmc_project.Manager");

    sdbusplus::asio::object_server obj_server(bus);

    phosphor::logging::log<phosphor::logging::level::INFO>("MANAGER: instantiating chm manager");
    Manager chm(is_simulation);

    //make thread for checking if gpio service is active and publishing. Publish value of sysreset and ps enable when it is active.
    thread gpioThread([&chm]() {
        chm.watch_gpios();
    });

    chm.set_sr();
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    phosphor::logging::log<phosphor::logging::level::INFO>("MANAGER: starting heartbeat thread");
    thread logThread([&chm]() {
        int heartbeat = 18;
        while(true) {
            chm.write_logs_to_flash();
            std::this_thread::sleep_for(std::chrono::milliseconds(60000));
            if (heartbeat % 20 == 0) {
                heartbeat = 0;
                chm.write_heartbeat();
            }
            heartbeat++;
        }
    });
    phosphor::logging::log<phosphor::logging::level::INFO>("MANAGER: checking if simulated");
    if (!is_simulation) {
        phosphor::logging::log<phosphor::logging::level::INFO>("MANAGER: Running wait for switch...");
        while (chm.wait_for_switch() == false) {std::this_thread::sleep_for(std::chrono::milliseconds(500));}
    
        phosphor::logging::log<phosphor::logging::level::INFO>("MANAGER: Running check fan control...");
        if (chm.getfancontrol()) {
            while (chm.get_fan_controller_status() == false) {std::this_thread::sleep_for(std::chrono::milliseconds(500));}
        }
        
        phosphor::logging::log<phosphor::logging::level::INFO>("MANAGER: Setting power supply inhibits...");
        while (chm.set_ps_inh1() == false || chm.set_ps_inh2() == false) {std::this_thread::sleep_for(std::chrono::milliseconds(500));}
    
        phosphor::logging::log<phosphor::logging::level::INFO>("MANAGER: Setting power state...");
        chm.set_power_state();
    
        int numchecks=0;
        phosphor::logging::log<phosphor::logging::level::INFO>("MANAGER: Waiting for safe voltages...");
        while (chm.update_readings(numchecks) == false) {std::this_thread::sleep_for(std::chrono::milliseconds(500));numchecks++;}
    
        phosphor::logging::log<phosphor::logging::level::INFO>("MANAGER: Unsetting system reset");
        while (chm.unset_sr() == false) {std::this_thread::sleep_for(std::chrono::milliseconds(500));}
    
    } else {
        phosphor::logging::log<phosphor::logging::level::INFO>("MANAGER: setting simulated system reset flag");
        chm.sim_sr();
    }
    chm.write_logs_to_flash();

    thread watcherThread([&chm]() {
        chm.watch_alarms();
    });

    io.run();

    while (true) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        // had an alarm go high
    }

    return 0;
}
