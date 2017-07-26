#include <mirrage/renderer/pass/gi_pass.hpp>

#include <mirrage/graphic/window.hpp>


namespace mirrage {
namespace renderer {
	
	using namespace graphic;
	

	namespace {
		struct Gi_constants {
			glm::mat4 reprojection; // prev_view * inverse(view)
			glm::mat4 prev_projection; // m[3].xy = current level, max level
		};


		auto build_integrate_brdf_render_pass(Deferred_renderer& renderer,
		                                      vk::Format target_format,
		                                      Render_target_2D& target,
		                                      Framebuffer& out_framebuffer) {

			auto builder = renderer.device().create_render_pass_builder();

			auto color = builder.add_attachment(vk::AttachmentDescription{
				vk::AttachmentDescriptionFlags{},
				target_format,
				vk::SampleCountFlagBits::e1,
				vk::AttachmentLoadOp::eDontCare,
				vk::AttachmentStoreOp::eStore,
				vk::AttachmentLoadOp::eDontCare,
				vk::AttachmentStoreOp::eDontCare,
				vk::ImageLayout::eUndefined,
				vk::ImageLayout::eShaderReadOnlyOptimal
			});

			auto pipeline = graphic::Pipeline_description {};
			pipeline.input_assembly.topology = vk::PrimitiveTopology::eTriangleList;
			pipeline.multisample = vk::PipelineMultisampleStateCreateInfo{};
			pipeline.color_blending = vk::PipelineColorBlendStateCreateInfo{};
			pipeline.depth_stencil = vk::PipelineDepthStencilStateCreateInfo{};

			auto& pass = builder.add_subpass(pipeline)
			                    .color_attachment(color);

			pass.stage("integrate"_strid)
			    .shader("frag_shader:gi_integrate_brdf"_aid, graphic::Shader_stage::fragment)
			    .shader("vert_shader:gi_integrate_brdf"_aid, graphic::Shader_stage::vertex);

			builder.add_dependency(util::nothing, vk::PipelineStageFlagBits::eColorAttachmentOutput,
			                       vk::AccessFlags{},
			                       pass, vk::PipelineStageFlagBits::eColorAttachmentOutput,
			                       vk::AccessFlagBits::eColorAttachmentRead|vk::AccessFlagBits::eColorAttachmentWrite);
			
			builder.add_dependency(pass, vk::PipelineStageFlagBits::eColorAttachmentOutput,
			                       vk::AccessFlagBits::eColorAttachmentRead|vk::AccessFlagBits::eColorAttachmentWrite,
			                       util::nothing, vk::PipelineStageFlagBits::eBottomOfPipe,
			                       vk::AccessFlagBits::eMemoryRead|vk::AccessFlagBits::eShaderRead|vk::AccessFlagBits::eTransferRead);


			auto render_pass = builder.build();

			out_framebuffer = builder.build_framebuffer(
					{target.view(), util::Rgba{}},
					target.width(),
					target.height());

			return render_pass;
		}

