/**************************************************************************/
/*  rendering_device_driver_vulkan.h                                      */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#ifndef RENDERING_DEVICE_DRIVER_VULKAN_H
#define RENDERING_DEVICE_DRIVER_VULKAN_H

#include "core/templates/hash_map.h"
#include "core/templates/paged_allocator.h"
#include "servers/rendering/rendering_device_driver.h"

#ifdef DEBUG_ENABLED
#ifndef _MSC_VER
#define _DEBUG
#endif
#endif
#include "thirdparty/vulkan/vk_mem_alloc.h"

#ifdef USE_VOLK
#include <volk.h>
#else
#include <vulkan/vulkan.h>
#endif

class VulkanContext;

// Design principles:
// - Vulkan structs are zero-initialized and fields not requiring a non-zero value are omitted (except in cases where expresivity reasons apply).
class RenderingDeviceDriverVulkan : public RenderingDeviceDriver {
	/*****************/
	/**** GENERIC ****/
	/*****************/

	VulkanContext *context = nullptr;
	VkDevice vk_device = VK_NULL_HANDLE; // Owned by the context.

	/****************/
	/**** MEMORY ****/
	/****************/

	VmaAllocator allocator = nullptr;
	HashMap<uint32_t, VmaPool> small_allocs_pools;

	VmaPool _find_or_create_small_allocs_pool(uint32_t p_mem_type_index);

	/*****************/
	/**** BUFFERS ****/
	/*****************/
private:
	struct BufferInfo {
		VkBuffer vk_buffer = VK_NULL_HANDLE;
		struct {
			VmaAllocation handle = nullptr;
			uint64_t size = UINT64_MAX;
		} allocation;
		uint64_t size = 0;
		VkBufferView vk_view = VK_NULL_HANDLE; // For texel buffers.
	};

public:
	virtual BufferID buffer_create(uint64_t p_size, BitField<BufferUsageBits> p_usage, MemoryAllocationType p_allocation_type) override final;
	virtual bool buffer_set_texel_format(BufferID p_buffer, DataFormat p_format) override final;
	virtual void buffer_free(BufferID p_buffer) override final;
	virtual uint64_t buffer_get_allocation_size(BufferID p_buffer) override final;
	virtual uint8_t *buffer_map(BufferID p_buffer) override final;
	virtual void buffer_unmap(BufferID p_buffer) override final;

	/*****************/
	/**** TEXTURE ****/
	/*****************/

	struct TextureInfo {
		VkImageView vk_view = VK_NULL_HANDLE;
		DataFormat rd_format = DATA_FORMAT_MAX;
		VkImageCreateInfo vk_create_info = {};
		VkImageViewCreateInfo vk_view_create_info = {};
		struct {
			VmaAllocation handle = nullptr;
			VmaAllocationInfo info = {};
		} allocation; // All 0/null if just a view.
	};

	VkSampleCountFlagBits _ensure_supported_sample_count(TextureSamples p_requested_sample_count);

public:
	virtual TextureID texture_create(const TextureFormat &p_format, const TextureView &p_view) override final;
	virtual TextureID texture_create_from_extension(uint64_t p_native_texture, TextureType p_type, DataFormat p_format, uint32_t p_array_layers, bool p_depth_stencil) override final;
	virtual TextureID texture_create_shared(TextureID p_original_texture, const TextureView &p_view) override final;
	virtual TextureID texture_create_shared_from_slice(TextureID p_original_texture, const TextureView &p_view, TextureSliceType p_slice_type, uint32_t p_layer, uint32_t p_layers, uint32_t p_mipmap, uint32_t p_mipmaps) override final;
	virtual void texture_free(TextureID p_texture) override final;
	virtual uint64_t texture_get_allocation_size(TextureID p_texture) override final;
	virtual void texture_get_copyable_layout(TextureID p_texture, const TextureSubresource &p_subresource, TextureCopyableLayout *r_layout) override final;
	virtual uint8_t *texture_map(TextureID p_texture, const TextureSubresource &p_subresource) override final;
	virtual void texture_unmap(TextureID p_texture) override final;
	virtual BitField<TextureUsageBits> texture_get_usages_supported_by_format(DataFormat p_format, bool p_cpu_readable) override final;

	/*****************/
	/**** SAMPLER ****/
	/*****************/
public:
	virtual SamplerID sampler_create(const SamplerState &p_state) final override;
	virtual void sampler_free(SamplerID p_sampler) final override;
	virtual bool sampler_is_format_supported_for_filter(DataFormat p_format, SamplerFilter p_filter) override final;

