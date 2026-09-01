#pragma once
#include <cstdint>
#include <mutex>
#include <optional>
#include <unordered_map>

class CustomVehicleBindingRegistry
{
public:
	static CustomVehicleBindingRegistry& Instance()
	{
		static CustomVehicleBindingRegistry instance;
		return instance;
	}

	void Bind(uint16_t sampVehicleId, uint32_t customModelId)
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		m_bindings[sampVehicleId] = customModelId;
	}

	void Unbind(uint16_t sampVehicleId)
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		auto it = m_bindings.find(sampVehicleId);
		if (it == m_bindings.end())
		{
			return;
		}
		m_bindings.erase(it);
	}

	std::optional<uint32_t> Get(uint16_t sampVehicleId) const
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		auto it = m_bindings.find(sampVehicleId);
		if (it == m_bindings.end())
			return std::nullopt;
		return it->second;
	}

private:
	CustomVehicleBindingRegistry() = default;

	mutable std::mutex m_mutex;
	std::unordered_map<uint16_t, uint32_t> m_bindings;
};