		auto build_reproject_pass(Deferred_renderer& renderer,
		                          vk::DescriptorSetLayout desc_set_layout,
		                          vk::Format history_weight_format,
		                          int min_mip_level,
		                          Render_target_2D& input,
		                          Render_target_2D& diffuse,
		                          Render_target_2D& specular,
		                          Render_target_2D& history_weight,
		                          Framebuffer& out_framebuffer) {

			auto builder = renderer.device().create_render_pass_builder();

			auto color_input = builder.add_attachment(vk::AttachmentDescription{
				vk::AttachmentDescriptionFlags{},
				renderer.gbuffer().color_format,
				vk::SampleCountFlagBits::e1,
				vk::AttachmentLoadOp::eLoad,
				vk::AttachmentStoreOp::eStore,
				vk::AttachmentLoadOp::eDontCare,
				vk::AttachmentStoreOp::eDontCare,
				vk::ImageLayout::eShaderReadOnlyOptimal,
				vk::ImageLayout::eShaderReadOnlyOptimal
			});
			auto color_diffuse = builder.add_attachment(vk::AttachmentDescription{
				vk::AttachmentDescriptionFlags{},
				renderer.gbuffer().color_format,
				vk::SampleCountFlagBits::e1,
				vk::AttachmentLoadOp::eDontCare,
				vk::AttachmentStoreOp::eStore,
				vk::AttachmentLoadOp::eDontCare,
				vk::AttachmentStoreOp::eDontCare,
				vk::ImageLayout::eShaderReadOnlyOptimal,
				vk::ImageLayout::eShaderReadOnlyOptimal
			});
			auto color_specular = builder.add_attachment(vk::AttachmentDescription{
				vk::AttachmentDescriptionFlags{},
				renderer.gbuffer().color_format,
				vk::SampleCountFlagBits::e1,
				vk::AttachmentLoadOp::eDontCare,
				vk::AttachmentStoreOp::eStore,
				vk::AttachmentLoadOp::eDontCare,
				vk::AttachmentStoreOp::eDontCare,
				vk::ImageLayout::eShaderReadOnlyOptimal,
				vk::ImageLayout::eShaderReadOnlyOptimal
			});
			auto color_weight = builder.add_attachment(vk::AttachmentDescription{
				vk::AttachmentDescriptionFlags{},
				history_weight_format,
				vk::SampleCountFlagBits::e1,
				vk::AttachmentLoadOp::eDontCare,
				vk::AttachmentStoreOp::eStore,
				vk::AttachmentLoadOp::eDontCare,
				vk::AttachmentStoreOp::eDontCare,
				vk::ImageLayout::eUndefined,
				vk::ImageLayout::eShaderReadOnlyOptimal
			});

			auto pipeline = graphic::Pipeline_description {};
			pipeline.input_assembly.topology = vk::PrimitiveTopology::eTriangleList;
			pipeline.multisample = vk::PipelineMultisampleStateCreateInfo{};
			pipeline.color_blending = vk::PipelineColorBlendStateCreateInfo{};
			pipeline.depth_stencil = vk::PipelineDepthStencilStateCreateInfo{};

			pipeline.add_descriptor_set_layout(renderer.global_uniforms_layout());
			pipeline.add_descriptor_set_layout(desc_set_layout);
			pipeline.add_push_constant("pcs"_strid, sizeof(Gi_constants),
			                           vk::ShaderStageFlagBits::eVertex|vk::ShaderStageFlagBits::eFragment);

			auto& pass = builder.add_subpass(pipeline)
			                    .color_attachment(color_input,
			                                      graphic::all_color_components,
			                                      graphic::blend_premultiplied_alpha)
			                    .color_attachment(color_diffuse,
			                                      graphic::all_color_components,
			                                      graphic::blend_premultiplied_alpha)
			                    .color_attachment(color_specular,
			                                      graphic::all_color_components,
			                                      graphic::blend_premultiplied_alpha)
			                    .color_attachment(color_weight,
			                                      graphic::all_color_components,
		                                          graphic::blend_premultiplied_alpha);

			pass.stage("reproject"_strid)
			    .shader("frag_shader:gi_reproject"_aid, graphic::Shader_stage::fragment)
			    .shader("vert_shader:gi_reproject"_aid, graphic::Shader_stage::vertex);

			builder.add_dependency(util::nothing, vk::PipelineStageFlagBits::eColorAttachmentOutput,
			                       vk::AccessFlags{},
			                       pass, vk::PipelineStageFlagBits::eColorAttachmentOutput,
			                       vk::AccessFlagBits::eColorAttachmentRead|vk::AccessFlagBits::eColorAttachmentWrite);
			
			builder.add_dependency(pass, vk::PipelineStageFlagBits::eColorAttachmentOutput,
			                       vk::AccessFlagBits::eColorAttachmentRead|vk::AccessFlagBits::eColorAttachmentWrite,
			                       util::nothing, vk::PipelineStageFlagBits::eBottomOfPipe,
			                       vk::AccessFlagBits::eMemoryRead|vk::AccessFlagBits::eShaderRead|vk::AccessFlagBits::eTransferRead);


			auto render_pass = builder.build();

			auto attachments = std::array<Framebuffer_attachment_desc, 4> {{
			    {input.view(min_mip_level), util::Rgba{}},
			    {diffuse.view(0), util::Rgba{}},
			    {specular.view(0), util::Rgba{0,0,0,0}},
			    {history_weight.view(0), util::Rgba{0,0,0,0}}
			}};

			out_framebuffer = builder.build_framebuffer(
					attachments,
					diffuse.width(0),
					diffuse.height(0));

			return render_pass;
		}
		
