#pragma once

#include "events.hpp"

#include <fsm/state.hpp>

#include <asio.hpp>

#include <chrono>

class ticked : public state<tock, terminated> {
public:
    ticked(asio::io_service& io) :
        state(io),
        timer(io)
    {}

    virtual void on_enter() override {
        timer.expires_from_now(std::chrono::seconds(3));
        timer.async_wait(track([this](const std::error_code& ec) {
            ec ? complete<terminated>(ec) : complete<tock>();
        }));
    }

    virtual void cancel() override {
        complete<terminated>();
        timer.cancel();
    }
private:
    asio::steady_timer timer;
};

class tocked : public state<tock, terminated> {
public:
    tocked(asio::io_service& io) :
        state(io),
        timer(io)
    {}

    virtual void on_enter() override {
        timer.expires_from_now(std::chrono::seconds(3));
        timer.async_wait(track([this](const std::error_code& ec) {
            ec ? complete<terminated>(ec) : complete<tock>();
        }));
    }

    virtual void cancel() override {
        complete<terminated>();
        timer.cancel();
    }
private:
    asio::steady_timer timer;
};

struct completed {
    completed(asio::io_service& io, const std::error_code& ec) {}
};