	/**********************/
	/**** VERTEX ARRAY ****/
	/**********************/
private:
	struct VertexFormatInfo {
		TightLocalVector<VkVertexInputBindingDescription> vk_bindings;
		TightLocalVector<VkVertexInputAttributeDescription> vk_attributes;
		VkPipelineVertexInputStateCreateInfo vk_create_info = {};
	};

public:
	virtual VertexFormatID vertex_format_create(VectorView<VertexAttribute> p_vertex_attribs) override final;
	virtual void vertex_format_free(VertexFormatID p_vertex_format) override final;

	/******************/
	/**** BARRIERS ****/
	/******************/

	virtual void command_pipeline_barrier(
			CommandBufferID p_cmd_buffer,
			BitField<PipelineStageBits> p_src_stages,
			BitField<PipelineStageBits> p_dst_stages,
			VectorView<MemoryBarrier> p_memory_barriers,
			VectorView<BufferBarrier> p_buffer_barriers,
			VectorView<TextureBarrier> p_texture_barriers) override final;

	/*************************/
	/**** COMMAND BUFFERS ****/
	/*************************/
private:
#ifdef DEBUG_ENABLED
	// Vulkan doesn't need to know if the command buffers created in a pool
	// will be primary or secondary, but RDD works like that, so we will enforce.

	HashSet<CommandPoolID> secondary_cmd_pools;
	HashSet<CommandBufferID> secondary_cmd_buffers;
#endif

public:
	// ----- POOL -----

	virtual CommandPoolID command_pool_create(CommandBufferType p_cmd_buffer_type) override final;
	virtual void command_pool_free(CommandPoolID p_cmd_pool) override final;

	// ----- BUFFER -----

	virtual CommandBufferID command_buffer_create(CommandBufferType p_cmd_buffer_type, CommandPoolID p_cmd_pool) override final;
	virtual bool command_buffer_begin(CommandBufferID p_cmd_buffer) override final;
	virtual bool command_buffer_begin_secondary(CommandBufferID p_cmd_buffer, RenderPassID p_render_pass, uint32_t p_subpass, FramebufferID p_framebuffer) override final;
	virtual void command_buffer_end(CommandBufferID p_cmd_buffer) override final;
	virtual void command_buffer_execute_secondary(CommandBufferID p_cmd_buffer, VectorView<CommandBufferID> p_secondary_cmd_buffers) override final;

	/*********************/
	/**** FRAMEBUFFER ****/
	/*********************/

	virtual FramebufferID framebuffer_create(RenderPassID p_render_pass, VectorView<TextureID> p_attachments, uint32_t p_width, uint32_t p_height) override final;
	virtual void framebuffer_free(FramebufferID p_framebuffer) override final;

	/****************/
	/**** SHADER ****/
	/****************/
private:
	struct ShaderBinary {
		// Version 1: initial.
		// Version 2: Added shader name.
		// Version 3: Added writable.
		// Version 4: 64-bit vertex input mask.
		static const uint32_t VERSION = 4;

		struct DataBinding {
			uint32_t type = 0;
			uint32_t binding = 0;
			uint32_t stages = 0;
			uint32_t length = 0; // Size of arrays (in total elements), or UBOs (in bytes * total elements).
			uint32_t writable = 0;
		};

		struct SpecializationConstant {
			uint32_t type = 0;
			uint32_t constant_id = 0;
			uint32_t int_value = 0;
			uint32_t stage_flags = 0;
		};

		struct Data {
			uint64_t vertex_input_mask = 0;
			uint32_t fragment_output_mask = 0;
			uint32_t specialization_constants_count = 0;
			uint32_t is_compute = 0;
			uint32_t compute_local_size[3] = {};
			uint32_t set_count = 0;
			uint32_t push_constant_size = 0;
			uint32_t vk_push_constant_stages_mask = 0;
			uint32_t stage_count = 0;
			uint32_t shader_name_len = 0;
		};
	};

	struct ShaderInfo {
		VkShaderStageFlags vk_push_constant_stages = 0;
		TightLocalVector<VkPipelineShaderStageCreateInfo> vk_stages_create_info;
		TightLocalVector<VkDescriptorSetLayout> vk_descriptor_set_layouts;
		VkPipelineLayout vk_pipeline_layout = VK_NULL_HANDLE;
	};

public:
	virtual String shader_get_binary_cache_key() override final;
	virtual Vector<uint8_t> shader_compile_binary_from_spirv(VectorView<ShaderStageSPIRVData> p_spirv, const String &p_shader_name) override final;
	virtual ShaderID shader_create_from_bytecode(const Vector<uint8_t> &p_shader_binary, ShaderDescription &r_shader_desc, String &r_name) override final;
	virtual void shader_free(ShaderID p_shader) override final;

