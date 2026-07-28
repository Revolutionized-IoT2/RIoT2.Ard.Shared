#include <riot2/TlsRootCa.h>

namespace riot2 {

const char* rootCaPem() {
#if defined(RIOT2_ROOT_CA_PEM)
    return RIOT2_ROOT_CA_PEM;
#else
    return "";
#endif
}

}  // namespace riot2
