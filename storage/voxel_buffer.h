#ifndef VOXEL_BUFFER_INTERNAL_H
#define VOXEL_BUFFER_INTERNAL_H

#include "../util/containers/fixed_array.h"
#include "../util/containers/flat_map.h"
#include "../util/containers/small_vector.h"
#include "../util/math/box3i.h"
#include "../util/math/ortho_basis.h"
#include "funcs.h"
#include "metadata/voxel_metadata.h"

#include <limits>

namespace zylann {

class DynamicBitset;

namespace voxel {

struct VoxelFormat;

// Dense voxels data storage.
// Organized in channels of configurable bit depth.
// Values can be interpreted either as unsigned integers or normalized floats.
class VoxelBuffer {
public:
	enum ChannelId {
		CHANNEL_TYPE = 0,
		CHANNEL_SDF,
		CHANNEL_COLOR,
		CHANNEL_INDICES,
		CHANNEL_WEIGHTS,
		CHANNEL_DATA5,
		CHANNEL_DATA6,
		CHANNEL_DATA7,
		// Arbitrary value, 8 should be enough. Tweak for your needs.
		MAX_CHANNELS
	};

	static const char *get_channel_name(const ChannelId id);

	static const int ALL_CHANNELS_MASK = 0xff;

	enum Compression : uint8_t {
		COMPRESSION_NONE = 0,
		COMPRESSION_UNIFORM, // aka "no voxels allocated"
		COMPRESSION_COUNT
	};

	enum Depth : uint8_t { //
		DEPTH_8_BIT,
		DEPTH_16_BIT,
		DEPTH_32_BIT,
		DEPTH_64_BIT,
		DEPTH_COUNT
	};

	enum Allocator : uint8_t { //
		// General-purpose allocator. malloc, Godot's default allocator. Deallocated when the buffer is destroyed.
		ALLOCATOR_DEFAULT,
		// VoxelMemoryPool. Should be faster but remains allocated. Preferred if buffers of similar size are frequently
		// created at runtime. Don't use for large, infrequent allocations or in-editor, to avoid hoarding memory.
		ALLOCATOR_POOL,
		ALLOCATOR_COUNT
	};

	static inline uint32_t get_depth_byte_count(VoxelBuffer::Depth d) {
		ZN_ASSERT(d >= 0 && d < VoxelBuffer::DEPTH_COUNT);
		return 1 << d;
	}

	static inline uint32_t get_depth_bit_count(Depth d) {
		// CRASH_COND(d < 0 || d >= VoxelBuffer::DEPTH_COUNT);
		return get_depth_byte_count(d) << 3;
	}

	static inline Depth get_depth_from_size(size_t size) {
		switch (size) {
			case 1:
				return DEPTH_8_BIT;
			case 2:
				return DEPTH_16_BIT;
			case 4:
				return DEPTH_32_BIT;
			case 8:
				return DEPTH_64_BIT;
			default:
				ZN_CRASH();
		}
		return DEPTH_COUNT;
	}

	static const Depth DEFAULT_CHANNEL_DEPTH = DEPTH_8_BIT;
	static const Depth DEFAULT_TYPE_CHANNEL_DEPTH = DEPTH_16_BIT;
	static const Depth DEFAULT_SDF_CHANNEL_DEPTH = DEPTH_16_BIT;
	static const Depth DEFAULT_INDICES_CHANNEL_DEPTH = DEPTH_16_BIT;
	static const Depth DEFAULT_WEIGHTS_CHANNEL_DEPTH = DEPTH_16_BIT;

	// Limit was made explicit for serialization reasons, and also because there must be a reasonable one
	static const uint32_t MAX_SIZE = 65535;

	struct Channel {
		union {
			// Allocated when the channel is populated.
			// Flat array, in order [z][x][y] because it allows faster vertical-wise access (the engine is Y-up).
			uint8_t *data;

			// Default value when the channel is not populated ().
			// This is an encoded value, so non-integer values may be obtained by converting it.
			uint64_t defval;
		};

		Depth depth = DEFAULT_CHANNEL_DEPTH;
		Compression compression = COMPRESSION_UNIFORM;
		// [...] 2 unused bytes

		// Storing gigabytes in a single buffer is neither supported nor practical.
		uint32_t size_in_bytes = 0;

