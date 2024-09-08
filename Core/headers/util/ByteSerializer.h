#pragma once
#include "util/FileUtil.h"
#include "util/util.h"

namespace SNAKE {
	template<typename T>
	concept IsSerializableContainer = requires(T t) {
		{ t.data() };
		{ t.size() };
	};

	template<typename T>
	concept IsResizableContainer = requires(T t) {
		{ t.resize(1) };
		{ t.clear() };
	};

	class ByteSerializer {
	public:
		ByteSerializer() {
			m_data.resize(64);
		}

		// data - pointer to data
		// size - size in bytes
		void Value(const std::byte* data, size_t size) {
			Value(size);

			if (m_pos + size >= m_data.size())
				m_data.resize(m_data.size() + size);

			memcpy(m_data.data() + m_pos, data, size);
			m_pos += size;
		}

		template<typename T>
		void Value(T&& val) {
			if (m_pos + sizeof(T) >= m_data.size())
				m_data.resize(m_data.size() + sizeof(T));

			memcpy(m_data.data() + m_pos, &val, sizeof(T));
			m_pos += sizeof(T);
		}


		template<IsSerializableContainer T>
		void Container(T&& val) {
			Value(reinterpret_cast<const std::byte*>(val.data()), val.size() * sizeof(*val.begin()));
		}

		void OutputToFile(const std::string& filepath) {
			files::WriteBinaryFile(filepath, m_data.data(), m_pos);
		}

		std::byte* data() {
			return m_data.data();
		}

		size_t size() {
			return m_pos;
		}

	private:
		size_t m_pos = 0;
		std::vector<std::byte> m_data;
	};

	class ByteDeserializer {
	public:
		ByteDeserializer(std::byte* data, size_t size) : m_data(data), m_data_size(size) {}

		template<typename T>
		void Value(T& output) {
			SNK_ASSERT(m_pos + sizeof(T) <= m_data_size);
			memcpy(&output, m_data + m_pos, sizeof(T));
			m_pos += sizeof(T);
		}

		// Deserializes a value that was serialized as a pointer to data with a fixed size in ByteSerializer
		// Output should be nullptr, it will be allocated here
		template<typename T>
		void Value(T*& output, [[maybe_unused]] size_t PLACEHOLDER) { // TODO: Placeholder?
			size_t size;
			Value(size);

			SNK_ASSERT(!output);
			SNK_ASSERT(m_pos + size <= m_data_size);

			output = new T[size / sizeof(T)];
			memcpy(output, m_data + m_pos, size);
			m_pos += size;
		}

		template<IsSerializableContainer T>
		void Container(T& output) {
			size_t size;
			Value(size);

			size_t required_output_container_size = size / sizeof(typename T::value_type);
			output.resize(required_output_container_size);

			SNK_ASSERT(m_pos + size <= m_data_size);
			memcpy(output.data(), m_data + m_pos, size);
			m_pos += size;
		}

	private:
		std::byte* m_data;
		size_t m_data_size;
		size_t m_pos = 0;
	};

}