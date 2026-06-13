#pragma once

#include <webgpu/webgpu_cpp.h>


namespace practice {

    class Device {

    public:
        void init();

        wgpu::CommandEncoder create_cmd_encoder() const {
            return device_.CreateCommandEncoder();
        }
        void process_events() const { instance_.ProcessEvents(); }

        wgpu::SurfaceCapabilities get_surface_caps(const wgpu::Surface&) const;

        wgpu::Instance instance_;
        wgpu::Adapter adapter_;
        wgpu::Device device_;
    };

}  // namespace practice