		auto build_sample_render_pass(Deferred_renderer& renderer,
		                              vk::DescriptorSetLayout desc_set_layout,
		                              int min_mip_level, int max_mip_level,
		                              Render_target_2D& gi_buffer,
		                              std::vector<Framebuffer>& out_framebuffers) {
			
			auto builder = renderer.device().create_render_pass_builder();
			
			auto color = builder.add_attachment(vk::AttachmentDescription{
				vk::AttachmentDescriptionFlags{},
				renderer.gbuffer().color_format,
				vk::SampleCountFlagBits::e1,
				vk::AttachmentLoadOp::eLoad,
				vk::AttachmentStoreOp::eStore,
				vk::AttachmentLoadOp::eDontCare,
				vk::AttachmentStoreOp::eDontCare,
				vk::ImageLayout::eShaderReadOnlyOptimal,
				vk::ImageLayout::eShaderReadOnlyOptimal
			});
			
			auto pipeline = graphic::Pipeline_description {};
			pipeline.input_assembly.topology = vk::PrimitiveTopology::eTriangleList;
			pipeline.multisample = vk::PipelineMultisampleStateCreateInfo{};
			pipeline.color_blending = vk::PipelineColorBlendStateCreateInfo{};
			pipeline.depth_stencil = vk::PipelineDepthStencilStateCreateInfo{};

			pipeline.add_descriptor_set_layout(renderer.global_uniforms_layout());
			pipeline.add_descriptor_set_layout(desc_set_layout);
			pipeline.add_push_constant("pcs"_strid, sizeof(Gi_constants),
			                           vk::ShaderStageFlagBits::eVertex|vk::ShaderStageFlagBits::eFragment);
			
			auto& pass = builder.add_subpass(pipeline)
			                    .color_attachment(color,
			                                      graphic::all_color_components,
			                                      graphic::blend_premultiplied_alpha);
			
			pass.stage("sample"_strid)
			    .shader("frag_shader:gi_sample"_aid, graphic::Shader_stage::fragment)
			    .shader("vert_shader:gi_sample"_aid, graphic::Shader_stage::vertex);
			
			pass.stage("sample_last"_strid)
			    .shader("frag_shader:gi_sample"_aid, graphic::Shader_stage::fragment, "main", 0, true)
			    .shader("vert_shader:gi_sample"_aid, graphic::Shader_stage::vertex);

			pass.stage("upsample"_strid)
			    .shader("frag_shader:gi_sample"_aid, graphic::Shader_stage::fragment, "main", 3, true)
			    .shader("vert_shader:gi_sample"_aid, graphic::Shader_stage::vertex);

			builder.add_dependency(util::nothing, vk::PipelineStageFlagBits::eColorAttachmentOutput,
			                       vk::AccessFlags{},
			                       pass, vk::PipelineStageFlagBits::eColorAttachmentOutput,
			                       vk::AccessFlagBits::eColorAttachmentRead|vk::AccessFlagBits::eColorAttachmentWrite);
			
			builder.add_dependency(pass, vk::PipelineStageFlagBits::eColorAttachmentOutput,
			                       vk::AccessFlagBits::eColorAttachmentRead|vk::AccessFlagBits::eColorAttachmentWrite,
			                       util::nothing, vk::PipelineStageFlagBits::eBottomOfPipe,
			                       vk::AccessFlagBits::eMemoryRead|vk::AccessFlagBits::eShaderRead|vk::AccessFlagBits::eTransferRead);


			auto render_pass = builder.build();
			
			auto end   = max_mip_level;
			for(auto i=0; i<end-min_mip_level; i++) {
				out_framebuffers.emplace_back( builder.build_framebuffer(
				        {gi_buffer.view(i), util::Rgba{}},
				        gi_buffer.width(i),
				        gi_buffer.height(i)));
				
			}
			
			return render_pass;
		}

		auto build_sample_spec_render_pass(Deferred_renderer& renderer,
		                                   vk::DescriptorSetLayout desc_set_layout,
		                                   Render_target_2D& gi_spec_buffer,
		                                   Framebuffer& out_framebuffer) {

			auto builder = renderer.device().create_render_pass_builder();

			auto color = builder.add_attachment(vk::AttachmentDescription{
				vk::AttachmentDescriptionFlags{},
				renderer.gbuffer().color_format,
				vk::SampleCountFlagBits::e1,
				vk::AttachmentLoadOp::eLoad,
				vk::AttachmentStoreOp::eStore,
				vk::AttachmentLoadOp::eDontCare,
				vk::AttachmentStoreOp::eDontCare,
				vk::ImageLayout::eShaderReadOnlyOptimal,
				vk::ImageLayout::eShaderReadOnlyOptimal
			});

			auto pipeline = graphic::Pipeline_description {};
			pipeline.input_assembly.topology = vk::PrimitiveTopology::eTriangleList;
			pipeline.multisample = vk::PipelineMultisampleStateCreateInfo{};
			pipeline.color_blending = vk::PipelineColorBlendStateCreateInfo{};
			pipeline.depth_stencil = vk::PipelineDepthStencilStateCreateInfo{};

			pipeline.add_descriptor_set_layout(renderer.global_uniforms_layout());
			pipeline.add_descriptor_set_layout(desc_set_layout);
			pipeline.add_push_constant("pcs"_strid, sizeof(Gi_constants),
			                           vk::ShaderStageFlagBits::eVertex|vk::ShaderStageFlagBits::eFragment);

			auto& pass = builder.add_subpass(pipeline)
			                    .color_attachment(color,
			                                      graphic::all_color_components,
			                                      graphic::blend_premultiplied_alpha);

			pass.stage("sample"_strid)
			    .shader("frag_shader:gi_sample_spec"_aid, graphic::Shader_stage::fragment)
			    .shader("vert_shader:gi_sample_spec"_aid, graphic::Shader_stage::vertex);

			builder.add_dependency(util::nothing, vk::PipelineStageFlagBits::eColorAttachmentOutput,
			                       vk::AccessFlags{},
			                       pass, vk::PipelineStageFlagBits::eColorAttachmentOutput,
			                       vk::AccessFlagBits::eColorAttachmentRead|vk::AccessFlagBits::eColorAttachmentWrite);
			
			builder.add_dependency(pass, vk::PipelineStageFlagBits::eColorAttachmentOutput,
			                       vk::AccessFlagBits::eColorAttachmentRead|vk::AccessFlagBits::eColorAttachmentWrite,
			                       util::nothing, vk::PipelineStageFlagBits::eBottomOfPipe,
			                       vk::AccessFlagBits::eMemoryRead|vk::AccessFlagBits::eShaderRead|vk::AccessFlagBits::eTransferRead);


			auto render_pass = builder.build();

			out_framebuffer = builder.build_framebuffer(
			        {gi_spec_buffer.view(0), util::Rgba{}},
			        gi_spec_buffer.width(),
			        gi_spec_buffer.height());

			return render_pass;
		}
		
