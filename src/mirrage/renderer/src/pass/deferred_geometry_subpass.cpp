#include <mirrage/renderer/pass/deferred_geometry_subpass.hpp>

#include <mirrage/renderer/animation_comp.hpp>
#include <mirrage/renderer/model_comp.hpp>
#include <mirrage/renderer/pass/deferred_pass.hpp>

#include <mirrage/ecs/components/transform_comp.hpp>
#include <mirrage/ecs/ecs.hpp>
#include <mirrage/ecs/entity_set_view.hpp>
#include <mirrage/graphic/render_pass.hpp>
#include <mirrage/renderer/model.hpp>

using mirrage::ecs::components::Transform_comp;

namespace mirrage::renderer {

	Deferred_geometry_subpass::Deferred_geometry_subpass(Deferred_renderer& r, ecs::Entity_manager& entities)
	  : _ecs(entities), _renderer(r)
	{
		entities.register_component_type<Model_comp>();
		entities.register_component_type<Pose_comp>();
		entities.register_component_type<Shared_pose_comp>();
	}

	void Deferred_geometry_subpass::configure_pipeline(Deferred_renderer&             renderer,
	                                                   graphic::Pipeline_description& p)
	{
		p.rasterization.cullMode = vk::CullModeFlagBits::eNone;
		p.add_descriptor_set_layout(renderer.model_descriptor_set_layout());
		p.vertex<Model_vertex>(
		        0, false, 0, &Model_vertex::position, 1, &Model_vertex::normal, 2, &Model_vertex::tex_coords);
	}
	void Deferred_geometry_subpass::configure_subpass(Deferred_renderer&, graphic::Subpass_builder& pass)
	{
		pass.stage("default"_strid)
		        .shader("frag_shader:model"_aid, graphic::Shader_stage::fragment)
		        .shader("vert_shader:model"_aid, graphic::Shader_stage::vertex)
		        .color_mask(3, vk::ColorComponentFlags{})
		        .color_mask(4, vk::ColorComponentFlags{});

		pass.stage("emissive"_strid)
		        .shader("frag_shader:model_emissive"_aid, graphic::Shader_stage::fragment)
		        .shader("vert_shader:model"_aid, graphic::Shader_stage::vertex)
		        .color_mask(3, graphic::all_color_components)
		        .color_mask(4, graphic::all_color_components);

		pass.stage("alphatest"_strid)
		        .shader("frag_shader:model_alphatest"_aid, graphic::Shader_stage::fragment)
		        .shader("vert_shader:model"_aid, graphic::Shader_stage::vertex)
		        .color_mask(3, vk::ColorComponentFlags{})
		        .color_mask(4, vk::ColorComponentFlags{});
	}

	void Deferred_geometry_subpass::configure_animation_pipeline(Deferred_renderer&             renderer,
	                                                             graphic::Pipeline_description& p)
	{

		p.rasterization.cullMode = vk::CullModeFlagBits::eNone;
		p.add_descriptor_set_layout(renderer.model_descriptor_set_layout());
		p.add_descriptor_set_layout(*renderer.gbuffer().animation_data_layout);
		p.vertex<Model_rigged_vertex>(0,
		                              false,
		                              0,
		                              &Model_rigged_vertex::position,
		                              1,
		                              &Model_rigged_vertex::normal,
		                              2,
		                              &Model_rigged_vertex::tex_coords,
		                              3,
		                              &Model_rigged_vertex::bone_ids,
		                              4,
		                              &Model_rigged_vertex::bone_weights);
	}
	void Deferred_geometry_subpass::configure_animation_subpass(Deferred_renderer&        renderer,
	                                                            graphic::Subpass_builder& pass)
	{
		// TODO: technically UB if !independentBlend, because the shaders never write the attachments
		//         but seems to work fine on GTX 1060 and would only affect Intel<=Ivy Bridge and Android
		auto gpu_features = renderer.device().physical_device().getFeatures();
		auto ignore_mask =
		        gpu_features.independentBlend ? vk::ColorComponentFlags{} : graphic::all_color_components;

		pass.stage("default"_strid)
		        .shader("frag_shader:model"_aid, graphic::Shader_stage::fragment)
		        .shader("vert_shader:model_animated"_aid, graphic::Shader_stage::vertex)
		        .color_mask(3, ignore_mask)
		        .color_mask(4, ignore_mask);

		pass.stage("emissive"_strid)
		        .shader("frag_shader:model_emissive"_aid, graphic::Shader_stage::fragment)
		        .shader("vert_shader:model_animated"_aid, graphic::Shader_stage::vertex)
		        .color_mask(3, graphic::all_color_components)
		        .color_mask(4, graphic::all_color_components);

		pass.stage("alphatest"_strid)
		        .shader("frag_shader:model_alphatest"_aid, graphic::Shader_stage::fragment)
		        .shader("vert_shader:model_animated"_aid, graphic::Shader_stage::vertex)
		        .color_mask(3, ignore_mask)
		        .color_mask(4, ignore_mask);


		pass.stage("dq_default"_strid)
		        .shader("frag_shader:model"_aid, graphic::Shader_stage::fragment)
		        .shader("vert_shader:model_animated_dqs"_aid, graphic::Shader_stage::vertex)
		        .color_mask(3, ignore_mask)
		        .color_mask(4, ignore_mask);

		pass.stage("dq_emissive"_strid)
		        .shader("frag_shader:model_emissive"_aid, graphic::Shader_stage::fragment)
		        .shader("vert_shader:model_animated_dqs"_aid, graphic::Shader_stage::vertex)
		        .color_mask(3, graphic::all_color_components)
		        .color_mask(4, graphic::all_color_components);

		pass.stage("dq_alphatest"_strid)
		        .shader("frag_shader:model_alphatest"_aid, graphic::Shader_stage::fragment)
		        .shader("vert_shader:model_animated_dqs"_aid, graphic::Shader_stage::vertex)
		        .color_mask(3, ignore_mask)
		        .color_mask(4, ignore_mask);
	}

