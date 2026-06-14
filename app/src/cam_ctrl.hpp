#pragma once

#include <SDL3/SDL_keycode.h>
#include <glm/glm.hpp>

#include "transform.hpp"


namespace practice {

    struct CameraInput {
        bool move_forward = false;
        bool move_back = false;
        bool move_left = false;
        bool move_right = false;
        bool move_up = false;
        bool move_down = false;
        bool fast = false;
        bool mouse_look = false;
        double yaw = 0.0;
        double pitch = 0.0;
    };


    glm::dvec3 make_camera_forward(double yaw, double pitch);

    void set_key(CameraInput& input, SDL_Keycode key, bool down);

    void update_camera(
        practice::TransformQuat& camera_view,
        const CameraInput& input,
        double dt
    );

}  // namespace practice