		auto build_blend_render_pass(Deferred_renderer& renderer,
		                             vk::DescriptorSetLayout desc_set_layout,
		                             Render_target_2D& color_in_out,
		                             Framebuffer& out_framebuffer) {
			
			auto builder = renderer.device().create_render_pass_builder();
			
			auto color = builder.add_attachment(vk::AttachmentDescription{
				vk::AttachmentDescriptionFlags{},
				renderer.gbuffer().color_format,
				vk::SampleCountFlagBits::e1,
				vk::AttachmentLoadOp::eLoad,
				vk::AttachmentStoreOp::eStore,
				vk::AttachmentLoadOp::eDontCare,
				vk::AttachmentStoreOp::eDontCare,
				vk::ImageLayout::eShaderReadOnlyOptimal,
				vk::ImageLayout::eShaderReadOnlyOptimal
			});
			
			auto pipeline = graphic::Pipeline_description {};
			pipeline.input_assembly.topology = vk::PrimitiveTopology::eTriangleList;
			pipeline.multisample = vk::PipelineMultisampleStateCreateInfo{};
			pipeline.color_blending = vk::PipelineColorBlendStateCreateInfo{};
			pipeline.depth_stencil = vk::PipelineDepthStencilStateCreateInfo{};

			pipeline.add_descriptor_set_layout(renderer.global_uniforms_layout());
			pipeline.add_descriptor_set_layout(desc_set_layout);
			pipeline.add_push_constant("pcs"_strid, sizeof(Gi_constants),
			                           vk::ShaderStageFlagBits::eVertex|vk::ShaderStageFlagBits::eFragment);
			
			auto& pass = builder.add_subpass(pipeline)
			                    .color_attachment(color,
			                                      graphic::all_color_components,
			                                      graphic::blend_premultiplied_alpha);
			
			pass.stage("mipgen"_strid)
			    .shader("frag_shader:gi_blend"_aid, graphic::Shader_stage::fragment)
			    .shader("vert_shader:gi_blend"_aid, graphic::Shader_stage::vertex);

			builder.add_dependency(util::nothing, vk::PipelineStageFlagBits::eColorAttachmentOutput,
			                       vk::AccessFlags{},
			                       pass, vk::PipelineStageFlagBits::eColorAttachmentOutput,
			                       vk::AccessFlagBits::eColorAttachmentRead|vk::AccessFlagBits::eColorAttachmentWrite);
			
			builder.add_dependency(pass, vk::PipelineStageFlagBits::eColorAttachmentOutput,
			                       vk::AccessFlagBits::eColorAttachmentRead|vk::AccessFlagBits::eColorAttachmentWrite,
			                       util::nothing, vk::PipelineStageFlagBits::eBottomOfPipe,
			                       vk::AccessFlagBits::eMemoryRead|vk::AccessFlagBits::eShaderRead|vk::AccessFlagBits::eTransferRead);


			auto render_pass = builder.build();
			
			out_framebuffer = builder.build_framebuffer(
					{color_in_out.view(0), util::Rgba{}},
					color_in_out.width(),
					color_in_out.height());
			
			return render_pass;
		}

