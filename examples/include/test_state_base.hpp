#pragma once

// ours
#include <log.hpp>

#include <afsm/state.hpp>
#include <afsm/util/type_name.hpp>

// thirdparty
#include <asio.hpp>

template<typename Host, typename ...Events>
class test_state_base : public afsm::state<Events...> {
public:
    test_state_base(asio::io_service& io) :
        afsm::state<Events...>(io) 
    {
        log("entering {}", afsm::util::type_name<Host>());
    }
    virtual ~test_state_base() = default;
};
