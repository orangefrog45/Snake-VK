#pragma once

namespace SNAKE {
	template<std::integral T>
		requires(std::is_same_v<T, uint32_t> || std::is_same_v<T, uint64_t>)
	class UUID {
	public:
		friend class SceneSerializer;
		UUID();
		explicit UUID(T uuid) : m_uuid(uuid) {
		}
		UUID(const UUID&) { UUID(); }

		explicit operator T() const { return m_uuid; }
		T operator() () const { return m_uuid; };


	private:
		T m_uuid;
	};

}

namespace std {

	template<>
	struct hash<SNAKE::UUID<uint64_t>> {
		std::size_t operator()(const SNAKE::UUID<uint64_t>& uuid) const {
			return hash<uint64_t>()((uint64_t)uuid);
		}
	};

	template<>
	struct hash<SNAKE::UUID<uint32_t>> {
		std::size_t operator()(const SNAKE::UUID<uint32_t>& uuid) const {
			return hash<uint32_t>()((uint32_t)uuid);
		}
	};
}