#include "cam_ctrl.hpp"


namespace practice {

    glm::dvec3 make_camera_forward(double yaw, double pitch) {
        const auto cos_pitch = std::cos(pitch);
        return glm::normalize(
            glm::dvec3(
                std::sin(yaw) * cos_pitch,
                std::sin(pitch),
                -std::cos(yaw) * cos_pitch
            )
        );
    }

    void set_key(CameraInput& input, SDL_Keycode key, bool down) {
        switch (key) {
            case SDLK_W:
                input.move_forward = down;
                break;
            case SDLK_S:
                input.move_back = down;
                break;
            case SDLK_A:
                input.move_left = down;
                break;
            case SDLK_D:
                input.move_right = down;
                break;
            case SDLK_E:
                input.move_up = down;
                break;
            case SDLK_Q:
                input.move_down = down;
                break;
            case SDLK_LSHIFT:
            case SDLK_RSHIFT:
                input.fast = down;
                break;
        }
    }

    void update_camera(
        practice::TransformQuat& camera_view,
        const CameraInput& input,
        double dt
    ) {
        const auto forward = make_camera_forward(input.yaw, input.pitch);
        const auto world_up = glm::dvec3(0, 1, 0);
        const auto right = glm::normalize(glm::cross(forward, world_up));

        glm::dvec3 move(0);
        if (input.move_forward)
            move += forward;
        if (input.move_back)
            move -= forward;
        if (input.move_right)
            move += right;
        if (input.move_left)
            move -= right;
        if (input.move_up)
            move += world_up;
        if (input.move_down)
            move -= world_up;

        if (glm::dot(move, move) > 0.0) {
            const auto speed = input.fast ? 8.0 : 3.0;
            camera_view.pos_ += glm::normalize(move) * speed * dt;
        }

        camera_view.look_along(forward, world_up);
    }

}  // namespace practice
