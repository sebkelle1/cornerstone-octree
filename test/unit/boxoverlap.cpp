/*
 * MIT License
 *
 * Copyright (c) 2021 CSCS, ETH Zurich
 *               2021 University of Basel
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/*! \file
 * \brief Box overlap tests
 *
 * \author Sebastian Keller <sebastian.f.keller@gmail.com>
 */


#include "gtest/gtest.h"

#include "cstone/boxoverlap.hpp"

using namespace cstone;

/*! \brief add (binary) zeros behind a prefix
 *
 * Allows comparisons, such as number of leading common bits (cpr)
 * of the prefix with Morton codes.
 *
 * @tparam I      32- or 64-bit unsigned integer type
 * @param prefix  the bit pattern
 * @param length  number of bits in the prefix
 * @return        prefix padded out with zeros
 *
 */
template <class I>
constexpr I pad(I prefix, int length)
{
    return prefix << (3*maxTreeLevel<I>{} - length);
}

TEST(BoxOverlap, padUtility)
{
    EXPECT_EQ(pad(0b011,   3), 0b00011 << 27);
    EXPECT_EQ(pad(0b011ul, 3), 0b0011ul << 60);
}

template<class I>
void pbcAdjust()
{
    constexpr int maxCoord = 1u<<maxTreeLevel<I>{};
    EXPECT_EQ(1,          pbcAdjust<maxCoord>(1));
    EXPECT_EQ(maxCoord-1, pbcAdjust<maxCoord>(-1));
    EXPECT_EQ(0,          pbcAdjust<maxCoord>(maxCoord));
    EXPECT_EQ(maxCoord-1, pbcAdjust<maxCoord>(-maxCoord-1));
    EXPECT_EQ(2,          pbcAdjust<maxCoord>(4*maxCoord + 2));
}

TEST(BoxOverlap, pbcAdjust)
{
    pbcAdjust<unsigned>();
    pbcAdjust<uint64_t>();
}

TEST(BoxOverlap, overlapRange)
{
    constexpr int R = 1024;

    EXPECT_TRUE (overlapRange<R>(0,2,1,3));
    EXPECT_FALSE(overlapRange<R>(0,1,1,2));
    EXPECT_FALSE(overlapRange<R>(0,1,2,3));
    EXPECT_TRUE (overlapRange<R>(0,1023,1,3));
    EXPECT_TRUE (overlapRange<R>(0,1024,1,3));
    EXPECT_TRUE (overlapRange<R>(0,2048,1,3));

    EXPECT_TRUE (overlapRange<R>(1022,1024,1023,1024));
    EXPECT_TRUE (overlapRange<R>(1023,1025,0,1));
    EXPECT_FALSE(overlapRange<R>(0,1,1023,1024));
    EXPECT_TRUE (overlapRange<R>(-1,1, 1023, 1024));
    EXPECT_FALSE(overlapRange<R>(-1,1, 1022, 1023));

    EXPECT_TRUE (overlapRange<R>(1023, 2048, 0, 1));
    EXPECT_TRUE (overlapRange<R>(512, 1024, 332, 820));
}

/*! \brief Test overlap between octree nodes and coordinate ranges
 *
 * The octree node is given as a Morton code plus number of bits
 * and the coordinates as integer ranges.
 */