		static const size_t MAX_SIZE_IN_BYTES = std::numeric_limits<uint32_t>::max();
	};

	// VoxelBuffer();
	VoxelBuffer(Allocator allocator);
	VoxelBuffer(VoxelBuffer &&src);

	~VoxelBuffer();

	VoxelBuffer &operator=(VoxelBuffer &&src);

	void create(unsigned int sx, unsigned int sy, unsigned int sz, const VoxelFormat *new_format = nullptr);
	void create(const Vector3i size, const VoxelFormat *new_format = nullptr);

	void clear(const VoxelFormat *new_format = nullptr);
	void clear_channel(unsigned int channel_index, uint64_t clear_value);
	void clear_channel_f(unsigned int channel_index, real_t clear_value);

	bool has_format(const VoxelFormat &p_format) const;

	inline Allocator get_allocator() const {
		return _allocator;
	}

	inline const Vector3i &get_size() const {
		return _size;
	}

	static uint64_t get_default_raw_value(const VoxelBuffer::ChannelId channel, const VoxelBuffer::Depth depth);
	static uint64_t get_default_sdf_raw_value(const Depth depth);
	static float get_default_sdf_value(const Depth depth);
	static uint64_t get_default_indices_raw_value(const Depth depth);

	uint64_t get_voxel(int x, int y, int z, unsigned int channel_index) const;
	void set_voxel(uint64_t value, int x, int y, int z, unsigned int channel_index);

	real_t get_voxel_f(int x, int y, int z, unsigned int channel_index) const;
	inline real_t get_voxel_f(Vector3i pos, unsigned int channel_index) const {
		return get_voxel_f(pos.x, pos.y, pos.z, channel_index);
	}
	void set_voxel_f(real_t value, int x, int y, int z, unsigned int channel_index);
	inline void set_voxel_f(real_t value, Vector3i pos, unsigned int channel_index) {
		set_voxel_f(value, pos.x, pos.y, pos.z, channel_index);
	}

	inline uint64_t get_voxel(const Vector3i pos, unsigned int channel_index) const {
		return get_voxel(pos.x, pos.y, pos.z, channel_index);
	}
	inline void set_voxel(int value, const Vector3i pos, unsigned int channel_index) {
		set_voxel(value, pos.x, pos.y, pos.z, channel_index);
	}

	void fill(uint64_t defval, unsigned int channel_index);
	void fill_area(uint64_t defval, Vector3i min, Vector3i max, unsigned int channel_index);
	void fill_area_f(float fvalue, Vector3i min, Vector3i max, unsigned int channel_index);
	void fill_f(real_t value, unsigned int channel);

	bool is_uniform(unsigned int channel_index) const;

	void compress_uniform_channels();
	void decompress_channel(unsigned int channel_index);
	Compression get_channel_compression(unsigned int channel_index) const;

	static size_t get_size_in_bytes_for_volume(Vector3i size, Depth depth);

	void copy_format(const VoxelBuffer &other);

	// Specialized copy functions.
	// Note: these functions don't include metadata on purpose.
	// If you also want to copy metadata, use the specialized functions.
	void copy_channels_from(const VoxelBuffer &other);
	void copy_channel_from(const VoxelBuffer &other, unsigned int channel_index);
	void copy_channel_from(
			const VoxelBuffer &other,
			Vector3i src_min,
			Vector3i src_max,
			Vector3i dst_min,
			unsigned int channel_index
	);

	// Copy a region from a box of values, passed as a raw array.
	// `src_size` is the total 3D size of the source box.
	// `src_min` and `src_max` are the sub-region of that box we want to copy.
	// `dst_min` is the lower corner where we want the data to be copied into the destination.
	template <typename T>
	void copy_channel_from(
			Span<const T> src,
			Vector3i src_size,
			Vector3i src_min,
			Vector3i src_max,
			Vector3i dst_min,
			unsigned int channel_index
	) {
		ZN_ASSERT_RETURN(channel_index < MAX_CHANNELS);

		Channel &channel = _channels[channel_index];
#ifdef DEBUG_ENABLED
		// Size of source and destination values must match
		ZN_ASSERT_RETURN(channel.depth == get_depth_from_size(sizeof(T)));
#endif

		// This function always decompresses the destination.
		// To keep it compressed, either check what you are about to copy,
		// or schedule a recompression for later.
		decompress_channel(channel_index);

		Span<T> dst = Span<uint8_t>(channel.data, channel.size_in_bytes).reinterpret_cast_to<T>();
		copy_3d_region_zxy<T>(dst, _size, dst_min, src, src_size, src_min, src_max);
	}

