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

/*! @file
 * @brief Testing of the naive collision detection implementation
 *
 * @author Sebastian Keller <sebastian.f.keller@gmail.com>
 */

#include "gtest/gtest.h"

#include "collisions_a2a.hpp"

using namespace cstone;

/*! @brief test the naive to-all collision detection function
 *
 * @tparam I  32- or 64-bit unsigned integer
 */
template<class I>
void collide2all()
{
    auto tree = OctreeMaker<I>{}.divide().divide(0).divide(0, 7).makeTree();

    // this search box intersects with neighbors in x direction and will intersect
    // with multiple smaller level 2 and level 3 nodes
    // it corresponds to the node with code 04000000000 (octal, 32-bit) with a dx=1 halo extension
    int r = 1u << (maxTreeLevel<I>{} - 1);
    IBox haloBox{r - 1, 2 * r, 0, r, 0, r};

    CollisionList collisionList;
    findCollisions2All(tree, collisionList, haloBox);

    std::vector<I> collisions(collisionList.size());
    for (std::size_t i = 0; i < collisions.size(); ++i)
        collisions[i] = tree[collisionList[i]];

    // list of octree leaf sfc codes (in octal) that should collide
    // with the halo box
    std::vector<I> refCollisions{pad(I(004), 6),  pad(I(005), 6),  pad(I(006), 6),  pad(I(0074), 9),
                                 pad(I(0075), 9), pad(I(0076), 9), pad(I(0077), 9), pad(I(04), 3)};

    EXPECT_EQ(collisions, refCollisions);
}

TEST(Collisions, collide2all)
{
    collide2all<unsigned>();
    collide2all<uint64_t>();
}

/*! @brief test the naive all-to-all collision detection function
 *
 * @tparam I  32- or 64-bit unsigned integer
 */
template<class I, class T>
void collideAll2all()
{
    auto tree = OctreeMaker<I>{}.divide().divide(0).divide(0, 7).makeTree();

    Box<T> box(0, 1);
    std::vector<T> haloRadii(nNodes(tree), 0.1);

    std::vector<CollisionList> allCollisions = findCollisionsAll2all(tree, haloRadii, box);

    // extract list of collisions for node with index 18, corresponding to {4}
    std::vector<I> n18coll(allCollisions[18].size());
    for (std::size_t i = 0; i < n18coll.size(); ++i)
        n18coll[i] = tree[allCollisions[18][i]];

    std::sort(begin(n18coll), end(n18coll));

    // reference list of collisions for node with index 18, corresponding to {4}
    std::vector<I> refCollisions{pad(I(004), 6),  pad(I(005), 6),  pad(I(006), 6), pad(I(0074), 9), pad(I(0075), 9),
                                 pad(I(0076), 9), pad(I(0077), 9), pad(I(01), 3),  pad(I(02), 3),   pad(I(03), 3),
                                 pad(I(04), 3),   pad(I(05), 3),   pad(I(06), 3),  pad(I(07), 3)};

    EXPECT_EQ(n18coll, refCollisions);
}

TEST(Collisions, collideAll2all)
{
    collideAll2all<unsigned, float>();
    collideAll2all<uint64_t, float>();
    collideAll2all<unsigned, double>();
    collideAll2all<uint64_t, double>();
}

/*! @brief test the naive all-to-all collision detection function, PBC-X case
 *
 * @tparam I  32- or 64-bit unsigned integer
 */
