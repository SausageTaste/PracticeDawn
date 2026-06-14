#include "transform.hpp"

#include <glm/glm.hpp>


namespace {

    template <typename T>
    glm::tquat<T> rotate_quat(
        const glm::tquat<T>& q, T radians, const glm::tvec3<T>& axis
    ) {
        return glm::normalize(glm::angleAxis<T>(radians, axis) * q);
    }

}  // namespace


namespace practice {

    void TransformQuat::set_pos(double x, double y, double z) {
        pos_.x = x;
        pos_.y = y;
        pos_.z = z;
    }

    void TransformQuat::set_quat(double w, double x, double y, double z) {
        rot_.w = w;
        rot_.x = x;
        rot_.y = y;
        rot_.z = z;
        rot_ = glm::normalize(rot_);
    }

    void TransformQuat::rotate(double radians, const glm::dvec3& axis) {
        rot_ = rotate_quat(rot_, radians, axis);
    }

    void TransformQuat::look_along(
        const glm::dvec3& dir, const glm::dvec3& up
    ) {
        const auto forward = glm::normalize(dir);
        const auto right = glm::normalize(glm::cross(forward, up));
        const auto ortho_up = glm::cross(right, forward);

        glm::dmat3 rot_mat(right, ortho_up, -forward);
        rot_ = glm::normalize(glm::quat_cast(rot_mat));
    }

    void TransformQuat::look_along(const glm::dvec3& dir) {
        return this->look_along(dir, glm::dvec3(0, 1, 0));
    }

    void TransformQuat::reset_rotation() { rot_ = glm::dquat(1, 0, 0, 0); }

    void TransformQuat::set_scale(double scalar) {
        scale_.x = scalar;
        scale_.y = scalar;
        scale_.z = scalar;
    }

    void TransformQuat::apply_transform(const glm::dmat4& m) {
        pos_ = glm::dvec3(m * glm::dvec4(pos_, 1));
        rot_ = glm::quat_cast(m) * rot_;
    }

    glm::dvec3 TransformQuat::make_forward_dir() const {
        return glm::normalize(glm::mat3_cast(rot_) * glm::dvec3(0, 0, -1));
    }

    glm::dvec3 TransformQuat::make_up_dir() const {
        return glm::normalize(glm::mat3_cast(rot_) * glm::dvec3(0, 1, 0));
    }

    glm::dvec3 TransformQuat::make_right_dir() const {
        return glm::normalize(glm::mat3_cast(rot_) * glm::dvec3(1, 0, 0));
    }

    glm::dmat4 TransformQuat::make_model_mat() const {
        const auto rot_mat = glm::mat4_cast(rot_);
        const auto scale_mat = glm::scale(glm::dmat4(1), scale_);
        const auto translate_mat = glm::translate(glm::dmat4(1), pos_);
        return translate_mat * rot_mat * scale_mat;
    }

    glm::dmat4 TransformQuat::make_view_mat() const {
        const auto rot_mat = glm::mat4_cast(glm::conjugate(rot_));
        const auto tran_mat = glm::translate(glm::dmat4(1), -pos_);
        return rot_mat * tran_mat;
    }

}  // namespace practice
