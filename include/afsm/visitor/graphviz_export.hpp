#pragma once

// ours
#include <afsm/util/type_name.hpp>

// thirdparty
#include <fmt/format.h>

// std
#include <iostream>
#include <utility>

namespace afsm {
namespace visitor {

struct graphviz_export {
    graphviz_export(std::ostream& out) : out(out), start_state_visited(false) {}

    template<typename MachineTraits>
    void start() {
        out << "digraph {" << std::endl;
        out << "node [shape=\"rectangle\"]" <<std::endl;
        out << "completed [shape=\"ellipse\"]" << std::endl;
        if (std::exchange(start_state_visited, false)) {
            out << util::type_name<typename MachineTraits::start_state>() << " [shape=\"diamond\"]" << std::endl;
        }
    }

    template<typename MachineTraits>
    void end() {
        out << "}" << std::endl;
    }

    template<typename Transition>
    void operator()() {
        // "a" -> "b"[label="foobar"];
        out << fmt::format("\"{}\" -> \"{}\" [label=\"{}\"]\n", util::type_name<typename Transition::source>(), util::type_name<typename Transition::next>(), util::type_name<typename Transition::event>());
    }

    std::ostream& out;
    bool start_state_visited;
};

} // namespace visitors
} // namespace afsm