	void Deferred_geometry_subpass::update(util::Time) {}

	void Deferred_geometry_subpass::pre_draw(Frame_data& frame)
	{
		// select relevant draw commands and partition into normal and rigged geometry
		auto geo_range = frame.partition_geometry(1u);

		auto rigged_begin = std::find_if(
		        geo_range.begin(), geo_range.end(), [](auto& geo) { return geo.model->rigged(); });

		_geometry_range        = util::range(geo_range.begin(), rigged_begin);
		_rigged_geometry_range = util::range(rigged_begin, geo_range.end());
	}

	void Deferred_geometry_subpass::draw(Frame_data& frame, graphic::Render_pass& render_pass)
	{
		auto _ = _renderer.profiler().push("Geometry");

		Deferred_push_constants dpc{};

		auto last_substance_id = ""_strid;
		auto last_material     = static_cast<const Material*>(nullptr);
		auto last_model        = static_cast<const Model*>(nullptr);

		auto prepare_draw = [&](auto& geo) {
			auto& sub_mesh = geo.model->sub_meshes().at(geo.sub_mesh);

			if(geo.substance_id != last_substance_id) {
				last_substance_id = geo.substance_id;
				render_pass.set_stage(geo.substance_id);
			}

			if(&*sub_mesh.material != last_material) {
				last_material = &*sub_mesh.material;
				last_material->bind(render_pass);
			}

			if(geo.model != last_model) {
				last_model = geo.model;
				geo.model->bind_mesh(frame.main_command_buffer, 0);
			}

			dpc.model    = glm::toMat4(geo.orientation) * glm::scale(glm::mat4(1.f), geo.scale);
			dpc.model[3] = glm::vec4(geo.position, 1.f);
			dpc.model    = _renderer.global_uniforms().view_mat * dpc.model;

			if(sub_mesh.material->substance_id() == "emissive"_strid) {
				auto emissive_color =
				        _ecs.get(geo.entity)
				                .get_or_throw()
				                .template get<Material_property_comp>()
				                .process(glm::vec3(200, 200, 200), [](auto& m) { return m.emissive_color; });

				dpc.light_data = glm::vec4(emissive_color / 10000.0f, 1.f);
			}
		};


		for(auto& geo : _geometry_range) {
			auto& sub_mesh = geo.model->sub_meshes().at(geo.sub_mesh);
			prepare_draw(geo);

			render_pass.push_constant("dpc"_strid, dpc);

			frame.main_command_buffer.drawIndexed(sub_mesh.index_count, 1, sub_mesh.index_offset, 0, 0);
		}

		// draw all animated models in a new subpass
		render_pass.next_subpass();
		last_substance_id = ""_strid;
		last_material     = static_cast<const Material*>(nullptr);
		last_model        = static_cast<const Model*>(nullptr);

		for(auto& geo : _rigged_geometry_range) {
			auto& sub_mesh = geo.model->sub_meshes().at(geo.sub_mesh);
			prepare_draw(geo);

			render_pass.push_constant("dpc"_strid, dpc);

			auto uniform_offset = geo.animation_uniform_offset.get_or_throw();
			render_pass.bind_descriptor_set(2, _renderer.gbuffer().animation_data, {&uniform_offset, 1u});

			frame.main_command_buffer.drawIndexed(sub_mesh.index_count, 1, sub_mesh.index_offset, 0, 0);
		}
	}
} // namespace mirrage::renderer