template <class I>
void overlapTest()
{
    // range of a level-2 node
    int r = I(1)<<(maxTreeLevel<I>{} - 2);

    // node range: [r,2r]^3
    I prefix         = pad(I(0b000111), 6);
    int prefixLength = 6;

    I bound = pad(I(0b001), 3);

    EXPECT_EQ(prefixLength, treeLevel(bound-prefix) * 3);

    /// Each test is a separate case

    EXPECT_FALSE(overlap(prefix, prefixLength, Box<int>{0, r, 0, r, 0, r}));
    EXPECT_FALSE(overlap(prefix, bound, Box<int>{0, r, 0, r, 0, r}));

    // exact match
    EXPECT_TRUE(overlap(prefix, prefixLength, Box<int>{r, 2*r, r, 2*r, r, 2*r}));
    // contained within (1,1,1) corner of node
    EXPECT_TRUE(overlap(prefix, prefixLength, Box<int>{2*r-1, 2*r, 2*r-1, 2*r, 2*r-1, 2*r}));
    // contained and exceeding (1,1,1) corner by 1 in all dimensions
    EXPECT_TRUE(overlap(prefix, prefixLength, Box<int>{2*r-1, 2*r+1, 2*r-1, 2*r+1, 2*r-1, 2*r+1}));

    // all of these miss the (1,1,1) corner by 1 in one of the three dimensions
    EXPECT_FALSE(overlap(prefix, prefixLength, Box<int>{2*r, 2*r+1, 2*r-1, 2*r, 2*r-1, 2*r}));
    EXPECT_FALSE(overlap(prefix, prefixLength, Box<int>{2*r-1, 2*r, 2*r, 2*r+1, 2*r-1, 2*r}));
    EXPECT_FALSE(overlap(prefix, prefixLength, Box<int>{2*r-1, 2*r, 2*r-1, 2*r, 2*r, 2*r+1}));

    // contained within (0,0,0) corner of node
    EXPECT_TRUE(overlap(prefix, prefixLength, Box<int>{r, r+1, r, r+1, r, r+1}));

    // all of these miss the (0,0,0) corner by 1 in one of the three dimensions
    EXPECT_FALSE(overlap(prefix, prefixLength, Box<int>{r-1, r, r, r+1, r, r+1}));
    EXPECT_FALSE(overlap(prefix, prefixLength, Box<int>{r, r+1, r-1, r, r, r+1}));
    EXPECT_FALSE(overlap(prefix, prefixLength, Box<int>{r, r+1, r, r+1, r-1, r}));
}

TEST(BoxOverlap, overlaps)
{
    overlapTest<unsigned>();
    overlapTest<uint64_t>();
}

//! \brief test overlaps of periodic halo boxes with parts of the SFC tree
template<class I>
void pbcOverlaps()
{
    int maxCoord = (1u<<maxTreeLevel<I>{}) - 1;
    {
        Box<int> haloBox{-1, 1, 0, 1, 0, 1};
        EXPECT_TRUE(overlap(I(0), I(1), haloBox));
    }
    {
        I firstCode  = codeFromBox<I>(maxCoord,0,0, maxTreeLevel<I>{});
        I secondCode = firstCode + 1;
        Box<int> haloBox{-1, 1, 0, 1, 0, 1};
        EXPECT_TRUE(overlap(firstCode, secondCode, haloBox));
    }
    {
        Box<int> haloBox{maxCoord, maxCoord+2, 0, 1, 0, 1};
        EXPECT_TRUE(overlap(I(0), I(1), haloBox));
    }
    {
        Box<int> haloBox{-1, 1, -1, 1, -1, 1};
        EXPECT_TRUE(overlap(nodeRange<I>(0)-1, nodeRange<I>(0), haloBox));
    }
}

TEST(BoxOverlap, pbcOverlaps)
{
    pbcOverlaps<unsigned>();
    pbcOverlaps<uint64_t>();
}


//! \brief check halo box ranges in all spatial dimensions
template<class I>
void makeHaloBoxXYZ()
{
    int r = I(1) << (maxTreeLevel<I>{} - 3);
    // node range: [r,2r]^3
    I nodeStart = pad(I(0b000000111), 9);
    I nodeEnd = pad(I(0b000001000), 9);

    /// internal node check
    {
        Box<int> haloBox = makeHaloBox(nodeStart, nodeEnd, 1, 0, 0);
        Box<int> refBox{r-1, 2*r+1, r, 2*r, r, 2*r};
        EXPECT_EQ(haloBox, refBox);
    }
    {
        Box<int> haloBox = makeHaloBox(nodeStart, nodeEnd, 0, 1, 0);
        Box<int> refBox{r, 2*r, r-1, 2*r+1, r, 2*r};
        EXPECT_EQ(haloBox, refBox);
    }
    {
        Box<int> haloBox = makeHaloBox(nodeStart, nodeEnd, 0, 0, 1);
        Box<int> refBox{r, 2*r, r, 2*r, r-1, 2*r+1};
        EXPECT_EQ(haloBox, refBox);
    }
}

