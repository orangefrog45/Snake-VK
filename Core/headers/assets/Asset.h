#pragma once
#include "util/UUID.h"

namespace SNAKE {
	class Asset {
	public:
		Asset(uint64_t _uuid) : uuid(_uuid) {};

		virtual ~Asset() = default;

		uint64_t GetRefCount() const noexcept {
			return ref_count;
		}

		UUID<uint64_t> uuid;

		std::string filepath;

		std::string name;

	private:
		// Number of AssetRef objects referencing this asset
		uint64_t ref_count = 0;

		template <typename T> friend class AssetRef;
	};

	template<typename AssetT>
	class AssetRef {
	public:
		AssetRef() = delete;

		~AssetRef() { if (p_asset) p_asset->ref_count--; }
		AssetRef(AssetT& _asset) : p_asset(&_asset) { p_asset->ref_count++; };
		AssetRef(AssetT* _asset) : p_asset(_asset) { if (p_asset) p_asset->ref_count++; };
		AssetRef(const AssetRef& other) : p_asset(other.p_asset) { if (p_asset) p_asset->ref_count++; };
		AssetRef(AssetRef&& other) : p_asset(other.p_asset) { other.p_asset = nullptr; };
		AssetRef& operator=(const AssetRef& other) {
			if (this == &other)
				return *this;

			p_asset = other.p_asset;
			if (p_asset) p_asset->ref_count++;
			return *this;
		};

		void SetAsset(AssetT* _asset) {
			if (p_asset) 
				p_asset->ref_count--;

			p_asset = _asset;
			_asset->ref_count++;
		}

		AssetT* operator->() const {
			return p_asset;
		}

	private:
		AssetT* p_asset;
	};

}