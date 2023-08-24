#include "binding/utils.h"

void bind_math(nb::module_&);
void bind_types(nb::module_&);

NB_MODULE(pyvkkk, m) {
    bind_math(m);
    bind_types(m);
}