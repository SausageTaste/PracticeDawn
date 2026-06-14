#pragma once

#include <glm/gtc/quaternion.hpp>


namespace practice {

    class TransformQuat {

    public:
        void set_pos(double x, double y, double z);

        void set_quat(double w, double x, double y, double z);
        void rotate(double radians, const glm::dvec3& axis);
        void look_along(const glm::dvec3& dir, const glm::dvec3& up);
        void look_along(const glm::dvec3& dir);
        void reset_rotation();

        void set_scale(double scalar);

        void apply_transform(const glm::dmat4& m);

        glm::dvec3 make_forward_dir() const;
        glm::dvec3 make_up_dir() const;
        glm::dvec3 make_right_dir() const;

        glm::dmat4 make_model_mat() const;
        glm::dmat4 make_view_mat() const;

    public:
        glm::dquat rot_{ 1, 0, 0, 0 };
        glm::dvec3 pos_{ 0, 0, 0 };
        glm::dvec3 scale_{ 1, 1, 1 };
    };

}  // namespace practice
