#include "device.hpp"

#include <stdexcept>
#include <string>


namespace practice {

    void DevicePackage::init() {
        {
            const std::vector<wgpu::InstanceFeatureName> required_features{
                wgpu::InstanceFeatureName::TimedWaitAny,
            };

            const wgpu::InstanceDescriptor instanceDescriptor{
                .requiredFeatureCount = required_features.size(),
                .requiredFeatures = required_features.data()
            };

            instance_ = wgpu::CreateInstance(&instanceDescriptor);
            if (!instance_) {
                throw std::runtime_error("Failed to create WebGPU instance!");
            }
        }

        {
            wgpu::RequestAdapterOptions adapterOptions{};
            instance_.WaitAny(
                instance_.RequestAdapter(
                    &adapterOptions,
                    wgpu::CallbackMode::WaitAnyOnly,
                    [this](
                        wgpu::RequestAdapterStatus status,
                        wgpu::Adapter a,
                        wgpu::StringView msg
                    ) {
                        if (status == wgpu::RequestAdapterStatus::Success)
                            adapter_ = a;
                        else
                            throw std::runtime_error(
                                "RequestAdapter failed: " +
                                std::string(msg.data, msg.length)
                            );
                    }
                ),
                UINT64_MAX
            );
            if (!adapter_)
                throw std::runtime_error("Failed to get WebGPU adapter!");
        }

        {
            wgpu::DeviceDescriptor deviceDesc{};
            instance_.WaitAny(
                adapter_.RequestDevice(
                    &deviceDesc,
                    wgpu::CallbackMode::WaitAnyOnly,
                    [this](
                        wgpu::RequestDeviceStatus status,
                        wgpu::Device d,
                        wgpu::StringView msg
                    ) {
                        if (status == wgpu::RequestDeviceStatus::Success)
                            device_ = d;
                        else
                            throw std::runtime_error(
                                "RequestDevice failed: " +
                                std::string(msg.data, msg.length)
                            );
                    }
                ),
                UINT64_MAX
            );
            if (!device_)
                throw std::runtime_error("Failed to get WebGPU device!");
        }
    }

    wgpu::SurfaceCapabilities DevicePackage::get_surface_caps(
        const wgpu::Surface& surface
    ) const {
        wgpu::SurfaceCapabilities caps;
        surface.GetCapabilities(adapter_, &caps);
        if (caps.formatCount == 0) {
            throw std::runtime_error("Surface has no supported formats!");
        }
        return caps;
    }

}  // namespace practice
