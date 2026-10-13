
#include "../include/chassis_control.hpp"

int main()
{

    boost::asio::io_context io;
    auto bus = std::make_shared<sdbusplus::asio::connection>(io);

    bus->request_name("xyz.openbmc_project.ChassisControl");

    sdbusplus::asio::object_server obj_server(bus);

    Chassis_Exposer instance(bus, obj_server);

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    instance.expose_to_DBus();

    io.run();

    return 0;
}