		auto get_brdf_format(graphic::Device& device) {
			auto format = device.get_supported_format({vk::Format::eR16G16Sfloat},
			                                          vk::FormatFeatureFlagBits::eColorAttachment
			                                          | vk::FormatFeatureFlagBits::eSampledImageFilterLinear);

			INVARIANT(format.is_some(), "No Float R16G16 format supported (required for BRDF preintegration)!");

			return format.get_or_throw();
		}
		auto get_history_weight_format(graphic::Device& device) {
			auto format = device.get_supported_format({vk::Format::eR8Unorm, vk::Format::eR8G8Unorm,
			                                          vk::Format::eR8G8B8A8Unorm},
			                                          vk::FormatFeatureFlagBits::eColorAttachment
			                                          | vk::FormatFeatureFlagBits::eSampledImageFilterLinear);

			INVARIANT(format.is_some(), "No Float R8 format supported!");

			return format.get_or_throw();
		}

		auto calc_base_mip_level(std::uint32_t width, std::uint32_t height) {
			return util::max(0, static_cast<int>(std::round(util::min(glm::log2(width/960.f),
			                                                          glm::log2(height/500.f)))));
		}
		auto calc_max_mip_level(std::uint32_t width, std::uint32_t height) {
			auto w = static_cast<float>(width);
			auto h = static_cast<float>(height);
			auto diagonal = std::sqrt(w*w + h*h);

			return static_cast<int>(std::ceil(glm::log2(diagonal / 40.f)));
		}
	}


	Gi_pass::Gi_pass(Deferred_renderer& renderer,
	                 graphic::Render_target_2D& in_out,
	                 graphic::Render_target_2D& diffuse_in)
	    : _renderer(renderer)
	    , _base_mip_level(calc_base_mip_level(in_out.width(), in_out.height()))
	    , _max_mip_level(calc_max_mip_level(in_out.width(), in_out.height()))
	    , _diffuse_mip_level(_base_mip_level + renderer.settings().gi_diffuse_mip_level)
	    , _specular_mip_level(_base_mip_level + renderer.settings().gi_specular_mip_level)
	    , _min_mip_level(std::min(_diffuse_mip_level, _specular_mip_level))
	    , _gbuffer_sampler(renderer.device().create_sampler(
	                           12, vk::SamplerAddressMode::eClampToEdge,
	                           vk::BorderColor::eIntOpaqueBlack,
	                           vk::Filter::eLinear,
	                           vk::SamplerMipmapMode::eLinear))
	    , _descriptor_set_layout(renderer.device(), *_gbuffer_sampler, 8)
	    , _color_in_out(in_out)
	    , _color_diffuse_in(diffuse_in)
	    
	    , _gi_diffuse(renderer.device(),
	                 {in_out.width(_min_mip_level), in_out.height(_min_mip_level)},
	                 0,
	                 renderer.gbuffer().color_format,
	                 vk::ImageUsageFlagBits::eSampled
	                 | vk::ImageUsageFlagBits::eTransferDst
	                 | vk::ImageUsageFlagBits::eTransferSrc
	                 | vk::ImageUsageFlagBits::eColorAttachment,
	                 vk::ImageAspectFlagBits::eColor)

	    , _gi_diffuse_history(renderer.device(),
	                 {in_out.width(_min_mip_level), in_out.height(_min_mip_level)},
	                 1,
	                 renderer.gbuffer().color_format,
	                 vk::ImageUsageFlagBits::eSampled
	                 | vk::ImageUsageFlagBits::eTransferDst
	                 | vk::ImageUsageFlagBits::eColorAttachment,
	                 vk::ImageAspectFlagBits::eColor)

	    , _gi_specular(renderer.device(),
	                   {in_out.width(_min_mip_level),
	                    in_out.height(_min_mip_level)},
	                   1,
	                   renderer.gbuffer().color_format,
	                   vk::ImageUsageFlagBits::eSampled
	                   | vk::ImageUsageFlagBits::eTransferSrc
	                   | vk::ImageUsageFlagBits::eTransferDst
	                   | vk::ImageUsageFlagBits::eColorAttachment,
	                   vk::ImageAspectFlagBits::eColor)

	    , _gi_specular_history(renderer.device(),
	                   {in_out.width(_min_mip_level),
	                    in_out.height(_min_mip_level)},
	                   1,
	                   renderer.gbuffer().color_format,
	                   vk::ImageUsageFlagBits::eSampled
	                   | vk::ImageUsageFlagBits::eTransferDst
	                   | vk::ImageUsageFlagBits::eColorAttachment,
	                   vk::ImageAspectFlagBits::eColor)

	    , _history_weight_format(get_history_weight_format(renderer.device()))
	    , _history_weight(renderer.device(), {in_out.width(_min_mip_level), in_out.height(_min_mip_level)},
	                      1, _history_weight_format,
	                      vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled,
	                      vk::ImageAspectFlagBits::eColor)

