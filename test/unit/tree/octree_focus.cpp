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
 * @brief Test locally essential octree
 *
 * @author Sebastian Keller <sebastian.f.keller@gmail.com>
 */

#include "gtest/gtest.h"

#include "cstone/tree/octree_focus.hpp"
#include "cstone/tree/octree_util.hpp"

#include "coord_samples/random.hpp"

namespace cstone
{

//! @brief various tests about merge/split decisions based on node counts and MACs
template<class KeyType>
void rebalanceDecision()
{
    std::vector<KeyType> cstree = OctreeMaker<KeyType>{}.divide().divide(0).divide(7).makeTree();

    Octree<KeyType> tree;
    tree.update(cstree.data(), cstree.data() + cstree.size());

    // for (int i = 0; i < tree.numTreeNodes(); ++i)
    //    std::cout << std::dec << i << " " << std::oct << tree.codeStart(i) << std::endl;

    unsigned bucketSize = 1;

    {
        // nodes 14-21 should be fused based on counts, and 14 should be split based on MACs. counts win, nodes are
        // fused
        //                               0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16 17 18 19 20 21
        std::vector<unsigned> leafCounts{1, 1, 1, 2, 1, 1, 1, 1, 1, 1, 2, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0};
        std::vector<char> macs{1, 1, 1, 0, 0, 1, 0, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0};

        std::vector<int> reference{1, 1, 1, 8, 1, 1, 1, 1, 1, 1, 8, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0};

        std::vector<int> nodeOps(nNodes(cstree));
        bool converged = rebalanceDecisionEssential(tree.treeLeaves().data(), tree.numInternalNodes(),
                                                    tree.numLeafNodes(), tree.leafParents(), leafCounts.data(),
                                                    macs.data(), 0, 8, bucketSize, nodeOps.data());

        EXPECT_EQ(nodeOps, reference);
        EXPECT_FALSE(converged);
    }
    {
        // nodes 14-21 should be split/stay based on counts, and should stay based on MACs.
        // MAC wins, nodes stay, but are not split
        //                               0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16 17 18 19 20 21
        std::vector<unsigned> leafCounts{1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 2, 1, 0, 0, 0, 0};
        std::vector<char> macs{1, 1, 1, 0, 0, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0};
        //                             ^
        //                             parent of leaf nodes 14-21
        std::vector<int> reference{1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};

        std::vector<int> nodeOps(nNodes(cstree));
        bool converged = rebalanceDecisionEssential(tree.treeLeaves().data(), tree.numInternalNodes(),
                                                    tree.numLeafNodes(), tree.leafParents(), leafCounts.data(),
                                                    macs.data(), 0, 8, bucketSize, nodeOps.data());

        EXPECT_EQ(nodeOps, reference);
        EXPECT_TRUE(converged);
    }
    {
        // nodes 14-21 should stay based on counts, and should be fused based on MACs. MAC wins, nodes are fused
        EXPECT_EQ(tree.parent(tree.toInternal(14)), 2);
        //                               0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16 17 18 19 20 21
        std::vector<unsigned> leafCounts{1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 2, 1, 0, 0, 0, 0};
        std::vector<char> macs{1, 1, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0};
        //                             ^
        //                             parent of leaf nodes 14-21
        std::vector<int> reference{1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0};

        std::vector<int> nodeOps(nNodes(cstree));
        bool converged = rebalanceDecisionEssential(tree.treeLeaves().data(), tree.numInternalNodes(),
                                                    tree.numLeafNodes(), tree.leafParents(), leafCounts.data(),
                                                    macs.data(), 0, 8, bucketSize, nodeOps.data());

        EXPECT_EQ(nodeOps, reference);
        EXPECT_FALSE(converged);
    }
    {
        // this example has a focus area that cuts through sets of 8 neighboring sibling nodes
        std::vector<KeyType> cstree = OctreeMaker<KeyType>{}.divide().divide(0).divide(1).makeTree();

        Octree<KeyType> tree;
        tree.update(cstree.data(), cstree.data() + cstree.size());
        // nodes 14-21 should stay based on counts, and should be fused based on MACs. MAC wins, nodes are fused
        EXPECT_EQ(tree.parent(tree.toInternal(14)), 2);
        //                               |                    |  |                    |
        //                               0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16 17 18 19 20 21
        std::vector<unsigned> leafCounts{1, 2, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 1, 2, 1, 2, 1, 1, 2, 1, 1};
        std::vector<char> macs{1, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0};
        //                 root ^  ^  ^
        //   parent of leaves 0-7  |  | parent of leaf nodes 8-15                  | here count says split, mac says
        //   merge, result: stay
        std::vector<int> reference{1, 8, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 8, 1, 1, 1, 1, 1};
        //                                                             ----------------
        //                   these nodes are kept alive because their siblings (8 and 9) are inside the focus and are
        //                   staying
        std::vector<int> nodeOps(nNodes(cstree));
        bool converged = rebalanceDecisionEssential(tree.treeLeaves().data(), tree.numInternalNodes(),
                                                    tree.numLeafNodes(), tree.leafParents(), leafCounts.data(),
                                                    macs.data(), 2, 10, bucketSize, nodeOps.data());

        EXPECT_EQ(nodeOps, reference);
        EXPECT_FALSE(converged);
    }
}

TEST(OctreeEssential, rebalanceDecision)
{
    rebalanceDecision<unsigned>();
    rebalanceDecision<uint64_t>();
}

template<class KeyType>
TreeNodeIndex numNodesInRange(const FocusedOctreeSingleNode<KeyType>& tree, KeyType a, KeyType b)
{
    auto csFocus = tree.treeLeaves();

    return std::lower_bound(csFocus.begin(), csFocus.end(), b) - std::lower_bound(csFocus.begin(), csFocus.end(), a);
}

template<class KeyType>
void computeEssentialTree()
{
    Box<double> box{-1, 1};
    int nParticles = 100000;
    unsigned csBucketSize = 16;

    RandomCoordinates<double, KeyType> randomBox(nParticles, box);
    std::vector<KeyType> codes = randomBox.mortonCodes();

    auto [csTree, csCounts] = computeOctree(codes.data(), codes.data() + nParticles, csBucketSize);
    Octree<KeyType> globalTree;
    globalTree.update(begin(csTree), end(csTree));

    unsigned bucketSize = 16;
    float theta = 1.0;
    FocusedOctreeSingleNode<KeyType> tree(bucketSize, theta);

    KeyType focusStart = 1;
    KeyType focusEnd = pad(KeyType(1), 3);
    while (!tree.update(box, codes, focusStart, focusEnd)) {}

    // in the focus area (the first octant) the essential tree and the csTree are identical
    TreeNodeIndex lastFocusNode =
        std::lower_bound(tree.treeLeaves().begin(), tree.treeLeaves().end(), focusEnd) - tree.treeLeaves().begin();
    EXPECT_TRUE(std::equal(begin(csTree), begin(csTree) + lastFocusNode, tree.treeLeaves().begin()));
    EXPECT_EQ(numNodesInRange(tree, pad(KeyType(7), 3), nodeRange<KeyType>(0)), 92);

    focusStart = pad(KeyType(6), 3);
    focusEnd = pad(KeyType(7), 3);
    while (!tree.update(box, codes, focusStart, focusEnd)) {}

    EXPECT_EQ(numNodesInRange(tree, pad(KeyType(1), 3), pad(KeyType(2), 3)), 92);

    focusStart = 0;
    focusEnd = pad(KeyType(1), 3);
    while (!tree.update(box, codes, focusStart, focusEnd)) {}

    // tree now focused again on first octant
    EXPECT_TRUE(std::equal(begin(csTree), begin(csTree) + lastFocusNode, tree.treeLeaves().begin()));
    EXPECT_EQ(numNodesInRange(tree, pad(KeyType(7), 3), nodeRange<KeyType>(0)), 92);
}

TEST(OctreeEssential, compute)
{
    computeEssentialTree<unsigned>();
    computeEssentialTree<uint64_t>();
}

} // namespace cstone