#pragma once

#include <mirrage/ecs/entity_handle.hpp>
#include <mirrage/utils/maybe.hpp>
#include <mirrage/utils/ranges.hpp>
#include <mirrage/utils/str_id.hpp>
#include <mirrage/utils/units.hpp>

#include <glm/gtx/quaternion.hpp>
#include <glm/vec3.hpp>
#include <vulkan/vulkan.hpp>

#include <functional>
#include <variant>


namespace mirrage {
	class Engine;
} // namespace mirrage

namespace mirrage::ecs {
	class Entity_manager;
	namespace components {
		struct Transform_comp;
	}
} // namespace mirrage::ecs

namespace mirrage::graphic {
	struct Device_create_info;
} // namespace mirrage::graphic

namespace mirrage::renderer {
	class Deferred_renderer;
	class Model;
	class Directional_light_comp;
	class Point_light_comp;
	struct Camera_state;
	struct Sub_mesh;
} // namespace mirrage::renderer


namespace mirrage::renderer {
	using Command_buffer_source = std::function<vk::CommandBuffer()>;

	struct Geometry {
		ecs::Entity_handle         entity;
		glm::vec3                  position{0, 0, 0};
		glm::quat                  orientation{1, 0, 0, 0};
		glm::vec3                  scale{1.f, 1.f, 1.f};
		const Model*               model;
		util::Str_id               substance_id;
		std::uint32_t              sub_mesh;
		std::uint32_t              culling_mask;
		util::maybe<std::uint32_t> animation_uniform_offset;

		Geometry() = default;
		Geometry(ecs::Entity_handle entity,
		         glm::vec3          position,
		         glm::quat          orientation,
		         glm::vec3          scale,
		         const Model*       model,
		         util::Str_id       substance_id,
		         std::uint32_t      sub_mesh,
		         std::uint32_t      culling_mask)
		  : entity(entity)
		  , position(position)
		  , orientation(orientation)
		  , scale(scale)
		  , model(model)
		  , substance_id(substance_id)
		  , sub_mesh(sub_mesh)
		  , culling_mask(culling_mask)
		{
		}
	};

	struct Light {
		using Transform_comp = mirrage::ecs::components::Transform_comp;
		using Light_comp     = std::variant<Directional_light_comp*, Point_light_comp*>;

		ecs::Entity_handle entity;
		Transform_comp*    transform;
		Light_comp         light;
		std::uint32_t      shadow_culling_mask;

		Light() = default;
		template <class L>
		Light(ecs::Entity_handle entity,
		      Transform_comp&    transform,
		      L&                 light,
		      std::uint32_t      shadow_culling_mask)
		  : entity(entity), transform(&transform), light(&light), shadow_culling_mask(shadow_culling_mask)
		{
		}
	};


	struct Debug_geometry {
		glm::vec3 start;
		glm::vec3 end;
		util::Rgb color;
	};

	class Frame_data {
	  public:
		vk::CommandBuffer main_command_buffer;
		vk::DescriptorSet global_uniform_set;
		std::size_t       swapchain_image;

		std::vector<Geometry>       geometry_queue;
		std::vector<Light>          light_queue;
		std::vector<Debug_geometry> debug_geometry_queue;

		auto partition_geometry(std::uint32_t mask) -> util::vector_range<Geometry>;
	};

	class Render_pass {
	  public:
		virtual ~Render_pass() = default;

		virtual void update(util::Time dt) = 0;
		virtual void draw(Frame_data&)     = 0;

		virtual void process_camera(Camera_state&) {} //< allows passes to modify the current camera

		virtual auto name() const noexcept -> const char* = 0;
	};

	class Render_pass_factory {
	  public:
		virtual ~Render_pass_factory() = default;

		virtual auto create_pass(Deferred_renderer&,
		                         ecs::Entity_manager&,
		                         Engine&,
		                         bool& write_first_pp_buffer) -> std::unique_ptr<Render_pass> = 0;

		virtual auto rank_device(vk::PhysicalDevice,
		                         util::maybe<std::uint32_t> graphics_queue,
		                         int                        current_score) -> int
		{
			(void) graphics_queue;
			return current_score;
		}

		virtual void configure_device(vk::PhysicalDevice,
		                              util::maybe<std::uint32_t> graphics_queue,
		                              graphic::Device_create_info&)
		{
			(void) graphics_queue;
		}
	};

} // namespace mirrage::renderer
