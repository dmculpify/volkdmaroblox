#include <unordered_map>
#include <string>
#include <chrono>
#include <mutex>
#include <sdk/sdk.h>
namespace cache
{
	void run();
	class RateLimiter {
	private:
		std::chrono::high_resolution_clock::time_point last_tick;
		std::chrono::milliseconds interval;
		std::mutex mtx;
	public:
		explicit RateLimiter(int interval_ms)
			: last_tick(std::chrono::high_resolution_clock::now())
			, interval(interval_ms) {
		}
		bool should_run() {
			std::lock_guard<std::mutex> lock(mtx);
			auto now = std::chrono::high_resolution_clock::now();
			auto delta = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_tick);
			if (delta < interval)
				return false;
			last_tick = now;
			return true;
		}
		static RateLimiter& get_full_cache() {
			static RateLimiter instance(1000);
			return instance;
		}
		static RateLimiter& get_position_update() {
			static RateLimiter instance(8);
			return instance;
		}
	};
	struct tool_info_t final
	{
		rbx::instance_t instance;
		std::string name;
	};
	struct device_info_t final
	{
		std::string operating_system;
		std::string platform;
	};
	struct entity_t final
	{
		rbx::instance_t instance;
		std::uint64_t team;
		std::string name;
		std::string country;
		std::string gender;
		cache::tool_info_t tool;
		cache::device_info_t device_info;
		std::uint64_t userid;
		float health;
		float max_health;
		std::int32_t distance = 100;
		rbx::humanoid_t humanoid;
		std::uint8_t rig_type;
		std::int16_t humanoid_state;
		std::unordered_map<std::string, rbx::meshpart_t> parts;
		std::unordered_map<std::string, rbx::primitive_t::batch_data_t> primitive_cache;
		math::vector3 root_position;
		std::uint64_t model_instance_addr = 0;
		bool operator==(entity_t& other) {
			return instance.address == other.instance.address;
		}
	};
	struct frame_data_t {
		std::vector<entity_t> entities;
		entity_t local_entity;
		std::mutex mtx;
	};
	inline frame_data_t frame_data;
	inline cache::entity_t local_entity;
	inline std::vector<cache::entity_t> entity_cache;
}