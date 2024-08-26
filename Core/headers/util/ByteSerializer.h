#pragma once
#include "util/util.h"
#include "util/FileUtil.h"

namespace SNAKE {
	template<typename T>
	concept IsSerializableContainer = requires(T t) {
		{ t.data() };
		{ t.size() };
	};

	template<typename T>
	concept IsResizableContainer = requires(T t) {
		t.resize(1);
	};

	class ByteSerializer {
	public:
		ByteSerializer() {
			m_data.resize(64);
		}

		// data - pointer to data
		// size - size in bytes
		void Value(const std::byte* data, size_t size) {
			if (m_pos + size >= m_data.size())
				m_data.resize(m_data.size() + size);

			Value(size);
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
		void Value(std::byte*& output, size_t PLACEHOLDER) {
			size_t size;
			Value(size);

			SNK_ASSERT(!output);
			SNK_ASSERT(m_pos + size <= m_data_size);

			output = new std::byte[size];
			memcpy(output, m_data + m_pos, size);
			m_pos += size;
		}

		template<IsSerializableContainer T>
		void Container(T& output) {
			size_t size;
			Value(size);

			if (output.size() < size) {
				if constexpr (IsResizableContainer<T>) {
					if (output.size() < size)
						output.resize(size);
				}
				else
					SNK_ASSERT_ARG(false, "Output container size not big enough");
			}

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