TEST(BoxOverlap, makeHaloBoxXYZ)
{
    makeHaloBoxXYZ<unsigned>();
    makeHaloBoxXYZ<uint64_t>();
}


//! \brief underflow check, non-periodic case
template<class I>
void makeHaloBoxUnderflow()
{
    int r = I(1) << (maxTreeLevel<I>{} - 1);
    // node range: [r,2r]^3
    I nodeStart = pad(I(0b000), 3);
    I nodeEnd = pad(I(0b001), 3);

    {
        Box<int> haloBox = makeHaloBox(nodeStart, nodeEnd, 1, 0, 0);
        Box<int> refBox{0, r+1, 0, r, 0, r};
        EXPECT_EQ(haloBox, refBox);
    }
    {
        Box<int> haloBox = makeHaloBox(nodeStart, nodeEnd, 0, 1, 0);
        Box<int> refBox{0, r, 0, r+1, 0, r};
        EXPECT_EQ(haloBox, refBox);
    }
    {
        Box<int> haloBox = makeHaloBox(nodeStart, nodeEnd, 0, 0, 1);
        Box<int> refBox{0, r, 0, r, 0, r+1};
        EXPECT_EQ(haloBox, refBox);
    }
}

TEST(BoxOverlap, makeHaloBoxUnderflow)
{
    makeHaloBoxUnderflow<unsigned>();
    makeHaloBoxUnderflow<uint64_t>();
}


//! \brief overflow check, non-periodic case
template<class I>
void makeHaloBoxOverflow()
{
    int r = I(1) << (maxTreeLevel<I>{} - 1);
    // node range: [r,2r]^3
    I nodeStart = pad(I(0b111), 3);
    I nodeEnd   = nodeRange<I>(0);

    {
        Box<int> haloBox = makeHaloBox(nodeStart, nodeEnd, 1, 0, 0);
        Box<int> refBox{r-1, 2*r, r, 2*r, r, 2*r};
        EXPECT_EQ(haloBox, refBox);
    }
    {
        Box<int> haloBox = makeHaloBox(nodeStart, nodeEnd, 0, 1, 0);
        Box<int> refBox{r, 2*r, r-1, 2*r, r, 2*r};
        EXPECT_EQ(haloBox, refBox);
    }
    {
        Box<int> haloBox = makeHaloBox(nodeStart, nodeEnd, 0, 0, 1);
        Box<int> refBox{r, 2*r, r, 2*r, r-1, 2*r};
        EXPECT_EQ(haloBox, refBox);
    }
}

TEST(BoxOverlap, makeHaloBoxOverflow)
{
    makeHaloBoxOverflow<unsigned>();
    makeHaloBoxOverflow<uint64_t>();
}

//! \brief check halo box ranges with periodic boundary conditions
template<class I>
void makeHaloBoxPbc()
{
    int r = I(1) << (maxTreeLevel<I>{} - 3);
    // node range: [r,2r]^3
    I nodeStart = pad(I(0b000000111), 9);
    I nodeEnd = pad(I(0b000001000), 9);

    Box<double> bbox(0.,1.,0.,1.,0.,1.,true,true,true);

    {
        double radius = 0.999/r; // normalize(radius) = 7.992
        Box<int> haloBox = makeHaloBox(nodeStart, nodeEnd, radius, bbox);
        Box<int> refBox{r-8, 2*r+8, r-8, 2*r+8, r-8, 2*r+8, true, true, true};
        EXPECT_EQ(haloBox, refBox);
    }
    {
        double radius = 1.000001/8; // normalize(radius) = r + epsilon
        Box<int> haloBox = makeHaloBox(nodeStart, nodeEnd, radius, bbox);
        Box<int> refBox{-1, 3*r+1, -1, 3*r+1, -1, 3*r+1, true, true, true};
        EXPECT_EQ(haloBox, refBox);
    }
}

