#include <glm/glm.hpp>

#include "binding/utils.h"

template <typename V>
nb::class_<V> bind_vec(nb::module_& m, const char* name) {
    nb::class_<V> vccl(m, name);
    
    vccl.def(nb::init<>())
        .def(nb::init<const float>())
        .def(nb::self + nb::self)
        .def(nb::self + float())
        .def(float() + nb::self)
        .def(nb::self += nb::self)
        .def(nb::self += float())
        .def(nb::self - nb::self)
        .def(nb::self - float())
        .def(float() - nb::self)
        .def(nb::self -= float())
        .def(nb::self -= nb::self)
        .def(nb::self * nb::self)
        .def(nb::self * float())
        .def(float() * nb::self)
        .def(nb::self *= nb::self)
        .def(nb::self *= float())
        .def(nb::self / nb::self)
        .def(nb::self / float())
        .def(float() / nb::self)
        .def(nb::self /= nb::self)
        .def(nb::self /= float());

    return vccl;
}

template <typename M>
nb::class_<M> bind_mat(nb::module_& m, const char* name) {
    nb::class_<M> mtcl(m, name);

    mtcl.def(nb::init<>())
        .def(nb::init<const float>())
        .def(nb::self + nb::self)
        .def(nb::self + float())
        .def(float() + nb::self)
        .def(nb::self += nb::self)
        .def(nb::self += float())
        .def(nb::self - nb::self)
        .def(nb::self - float())
        .def(float() - nb::self)
        .def(nb::self -= nb::self)
        .def(nb::self -= float())
        .def(nb::self * float())
        .def(float() * nb::self)
        .def(nb::self * nb::self)
        .def(nb::self / float())
        .def(float() / nb::self);

    return mtcl;
}

void bind_math(nb::module_& m) {
    auto v3cl = bind_vec<glm::vec3>(m, "Vec3");
    v3cl.def(nb::init<const float, const float, const float>());
    auto v4cl = bind_vec<glm::vec4>(m, "Vec4");
    v4cl.def(nb::init<const float, const float, const float, const float>());

    auto m3cl = bind_mat<glm::mat3>(m, "Mat3");
    m3cl.def(nb::init<const float, const float, const float,
        const float, const float, const float,
        const float, const float, const float>())
        .def(nb::self * glm::vec3())
        .def(glm::vec3() * nb::self);
    auto m4cl = bind_mat<glm::mat4>(m, "Mat4");
    m4cl.def(nb::init<
        const float, const float, const float, const float,
        const float, const float, const float, const float,
        const float, const float, const float, const float,
        const float, const float, const float, const float>())
        .def(nb::self * glm::vec4())
        .def(glm::vec4() * nb::self);
}