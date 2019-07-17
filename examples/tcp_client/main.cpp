
// ours
#include "tcp_client.hpp"
#include <log.hpp>

#include <afsm/visitor/graphviz_export.hpp>

// thirdparty
#include <asio.hpp>

int main(int argc, char *argv[]) {
    std::string server_address = "127.0.0.1";
    asio::io_service io;

    afsm::visitor::graphviz_export exporter(std::cout);
    client::static_visit(exporter);

    client c(io);
    asio::signal_set sigs(io, SIGINT);

    c.async_wait([&](const std::error_code& ec) {
        if (ec) {
            return log("client failed: {}", ec.message());
        }

        sigs.cancel();
        log("client done");
    }, server_address, "5555");

    sigs.async_wait([&](const std::error_code& ec, int signo) {
        if (ec) {
            log("signal handler completed with: {}", ec.message());
            return;
        }

        log("got SIGINT, exiting");
        c.cancel();
    });
    io.run();
    return 0;
}