TEST(BoxOverlap, makeHaloBoxPbc)
{
    makeHaloBoxPbc<unsigned>();
    makeHaloBoxPbc<uint64_t>();
}

template<class I>
void haloBoxContainedIn()
{
    {
        Box<int> haloBox{0, 1, 0, 1, 0, 1};
        EXPECT_TRUE(containedIn(I(0), I(1), haloBox));
    }
    {
        Box<int> haloBox{0, 1, 0, 1, 0, 2};
        EXPECT_FALSE(containedIn(I(0), I(1), haloBox));
    }
    {
        Box<int> haloBox{0, 1, 0, 1, 0, 2};
        EXPECT_TRUE(containedIn(I(0), I(2), haloBox));
    }
    {
        Box<int> haloBox{0, 1, 0, 2, 0, 2};
        EXPECT_FALSE(containedIn(I(0), I(3), haloBox));
    }
    {
        Box<int> haloBox{0, 1, 0, 2, 0, 2};
        EXPECT_TRUE(containedIn(I(0), I(4), haloBox));
    }
    {
        Box<int> haloBox{0, 2, 0, 2, 0, 2};
        EXPECT_FALSE(containedIn(I(0), I(7), haloBox));
    }
    {
        Box<int> haloBox{0, 2, 0, 2, 0, 2};
        EXPECT_TRUE(containedIn(I(0), I(8), haloBox));
    }

    int maxCoord = (1u<<maxTreeLevel<I>{}) - 1;
    {
        I firstCode  = codeFromBox<I>(0,0,maxCoord, maxTreeLevel<I>{});
        I secondCode = firstCode + 1;
        Box<int> haloBox{0, 1, 0, 1, maxCoord, maxCoord + 1};
        EXPECT_TRUE(containedIn(firstCode, secondCode, haloBox));
    }
    {
        I firstCode  = codeFromBox<I>(0,0,maxCoord, maxTreeLevel<I>{});
        I secondCode = firstCode + 1;
        Box<int> haloBox{0, 1, 0, 2, maxCoord, maxCoord + 1};
        EXPECT_FALSE(containedIn(firstCode, secondCode, haloBox));
    }
    {
        I firstCode  = codeFromBox<I>(0,0,maxCoord, maxTreeLevel<I>{});
        I secondCode = firstCode + 2;
        Box<int> haloBox{0, 1, 0, 2, maxCoord, maxCoord + 1};
        EXPECT_FALSE(containedIn(firstCode, secondCode, haloBox));
    }
    {
        I firstCode  = codeFromBox<I>(0,0,maxCoord, maxTreeLevel<I>{});
        I secondCode = firstCode + 3;
        Box<int> haloBox{0, 1, 0, 2, maxCoord, maxCoord + 1};
        EXPECT_TRUE(containedIn(firstCode, secondCode, haloBox));
    }
    {
        I firstCode  = codeFromBox<I>(maxCoord,maxCoord,maxCoord, maxTreeLevel<I>{});
        I secondCode = firstCode + 1;
        Box<int> haloBox{maxCoord, maxCoord+1, maxCoord, maxCoord+1, maxCoord, maxCoord + 1};
        EXPECT_TRUE(containedIn(firstCode, secondCode, haloBox));
    }

    /// PBC cases
    {
        Box<int> haloBox{-1, 1, 0, 1, 0, 1};
        EXPECT_FALSE(containedIn(I(0), I(1), haloBox));
    }
    {
        I firstCode  = codeFromBox<I>(0,0,maxCoord, maxTreeLevel<I>{});
        I secondCode = firstCode + 3;
        Box<int> haloBox{0, 1, 0, 1, maxCoord, maxCoord + 2};
        EXPECT_FALSE(containedIn(firstCode, secondCode, haloBox));
    }

}

//! \brief test containment of a box within a Morton code range
TEST(BoxOverlap, haloBoxContainedIn)
{
    haloBoxContainedIn<unsigned>();
    haloBoxContainedIn<uint64_t>();
}
