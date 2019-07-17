#pragma once

// ours
#include "events.hpp"

#include <test_state_base.hpp>

#include <afsm/state.hpp>

// thirdparty
#include <asio.hpp>

// std
#include <chrono>

class ticked : public test_state_base<ticked, tock, terminated> {
public:
    ticked(asio::io_service& io) :
        test_state_base(io),
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

class tocked : public test_state_base<tocked, tick, terminated> {
public:
    tocked(asio::io_service& io) :
        test_state_base(io),
        timer(io)
    {}

    virtual void on_enter() override {
        timer.expires_from_now(std::chrono::seconds(3));
        timer.async_wait(track([this](const std::error_code& ec) {
            ec ? complete<terminated>(ec) : complete<tick>();
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
