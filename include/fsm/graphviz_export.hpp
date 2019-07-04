#pragma once

#include <iostream>
#include <utility>

#include <fsm/helpers.hpp>

struct graphviz_export {
    graphviz_export(std::ostream& out) : out(out), start_state_visited(false) {}

    template<typename MachineTraits>
    void start() {
        out << "digraph {" << std::endl;
        out << "node [shape=\"rectangle\"]" <<std::endl;
        out << "completed [shape=\"ellipse\"]" << std::endl;
        if (std::exchange(start_state_visited, false)) {
            out << type_name<typename MachineTraits::start_state>() << " [shape=\"diamond\"]" << std::endl;
        }
    }

    template<typename MachineTraits>
    void end() {
        out << "}" << std::endl;
    }

    template<typename Transition>
    void operator()() {
        // "a" -> "b"[label="foobar"];
        out << tfm::format("\"%s\" -> \"%s\" [label=\"%s\"]\n", type_name<typename Transition::source>(), type_name<typename Transition::next>(), type_name<typename Transition::event>());
    }

    std::ostream& out;
    bool start_state_visited;
};