template<class I, class T>
void collideAll2allPbcX()
{
    auto tree = OctreeMaker<I>{}.divide().divide(0).divide(0, 7).makeTree();

    Box<T> box(0, 1, 0, 1, 0, 1, true, false, false);
    std::vector<T> haloRadii(nNodes(tree), 0.1);

    std::vector<CollisionList> allCollisions = findCollisionsAll2all(tree, haloRadii, box);

    // extract list of collisions for node with index 18, corresponding to {4}
    std::vector<I> n18coll(allCollisions[18].size());
    for (std::size_t i = 0; i < n18coll.size(); ++i)
        n18coll[i] = tree[allCollisions[18][i]];

    std::sort(begin(n18coll), end(n18coll));

    // reference list of collisions for node with index 18, corresponding to {4}
    std::vector<I> refCollisions{pad(I(000), 6), // due to pbc X
                                 pad(I(001), 6), // due to pbc X
                                 pad(I(002), 6), // due to pbc X
                                 pad(I(003), 6), // due to pbc X
                                 pad(I(004), 6),  pad(I(005), 6),  pad(I(006), 6), pad(I(0074), 9), pad(I(0075), 9),
                                 pad(I(0076), 9), pad(I(0077), 9), pad(I(01), 3),  pad(I(02), 3),   pad(I(03), 3),
                                 pad(I(04), 3),   pad(I(05), 3),   pad(I(06), 3),  pad(I(07), 3)};

    EXPECT_EQ(n18coll, refCollisions);
}

TEST(Collisions, collideAll2allPbcX)
{
    collideAll2allPbcX<unsigned, float>();
    collideAll2allPbcX<uint64_t, float>();
    collideAll2allPbcX<unsigned, double>();
    collideAll2allPbcX<uint64_t, double>();
}

/*! @brief test the naive all-to-all collision detection function, PBC-XYZ case
 *
 * @tparam I  32- or 64-bit unsigned integer
 */
template<class I, class T>
void collideAll2allPbcXYZ()
{
    auto tree = OctreeMaker<I>{}.divide().divide(0).divide(0, 7).divide(5).divide(6).makeTree();

    Box<T> box(0, 1, 0, 1, 0, 1, true, true, true);
    std::vector<T> haloRadii(nNodes(tree), 0.1);

    std::vector<CollisionList> allCollisions = findCollisionsAll2all(tree, haloRadii, box);

    // extract list of collisions for node with index 18, corresponding to {4}
    std::vector<I> n18coll(allCollisions[18].size());
    for (std::size_t i = 0; i < n18coll.size(); ++i)
        n18coll[i] = tree[allCollisions[18][i]];

    std::sort(begin(n18coll), end(n18coll));

    // reference list of collisions for node with index 18, corresponding to {4}
    std::vector<I> refCollisions{pad(I(000), 6), // due to pbc X
                                 pad(I(001), 6), // due to pbc X
                                 pad(I(002), 6), // due to pbc X
                                 pad(I(003), 6), // due to pbc X
                                 pad(I(004), 6),  pad(I(005), 6),  pad(I(006), 6), pad(I(0074), 9), pad(I(0075), 9),
                                 pad(I(0076), 9), pad(I(0077), 9), pad(I(01), 3),  pad(I(02), 3),   pad(I(03), 3),
                                 pad(I(04), 3),   pad(I(050), 6),  pad(I(051), 6), // due to pbc Z
                                 pad(I(052), 6),  pad(I(053), 6),                  // due to pbc Z
                                 pad(I(054), 6),  pad(I(055), 6),                  // due to pbc Z
                                 pad(I(056), 6),  pad(I(057), 6),                  // due to pbc Z
                                 pad(I(060), 6),  pad(I(061), 6),  pad(I(062), 6), // due to pbc Y
                                 pad(I(063), 6),                                   // due to pbc Y
                                 pad(I(064), 6),  pad(I(065), 6),  pad(I(066), 6), // due to pbc Y
                                 pad(I(067), 6),                                   // due to pbc Y
                                 pad(I(07), 3)};

    EXPECT_EQ(n18coll, refCollisions);
}

TEST(Collisions, collideAll2allPbcXYZ)
{
    collideAll2allPbcXYZ<unsigned, float>();
    collideAll2allPbcXYZ<uint64_t, float>();
    collideAll2allPbcXYZ<unsigned, double>();
    collideAll2allPbcXYZ<uint64_t, double>();
}
