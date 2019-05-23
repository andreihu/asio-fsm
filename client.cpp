#include "helpers.hpp"

#include "client.hpp"
#include "helpers.hpp"

#include <asio.hpp>

int main(int argc, char *argv[]) {
    std::string server_address = "127.0.0.1";
    asio::io_service io;
    client c(io);
    c.async_wait("127.0.0.1", "5555", [&](const std::error_code& ec) {
        if (ec) {
            return log("client failed: %s", ec.message());
        }

        log("client done");
    });
    io.run();
    return 0;

}