	    , _integrated_brdf_format(get_brdf_format(renderer.device()))
	    , _integrated_brdf(renderer.device(),
	                       {512, 512}, 1,
	                       _integrated_brdf_format,
	                       vk::ImageUsageFlagBits::eSampled
	                       | vk::ImageUsageFlagBits::eColorAttachment,
	                       vk::ImageAspectFlagBits::eColor)
	    , _brdf_integration_renderpass(build_integrate_brdf_render_pass(renderer,
	                                                                    _integrated_brdf_format,
	                                                                    _integrated_brdf,
	                                                                    _brdf_integration_framebuffer))

	    , _reproject_renderpass(build_reproject_pass(renderer,
	                                                 *_descriptor_set_layout,
	                                                 _history_weight_format,
	                                                 _min_mip_level,
	                                                 _color_diffuse_in,
	                                                 _gi_diffuse,
	                                                 _gi_specular,
	                                                 _history_weight,
	                                                 _reproject_framebuffer))
	    , _reproject_descriptor_set(_descriptor_set_layout.create_set(
	                                    renderer.descriptor_pool(),
	                                    {renderer.gbuffer().depth.view(),
	                                     renderer.gbuffer().mat_data.view(),
	                                     renderer.gbuffer().albedo_mat_id.view(0),
	                                     renderer.gbuffer().ambient_occlusion.get_or_other(_gi_diffuse_history).view(),
	                                     _gi_diffuse_history.view(),
	                                     _gi_specular_history.view(),
	                                     renderer.gbuffer().prev_depth.view(),
	                                     _integrated_brdf.view()}))
	    
	    , _sample_renderpass(build_sample_render_pass(renderer,
	                                                  *_descriptor_set_layout,
	                                                  _min_mip_level, _max_mip_level,
	                                                  _gi_diffuse,
	                                                  _sample_framebuffers))

	    , _sample_spec_renderpass(build_sample_spec_render_pass(renderer,
	                                                            *_descriptor_set_layout,
	                                                            _gi_specular,
	                                                            _sample_spec_framebuffer))
	    , _sample_spec_descriptor_set(_descriptor_set_layout.create_set(
	                                     renderer.descriptor_pool(),
	                                     {_color_diffuse_in.view(),
	                                      renderer.gbuffer().depth.view(),
	                                      renderer.gbuffer().mat_data.view(),
	                                      _gi_diffuse.view(),
	                                      _history_weight.view()}))
	    
	    , _blend_renderpass(build_blend_render_pass(renderer,
	                                                *_descriptor_set_layout,
	                                                _color_in_out,
	                                                _blend_framebuffer))
	    , _blend_descriptor_set(_descriptor_set_layout.create_set(
	                                    renderer.descriptor_pool(),
	                                    {renderer.gbuffer().depth.view(),
	                                     renderer.gbuffer().mat_data.view(),
	                                     _gi_diffuse.view(),
	                                     _gi_specular.view(),
	                                     renderer.gbuffer().albedo_mat_id.view(0),
	                                     renderer.gbuffer().ambient_occlusion.get_or_other(_gi_diffuse).view(),
	                                     _integrated_brdf.view()}))
	{
		auto end   = _max_mip_level;
		_sample_descriptor_sets.reserve(end-_min_mip_level);
		for(auto i=0; i<end-_min_mip_level; i++) {
			auto images = {
			    _color_diffuse_in.view(i+_min_mip_level),
			    renderer.gbuffer().depth.view(i+_min_mip_level),
			    renderer.gbuffer().mat_data.view(i+_min_mip_level),
			    _gi_diffuse.view(i+1),
			    _history_weight.view(),
			    renderer.gbuffer().depth.view(),
			    renderer.gbuffer().mat_data.view()
			};

			_sample_descriptor_sets.emplace_back(_descriptor_set_layout.create_set(
			                                       renderer.descriptor_pool(), images));
		}
	}


	void Gi_pass::update(util::Time) {
	}

	void Gi_pass::draw(vk::CommandBuffer& command_buffer,
	                   Command_buffer_source&,
	                   vk::DescriptorSet global_uniform_set,
	                   std::size_t) {

		if(!_renderer.settings().gi) {
			_first_frame = true;
			return;
		}
		
		if(_first_frame) {
			_first_frame = false;
			
			_integrate_brdf(command_buffer);

			graphic::clear_texture(command_buffer, _gi_diffuse, util::Rgba{0,0,0,0},
			                       vk::ImageLayout::eUndefined,
			                       vk::ImageLayout::eShaderReadOnlyOptimal);

			for(auto rt : {&_gi_specular, &_gi_diffuse_history, &_gi_specular_history}) {
				graphic::clear_texture(command_buffer, *rt, util::Rgba{0,0,0,0},
				                       vk::ImageLayout::eUndefined,
				                       vk::ImageLayout::eShaderReadOnlyOptimal, 0, 1);
			}

			_prev_view = _renderer.global_uniforms().view_mat;
			_prev_proj = _renderer.global_uniforms().proj_mat;
		}


		_reproject_history(command_buffer, global_uniform_set);
		_generate_mipmaps(command_buffer, global_uniform_set);
		_generate_gi_samples(command_buffer);
		_draw_gi(command_buffer);


		// copy results into history_buffer
		graphic::blit_texture(command_buffer, _gi_diffuse,
		                      vk::ImageLayout::eShaderReadOnlyOptimal, vk::ImageLayout::eShaderReadOnlyOptimal,
		                      _gi_diffuse_history,
		                      vk::ImageLayout::eUndefined, vk::ImageLayout::eShaderReadOnlyOptimal);
		graphic::blit_texture(command_buffer, _gi_specular,
		                      vk::ImageLayout::eShaderReadOnlyOptimal, vk::ImageLayout::eShaderReadOnlyOptimal,
		                      _gi_specular_history,
		                      vk::ImageLayout::eUndefined, vk::ImageLayout::eShaderReadOnlyOptimal);
	}

