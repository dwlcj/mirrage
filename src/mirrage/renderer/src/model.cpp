#include <sf2/sf2.hpp>

#include <mirrage/renderer/model.hpp>

#include <mirrage/graphic/device.hpp>
#include <mirrage/graphic/render_pass.hpp>


using namespace mirrage::graphic;

namespace mirrage::renderer {

	namespace {
		constexpr auto material_textures = std::uint32_t(2);
	} // namespace

	auto create_material_descriptor_set_layout(Device& device, vk::Sampler sampler)
	        -> vk::UniqueDescriptorSetLayout {
		auto bindings = std::array<vk::DescriptorSetLayoutBinding, material_textures>();
		auto samplers = std::array<vk::Sampler, material_textures>();

		std::fill_n(samplers.begin(), material_textures, sampler);

		std::fill_n(bindings.begin(),
		            material_textures,
		            vk::DescriptorSetLayoutBinding{0,
		                                           vk::DescriptorType::eCombinedImageSampler,
		                                           1,
		                                           vk::ShaderStageFlagBits::eFragment,
		                                           samplers.data()});

		for(auto i : util::range(material_textures)) {
			bindings[i].binding = i;
		}

		return device.create_descriptor_set_layout(bindings);
	}

	Material::Material(Device&                device,
	                   graphic::DescriptorSet descriptor_set,
	                   vk::Sampler            sampler,
	                   graphic::Texture_ptr   albedo,
	                   graphic::Texture_ptr   mat_data,
	                   util::Str_id           substance_id)
	  : _descriptor_set(std::move(descriptor_set))
	  , _albedo(std::move(albedo))
	  , _mat_data(std::move(mat_data))
	  , _material_id(substance_id) {

		auto desc_images = std::array<vk::DescriptorImageInfo, material_textures>();
		desc_images[0] =
		        vk::DescriptorImageInfo{sampler, _albedo->view(), vk::ImageLayout::eShaderReadOnlyOptimal};
		desc_images[1] =
		        vk::DescriptorImageInfo{sampler, _mat_data->view(), vk::ImageLayout::eShaderReadOnlyOptimal};

		auto desc_writes = std::array<vk::WriteDescriptorSet, 1>();
		desc_writes[0]   = vk::WriteDescriptorSet{*_descriptor_set,
                                                0,
                                                0,
                                                material_textures,
                                                vk::DescriptorType::eCombinedImageSampler,
                                                desc_images.data(),
                                                nullptr};

		device.vk_device()->updateDescriptorSets(desc_writes.size(), desc_writes.data(), 0, nullptr);
	}

	void Material::bind(graphic::Render_pass& pass) const {
		pass.bind_descriptor_sets(1, {_descriptor_set.get_ptr(), 1});
	}


	Model::Model(graphic::Mesh mesh, std::vector<Sub_mesh> sub_meshes, util::maybe<asset::AID> aid)
	  : _mesh(std::move(mesh)), _sub_meshes(std::move(sub_meshes)), _aid(aid) {}

	void Model::bind_mesh(const graphic::Command_buffer& cb, std::uint32_t vertex_binding) const {
		_mesh.bind(cb, vertex_binding);
	}

	auto Model::bind_sub_mesh(graphic::Render_pass& pass, std::size_t index) const
	        -> std::pair<std::size_t, std::size_t> {
		auto& sm = _sub_meshes.at(index);
		sm.material->bind(pass);
		return std::make_pair(sm.index_offset, sm.index_count);
	}

} // namespace mirrage::renderer


namespace mirrage::asset {

	Loader<renderer::Material>::Loader(graphic::Device&        device,
	                                   asset::Asset_manager&   assets,
	                                   vk::Sampler             sampler,
	                                   vk::DescriptorSetLayout layout)
	  : _device(device)
	  , _assets(assets)
	  , _sampler(sampler)
	  , _descriptor_set_layout(layout)
	  , _descriptor_set_pool(
	            device.create_descriptor_pool(256, {vk::DescriptorType::eCombinedImageSampler})) {}

