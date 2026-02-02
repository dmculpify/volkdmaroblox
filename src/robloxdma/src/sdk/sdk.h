#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <sdk/math/math.h>
typedef void* VMMDLL_SCATTER_HANDLE;
#include <mutex>
extern std::mutex g_scatter_mutex;
VMMDLL_SCATTER_HANDLE get_global_scatter_handle();
namespace rbx
{
	class instance_t;
	class part_t;
	struct addressable_t
	{
		std::uint64_t address;
		addressable_t() : address(0) {}
		addressable_t(std::uint64_t address) : address(address) {}
	};
	struct nameable_t : public addressable_t
	{
		using addressable_t::addressable_t;
		std::string get_name() const;
		std::string get_class_name() const;
	};
	struct node_t
	{
		template <typename T>
		std::vector<T> get_children();
		std::vector<instance_t> get_children();
		std::uint64_t find_first_child(const std::string& name);
		std::uint64_t find_first_child_by_class(const std::string& name);
		std::uint64_t get_parent();
	};
	template <typename T>
	struct value_holder_t : public addressable_t
	{
		using addressable_t::addressable_t;
		T get_value();
	};
	struct instance_t : public nameable_t, public node_t
	{
		using nameable_t::nameable_t;
	};
	struct humanoid_t final : public addressable_t {
		using addressable_t::addressable_t;
		float get_health();
		float get_max_health();
		float get_hip_height();
		void set_jump_height(float jump);
		void set_walk_speed(float speed);
		std::uint8_t get_rig_type();
		std::int16_t get_state();
		struct batch_data_t {
			float health;
			float max_health;
			float hip_height;
			std::uint8_t rig_type;
			std::int16_t state;
		};
		batch_data_t get_batch_data();
		void add_batch_read_requests(void* handle, batch_data_t* data, std::uint64_t* humanoid_state_addr = nullptr);
	};
	struct model_instance_t final : public addressable_t, public node_t
	{
		using addressable_t::addressable_t;
	};
	struct player_t final : public instance_t
	{
		using instance_t::instance_t;
		rbx::model_instance_t get_model_instance();
		std::uint64_t get_team();
		std::uint64_t get_user_id();
		std::string get_display_name();
		std::string get_country_code();
		std::string get_gender();
		std::string get_platform();
		std::string get_operating_system();
		struct batch_data_t {
			std::uint64_t team;
			std::uint64_t user_id;
			std::uint64_t model_instance;
			std::uint64_t name_addr;
			std::uint64_t country_addr;
			std::uint64_t gender_addr;
			std::uint64_t platform_addr;
			std::uint64_t os_addr;
		};
		void add_batch_read_requests(void* handle, batch_data_t* data);
	};
	struct primitive_t final : public addressable_t
	{
		using addressable_t::addressable_t;
		math::vector3 get_velocity();
		math::vector3 get_position();
		math::matrix3 get_rotation();
		void set_rotation(math::matrix3& rotation);
		math::vector3 get_size();
		math::color3 get_color();
		float get_transparency();
		struct batch_data_t {
			math::vector3 velocity;
			math::vector3 position;
			math::matrix3 rotation;
			math::vector3 size;
			math::color3 color;
			float transparency;
		};
		batch_data_t get_batch_data();
		void add_batch_read_requests(void* handle, batch_data_t* data);
	};
	struct part_t : public nameable_t
	{
		using nameable_t::nameable_t;
		rbx::primitive_t get_primitive();
		std::int32_t get_material();
	};
	struct meshpart_t final : public part_t
	{
		using part_t::part_t;
		std::string get_mesh_id();
		std::string get_texture_id();
	};
	struct camera_t final : public addressable_t
	{
		using addressable_t::addressable_t;
		math::matrix3 get_rotation();
		math::vector3 get_position();
		float get_field_of_view();
		struct batch_data_t {
			math::matrix3 rotation;
			math::vector3 position;
			float field_of_view;
		};
		batch_data_t get_batch_data();
	};
	struct workspace_t final : public addressable_t
	{
		using addressable_t::addressable_t;
		rbx::camera_t get_current_camera();
	};
	struct player_service_t : public instance_t {
		using instance_t::instance_t;
		std::vector<rbx::player_t> get_players();
		rbx::player_t get_local_player();
	};
	struct datamodel_t final : public instance_t
	{
		using instance_t::instance_t;
		std::uint64_t get_game_id();
		std::uint64_t get_place_id();
		std::uint64_t get_creator_id();
		std::string get_server_ip();
		struct batch_data_t {
			std::uint64_t game_id;
			std::uint64_t place_id;
			std::uint64_t creator_id;
		};
		batch_data_t get_batch_data();
		void add_batch_read_requests(void* handle, batch_data_t* data);
	};
	struct visualengine_t final : public addressable_t {
		using addressable_t::addressable_t;
		math::vector2 get_dimensions();
		math::matrix4 get_viewmatrix();
		bool world_to_screen(const math::vector3& world, math::vector2& out);
		struct batch_data_t {
			math::vector2 dimensions;
			math::matrix4 viewmatrix;
		};
		batch_data_t get_batch_data();
	};
}