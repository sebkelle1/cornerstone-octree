#pragma once

#include <unordered_map>

#include "sfc/domaindecomp.hpp"

namespace sphexa
{

/*! \brief Stores offsets into particle buffers for all nodes present on a given rank
 *
 * Each rank will be assigned a part of the SFC, equating to one or multiple ranges of
 * node indices of the global cornerstone octree. In a addition to the assigned nodes,
 * each rank must also store particle data for those nodes in the global octree which are
 * halos of the assigned nodes. Both types of nodes present on the rank are stored in the same
 * particle array (x,y,z,h,...) according to increasing node index, which is the same
 * as increasing Morton code.
 *
 * This class stores the position and size of each node (halo or assigned node).
 * The resulting layout is valid for all particle buffers, such as x,y,z,h,d,p,...
 */
class ArrayLayout
{
public:

    //! \brief construct from sorted nodeList and offsets (use std::move)
    ArrayLayout(std::vector<int>&& nodeList, std::vector<int>&& offsets)
        : nodeList_(nodeList), offsets_(offsets)
    {
        for (int i = 0; i < nodeList.size(); ++i)
            global2local_[nodeList[i]] = i;
    }

    //! \brief number of local node ranges
    int nLocalRanges() { return ranges_.nRanges(); }

    //! \brief
    int localRangePosition(int rangeIndex) const { return ranges_.rangeStart(rangeIndex); }

    //! \brief  number of particles in local range
    int localRangeCount(int rangeIndex) const { return ranges_.count(rangeIndex); }

    //! \brief number of particles in all local ranges
    int localCount() const { return ranges_.totalCount(); }

    //! \brief array offset
    int nodePosition(int globalNodeIndex) const
    {
        // note: globalNodeIndex needs to exist!
        int localIndex = global2local_.at(globalNodeIndex);
        return offsets_[localIndex];
    }

    //! \brief number of particles per node
    int nodeCount(int globalNodeIndex) const
    {
        int localIndex = global2local_.at(globalNodeIndex);
        return offsets_[localIndex+1] - offsets_[localIndex];
    }

    //! \brief sum of all internal node and halo node sizes present in the layout
    int totalSize() const { return offsets_[nodeList_.size()]; }


    /*! \brief mark specified range of nodes as local, i.e. part of rank assignment
     *
     * @param lowerGlobalNodeIndex
     * @param upperGlobalNodeIndex
     *
     * Calling this function only works if the specified index range is consistent
     * with the node lists used upon construction.
     */
    void addLocalRange(int lowerGlobalNodeIndex, int upperGlobalNodeIndex)
    {
        int nNodes     = upperGlobalNodeIndex - lowerGlobalNodeIndex;
        int localIndex = global2local_.at(lowerGlobalNodeIndex);

        int lowerOffset = offsets_[localIndex];
        int upperOffset = offsets_[localIndex + nNodes];
        ranges_.addRange(lowerOffset, upperOffset);
    }

private:
    //! \brief pairs of node indices, one pair per local range
    IndexRanges<int> ranges_;

    //! \brief pairs of (global octree node index, local index into offsets_ and nodeList_)
    std::unordered_map<int, int> global2local_;

    //! \brief sorted list of nodes
    std::vector<int> nodeList_;
    //! \brief array offset per node
    std::vector<int> offsets_;
};


/*! \brief  Finds the ranges of node indices of the tree that are assigned to a given rank
 *
 * @tparam I           32- or 64-bit unsigned integer
 * @param tree         global cornerstone octree
 * @param assignment   assignment of Morton code ranges to ranks
 * @param rank         extract rank's part from assignment
 * @return             ranges of node indices in \a tree that belong to rank \a rank
 */
template<class I>
IndexRanges<int> computeLocalNodeRanges(const std::vector<I>& tree,
                                        const SpaceCurveAssignment<I>& assignment,
                                        int rank)
{
    IndexRanges<int> ret;

    for (int rangeIndex = 0; rangeIndex < assignment.nRanges(rank); ++rangeIndex)
    {
        int firstNodeIndex  = std::lower_bound(begin(tree), end(tree),
                                               assignment.rangeStart(rank, rangeIndex)) - begin(tree);
        int secondNodeIndex = std::lower_bound(begin(tree), end(tree),
                                               assignment.rangeEnd(rank, rangeIndex)) - begin(tree);

        ret.addRange(firstNodeIndex, secondNodeIndex, secondNodeIndex - firstNodeIndex);
    }

    return ret;
}

//! \brief create a sorted list of nodes from the hierarchical per rank node list
std::vector<int> flattenNodeList(const std::vector<std::vector<int>>& groupedNodes)
{
    int nNodes = 0;
    for (auto& v : groupedNodes) nNodes += v.size();

    std::vector<int> nodeList;
    nodeList.reserve(nNodes);

    // add all halos to nodeList
    for (const auto& group : groupedNodes)
    {
        std::copy(begin(group), end(group), std::back_inserter(nodeList));
    }

    return nodeList;
}

/*! \brief computes the array layout for particle buffers of the executing rank
 *
 * @tparam I                32- or 64-bit unsigned integer
 * @param localNodes        Ranges of node indices, assigned to executing rank
 * @param haloNodes         List of halo node indices. From the perspective of the
 *                          executing rank, these are incoming halo nodes.
 * @param globalNodeCounts  Particle count per node in the global octree
 * @return                  The array layout, see class ArrayLayout
 */
template<class I>
ArrayLayout computeLayout(const IndexRanges<I>& localNodes,
                          std::vector<int> haloNodes,
                          const std::vector<std::size_t>& globalNodeCounts)
{
    std::vector<int>& nodeList = haloNodes;

    // add all local nodes to nodeList
    for (int rangeIndex = 0; rangeIndex < localNodes.nRanges(); ++rangeIndex)
    {
        int lower = localNodes.rangeStart(rangeIndex);
        int upper = localNodes.rangeEnd(rangeIndex);
        for (int i = lower; i < upper; ++i)
            nodeList.push_back(i);
    }

    std::sort(begin(nodeList), end(nodeList));

    // an extract of globalNodeCounts, containing only nodes listed in localNodes and incomingHalos
    std::vector<int> nodeCounts(nodeList.size());

    // extract particle count information for all nodes in nodeList
    for (int i = 0; i < nodeList.size(); ++i)
    {
        int globalNodeIndex = nodeList[i];
        nodeCounts[i]       = globalNodeCounts[globalNodeIndex];
    }

    // the last element stores the total size of the layout
    std::vector<int> offsets(nodeList.size() + 1);
    {
        int offset = 0;
        for (int i = 0; i < nodeList.size(); ++i)
        {
            offsets[i] = offset;
            offset += nodeCounts[i];
        }
        offsets[nodeList.size()] = offset;
    }

    ArrayLayout layout(std::move(nodeList), std::move(offsets));
    // register which ranges of nodes are part of the local assignment
    for (int rangeIndex = 0; rangeIndex < localNodes.nRanges(); ++rangeIndex)
    {
        int lower = localNodes.rangeStart(rangeIndex);
        int upper = localNodes.rangeEnd(rangeIndex);
        layout.addLocalRange(lower, upper);
    }

    return layout;
}


} // namespace sphexa