	// Copy a region of the data into a dense buffer.
	// If the source is compressed, it is decompressed.
	// `dst` is a raw array storing grid values in a box.
	// `dst_size` is the total size of the box.
	// `dst_min` is the lower corner of where we want the source data to be stored.
	// `src_min` and `src_max` is the sub-region of the source we want to copy.
	template <typename T>
	void copy_channel_to(
			Span<T> dst,
			Vector3i dst_size,
			Vector3i dst_min,
			Vector3i src_min,
			Vector3i src_max,
			unsigned int channel_index
	) const {
		ZN_ASSERT_RETURN(channel_index < MAX_CHANNELS);

		const Channel &channel = _channels[channel_index];
#ifdef DEBUG_ENABLED
		// Size of source and destination values must match
		ZN_ASSERT_RETURN(channel.depth == get_depth_from_size(sizeof(T)));
#endif

		if (channel.compression == COMPRESSION_UNIFORM) {
			fill_3d_region_zxy<T>(dst, dst_size, dst_min, dst_min + (src_max - src_min), channel.defval);
		} else {
			Span<const T> src(static_cast<const T *>(channel.data), channel.size_in_bytes / sizeof(T));
			copy_3d_region_zxy<T>(dst, dst_size, dst_min, src, _size, src_min, src_max);
		}
	}

	// TODO Deprecate?
	// Executes a read-write action on all cells of the provided box that intersect with this buffer.
	// `action_func` receives a voxel value from the channel, and returns a modified value.
	// if the returned value is different, it will be applied to the buffer.
	// Can be used to blend voxels together.
	template <typename F>
	inline void read_write_action(Box3i box, unsigned int channel_index, F action_func) {
		ZN_ASSERT_RETURN(channel_index < MAX_CHANNELS);

		box.clip(Box3i(Vector3i(), _size));
		const Vector3i min_pos = box.position;
		const Vector3i max_pos = box.position + box.size;
		Vector3i pos;
		for (pos.z = min_pos.z; pos.z < max_pos.z; ++pos.z) {
			for (pos.x = min_pos.x; pos.x < max_pos.x; ++pos.x) {
				for (pos.y = min_pos.y; pos.y < max_pos.y; ++pos.y) {
					// TODO Optimization: a bunch of checks and branching could be skipped
					const uint64_t v0 = get_voxel(pos, channel_index);
					const uint64_t v1 = action_func(pos, v0);
					if (v0 != v1) {
						set_voxel(v1, pos, channel_index);
					}
				}
			}
		}
	}

	static inline size_t get_index(const Vector3i pos, const Vector3i size) {
		return Vector3iUtil::get_zxy_index(pos, size);
	}

	inline size_t get_index(unsigned int x, unsigned int y, unsigned int z) const {
		return y + _size.y * (x + _size.x * z); // ZXY index
	}

	template <typename F>
	inline void for_each_index_and_pos(const Box3i &box, F f) {
		const Vector3i min_pos = box.position;
		const Vector3i max_pos = box.position + box.size;
		Vector3i pos;
		for (pos.z = min_pos.z; pos.z < max_pos.z; ++pos.z) {
			for (pos.x = min_pos.x; pos.x < max_pos.x; ++pos.x) {
				pos.y = min_pos.y;
				size_t i = get_index(pos.x, pos.y, pos.z);
				for (; pos.y < max_pos.y; ++pos.y) {
					f(i, pos);
					++i;
				}
			}
		}
	}

	// Data_T action_func(Vector3i pos, Data_T in_v)
	template <typename F, typename Data_T>
	void write_box_template(const Box3i &box, unsigned int channel_index, F action_func, Vector3i offset) {
		decompress_channel(channel_index);
		Channel &channel = _channels[channel_index];
#ifdef DEBUG_ENABLED
		ZN_ASSERT_RETURN(Box3i(Vector3i(), _size).contains(box));
		ZN_ASSERT_RETURN(get_depth_byte_count(channel.depth) == sizeof(Data_T));
#endif
		Span<Data_T> data = Span<uint8_t>(channel.data, channel.size_in_bytes).reinterpret_cast_to<Data_T>();
		// `&` is required because lambda captures are `const` by default and `mutable` can be used only from C++23
		for_each_index_and_pos(box, [&data, action_func, offset](size_t i, Vector3i pos) {
			// This does not require the action to use the exact type, conversion can occur here.
			data.set(i, action_func(pos + offset, data[i]));
		});
		compress_if_uniform(channel);
	}

