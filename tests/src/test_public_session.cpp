// This translation unit deliberately includes only the supported platform facade.
#include <metasequoia/session.h>
#ifdef SQLITE_VERSION
#error "The public session facade must not expose SQLite headers"
#endif
#include <stdexcept>

void test_public_session_interface()
{
    bool rejected = false;
    try
    {
        metasequoia::Session session(metasequoia::SessionOptions{});
    }
    catch (const std::invalid_argument &)
    {
        rejected = true;
    }
    if (!rejected)
        throw std::runtime_error("Public session accepted unspecified runtime paths");
}
