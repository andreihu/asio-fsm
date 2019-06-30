#include "tcp_client.hpp"

#include <graphviz_export.hpp>
#include <helpers.hpp>

#include <asio.hpp>

int main(int argc, char *argv[]) {
    std::string server_address = "127.0.0.1";
    asio::io_service io;

    graphviz_export exporter(std::cout);
    client::static_visit(exporter);

    client c(io);
    asio::signal_set sigs(io, SIGINT);

    c.async_wait([&](const std::error_code& ec) {
        if (ec) {
            return log("client failed: %s", ec.message());
        }

        sigs.cancel();
        log("client done");
    }, "127.0.0.1", "5555");

    sigs.async_wait([&](const std::error_code& ec, int signo) {
        if (ec) {
            log("signal handler completed with: %s", ec.message());
            return;
        }

        log("got SIGINT, exiting");
        c.cancel();
    });
    io.run();
    return 0;
}