	/*********************/
	/**** UNIFORM SET ****/
	/*********************/

	// Descriptor sets require allocation from a pool.
	// The documentation on how to use pools properly
	// is scarce, and the documentation is strange.
	//
	// Basically, you can mix and match pools as you
	// like, but you'll run into fragmentation issues.
	// Because of this, the recommended approach is to
	// create a pool for every descriptor set type, as
	// this prevents fragmentation.
	//
	// This is implemented here as a having a list of
	// pools (each can contain up to 64 sets) for each
	// set layout. The amount of sets for each type
	// is used as the key.

private:
	static const uint32_t MAX_UNIFORM_POOL_ELEMENT = 65535;

	struct DescriptorSetPoolKey {
		uint16_t uniform_type[UNIFORM_TYPE_MAX] = {};

		bool operator<(const DescriptorSetPoolKey &p_other) const {
			return memcmp(uniform_type, p_other.uniform_type, sizeof(uniform_type)) < 0;
		}
	};

	using DescriptorSetPools = RBMap<DescriptorSetPoolKey, HashMap<VkDescriptorPool, uint32_t>>;
	DescriptorSetPools descriptor_set_pools;
	uint32_t max_descriptor_sets_per_pool = 0;

	VkDescriptorPool _descriptor_set_pool_find_or_create(const DescriptorSetPoolKey &p_key, DescriptorSetPools::Iterator *r_pool_sets_it);
	void _descriptor_set_pool_unreference(DescriptorSetPools::Iterator p_pool_sets_it, VkDescriptorPool p_vk_descriptor_pool);

	struct UniformSetInfo {
		VkDescriptorSet vk_descriptor_set = VK_NULL_HANDLE;
		VkDescriptorPool vk_descriptor_pool = VK_NULL_HANDLE;
		DescriptorSetPools::Iterator pool_sets_it = {};
	};

public:
	virtual UniformSetID uniform_set_create(VectorView<BoundUniform> p_uniforms, ShaderID p_shader, uint32_t p_set_index) override final;
	virtual void uniform_set_free(UniformSetID p_uniform_set) override final;

	// ----- COMMANDS -----

	virtual void command_uniform_set_prepare_for_use(CommandBufferID p_cmd_buffer, UniformSetID p_uniform_set, ShaderID p_shader, uint32_t p_set_index) override final;

	/******************/
	/**** TRANSFER ****/
	/******************/

	virtual void command_clear_buffer(CommandBufferID p_cmd_buffer, BufferID p_buffer, uint64_t p_offset, uint64_t p_size) override final;
	virtual void command_copy_buffer(CommandBufferID p_cmd_buffer, BufferID p_src_buffer, BufferID p_dst_buffer, VectorView<BufferCopyRegion> p_regions) override final;

	virtual void command_copy_texture(CommandBufferID p_cmd_buffer, TextureID p_src_texture, TextureLayout p_src_texture_layout, TextureID p_dst_texture, TextureLayout p_dst_texture_layout, VectorView<TextureCopyRegion> p_regions) override final;
	virtual void command_resolve_texture(CommandBufferID p_cmd_buffer, TextureID p_src_texture, TextureLayout p_src_texture_layout, uint32_t p_src_layer, uint32_t p_src_mipmap, TextureID p_dst_texture, TextureLayout p_dst_texture_layout, uint32_t p_dst_layer, uint32_t p_dst_mipmap) override final;
	virtual void command_clear_color_texture(CommandBufferID p_cmd_buffer, TextureID p_texture, TextureLayout p_texture_layout, const Color &p_color, const TextureSubresourceRange &p_subresources) override final;

	virtual void command_copy_buffer_to_texture(CommandBufferID p_cmd_buffer, BufferID p_src_buffer, TextureID p_dst_texture, TextureLayout p_dst_texture_layout, VectorView<BufferTextureCopyRegion> p_regions) override final;
	virtual void command_copy_texture_to_buffer(CommandBufferID p_cmd_buffer, TextureID p_src_texture, TextureLayout p_src_texture_layout, BufferID p_dst_buffer, VectorView<BufferTextureCopyRegion> p_regions) override final;

	/******************/
	/**** PIPELINE ****/
	/******************/
private:
	struct PipelineCacheHeader {
		uint32_t magic = 0;
		uint32_t data_size = 0;
		uint64_t data_hash = 0;
		uint32_t vendor_id = 0;
		uint32_t device_id = 0;
		uint32_t driver_version = 0;
		uint8_t uuid[VK_UUID_SIZE] = {};
		uint8_t driver_abi = 0;
	};

