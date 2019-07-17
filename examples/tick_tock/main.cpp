#include "tick_tock.hpp"

#include <log.hpp>

#include <afsm/state_machine.hpp>
#include <afsm/visitor/graphviz_export.hpp>

int main() {
    afsm::visitor::graphviz_export exporter(std::cout);
    tick_tock::static_visit(exporter);

    asio::io_service io;
    tick_tock tt(io);

    asio::signal_set sigs(io, SIGINT);

    tt.async_wait([&](const std::error_code& ec) {
        if (ec) {
            return log("client failed: {}", ec.message());
        }

        sigs.cancel();
        log("client done");
    });
    sigs.async_wait([&](const std::error_code& ec, int signo) {
        if (ec) {
            log("signal handler completed with: {}", ec.message());
            return;
        }

        log("got SIGINT, exiting");
        tt.cancel();
    });
    io.run();
    return 0;
}