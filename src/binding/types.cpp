#include "binding/utils.h"
#include "vk_ins/vkabstraction.h"

using namespace vkkk;

void bind_types(nb::module_& m) {
    nb::class_<VkWrappedInstance> pycl(m, "VkInstance");

    pycl.def(nb::init<>());
}