	struct PipelineCache {
		String file_path;
		size_t current_size = 0;
		Vector<uint8_t> buffer; // Header then data.
		VkPipelineCache vk_cache = VK_NULL_HANDLE;
	};

	static int caching_instance_count;
	PipelineCache pipelines_cache;

public:
	virtual void pipeline_free(PipelineID p_pipeline) override final;

	// ----- BINDING -----

	virtual void command_bind_push_constants(CommandBufferID p_cmd_buffer, ShaderID p_shader, uint32_t p_first_index, VectorView<uint32_t> p_data) override final;

	// ----- CACHE -----

	virtual bool pipeline_cache_create(const Vector<uint8_t> &p_data) override final;
	virtual void pipeline_cache_free() override final;
	virtual size_t pipeline_cache_query_size() override final;
	virtual Vector<uint8_t> pipeline_cache_serialize() override final;

	/*******************/
	/**** RENDERING ****/
	/*******************/

	// ----- SUBPASS -----

	virtual RenderPassID render_pass_create(VectorView<Attachment> p_attachments, VectorView<Subpass> p_subpasses, VectorView<SubpassDependency> p_subpass_dependencies, uint32_t p_view_count) override final;
	virtual void render_pass_free(RenderPassID p_render_pass) override final;

	// ----- COMMANDS -----

	virtual void command_begin_render_pass(CommandBufferID p_cmd_buffer, RenderPassID p_render_pass, FramebufferID p_framebuffer, CommandBufferType p_cmd_buffer_type, const Rect2i &p_rect, VectorView<RenderPassClearValue> p_clear_values) override final;
	virtual void command_end_render_pass(CommandBufferID p_cmd_buffer) override final;
	virtual void command_next_render_subpass(CommandBufferID p_cmd_buffer, CommandBufferType p_cmd_buffer_type) override final;
	virtual void command_render_set_viewport(CommandBufferID p_cmd_buffer, VectorView<Rect2i> p_viewports) override final;
	virtual void command_render_set_scissor(CommandBufferID p_cmd_buffer, VectorView<Rect2i> p_scissors) override final;
	virtual void command_render_clear_attachments(CommandBufferID p_cmd_buffer, VectorView<AttachmentClear> p_attachment_clears, VectorView<Rect2i> p_rects) override final;

	// Binding.
	virtual void command_bind_render_pipeline(CommandBufferID p_cmd_buffer, PipelineID p_pipeline) override final;
	virtual void command_bind_render_uniform_set(CommandBufferID p_cmd_buffer, UniformSetID p_uniform_set, ShaderID p_shader, uint32_t p_set_index) override final;

	// Drawing.
	virtual void command_render_draw(CommandBufferID p_cmd_buffer, uint32_t p_vertex_count, uint32_t p_instance_count, uint32_t p_base_vertex, uint32_t p_first_instance) override final;
	virtual void command_render_draw_indexed(CommandBufferID p_cmd_buffer, uint32_t p_index_count, uint32_t p_instance_count, uint32_t p_first_index, int32_t p_vertex_offset, uint32_t p_first_instance) override final;
	virtual void command_render_draw_indexed_indirect(CommandBufferID p_cmd_buffer, BufferID p_indirect_buffer, uint64_t p_offset, uint32_t p_draw_count, uint32_t p_stride) override final;
	virtual void command_render_draw_indexed_indirect_count(CommandBufferID p_cmd_buffer, BufferID p_indirect_buffer, uint64_t p_offset, BufferID p_count_buffer, uint64_t p_count_buffer_offset, uint32_t p_max_draw_count, uint32_t p_stride) override final;
	virtual void command_render_draw_indirect(CommandBufferID p_cmd_buffer, BufferID p_indirect_buffer, uint64_t p_offset, uint32_t p_draw_count, uint32_t p_stride) override final;
	virtual void command_render_draw_indirect_count(CommandBufferID p_cmd_buffer, BufferID p_indirect_buffer, uint64_t p_offset, BufferID p_count_buffer, uint64_t p_count_buffer_offset, uint32_t p_max_draw_count, uint32_t p_stride) override final;

	// Buffer binding.
	virtual void command_render_bind_vertex_buffers(CommandBufferID p_cmd_buffer, uint32_t p_binding_count, const BufferID *p_buffers, const uint64_t *p_offsets) override final;
	virtual void command_render_bind_index_buffer(CommandBufferID p_cmd_buffer, BufferID p_buffer, IndexBufferFormat p_format, uint64_t p_offset) override final;