	void Gi_pass::_integrate_brdf(vk::CommandBuffer& command_buffer) {
		_brdf_integration_renderpass.execute(command_buffer, _brdf_integration_framebuffer, [&] {
			command_buffer.draw(3, 1, 0, 0);
		});
	}

	void Gi_pass::_reproject_history(vk::CommandBuffer& command_buffer, vk::DescriptorSet globals) {
		auto _ = _renderer.profiler().push("Reproject");

		if(_min_mip_level>0) {
			graphic::generate_mipmaps(command_buffer, _color_diffuse_in.image(),
			                          vk::ImageLayout::eShaderReadOnlyOptimal,
			                          vk::ImageLayout::eShaderReadOnlyOptimal,
			                          _color_diffuse_in.width(), _color_diffuse_in.height(),
			                          _min_mip_level+1);
		}

		_reproject_renderpass.execute(command_buffer, _reproject_framebuffer, [&] {
			auto descriptor_sets = std::array<vk::DescriptorSet, 2> {
				globals,
				*_reproject_descriptor_set
			};
			_reproject_renderpass.bind_descriptor_sets(0, descriptor_sets);

			auto pcs = Gi_constants{};
			pcs.reprojection = _prev_view * glm::inverse(_renderer.global_uniforms().view_mat);
			pcs.prev_projection = _prev_proj;
			INVARIANT(pcs.prev_projection[0][3]==0 && pcs.prev_projection[1][3]==0 && pcs.prev_projection[3][3]==0,
			          "m[0][3]!=0 or m[1][3]!=0 or m[3][3]!=0");

			pcs.prev_projection[0][3] = _min_mip_level;
			pcs.prev_projection[1][3] = _max_mip_level - 1;

			if(_renderer.gbuffer().ambient_occlusion.is_some()) {
				pcs.prev_projection[3][3] = 1.0;
			} else {
				pcs.prev_projection[3][3] = 0.0;
			}

			_reproject_renderpass.push_constant("pcs"_strid, pcs);

			command_buffer.draw(3, 1, 0, 0);
		});

		_prev_view = _renderer.global_uniforms().view_mat;
		_prev_proj = _renderer.global_uniforms().proj_mat;
	}

	void Gi_pass::_generate_mipmaps(vk::CommandBuffer& command_buffer,
	                                vk::DescriptorSet) {
		auto _ = _renderer.profiler().push("Gen. Mipmaps");

		graphic::generate_mipmaps(command_buffer, _color_diffuse_in.image(),
		                          vk::ImageLayout::eShaderReadOnlyOptimal,
		                          vk::ImageLayout::eShaderReadOnlyOptimal,
		                          _color_diffuse_in.width(), _color_diffuse_in.height(),
		                          _renderer.gbuffer().mip_levels,
		                          _min_mip_level);
	}

