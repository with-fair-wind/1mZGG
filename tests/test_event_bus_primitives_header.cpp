#include "dss/core/detail/event_bus_primitives.h"

namespace {

struct CombinerWithAccumulator {
    struct Accumulator {};
};

struct CombinerWithoutAccumulator {};

static_assert(Dss::Evt::detail::HasAccumulator<CombinerWithAccumulator>::value);
static_assert(!Dss::Evt::detail::HasAccumulator<CombinerWithoutAccumulator>::value);

} // namespace

void eventBusPrimitivesHeaderCompilesStandalone() {}
