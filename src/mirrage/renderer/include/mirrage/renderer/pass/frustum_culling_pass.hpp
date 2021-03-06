#pragma once

#include <mirrage/renderer/deferred_renderer.hpp>

namespace mirrage::renderer {

	class Frustum_culling_pass : public Render_pass {
	  public:
		Frustum_culling_pass(Deferred_renderer&, ecs::Entity_manager&);


		void update(util::Time dt) override;
		void draw(Frame_data&) override;

		auto name() const noexcept -> const char* override { return "Frustum_culling"; }

	  private:
		Deferred_renderer&   _renderer;
		ecs::Entity_manager& _ecs;
	};

	class Frustum_culling_pass_factory : public Render_pass_factory {
	  public:
		auto create_pass(Deferred_renderer&, ecs::Entity_manager&, Engine&, bool&)
		        -> std::unique_ptr<Render_pass> override;

		auto rank_device(vk::PhysicalDevice, util::maybe<std::uint32_t>, int) -> int override;

		void configure_device(vk::PhysicalDevice,
		                      util::maybe<std::uint32_t>,
		                      graphic::Device_create_info&) override;
	};
} // namespace mirrage::renderer