	namespace {
		float calc_ds_factor(int lod, float fov_h, float fov_v, float screen_width, float screen_height) {
			float dp = glm::pow(2.f, 2.f * lod); // screen area of one pixel (w*h)
			return (4.0f * glm::tan(fov_h/2.f) * glm::tan(fov_v/2.f) * dp)
			     / (screen_width*screen_height);
		}
	}
	void Gi_pass::_generate_gi_samples(vk::CommandBuffer& command_buffer) {
		auto begin   = _diffuse_mip_level;
		auto end     = _max_mip_level;


		auto pcs = Gi_constants{};
		pcs.prev_projection[1][3] = _max_mip_level - 1;
		pcs.prev_projection[3][3] = _min_mip_level;

		{
			auto _ = _renderer.profiler().push("Sample (diffuse)");
			for(auto i=end-1; i>=std::min(_min_mip_level, begin); i--) {
				auto& fb = _sample_framebuffers.at(i-_min_mip_level);

				if(i < end-1) {
					// barrier against write to previous mipmap level
					auto subresource = vk::ImageSubresourceRange {
						vk::ImageAspectFlagBits::eColor, gsl::narrow<std::uint32_t>(i-_min_mip_level + 1), 1, 0, 1
					};
					auto barrier = vk::ImageMemoryBarrier {
						vk::AccessFlagBits::eColorAttachmentWrite, vk::AccessFlagBits::eShaderRead,
						vk::ImageLayout::eShaderReadOnlyOptimal, vk::ImageLayout::eShaderReadOnlyOptimal,
						VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
						_gi_diffuse.image(), subresource
					};
					command_buffer.pipelineBarrier(vk::PipelineStageFlagBits::eColorAttachmentOutput,
					                   vk::PipelineStageFlagBits::eFragmentShader,
					                   vk::DependencyFlags{}, {}, {}, {barrier});
				}

				_sample_renderpass.execute(command_buffer, fb, [&] {
					if(i==begin) {
						_sample_renderpass.set_stage("sample_last"_strid);
					} else if(i<begin) {
						_sample_renderpass.set_stage("upsample"_strid);
					}

					_sample_renderpass.bind_descriptor_set(1, *_sample_descriptor_sets[i-_min_mip_level]);

					pcs.prev_projection[0][3] = i;
					auto fov_h = _renderer.global_uniforms().proj_planes.z;
					auto fov_v = _renderer.global_uniforms().proj_planes.w;
					pcs.prev_projection[2][3] = calc_ds_factor(i, fov_h, fov_v, _color_in_out.width(0), _color_in_out.height(0));
					_sample_renderpass.push_constant("pcs"_strid, pcs);

					command_buffer.draw(3, 1, 0, 0);
				});
			}
		}

		auto _ = _renderer.profiler().push("Sample (spec)");
		_sample_spec_renderpass.execute(command_buffer, _sample_spec_framebuffer, [&] {
			_sample_spec_renderpass.bind_descriptor_set(1, *_sample_spec_descriptor_set);

			// Mip-level used by spec.
			//  Always zero (for now). Skips pixels when gi_spec_mip_level>0 but reduces blocky artefacts.
			pcs.prev_projection[0][3] = 0;

			auto screen_size = glm::vec2{_color_diffuse_in.width(pcs.prev_projection[0][3]), _color_diffuse_in.height(pcs.prev_projection[0][3])};
			auto ndc_to_uv = glm::translate({}, glm::vec3(screen_size/2.f, 0.f))
			        * glm::scale({}, glm::vec3(screen_size/2.f, 1.f));
			pcs.reprojection = ndc_to_uv * _renderer.global_uniforms().proj_mat;

			_sample_spec_renderpass.push_constant("pcs"_strid, pcs);

			command_buffer.draw(3, 1, 0, 0);
		});
	}

	void Gi_pass::_draw_gi(vk::CommandBuffer& command_buffer) {
		auto _ = _renderer.profiler().push("Combine");

		// blend input into result
		_blend_renderpass.execute(command_buffer, _blend_framebuffer, [&] {
			_blend_renderpass.bind_descriptor_set(1, *_blend_descriptor_set);

			auto pcs = Gi_constants{};
			pcs.prev_projection[0][3] = _min_mip_level;
			pcs.prev_projection[1][3] = _max_mip_level - 1;
			pcs.prev_projection[2][3] = _renderer.settings().debug_gi_layer;

			if(_renderer.gbuffer().ambient_occlusion.is_some()) {
				pcs.prev_projection[3][3] = 1.0;
			} else {
				pcs.prev_projection[3][3] = 0.0;
			}

			_blend_renderpass.push_constant("pcs"_strid, pcs);

			command_buffer.draw(3, 1, 0, 0);
		});	
	}
	
	


	auto Gi_pass_factory::create_pass(Deferred_renderer& renderer,
	                                  ecs::Entity_manager& entities,
	                                  util::maybe<Meta_system&> meta_system,
	                                  bool& write_first_pp_buffer) -> std::unique_ptr<Pass> {
		auto& in_out = !write_first_pp_buffer ? renderer.gbuffer().colorA
		                                      : renderer.gbuffer().colorB;

		auto& in_diff = !write_first_pp_buffer ? renderer.gbuffer().colorB
		                                       : renderer.gbuffer().colorA;
		
		// writes back to the read texture, so we don't have to flip write_first_pp_buffer
		
		return std::make_unique<Gi_pass>(renderer, in_out, in_diff);
	}

	auto Gi_pass_factory::rank_device(vk::PhysicalDevice, util::maybe<std::uint32_t> graphics_queue,
	                                  int current_score) -> int {
		return current_score;
	}

	void Gi_pass_factory::configure_device(vk::PhysicalDevice,
	                                       util::maybe<std::uint32_t>,
	                                       graphic::Device_create_info&) {
	}


}
}