	// Dynamic state.
	virtual void command_render_set_blend_constants(CommandBufferID p_cmd_buffer, const Color &p_constants) override final;
	virtual void command_render_set_line_width(CommandBufferID p_cmd_buffer, float p_width) override final;

	// ----- PIPELINE -----

	virtual PipelineID render_pipeline_create(
			ShaderID p_shader,
			VertexFormatID p_vertex_format,
			RenderPrimitive p_render_primitive,
			PipelineRasterizationState p_rasterization_state,
			PipelineMultisampleState p_multisample_state,
			PipelineDepthStencilState p_depth_stencil_state,
			PipelineColorBlendState p_blend_state,
			VectorView<int32_t> p_color_attachments,
			BitField<PipelineDynamicStateFlags> p_dynamic_state,
			RenderPassID p_render_pass,
			uint32_t p_render_subpass,
			VectorView<PipelineSpecializationConstant> p_specialization_constants) override final;

	/*****************/
	/**** COMPUTE ****/
	/*****************/

	// ----- COMMANDS -----

	// Binding.
	virtual void command_bind_compute_pipeline(CommandBufferID p_cmd_buffer, PipelineID p_pipeline) override final;
	virtual void command_bind_compute_uniform_set(CommandBufferID p_cmd_buffer, UniformSetID p_uniform_set, ShaderID p_shader, uint32_t p_set_index) override final;

	// Dispatching.
	virtual void command_compute_dispatch(CommandBufferID p_cmd_buffer, uint32_t p_x_groups, uint32_t p_y_groups, uint32_t p_z_groups) override final;
	virtual void command_compute_dispatch_indirect(CommandBufferID p_cmd_buffer, BufferID p_indirect_buffer, uint64_t p_offset) override final;

	// ----- PIPELINE -----

	virtual PipelineID compute_pipeline_create(ShaderID p_shader, VectorView<PipelineSpecializationConstant> p_specialization_constants) override final;

	/*****************/
	/**** QUERIES ****/
	/*****************/

	// ----- TIMESTAMP -----

	// Basic.
	virtual QueryPoolID timestamp_query_pool_create(uint32_t p_query_count) override final;
	virtual void timestamp_query_pool_free(QueryPoolID p_pool_id) override final;
	virtual void timestamp_query_pool_get_results(QueryPoolID p_pool_id, uint32_t p_query_count, uint64_t *r_results) override final;
	virtual uint64_t timestamp_query_result_to_time(uint64_t p_result) override final;

	// Commands.
	virtual void command_timestamp_query_pool_reset(CommandBufferID p_cmd_buffer, QueryPoolID p_pool_id, uint32_t p_query_count) override final;
	virtual void command_timestamp_write(CommandBufferID p_cmd_buffer, QueryPoolID p_pool_id, uint32_t p_index) override final;

	/****************/
	/**** LABELS ****/
	/****************/

	virtual void command_begin_label(CommandBufferID p_cmd_buffer, const char *p_label_name, const Color &p_color) override final;
	virtual void command_end_label(CommandBufferID p_cmd_buffer) override final;

	/****************/
	/**** SCREEN ****/
	/****************/

	virtual DataFormat screen_get_format() override final;

	/********************/
	/**** SUBMISSION ****/
	/********************/

	virtual void begin_segment(CommandBufferID p_cmd_buffer, uint32_t p_frame_index, uint32_t p_frames_drawn) override final;
	virtual void end_segment() override final;

	/**************/
	/**** MISC ****/
	/**************/

	VkPhysicalDeviceLimits limits = {};

	virtual void set_object_name(ObjectType p_type, ID p_driver_id, const String &p_name) override final;
	virtual uint64_t get_resource_native_handle(DriverResource p_type, ID p_driver_id) override final;
	virtual uint64_t get_total_memory_used() override final;
	virtual uint64_t limit_get(Limit p_limit) override final;
	virtual uint64_t api_trait_get(ApiTrait p_trait) override final;
	virtual bool has_feature(Features p_feature) override final;
	virtual const MultiviewCapabilities &get_multiview_capabilities() override final;

private:
	/*********************/
	/**** BOOKKEEPING ****/
	/*********************/

	using VersatileResource = VersatileResourceTemplate<
			BufferInfo,
			TextureInfo,
			VertexFormatInfo,
			ShaderInfo,
			UniformSetInfo>;
	PagedAllocator<VersatileResource> resources_allocator;

	/******************/

public:
	RenderingDeviceDriverVulkan(VulkanContext *p_context, VkDevice p_vk_device);
	virtual ~RenderingDeviceDriverVulkan();
};

#endif // RENDERING_DEVICE_DRIVER_VULKAN_H
