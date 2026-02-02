#include <sdk/sdk.h>
#include <globals.h>
#include <sdk/offsets/offsets.h>
#include <Memory/Memory.h>
#include <unordered_map>
#include <mutex>
static VMMDLL_SCATTER_HANDLE g_scatter_handle = nullptr;
std::mutex g_scatter_mutex;
static std::unordered_map<std::uint64_t, std::string> g_name_cache;
static std::unordered_map<std::uint64_t, std::string> g_class_name_cache;
static std::mutex g_cache_mutex;
static void init_scatter_handle()
{
	if (g_scatter_handle == nullptr)
	{
		std::lock_guard<std::mutex> lock(g_scatter_mutex);
		if (g_scatter_handle == nullptr)
		{
			g_scatter_handle = memory.CreateScatterHandle();
			if (!g_scatter_handle)
			{
				LOG("[!] CRITICAL: Failed to create global scatter handle\n");
			}
			else
			{
				LOG("[+] Global scatter handle created successfully\n");
			}
		}
	}
}
VMMDLL_SCATTER_HANDLE get_global_scatter_handle()
{
	init_scatter_handle();
	return g_scatter_handle;
}
static VMMDLL_SCATTER_HANDLE get_scatter_handle()
{
	return get_global_scatter_handle();
}
std::string rbx::nameable_t::get_name() const
{
	{
		std::lock_guard<std::mutex> lock(g_cache_mutex);
		auto it = g_name_cache.find(this->address);
		if (it != g_name_cache.end())
		{
			return it->second;
		}
	}
	std::uint64_t name = 0;
	VMMDLL_SCATTER_HANDLE handle = get_scatter_handle();
	{
		std::lock_guard<std::mutex> lock(g_scatter_mutex);
		memory.AddScatterReadRequest(handle, this->address + Offsets::Instance::Name, &name, sizeof(name));
		memory.ExecuteReadScatter(handle);
	}
	std::string result = "unknown";
	if (name != 0 && name > 0x10000 && name < 0x7FFFFFFFFFFF)
	{
		try
		{
			result = memory.Read_string(name);
			if (result.length() > 256)
			{
				result = "unknown";
			}
			else
			{
				std::lock_guard<std::mutex> lock(g_cache_mutex);
				g_name_cache[this->address] = result;
			}
		}
		catch (const std::exception&)
		{
			result = "unknown";
		}
	}
	return result;
}
std::string rbx::nameable_t::get_class_name() const
{
	{
		std::lock_guard<std::mutex> lock(g_cache_mutex);
		auto it = g_class_name_cache.find(this->address);
		if (it != g_class_name_cache.end())
		{
			return it->second;
		}
	}
	std::uint64_t class_descriptor = 0;
	std::uint64_t class_name = 0;
	VMMDLL_SCATTER_HANDLE handle = get_scatter_handle();
	{
		std::lock_guard<std::mutex> lock(g_scatter_mutex);
		memory.AddScatterReadRequest(handle, this->address + Offsets::Instance::ClassDescriptor, &class_descriptor, sizeof(class_descriptor));
		memory.ExecuteReadScatter(handle);
		if (class_descriptor != 0)
		{
			memory.AddScatterReadRequest(handle, class_descriptor + Offsets::Instance::ClassName, &class_name, sizeof(class_name));
			memory.ExecuteReadScatter(handle);
		}
	}
	std::string result = "unknown";
	if (class_name != 0 && class_name > 0x10000 && class_name < 0x7FFFFFFFFFFF)
	{
		try
		{
			result = memory.Read_string(class_name);
			if (result.length() > 256)
			{
				result = "unknown";
			}
			else
			{
				std::lock_guard<std::mutex> lock(g_cache_mutex);
				g_class_name_cache[this->address] = result;
			}
		}
		catch (const std::exception&)
		{
			result = "unknown";
		}
	}
	return result;
}
std::vector<rbx::instance_t> rbx::node_t::get_children()
{
	rbx::instance_t* self = static_cast<rbx::instance_t*>(this);
	std::vector<rbx::instance_t> container;
	container.reserve(32);
	std::uint64_t children_container = 0;
	VMMDLL_SCATTER_HANDLE handle = get_scatter_handle();
	{
		std::lock_guard<std::mutex> lock(g_scatter_mutex);
		memory.AddScatterReadRequest(handle, self->address + Offsets::Instance::ChildrenStart, &children_container, sizeof(children_container));
		memory.ExecuteReadScatter(handle);
	}
	if (children_container == 0 || children_container < 0x10000) return container;
	std::uint64_t children_start = 0;
	std::uint64_t children_end = 0;
	{
		std::lock_guard<std::mutex> lock(g_scatter_mutex);
		memory.AddScatterReadRequest(handle, children_container + 0x0, &children_start, sizeof(children_start));
		memory.AddScatterReadRequest(handle, children_container + Offsets::Instance::ChildrenEnd, &children_end, sizeof(children_end));
		memory.ExecuteReadScatter(handle);
	}
	if (children_start == 0 || children_start < 0x10000) return container;
	size_t array_size_bytes = 0;
	if (children_end > children_start && children_end < (children_start + (512 * sizeof(std::uint64_t))))
	{
		array_size_bytes = children_end - children_start;
		size_t child_count = array_size_bytes / sizeof(std::uint64_t);
		if (child_count > 128) child_count = 128;
		if (child_count == 0) return container;
		std::vector<std::uint64_t> child_ptrs(child_count, 0);
		{
			std::lock_guard<std::mutex> lock(g_scatter_mutex);
			for (size_t i = 0; i < child_count; ++i)
			{
				std::uint64_t read_addr = children_start + (i * sizeof(std::uint64_t));
				memory.AddScatterReadRequest(handle, read_addr, &child_ptrs[i], sizeof(std::uint64_t));
			}
			memory.ExecuteReadScatter(handle);
		}
		for (size_t i = 0; i < child_count; ++i)
		{
			std::uint64_t child_addr = child_ptrs[i];
			if (child_addr == 0 || child_addr < 0x10000)
				break;
			container.emplace_back(child_addr);
		}
	}
	else
	{
		std::vector<std::uint64_t> child_ptrs(64, 0);
		{
			std::lock_guard<std::mutex> lock(g_scatter_mutex);
			for (size_t i = 0; i < 64; ++i)
			{
				std::uint64_t read_addr = children_container + (i * sizeof(std::uint64_t));
				memory.AddScatterReadRequest(handle, read_addr, &child_ptrs[i], sizeof(std::uint64_t));
			}
			memory.ExecuteReadScatter(handle);
		}
		for (size_t i = 2; i < 64; ++i)
		{
			std::uint64_t child_addr = child_ptrs[i];
			if (child_addr == 0 || child_addr < 0x10000)
				break;
			container.emplace_back(child_addr);
		}
	}
	return container;
}
template <typename T>
std::vector<T> rbx::node_t::get_children()
{
	rbx::instance_t* self = static_cast<rbx::instance_t*>(this);
	std::vector<T> container;
	container.reserve(32);
	std::vector<rbx::instance_t> all_children = this->get_children();
	constexpr bool is_player = std::is_same_v<T, rbx::player_t>;
	constexpr bool is_meshpart = std::is_same_v<T, rbx::meshpart_t>;
	constexpr bool needs_filtering = is_player || is_meshpart;
	for (const auto& child : all_children)
	{
		if constexpr (needs_filtering)
		{
			std::string child_class_name = child.get_class_name();
			if constexpr (is_player)
			{
				if (child_class_name != "Player")
					continue;
			}
			else if constexpr (is_meshpart)
			{
				if (child_class_name != "MeshPart")
					continue;
			}
		}
		container.emplace_back(child.address);
	}
	return container;
}
template std::vector<rbx::meshpart_t> rbx::node_t::get_children<rbx::meshpart_t>();
template std::vector<rbx::player_t> rbx::node_t::get_children<rbx::player_t>();
std::uint64_t rbx::node_t::find_first_child(const std::string& name)
{
	std::vector<rbx::instance_t> children = this->get_children();
	for (rbx::instance_t& child : children)
	{
		if (child.get_name() == name)
		{
			return child.address;
		}
	}
	return 0;
}
std::uint64_t rbx::node_t::find_first_child_by_class(const std::string& name)
{
	std::vector<rbx::instance_t> children = this->get_children();
	for (rbx::instance_t& child : children)
	{
		if (child.get_class_name() == name)
		{
			return child.address;
		}
	}
	return 0;
}
std::uint64_t rbx::node_t::get_parent()
{
	rbx::addressable_t* self = reinterpret_cast<rbx::addressable_t*>(this);
	std::uint64_t parent = 0;
	VMMDLL_SCATTER_HANDLE handle = get_scatter_handle();
	{
		std::lock_guard<std::mutex> lock(g_scatter_mutex);
		memory.AddScatterReadRequest(handle, self->address + Offsets::Instance::Parent, &parent, sizeof(parent));
		memory.ExecuteReadScatter(handle);
	}
	return parent;
}
template <typename T>
T rbx::value_holder_t<T>::get_value()
{
	T value{};
	VMMDLL_SCATTER_HANDLE handle = get_scatter_handle();
	{
		std::lock_guard<std::mutex> lock(g_scatter_mutex);
		memory.AddScatterReadRequest(handle, this->address + Offsets::Misc::Value, &value, sizeof(T));
		memory.ExecuteReadScatter(handle);
	}
	return value;
}
float rbx::humanoid_t::get_health()
{
	float health = 0.0f;
	VMMDLL_SCATTER_HANDLE handle = get_scatter_handle();
	{
		std::lock_guard<std::mutex> lock(g_scatter_mutex);
		memory.AddScatterReadRequest(handle, this->address + Offsets::Humanoid::Health, &health, sizeof(health));
		memory.ExecuteReadScatter(handle);
	}
	return health;
}
float rbx::humanoid_t::get_max_health()
{
	float max_health = 0.0f;
	VMMDLL_SCATTER_HANDLE handle = get_scatter_handle();
	{
		std::lock_guard<std::mutex> lock(g_scatter_mutex);
		memory.AddScatterReadRequest(handle, this->address + Offsets::Humanoid::MaxHealth, &max_health, sizeof(max_health));
		memory.ExecuteReadScatter(handle);
	}
	return max_health;
}
float rbx::humanoid_t::get_hip_height()
{
	float hip_height = 0.0f;
	VMMDLL_SCATTER_HANDLE handle = get_scatter_handle();
	{
		std::lock_guard<std::mutex> lock(g_scatter_mutex);
		memory.AddScatterReadRequest(handle, this->address + Offsets::Humanoid::HipHeight, &hip_height, sizeof(hip_height));
		memory.ExecuteReadScatter(handle);
	}
	return hip_height;
}
void rbx::humanoid_t::set_walk_speed(float speed)
{
	memory.Write<float>(this->address + Offsets::Humanoid::Walkspeed, speed);
	memory.Write<float>(this->address + Offsets::Humanoid::WalkspeedCheck, speed);
}
void rbx::humanoid_t::set_jump_height(float jump)
{
	memory.Write<float>(this->address + Offsets::Humanoid::JumpPower, jump);
}
std::uint8_t rbx::humanoid_t::get_rig_type()
{
	std::uint8_t rig_type = 0;
	VMMDLL_SCATTER_HANDLE handle = get_scatter_handle();
	{
		std::lock_guard<std::mutex> lock(g_scatter_mutex);
		memory.AddScatterReadRequest(handle, this->address + Offsets::Humanoid::RigType, &rig_type, sizeof(rig_type));
		memory.ExecuteReadScatter(handle);
	}
	return rig_type;
}
std::int16_t rbx::humanoid_t::get_state()
{
	std::uint64_t humanoid_state = 0;
	std::int16_t state_id = 0;
	VMMDLL_SCATTER_HANDLE handle = get_scatter_handle();
	{
		std::lock_guard<std::mutex> lock(g_scatter_mutex);
		memory.AddScatterReadRequest(handle, this->address + Offsets::Humanoid::HumanoidState, &humanoid_state, sizeof(humanoid_state));
		memory.ExecuteReadScatter(handle);
		if (humanoid_state != 0)
		{
			memory.AddScatterReadRequest(handle, humanoid_state + Offsets::Humanoid::HumanoidStateID, &state_id, sizeof(state_id));
			memory.ExecuteReadScatter(handle);
		}
	}
	return state_id;
}
rbx::model_instance_t rbx::player_t::get_model_instance()
{
	std::uint64_t model_instance = 0;
	VMMDLL_SCATTER_HANDLE handle = get_scatter_handle();
	{
		std::lock_guard<std::mutex> lock(g_scatter_mutex);
		memory.AddScatterReadRequest(handle, this->address + Offsets::Player::ModelInstance, &model_instance, sizeof(model_instance));
		memory.ExecuteReadScatter(handle);
	}
	return rbx::model_instance_t(model_instance);
}
std::uint64_t rbx::player_t::get_team()
{
	std::uint64_t team = 0;
	VMMDLL_SCATTER_HANDLE handle = get_scatter_handle();
	{
		std::lock_guard<std::mutex> lock(g_scatter_mutex);
		memory.AddScatterReadRequest(handle, this->address + Offsets::Player::Team, &team, sizeof(team));
		memory.ExecuteReadScatter(handle);
	}
	return team;
}
std::uint64_t rbx::player_t::get_user_id()
{
	std::uint64_t user_id = 0;
	VMMDLL_SCATTER_HANDLE handle = get_scatter_handle();
	{
		std::lock_guard<std::mutex> lock(g_scatter_mutex);
		memory.AddScatterReadRequest(handle, this->address + Offsets::Player::UserId, &user_id, sizeof(user_id));
		memory.ExecuteReadScatter(handle);
	}
	return user_id;
}
std::string rbx::player_t::get_display_name()
{
	try
	{
		return memory.Read_string(this->address + Offsets::Player::DisplayName);
	}
	catch (const std::exception&)
	{
		return "";
	}
}
std::string rbx::player_t::get_country_code()
{
	try
	{
		return memory.Read_string(this->address + Offsets::Player::Country);
	}
	catch (const std::exception&)
	{
		return "";
	}
}
std::string rbx::player_t::get_gender()
{
	try
	{
		return memory.Read_string(this->address + Offsets::Player::Gender);
	}
	catch (const std::exception&)
	{
		return "";
	}
}
std::string rbx::player_t::get_platform()
{
	try
	{
		return memory.Read_string(this->address + 0xe50);
	}
	catch (const std::exception&)
	{
		return "";
	}
}
std::string rbx::player_t::get_operating_system()
{
	try
	{
		return memory.Read_string(this->address + 0xe70);
	}
	catch (const std::exception&)
	{
		return "";
	}
}
math::vector3 rbx::primitive_t::get_velocity()
{
	math::vector3 velocity{};
	VMMDLL_SCATTER_HANDLE handle = get_scatter_handle();
	{
		std::lock_guard<std::mutex> lock(g_scatter_mutex);
		memory.AddScatterReadRequest(handle, this->address + Offsets::BasePart::AssemblyLinearVelocity, &velocity, sizeof(velocity));
		memory.ExecuteReadScatter(handle);
	}
	return velocity;
}
math::vector3 rbx::primitive_t::get_position()
{
	math::vector3 position{};
	VMMDLL_SCATTER_HANDLE handle = get_scatter_handle();
	{
		std::lock_guard<std::mutex> lock(g_scatter_mutex);
		memory.AddScatterReadRequest(handle, this->address + Offsets::BasePart::Position, &position, sizeof(position));
		memory.ExecuteReadScatter(handle);
	}
	return position;
}
math::vector3 rbx::primitive_t::get_size()
{
	math::vector3 size{};
	VMMDLL_SCATTER_HANDLE handle = get_scatter_handle();
	{
		std::lock_guard<std::mutex> lock(g_scatter_mutex);
		memory.AddScatterReadRequest(handle, this->address + Offsets::BasePart::Size, &size, sizeof(size));
		memory.ExecuteReadScatter(handle);
	}
	return size;
}
math::matrix3 rbx::primitive_t::get_rotation()
{
	math::matrix3 rotation{};
	VMMDLL_SCATTER_HANDLE handle = get_scatter_handle();
	{
		std::lock_guard<std::mutex> lock(g_scatter_mutex);
		memory.AddScatterReadRequest(handle, this->address + Offsets::BasePart::Rotation, &rotation, sizeof(rotation));
		memory.ExecuteReadScatter(handle);
	}
	return rotation;
}
void rbx::primitive_t::set_rotation(math::matrix3& rotation)
{
	memory.Write<math::matrix3>(this->address + Offsets::BasePart::Rotation, rotation);
}
math::color3 rbx::primitive_t::get_color()
{
	math::color3 color{};
	VMMDLL_SCATTER_HANDLE handle = get_scatter_handle();
	{
		std::lock_guard<std::mutex> lock(g_scatter_mutex);
		memory.AddScatterReadRequest(handle, this->address + Offsets::BasePart::Color3, &color, sizeof(color));
		memory.ExecuteReadScatter(handle);
	}
	return color;
}
float rbx::primitive_t::get_transparency()
{
	float transparency = 0.0f;
	VMMDLL_SCATTER_HANDLE handle = get_scatter_handle();
	{
		std::lock_guard<std::mutex> lock(g_scatter_mutex);
		memory.AddScatterReadRequest(handle, this->address + Offsets::BasePart::Transparency, &transparency, sizeof(transparency));
		memory.ExecuteReadScatter(handle);
	}
	return transparency;
}
rbx::primitive_t rbx::part_t::get_primitive()
{
	std::uint64_t primitive = 0;
	VMMDLL_SCATTER_HANDLE handle = get_scatter_handle();
	{
		std::lock_guard<std::mutex> lock(g_scatter_mutex);
		memory.AddScatterReadRequest(handle, this->address + Offsets::BasePart::Primitive, &primitive, sizeof(primitive));
		memory.ExecuteReadScatter(handle);
	}
	return rbx::primitive_t(primitive);
}
std::int32_t rbx::part_t::get_material()
{
	std::int32_t material = 0;
	VMMDLL_SCATTER_HANDLE handle = get_scatter_handle();
	{
		std::lock_guard<std::mutex> lock(g_scatter_mutex);
		memory.AddScatterReadRequest(handle, this->address + Offsets::BasePart::Material, &material, sizeof(material));
		memory.ExecuteReadScatter(handle);
	}
	return material;
}
std::string rbx::meshpart_t::get_mesh_id()
{
	return memory.Read_string(this->address + Offsets::MeshPart::MeshId);
}
std::string rbx::meshpart_t::get_texture_id()
{
	return memory.Read_string(this->address + Offsets::MeshPart::Texture);
}
math::matrix3 rbx::camera_t::get_rotation()
{
	math::matrix3 rotation{};
	VMMDLL_SCATTER_HANDLE handle = get_scatter_handle();
	{
		std::lock_guard<std::mutex> lock(g_scatter_mutex);
		memory.AddScatterReadRequest(handle, this->address + Offsets::Camera::Rotation, &rotation, sizeof(rotation));
		memory.ExecuteReadScatter(handle);
	}
	return rotation;
}
math::vector3 rbx::camera_t::get_position()
{
	math::vector3 position{};
	VMMDLL_SCATTER_HANDLE handle = get_scatter_handle();
	{
		std::lock_guard<std::mutex> lock(g_scatter_mutex);
		memory.AddScatterReadRequest(handle, this->address + Offsets::Camera::Position, &position, sizeof(position));
		memory.ExecuteReadScatter(handle);
	}
	return position;
}
float rbx::camera_t::get_field_of_view()
{
	float fov = 0.0f;
	VMMDLL_SCATTER_HANDLE handle = get_scatter_handle();
	{
		std::lock_guard<std::mutex> lock(g_scatter_mutex);
		memory.AddScatterReadRequest(handle, this->address + Offsets::Camera::FieldOfView, &fov, sizeof(fov));
		memory.ExecuteReadScatter(handle);
	}
	return fov;
}
rbx::camera_t rbx::workspace_t::get_current_camera()
{
	std::uint64_t camera = 0;
	VMMDLL_SCATTER_HANDLE handle = get_scatter_handle();
	{
		std::lock_guard<std::mutex> lock(g_scatter_mutex);
		memory.AddScatterReadRequest(handle, this->address + Offsets::Workspace::CurrentCamera, &camera, sizeof(camera));
		memory.ExecuteReadScatter(handle);
	}
	return rbx::camera_t(camera);
}
std::uint64_t rbx::datamodel_t::get_game_id()
{
	std::uint64_t game_id = 0;
	VMMDLL_SCATTER_HANDLE handle = get_scatter_handle();
	{
		std::lock_guard<std::mutex> lock(g_scatter_mutex);
		memory.AddScatterReadRequest(handle, this->address + Offsets::DataModel::GameId, &game_id, sizeof(game_id));
		memory.ExecuteReadScatter(handle);
	}
	return game_id;
}
std::uint64_t rbx::datamodel_t::get_place_id()
{
	std::uint64_t place_id = 0;
	VMMDLL_SCATTER_HANDLE handle = get_scatter_handle();
	{
		std::lock_guard<std::mutex> lock(g_scatter_mutex);
		memory.AddScatterReadRequest(handle, this->address + Offsets::DataModel::PlaceId, &place_id, sizeof(place_id));
		memory.ExecuteReadScatter(handle);
	}
	return place_id;
}
std::uint64_t rbx::datamodel_t::get_creator_id()
{
	std::uint64_t creator_id = 0;
	VMMDLL_SCATTER_HANDLE handle = get_scatter_handle();
	{
		std::lock_guard<std::mutex> lock(g_scatter_mutex);
		memory.AddScatterReadRequest(handle, this->address + Offsets::DataModel::CreatorId, &creator_id, sizeof(creator_id));
		memory.ExecuteReadScatter(handle);
	}
	return creator_id;
}
std::string rbx::datamodel_t::get_server_ip()
{
	try
	{
		return memory.Read_string(this->address + Offsets::DataModel::ServerIP);
	}
	catch (const std::exception&)
	{
		return "";
	}
}
math::vector2 rbx::visualengine_t::get_dimensions()
{
	math::vector2 dimensions{};
	VMMDLL_SCATTER_HANDLE handle = get_scatter_handle();
	{
		std::lock_guard<std::mutex> lock(g_scatter_mutex);
		memory.AddScatterReadRequest(handle, this->address + Offsets::VisualEngine::Dimensions, &dimensions, sizeof(dimensions));
		memory.ExecuteReadScatter(handle);
	}
	return dimensions;
}
math::matrix4 rbx::visualengine_t::get_viewmatrix()
{
	math::matrix4 viewmatrix{};
	VMMDLL_SCATTER_HANDLE handle = get_scatter_handle();
	{
		std::lock_guard<std::mutex> lock(g_scatter_mutex);
		memory.AddScatterReadRequest(handle, this->address + Offsets::VisualEngine::ViewMatrix, &viewmatrix, sizeof(viewmatrix));
		memory.ExecuteReadScatter(handle);
	}
	return viewmatrix;
}
bool rbx::visualengine_t::world_to_screen(const math::vector3& world, math::vector2& out)
{
	batch_data_t batch = this->get_batch_data();
	math::matrix4 view = batch.viewmatrix;
	math::vector2 dims = batch.dimensions;
	static math::vector4 clip{};
	clip.x = world.x * view(0, 0) + world.y * view(0, 1) + world.z * view(0, 2) + view(0, 3);
	clip.y = world.x * view(1, 0) + world.y * view(1, 1) + world.z * view(1, 2) + view(1, 3);
	clip.z = world.x * view(2, 0) + world.y * view(2, 1) + world.z * view(2, 2) + view(2, 3);
	clip.w = world.x * view(3, 0) + world.y * view(3, 1) + world.z * view(3, 2) + view(3, 3);
	if (clip.w < 0.1f)
	{
		return false;
	}
	clip.x /= clip.w;
	clip.y /= clip.w;
	static math::vector2 screen;
	out.x = (dims.x * 0.5f * clip.x) + (dims.x * 0.5f);
	out.y = -(dims.y * 0.5f * clip.y) + (dims.y * 0.5f);
	return true;
}
rbx::humanoid_t::batch_data_t rbx::humanoid_t::get_batch_data()
{
	batch_data_t data{};
	VMMDLL_SCATTER_HANDLE handle = get_scatter_handle();
	std::uint64_t humanoid_state = 0;
	{
		std::lock_guard<std::mutex> lock(g_scatter_mutex);
		memory.AddScatterReadRequest(handle, this->address + Offsets::Humanoid::Health, &data.health, sizeof(data.health));
		memory.AddScatterReadRequest(handle, this->address + Offsets::Humanoid::MaxHealth, &data.max_health, sizeof(data.max_health));
		memory.AddScatterReadRequest(handle, this->address + Offsets::Humanoid::HipHeight, &data.hip_height, sizeof(data.hip_height));
		memory.AddScatterReadRequest(handle, this->address + Offsets::Humanoid::RigType, &data.rig_type, sizeof(data.rig_type));
		memory.AddScatterReadRequest(handle, this->address + Offsets::Humanoid::HumanoidState, &humanoid_state, sizeof(humanoid_state));
		memory.ExecuteReadScatter(handle);
		if (humanoid_state != 0)
		{
			memory.AddScatterReadRequest(handle, humanoid_state + Offsets::Humanoid::HumanoidStateID, &data.state, sizeof(data.state));
			memory.ExecuteReadScatter(handle);
		}
	}
	return data;
}
rbx::primitive_t::batch_data_t rbx::primitive_t::get_batch_data()
{
	batch_data_t data{};
	VMMDLL_SCATTER_HANDLE handle = get_scatter_handle();
	{
		std::lock_guard<std::mutex> lock(g_scatter_mutex);
		memory.AddScatterReadRequest(handle, this->address + Offsets::BasePart::AssemblyLinearVelocity, &data.velocity, sizeof(data.velocity));
		memory.AddScatterReadRequest(handle, this->address + Offsets::BasePart::Position, &data.position, sizeof(data.position));
		memory.AddScatterReadRequest(handle, this->address + Offsets::BasePart::Rotation, &data.rotation, sizeof(data.rotation));
		memory.AddScatterReadRequest(handle, this->address + Offsets::BasePart::Size, &data.size, sizeof(data.size));
		memory.AddScatterReadRequest(handle, this->address + Offsets::BasePart::Color3, &data.color, sizeof(data.color));
		memory.AddScatterReadRequest(handle, this->address + Offsets::BasePart::Transparency, &data.transparency, sizeof(data.transparency));
		memory.ExecuteReadScatter(handle);
	}
	return data;
}
rbx::camera_t::batch_data_t rbx::camera_t::get_batch_data()
{
	batch_data_t data{};
	VMMDLL_SCATTER_HANDLE handle = get_scatter_handle();
	{
		std::lock_guard<std::mutex> lock(g_scatter_mutex);
		memory.AddScatterReadRequest(handle, this->address + Offsets::Camera::Rotation, &data.rotation, sizeof(data.rotation));
		memory.AddScatterReadRequest(handle, this->address + Offsets::Camera::Position, &data.position, sizeof(data.position));
		memory.AddScatterReadRequest(handle, this->address + Offsets::Camera::FieldOfView, &data.field_of_view, sizeof(data.field_of_view));
		memory.ExecuteReadScatter(handle);
	}
	return data;
}
rbx::datamodel_t::batch_data_t rbx::datamodel_t::get_batch_data()
{
	batch_data_t data{};
	VMMDLL_SCATTER_HANDLE handle = get_scatter_handle();
	{
		std::lock_guard<std::mutex> lock(g_scatter_mutex);
		memory.AddScatterReadRequest(handle, this->address + Offsets::DataModel::GameId, &data.game_id, sizeof(data.game_id));
		memory.AddScatterReadRequest(handle, this->address + Offsets::DataModel::PlaceId, &data.place_id, sizeof(data.place_id));
		memory.AddScatterReadRequest(handle, this->address + Offsets::DataModel::CreatorId, &data.creator_id, sizeof(data.creator_id));
		memory.ExecuteReadScatter(handle);
	}
	return data;
}
rbx::visualengine_t::batch_data_t rbx::visualengine_t::get_batch_data()
{
	batch_data_t data{};
	VMMDLL_SCATTER_HANDLE handle = get_scatter_handle();
	{
		std::lock_guard<std::mutex> lock(g_scatter_mutex);
		memory.AddScatterReadRequest(handle, this->address + Offsets::VisualEngine::Dimensions, &data.dimensions, sizeof(data.dimensions));
		memory.AddScatterReadRequest(handle, this->address + Offsets::VisualEngine::ViewMatrix, &data.viewmatrix, sizeof(data.viewmatrix));
		memory.ExecuteReadScatter(handle);
	}
	return data;
}
void rbx::datamodel_t::add_batch_read_requests(void* handle, batch_data_t* data)
{
	VMMDLL_SCATTER_HANDLE scatter_handle = reinterpret_cast<VMMDLL_SCATTER_HANDLE>(handle);
	memory.AddScatterReadRequest(scatter_handle, this->address + Offsets::DataModel::GameId, &data->game_id, sizeof(data->game_id));
	memory.AddScatterReadRequest(scatter_handle, this->address + Offsets::DataModel::PlaceId, &data->place_id, sizeof(data->place_id));
	memory.AddScatterReadRequest(scatter_handle, this->address + Offsets::DataModel::CreatorId, &data->creator_id, sizeof(data->creator_id));
}
void rbx::player_t::add_batch_read_requests(void* handle, batch_data_t* data)
{
	VMMDLL_SCATTER_HANDLE scatter_handle = reinterpret_cast<VMMDLL_SCATTER_HANDLE>(handle);
	memory.AddScatterReadRequest(scatter_handle, this->address + Offsets::Player::Team, &data->team, sizeof(data->team));
	memory.AddScatterReadRequest(scatter_handle, this->address + Offsets::Player::UserId, &data->user_id, sizeof(data->user_id));
	memory.AddScatterReadRequest(scatter_handle, this->address + Offsets::Player::ModelInstance, &data->model_instance, sizeof(data->model_instance));
	memory.AddScatterReadRequest(scatter_handle, this->address + Offsets::Instance::Name, &data->name_addr, sizeof(data->name_addr));
	memory.AddScatterReadRequest(scatter_handle, this->address + Offsets::Player::Country, &data->country_addr, sizeof(data->country_addr));
	memory.AddScatterReadRequest(scatter_handle, this->address + Offsets::Player::Gender, &data->gender_addr, sizeof(data->gender_addr));
	memory.AddScatterReadRequest(scatter_handle, this->address + 0xe50, &data->platform_addr, sizeof(data->platform_addr));
	memory.AddScatterReadRequest(scatter_handle, this->address + 0xe70, &data->os_addr, sizeof(data->os_addr));
}
void rbx::humanoid_t::add_batch_read_requests(void* handle, batch_data_t* data, std::uint64_t* humanoid_state_addr)
{
	VMMDLL_SCATTER_HANDLE scatter_handle = reinterpret_cast<VMMDLL_SCATTER_HANDLE>(handle);
	memory.AddScatterReadRequest(scatter_handle, this->address + Offsets::Humanoid::Health, &data->health, sizeof(data->health));
	memory.AddScatterReadRequest(scatter_handle, this->address + Offsets::Humanoid::MaxHealth, &data->max_health, sizeof(data->max_health));
	memory.AddScatterReadRequest(scatter_handle, this->address + Offsets::Humanoid::HipHeight, &data->hip_height, sizeof(data->hip_height));
	memory.AddScatterReadRequest(scatter_handle, this->address + Offsets::Humanoid::RigType, &data->rig_type, sizeof(data->rig_type));
	if (humanoid_state_addr != nullptr)
	{
		memory.AddScatterReadRequest(scatter_handle, this->address + Offsets::Humanoid::HumanoidState, humanoid_state_addr, sizeof(std::uint64_t));
	}
}
void rbx::primitive_t::add_batch_read_requests(void* handle, batch_data_t* data)
{
	VMMDLL_SCATTER_HANDLE scatter_handle = reinterpret_cast<VMMDLL_SCATTER_HANDLE>(handle);
	memory.AddScatterReadRequest(scatter_handle, this->address + Offsets::BasePart::AssemblyLinearVelocity, &data->velocity, sizeof(data->velocity));
	memory.AddScatterReadRequest(scatter_handle, this->address + Offsets::BasePart::Position, &data->position, sizeof(data->position));
	memory.AddScatterReadRequest(scatter_handle, this->address + Offsets::BasePart::Rotation, &data->rotation, sizeof(data->rotation));
	memory.AddScatterReadRequest(scatter_handle, this->address + Offsets::BasePart::Size, &data->size, sizeof(data->size));
	memory.AddScatterReadRequest(scatter_handle, this->address + Offsets::BasePart::Color3, &data->color, sizeof(data->color));
	memory.AddScatterReadRequest(scatter_handle, this->address + Offsets::BasePart::Transparency, &data->transparency, sizeof(data->transparency));
}