	auto Loader<renderer::Material>::load(istream in) -> async::task<renderer::Material> {
		auto data = Loader<renderer::Material_data>::load(std::move(in));

		auto load_tex = [&](auto&& id) {
			return _assets.load<graphic::Texture_2D>(id.empty() ? "tex:placeholder"_aid
			                                                    : asset::AID("tex"_strid, id));
		};

		auto sub_id   = data.substance_id;
		auto desc_set = _descriptor_set_pool.create_descriptor(_descriptor_set_layout);

		auto albedo   = load_tex(data.albedo_aid);
		auto mat_data = load_tex(data.mat_data_aid);

		using Tex_task = decltype(albedo.internal_task());

		return async::when_all(albedo.internal_task(), mat_data.internal_task()).then([
			=,
			desc_set = std::move(desc_set)
		](std::tuple<Tex_task, Tex_task>) mutable {
			return renderer::Material(_device, std::move(desc_set), _sampler, albedo, mat_data, sub_id);
		});
	}

	namespace {
		template <typename T>
		void read(std::istream& in, T& value) {
			static_assert(!std::is_pointer<T>::value,
			              "T is a pointer. That is DEFINITLY not what you wanted!");
			in.read(reinterpret_cast<char*>(&value), sizeof(T));
		}

		template <typename T>
		void read(std::istream& in, std::vector<T>& value) {
			static_assert(!std::is_pointer<T>::value,
			              "T is a pointer. That is DEFINITLY not what you wanted!");

			in.read(reinterpret_cast<char*>(value.data()), value.size() * sizeof(T));
		}
	} // namespace

	auto Loader<renderer::Model>::load(istream in) -> async::task<renderer::Model> {

		auto header = renderer::Model_file_header{};
		read(in, header);

		if(header.type_tag != renderer::Model_file_header::type_tag_value) {
			MIRRAGE_FAIL("The loaded file \"" << in.aid().str() << "\" is not a valid model file!");
		}

		if(header.version != renderer::Model_file_header::version_value) {
			MIRRAGE_FAIL("The loaded model file \""
			             << in.aid().str() << "\" is not compatible with this version of the application!");
		}

		// load sub meshes and materials
		auto sub_meshes = std::vector<renderer::Sub_mesh>();
		sub_meshes.reserve(header.submesh_count);
		auto material_load_tasks = std::vector<async::shared_task<renderer::Material>>();
		material_load_tasks.reserve(header.submesh_count);

		for(auto i : util::range(header.submesh_count)) {
			(void) i;

			sub_meshes.emplace_back();
			auto& sub_mesh = sub_meshes.back();
			read(in, sub_mesh.index_offset);
			read(in, sub_mesh.index_count);

			auto material_id_length = std::uint32_t(0);
			read(in, material_id_length);

			auto material_id = std::string();
			material_id.resize(material_id_length);
			in.read(material_id.data(), material_id_length);

			sub_mesh.material = _assets.load<renderer::Material>(asset::AID("mat"_strid, material_id));
			material_load_tasks.emplace_back(sub_mesh.material.internal_task());
		}

		// transfer mesh data to gpu
		auto mesh = graphic::Mesh(_device,
		                          _owner_qfamily,
		                          header.vertex_count,
		                          header.index_count,
		                          [&](char* dest) { in.read_direct(dest, header.vertex_count); },
		                          [&](char* dest) { in.read_direct(dest, header.index_count); });

		auto footer = std::uint32_t(0);
		read(in, footer);

		if(footer != renderer::Model_file_header::type_tag_value) {
			MIRRAGE_WARN("Invalid footer in model file \"" << in.aid().str()
			                                               << "\"! The file is probably corrupted!");
		}

		auto all_materials_loaded = async::when_all(material_load_tasks);
		auto all_loaded = async::when_all(all_materials_loaded, mesh.internal_buffer().transfer_task());
		using Task_type = decltype(all_loaded)::result_type;

		return all_loaded.then([model = renderer::Model(std::move(mesh), std::move(sub_meshes), in.aid())](
		        const Task_type&) mutable { return std::move(model); });
	}

} // namespace mirrage::asset
