#pragma once

namespace afsm {

template<typename SourceState, typename Event, typename NextState>
struct transition {
    using source = SourceState;
    using event = Event;
    using next = NextState;
};
    
} // namespace afsm