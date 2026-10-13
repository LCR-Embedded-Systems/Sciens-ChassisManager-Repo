
#include "../include/fan_manipulation.hpp"

int main()
{
    boost::asio::io_context io;
    auto conn = std::make_shared<sdbusplus::asio::connection>(io);
    conn->request_name("xyz.openbmc_project.lcr_fan_controller");
    sdbusplus::asio::object_server obj_server(conn);

    fan_loop fanInstance(conn, obj_server, io);

    auto work = boost::asio::make_work_guard(io);
    io.run();

    phosphor::logging::log<phosphor::logging::level::ERR>("fan controller failed due to bus errors");
    return 0;
}
