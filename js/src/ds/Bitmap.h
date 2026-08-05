/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ds_Bitmap_h
#define ds_Bitmap_h

#include "mozilla/PodOperations.h"

#include <stdint.h>
#include <string.h>

namespace js {

// DenseBitmap is an simple dynamically-sized bitmap backed by a vector of
// words.
class DenseBitmap
{
    using Word = uintptr_t;

    Word* data_;
    size_t numWords_;

  public:
    DenseBitmap()
      : data_(nullptr), numWords_(0)
    {}

    ~DenseBitmap()
    {
        js_free(data_);
    }

    bool ensureSpace(size_t newWords)
    {
        if (newWords <= numWords_)
            return true;

        size_t newSize = sizeof(Word) * newWords;
        Word* newData = static_cast<Word*>(js_realloc(data_, newSize));
        if (!newData)
            return false;

        mozilla::PodZero(newData + numWords_, newWords - numWords_);
        data_ = newData;
        numWords_ = newWords;
        return true;
    }

    void copyBitsFrom(size_t dstStart, size_t wordCount, const Word* src)
    {
        MOZ_ASSERT(dstStart + wordCount <= numWords_);
        memcpy(data_ + dstStart, src, sizeof(Word) * wordCount);
    }

    void bitwiseAndWith(const DenseBitmap& other)
    {
        MOZ_ASSERT(numWords_ == other.numWords_);
        for (size_t i = 0; i < numWords_; i++)
            data_[i] &= other.data_[i];
    }

    void bitwiseOrRangeInto(size_t srcStart, size_t wordCount, Word* dst) const
    {
        MOZ_ASSERT(srcStart + wordCount <= numWords_);
        for (size_t i = 0; i < wordCount; i++)
            dst[i] |= data_[srcStart + i];
    }

    void bitwiseOrInto(DenseBitmap& other) const
    {
        MOZ_ASSERT(numWords_ == other.numWords_);
        for (size_t i = 0; i < numWords_; i++)
            other.data_[i] |= data_[i];
    }

    void bitwiseOrWith(const DenseBitmap& other)
    {
        MOZ_ASSERT(numWords_ == other.numWords_);
        for (size_t i = 0; i < numWords_; i++)
            data_[i] |= other.data_[i];
    }

    bool getBit(size_t bit) const
    {
        size_t wordIndex = bit / (sizeof(Word) * 8);
        size_t bitIndex = bit % (sizeof(Word) * 8);
        MOZ_ASSERT(wordIndex < numWords_);
        return data_[wordIndex] & (Word(1) << bitIndex);
    }

    void setBit(size_t bit)
    {
        size_t wordIndex = bit / (sizeof(Word) * 8);
        size_t bitIndex = bit % (sizeof(Word) * 8);
        MOZ_ASSERT(wordIndex < numWords_);
        data_[wordIndex] |= (Word(1) << bitIndex);
    }

    MOZ_MUST_USE bool init()
    {
        return true;
    }

    Word* data() { return data_; }
    size_t numWords() const { return numWords_; }
};

} // namespace js

#endif // ds_Bitmap_h
