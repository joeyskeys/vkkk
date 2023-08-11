#include "binding/utils.h"

void bind_types(nb::module_&);

NB_MODULE(pyvkkk, m) {
    bind_types(m);
}