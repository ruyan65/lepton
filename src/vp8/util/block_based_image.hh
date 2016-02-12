#ifndef _BLOCK_BASED_IMAGE_HH_
#define _BLOCK_BASED_IMAGE_HH_
#include "memory.hh"
#include "aligned_block.hh"
#include "block_context.hh"
#include <map>
extern bool g_allow_progressive;
template<bool force_memory_optimization=false>
class BlockBasedImageBase {
    typedef AlignedBlock Block;
    Block *image_;
    uint32_t width_;
    uint32_t nblocks_;
    uint8_t *storage_;
    // if true, this image only contains 2 rows during decode
    bool memory_optimized_image_;
    BlockBasedImageBase(const BlockBasedImageBase&) = delete;
    BlockBasedImageBase& operator=(const BlockBasedImageBase&) = delete;
public:
    BlockBasedImageBase()
      : memory_optimized_image_(force_memory_optimization) {
        image_ = nullptr;
        storage_ = nullptr;
        width_ = 0;
        nblocks_ = 0;
    }
    bool is_memory_optimized() const {
        return force_memory_optimization
            || memory_optimized_image_;
    }
    size_t bytes_allocated() const {
        return 32 + nblocks_ * sizeof(Block);
    }
    size_t blocks_allocated() const {
        return nblocks_;
    }
    void init (uint32_t width, uint32_t height, uint32_t nblocks, bool memory_optimized_image) {
        if (force_memory_optimization) {
            assert(memory_optimized_image && "MemoryOptimized must match template");
        }
        memory_optimized_image_ = force_memory_optimization || memory_optimized_image;
        assert(nblocks <= width * height);
        width_ = width;
        if (force_memory_optimization || memory_optimized_image_) {
#ifdef ALLOW_3_OR_4_SCALING_FACTOR
            nblocks = width * 4;
#else
            nblocks = width * 2;
#endif
        }
        nblocks_ = nblocks;
        storage_ = (uint8_t*)custom_calloc(nblocks * sizeof(Block) + 31);
        size_t offset = storage_ - (uint8_t*)nullptr;
        if (offset & 31) { //needs alignment adjustment
            image_ = (Block*)(storage_ + 32 - (offset & 31));
        } else { // already aligned
            image_ = (Block*)storage_;
        }
    }
    BlockContext begin(std::vector<NeighborSummary>::iterator num_nonzeros_begin) {
        return {image_, nullptr, num_nonzeros_begin, num_nonzeros_begin + width_};
    }
    ConstBlockContext begin(std::vector<NeighborSummary>::iterator num_nonzeros_begin) const {
        return {image_, nullptr, num_nonzeros_begin, num_nonzeros_begin + width_};
    }
    BlockContext off_y(int y,
                       std::vector<NeighborSummary>::iterator num_nonzeros_begin) {
        if (force_memory_optimization || memory_optimized_image_) {
#ifdef ALLOW_3_OR_4_SCALING_FACTOR
            return {image_ + width_ * (y & 3),
                    image_ + ((y + 3) & 3) * width_,
                    (y & 1) ? num_nonzeros_begin + width_ : num_nonzeros_begin,
                    (y & 1) ? num_nonzeros_begin : num_nonzeros_begin + width_};
#else
            return {(y & 1) ? image_ + width_ : image_,
                    (y & 1) ? image_ : image_ + width_,
                    (y & 1) ? num_nonzeros_begin + width_ : num_nonzeros_begin,
                    (y & 1) ? num_nonzeros_begin : num_nonzeros_begin + width_};
#endif
        }
        return {image_ + width_ * y,
                (y != 0) ? image_ + width_ * (y - 1) : nullptr,
                (y & 1) ? num_nonzeros_begin + width_ : num_nonzeros_begin,
                (y & 1) ? num_nonzeros_begin : num_nonzeros_begin + width_};
    }
    ConstBlockContext off_y(int y,
                            std::vector<NeighborSummary>::iterator num_nonzeros_begin) const {
        if (force_memory_optimization || memory_optimized_image_) {
#ifdef ALLOW_3_OR_4_SCALING_FACTOR
            return {image_ + width_ * (y & 3),
                    image_ + ((y + 3) & 3) * width_,
                    (y & 1) ? num_nonzeros_begin + width_ : num_nonzeros_begin,
                    (y & 1) ? num_nonzeros_begin : num_nonzeros_begin + width_};
#else
            return {(y & 1) ? image_ + width_ : image_,
                    (y & 1) ? image_ : image_ + width_,
                    (y & 1) ? num_nonzeros_begin + width_ : num_nonzeros_begin,
                    (y & 1) ? num_nonzeros_begin : num_nonzeros_begin + width_};
#endif
        }
        return {image_ + width_ * y,
                (y != 0) ? image_ + width_ * (y - 1) : nullptr,
                (y & 1) ? num_nonzeros_begin + width_ : num_nonzeros_begin,
                (y & 1) ? num_nonzeros_begin : num_nonzeros_begin + width_};
    }
    template <class BlockContext> uint32_t next(BlockContext& it, bool has_left) const {
        it.context.cur += 1;
        ptrdiff_t offset = it.context.cur - image_;
        uint32_t retval = offset;
        if (force_memory_optimization || memory_optimized_image_) {
#ifdef ALLOW_3_OR_4_SCALING_FACTOR
            if (__builtin_expect(offset == (width_ << 2), 0)) {
                retval = offset = 0;
                it.context.cur = image_;
            }
            if (retval >= (width_ << 1)) {
                retval -= (width_ << 1);
            }
            if (retval >= width_) {
                retval -= width_;
            }
            retval += width_ * it.y;
#else
            if (__builtin_expect(offset == (width_ << 1), 0)) {
                retval = offset = 0;
                it.context.cur = image_;
            }
            if (retval >= width_) {
                retval -= width_;
            }
            retval += width_ * it.y;
#endif
        }
        if (__builtin_expect(offset < width_, 0)) {
#ifdef ALLOW_3_OR_4_SCALING_FACTOR
            it.context.above = it.context.cur + 3 * width_;
#else
            it.context.above = it.context.cur + width_;
#endif
        } else {
            it.context.above = it.context.cur - width_;
        }
        ++it.context.num_nonzeros_here;
        ++it.context.num_nonzeros_above;
        if (!has_left) {
            bool cur_row_first = (it.context.num_nonzeros_here < it.context.num_nonzeros_above);
            if (cur_row_first) {
                it.context.num_nonzeros_above -= width_;
                it.context.num_nonzeros_above -= width_;
            } else {
                it.context.num_nonzeros_here -= width_;
                it.context.num_nonzeros_here -= width_;
            }
        }
        return retval;
    }
    AlignedBlock& at(uint32_t y, uint32_t x) {
        uint32_t index;
        if (force_memory_optimization || memory_optimized_image_) {
#ifdef ALLOW_3_OR_4_SCALING_FACTOR
            index = x + (y & 3) * width_;
#else
            index = (y & 1) ? width_ + x : x;
#endif
            if (__builtin_expect(x >= width_, 0)) {
                custom_exit(ExitCode::OOM);
            }
        } else {
            index = y * width_ + x;
            if (__builtin_expect(index >= nblocks_, 0)) {
                custom_exit(ExitCode::OOM);
            }
        }
        return image_[index];
    }
    const AlignedBlock& at(uint32_t y, uint32_t x) const {
        uint32_t index;
        if (force_memory_optimization || memory_optimized_image_) {
#ifdef ALLOW_3_OR_4_SCALING_FACTOR
            index = x + (y & 3) * width_;
#else
            index = (y & 1) ? width_ + x : x;
#endif
            if (__builtin_expect(x >= width_, 0)) {
                custom_exit(ExitCode::OOM);
            }
        } else {
            index = y * width_ + x;
            if (__builtin_expect(index >= nblocks_, 0)) {
                custom_exit(ExitCode::OOM);
            }
        }
        return image_[index];
    }