	// void action_func(Vector3i pos, Data0_T &inout_v0, Data1_T &inout_v1)
	template <typename F, typename Data0_T, typename Data1_T>
	void write_box_2_template(
			const Box3i &box,
			unsigned int channel_index0,
			unsigned int channel_index1,
			F action_func,
			Vector3i offset
	) {
		decompress_channel(channel_index0);
		decompress_channel(channel_index1);
		Channel &channel0 = _channels[channel_index0];
		Channel &channel1 = _channels[channel_index1];
#ifdef DEBUG_ENABLED
		ZN_ASSERT_RETURN(Box3i(Vector3i(), _size).contains(box));
		ZN_ASSERT_RETURN(get_depth_byte_count(channel0.depth) == sizeof(Data0_T));
		ZN_ASSERT_RETURN(get_depth_byte_count(channel1.depth) == sizeof(Data1_T));
#endif
		Span<Data0_T> data0 = Span<uint8_t>(channel0.data, channel0.size_in_bytes).reinterpret_cast_to<Data0_T>();
		Span<Data1_T> data1 = Span<uint8_t>(channel1.data, channel1.size_in_bytes).reinterpret_cast_to<Data1_T>();
		for_each_index_and_pos(box, [action_func, offset, &data0, &data1](size_t i, Vector3i pos) {
			// TODO The caller must still specify exactly the correct type, maybe some conversion could be used
			action_func(pos + offset, data0[i], data1[i]);
		});
		compress_if_uniform(channel0);
		compress_if_uniform(channel1);
	}

	template <typename F>
	void write_box(const Box3i &box, unsigned int channel_index, F action_func, Vector3i offset) {
#ifdef DEBUG_ENABLED
		ZN_ASSERT_RETURN(channel_index < MAX_CHANNELS);
#endif
		const Channel &channel = _channels[channel_index];
		switch (channel.depth) {
			case DEPTH_8_BIT:
				write_box_template<F, uint8_t>(box, channel_index, action_func, offset);
				break;
			case DEPTH_16_BIT:
				write_box_template<F, uint16_t>(box, channel_index, action_func, offset);
				break;
			case DEPTH_32_BIT:
				write_box_template<F, uint32_t>(box, channel_index, action_func, offset);
				break;
			case DEPTH_64_BIT:
				write_box_template<F, uint64_t>(box, channel_index, action_func, offset);
				break;
			default:
				ZN_PRINT_ERROR("Unknown channel");
				break;
		}
	}

	/*template <typename F>
	void write_box_2(const Box3i &box, unsigned int channel_index0, unsigned int channel_index1, F action_func,
			Vector3i offset) {
#ifdef DEBUG_ENABLED
		ERR_FAIL_INDEX(channel_index0, MAX_CHANNELS);
		ERR_FAIL_INDEX(channel_index1, MAX_CHANNELS);
#endif
		const Channel &channel0 = _channels[channel_index0];
		const Channel &channel1 = _channels[channel_index1];
#ifdef DEBUG_ENABLED
		// TODO Find a better way to handle combination explosion. For now I allow only what's really used.
		ERR_FAIL_COND_MSG(channel1.depth != DEPTH_16_BIT, "Second channel depth is hardcoded to 16 for now");
#endif
		switch (channel.depth) {
			case DEPTH_8_BIT:
				write_box_2_template<F, uint8_t, uint16_t>(box, channel_index0, channel_index1, action_func, offset);
				break;
			case DEPTH_16_BIT:
				write_box_2_template<F, uint16_t, uint16_t>(box, channel_index0, channel_index1, action_func, offset);
				break;
			case DEPTH_32_BIT:
				write_box_2_template<F, uint32_t, uint16_t>(box, channel_index0, channel_index1, action_func, offset);
				break;
			case DEPTH_64_BIT:
				write_box_2_template<F, uint64_t, uint16_t>(box, channel_index0, channel_index1, action_func, offset);
				break;
			default:
				ERR_FAIL();
				break;
		}
	}*/

	static inline SmallVector<uint8_t, MAX_CHANNELS> mask_to_channels_list(uint8_t channels_mask) {
		SmallVector<uint8_t, MAX_CHANNELS> channels;
		for (unsigned int channel_index = 0; channel_index < VoxelBuffer::MAX_CHANNELS; ++channel_index) {
			if (((1 << channel_index) & channels_mask) != 0) {
				channels.push_back(channel_index);
			}
		}
		return channels;
	}

	void copy_to(VoxelBuffer &dst, bool include_metadata) const;
	void move_to(VoxelBuffer &dst);

	inline bool is_position_valid(unsigned int x, unsigned int y, unsigned int z) const {
		return x < (unsigned)_size.x && y < (unsigned)_size.y && z < (unsigned)_size.z;
	}

	inline bool is_position_valid(const Vector3i pos) const {
		return is_position_valid(pos.x, pos.y, pos.z);
	}

	inline bool is_box_valid(const Box3i box) const {
		return Box3i(Vector3i(), _size).contains(box);
	}

	inline uint64_t get_volume() const {
		return Vector3iUtil::get_volume_u64(_size);
	}

	// Gets a slice aliasing the channel's data
	bool get_channel_as_bytes(unsigned int channel_index, Span<uint8_t> &slice);

	// Gets a read-only slice aliasing the channel's data
	bool get_channel_as_bytes_read_only(unsigned int channel_index, Span<const uint8_t> &slice) const;

	// Gets a slice aliasing the channel's data, reinterpreted to a specific type
	template <typename T>
	bool get_channel_data(unsigned int channel_index, Span<T> &dst) {
		Span<uint8_t> dst8;
		ZN_ASSERT_RETURN_V(get_channel_as_bytes(channel_index, dst8), false);
		dst = dst8.reinterpret_cast_to<T>();
		return true;
	}

	// Gets a read-only slice aliasing the channel's data, reinterpreted to a specific type
	template <typename T>
	bool get_channel_data_read_only(unsigned int channel_index, Span<const T> &dst) const {
		Span<const uint8_t> dst8;
		ZN_ASSERT_RETURN_V(get_channel_as_bytes_read_only(channel_index, dst8), false);
		dst = dst8.reinterpret_cast_to<const T>();
		return true;
	}

	// Overwrites contents of a channel with raw data. This skips default initialization of the channel, so it
	// can be a little bit faster than using `decompress_channel`. The input data must have the right size.
	void set_channel_from_bytes(const unsigned int channel_index, Span<const uint8_t> src);

	void downscale_to(VoxelBuffer &dst, Vector3i src_min, Vector3i src_max, Vector3i dst_min) const;

	bool equals(const VoxelBuffer &p_other) const;

	void set_channel_depth(unsigned int channel_index, Depth new_depth);
	Depth get_channel_depth(unsigned int channel_index) const;

	// When using lower than 32-bit resolution for terrain signed distance fields,
	// it should be scaled to better fit the range of represented values since the storage is normalized to -1..1.
	// This returns that scale for a given depth configuration.
	static float get_sdf_quantization_scale(Depth d);

	void get_range_f(float &out_min, float &out_max, ChannelId channel_index) const;

	void transform(const math::OrthoBasis &basis);

	// Metadata

	VoxelMetadata &get_block_metadata() {
		return _block_metadata;
	}
	const VoxelMetadata &get_block_metadata() const {
		return _block_metadata;
	}

	const VoxelMetadata *get_voxel_metadata(Vector3i pos) const;
	VoxelMetadata *get_voxel_metadata(Vector3i pos);
	VoxelMetadata *get_or_create_voxel_metadata(Vector3i pos);
	void erase_voxel_metadata(Vector3i pos);

	void clear_and_set_voxel_metadata(Span<FlatMapMoveOnly<Vector3i, VoxelMetadata>::Pair> pairs);