    AlignedBlock& raster(uint32_t offset) {
        if (force_memory_optimization || memory_optimized_image_) {
#ifdef ALLOW_3_OR_4_SCALING_FACTOR
            offset = offset % (width_ << 2);
#else
            offset = offset % (width_ << 1);
#endif
            assert(offset <= nblocks_ && "we mod offset by width_: it is < nblocks_");
        } else if (offset >= nblocks_) {
            custom_exit(ExitCode::OOM);
        }
        return image_[offset];
    }
    const AlignedBlock& raster(uint32_t offset) const {
        if (force_memory_optimization || memory_optimized_image_) {
#ifdef ALLOW_3_OR_4_SCALING_FACTOR
            offset = offset % (width_ << 2);
#else
            offset = offset % (width_ << 1);
#endif
            assert(offset <= nblocks_ && "we mod offset by width_: it is < nblocks_");
        } else if (__builtin_expect(offset >= nblocks_, 0)) {
            custom_exit(ExitCode::OOM);
        }
        return image_[offset];
    }
};
class BlockBasedImage : public BlockBasedImageBase<false> {
    BlockBasedImage(const BlockBasedImage&) = delete;
    BlockBasedImage& operator=(const BlockBasedImage&) = delete;
public:
    BlockBasedImage() {}
};
inline
BlockColorContext get_color_context_blocks(
                                        const BlockColorContextIndices & indices,
                                           const Sirikata::Array1d<BlockBasedImage,
                                                                   (uint32_t)ColorChannel::NumBlockTypes> &jpeg,
                                           uint8_t component) {
    BlockColorContext retval = {(uint8_t)component};
    retval.color = component;
    (void)indices;
    (void)jpeg;
#ifdef USE_COLOR_VALUES
    for (size_t i = 0; i < sizeof(indices.luminanceIndex)/sizeof(indices.luminanceIndex[0]); ++i) {
        for (size_t j = 0; j < sizeof(indices.luminanceIndex[0])/sizeof(indices.luminanceIndex[0][0]); ++j) {
            if (indices.luminanceIndex[i][j].initialized()) {
                retval.luminance[i][j] = &jpeg[0].at(indices.luminanceIndex[i][j].get().second,indices.luminanceIndex[i][j].get().first);
            }
        }
    }
    if (indices.chromaIndex.initialized()) {
        retval.chroma = &jpeg[1].at(indices.chromaIndex.get().second,indices.chromaIndex.get().first);
    }
#endif
    return retval;
}
#endif