	template <typename F>
	void for_each_voxel_metadata_in_area(Box3i box, F callback) const {
		// TODO For `find`s and this kind of iteration, we may want to separate keys and values in FlatMap's internal
		// storage, to reduce cache misses
		for (FlatMapMoveOnly<Vector3i, VoxelMetadata>::ConstIterator it = _voxel_metadata.begin();
			 it != _voxel_metadata.end();
			 ++it) {
			if (box.contains(it->key)) {
				callback(it->key, it->value);
			}
		}
	}

	template <typename F>
	inline void erase_voxel_metadata_if(F predicate) {
		_voxel_metadata.remove_if(predicate);
	}

	// #ifdef ZN_GODOT
	// 	// TODO Move out of here
	// 	void for_each_voxel_metadata(const Callable &callback) const;
	// 	void for_each_voxel_metadata_in_area(const Callable &callback, Box3i box) const;
	// #endif

	void clear_voxel_metadata();
	void clear_voxel_metadata_in_area(Box3i box);
	void copy_voxel_metadata_in_area(const VoxelBuffer &src_buffer, Box3i src_box, Vector3i dst_origin);
	void copy_voxel_metadata(const VoxelBuffer &src_buffer);

	const FlatMapMoveOnly<Vector3i, VoxelMetadata> &get_voxel_metadata() const {
		return _voxel_metadata;
	}

#ifdef VOXEL_TESTS
	void check_voxel_metadata_integrity() const;
#endif

private:
	void init_channel_defaults();
	bool create_channel_noinit(int i, Vector3i size);
	bool create_channel(int i, uint64_t defval);
	void delete_channel(int i);
	void compress_if_uniform(Channel &channel);
	static void delete_channel(Channel &channel, Allocator allocator);
	static void clear_channel(Channel &channel, uint64_t clear_value, Allocator allocator);
	static bool is_uniform(const Channel &channel);

private:
	// Each channel can store arbitrary data.
	// For example, you can decide to store colors (R, G, B, A), gameplay types (type, state, light) or both.
	FixedArray<Channel, MAX_CHANNELS> _channels;

	// How many voxels are there in the three directions. All populated channels have the same size.
	Vector3i _size;

	// Which allocator will be used when storing individual voxels is needed.
	// The default is the least likely to be misused, though not necessarily the fastest.
	Allocator _allocator = ALLOCATOR_DEFAULT;

	// TODO Could we separate metadata from VoxelBuffer?
	VoxelMetadata _block_metadata;
	// This metadata is expected to be sparse, with low amount of items.
	FlatMapMoveOnly<Vector3i, VoxelMetadata> _voxel_metadata;
};

void get_unscaled_sdf(const VoxelBuffer &voxels, Span<float> sdf);
void scale_and_store_sdf(VoxelBuffer &voxels, Span<float> sdf);
void scale_and_store_sdf_if_modified(VoxelBuffer &voxels, Span<float> sdf, Span<const float> comparand);

void paste(
		Span<const uint8_t> channels,
		const VoxelBuffer &src_buffer,
		VoxelBuffer &dst_buffer,
		const Vector3i dst_base_pos,
		bool with_metadata
);

// Paste if the source is not a certain value
void paste_src_masked(
		Span<const uint8_t> channels,
		const VoxelBuffer &src_buffer,
		unsigned int src_mask_channel,
		uint64_t src_mask_value,
		VoxelBuffer &dst_buffer,
		const Vector3i dst_base_pos,
		bool with_metadata
);

// Paste if the source is not a certain value, and the destination is a certain value
void paste_src_masked_dst_writable_value(
		Span<const uint8_t> channels,
		const VoxelBuffer &src_buffer,
		unsigned int src_mask_channel,
		uint64_t src_mask_value,
		VoxelBuffer &dst_buffer,
		const Vector3i dst_base_pos,
		unsigned int dst_mask_channel,
		uint64_t dst_mask_value,
		bool with_metadata
);

// Paste if the source is not a certain value, and the specified bitset contains the destination value
void paste_src_masked_dst_writable_bitarray(
		Span<const uint8_t> channels,
		const VoxelBuffer &src_buffer,
		unsigned int src_mask_channel,
		uint64_t src_mask_value,
		VoxelBuffer &dst_buffer,
		const Vector3i dst_base_pos,
		unsigned int dst_mask_channel,
		const DynamicBitset &bitarray,
		bool with_metadata
);

} // namespace voxel
} // namespace zylann

#endif // VOXEL_BUFFER_INTERNAL_H
