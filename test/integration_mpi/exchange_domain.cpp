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
 * @brief Tests the particle exchange used for exchanging assigned particles, i.e. not halos.
 *
 * @author Sebastian Keller <sebastian.f.keller@gmail.com>
 */

#include <vector>

#include <gtest/gtest.h>

#include "cstone/domain/domaindecomp_mpi.hpp"

using namespace cstone;

/*! @brief all-to-all exchange, the most communication possible
 *
 * @tparam T       float, double or int
 * @param thisRank executing rank
 * @param nRanks   total number of ranks
 *
 * Each rank keeps (1/numRanks)-th of its local elements and sends the
 * other nRanks-1 chunks to the other numRanks-1 ranks.
 */
template<class T>
void exchangeAllToAll(int thisRank, int nRanks)
{
    int gridSize = 64;

    std::vector<T> x(gridSize), y(gridSize);
    std::vector<int> ordering(gridSize);

    std::iota(begin(x), end(x), 0);
    // unique element id across all ranks
    std::iota(begin(y), end(y), gridSize * thisRank);
    // start from trivial ordering
    std::iota(begin(ordering), end(ordering), 0);

    {
        // A simple, but nontrivial ordering.
        // Simulates the use case where the x,y,z coordinate arrays
        // are not sorted according to the Morton code ordering for which the
        // the index ranges in the SendList are valid.
        int swap1 = 0;
        int swap2 = gridSize - 1;
        std::swap(x[swap1], x[swap2]);
        std::swap(y[swap1], y[swap2]);
        std::swap(ordering[swap1], ordering[swap2]);
    }

    int segmentSize = gridSize / nRanks;

    SendList sendList(nRanks);
    for (int rank = 0; rank < nRanks; ++rank)
    {
        int lower = rank * segmentSize;
        int upper = lower + segmentSize;

        if (rank == nRanks - 1) upper += gridSize % nRanks;

        sendList[rank].addRange(lower, upper);
    }

    // there's only one range per rank
    segmentSize = sendList[thisRank].count(0);
    int nParticlesThisRank = segmentSize * nRanks;

    reallocate(nParticlesThisRank, x, y);
    exchangeParticles<T>(sendList, Rank(thisRank), nParticlesThisRank, ordering.data(), x.data(), y.data());

    std::vector<T> refX(nParticlesThisRank);
    for (int rank = 0; rank < nRanks; ++rank)
    {
        std::iota(begin(refX) + rank * segmentSize, begin(refX) + rank * segmentSize + segmentSize,
                  sendList[thisRank].rangeStart(0));
    }

    std::vector<T> refY;
    for (int rank = 0; rank < nRanks; ++rank)
    {
        int seqStart = rank * gridSize + (gridSize / nRanks) * thisRank;

        for (int i = 0; i < segmentSize; ++i)
            refY.push_back(seqStart++);
    }

    // received particles are in indeterminate order
    std::sort(begin(y), end(y));

    EXPECT_EQ(refX, x);
    EXPECT_EQ(refY, y);
}

TEST(GlobalDomain, exchangeAllToAll)
{
    int rank = 0, nRanks = 0;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &nRanks);

    exchangeAllToAll<double>(rank, nRanks);
    exchangeAllToAll<float>(rank, nRanks);
    exchangeAllToAll<int>(rank, nRanks);
}

template<class T>
void exchangeCyclicNeighbors(int thisRank, int nRanks)
{
    int gridSize = 64;

    // x and y are filled with one value that is different for each rank
    std::vector<T> x(gridSize, thisRank), y(gridSize, -thisRank);
    std::vector<int> ordering(gridSize);
    std::iota(begin(ordering), end(ordering), 0);

    // send the last nex elements to the next rank
    int nex = 10;
    int nextRank = (thisRank + 1) % nRanks;

    SendList sendList(nRanks);
    // keep all but the last nex elements
    sendList[thisRank].addRange(0, gridSize - nex);
    // send last nex to nextRank
    sendList[nextRank].addRange(gridSize - nex, gridSize);

    reallocate(gridSize, x, y);
    exchangeParticles<T>(sendList, Rank(thisRank), gridSize, ordering.data(), x.data(), y.data());

    int incomingRank = (thisRank - 1 + nRanks) % nRanks;
    std::vector<T> refX(gridSize, thisRank);
    std::fill(begin(refX) + gridSize - nex, end(refX), incomingRank);

    auto refY = refX;
    for (auto& yi : refY)
        yi = -yi;

    EXPECT_EQ(refX, x);
    EXPECT_EQ(refY, y);
}

TEST(GlobalDomain, exchangeCyclicNeighbors)
{
    int rank = 0, nRanks = 0;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &nRanks);

    exchangeCyclicNeighbors<double>(rank, nRanks);
    exchangeCyclicNeighbors<float>(rank, nRanks);
    exchangeCyclicNeighbors<int>(rank, nRanks);
}

/*! @brief particle exchange test with offsets
 *
 * Before the exchange, the buffer layout of each rank is
 *
 * |P|XXXX...XXX|, with X a unique value per rank, and P an arbitrary non-zero number
 *  ^   ^ 64 X'es
 *  input offset is 1
 *
 * after the exchange, it becomes
 *
 * |??|XXXXX...Y...|????|  (total size: 70)
 *  ^    ^       ^
 *  |  54 X'es  10 Y's
 *  2 unknown values (output offset)
 *
 *  Y is the unique number of the preceding rank
 */
template<class T>
void exchangeCyclicNeighborsOffsets(int thisRank, int nRanks)
{
    int originalSize = 65;
    int assignedSize = 64;
    int finalSize = 70;

    int inputOffset = 1;
    int outputOffset = 2;

    // x and y are filled with one value that is different for each rank
    std::vector<T> x(originalSize, thisRank), y(originalSize, -thisRank);
    // random pollution before the inputOffset
    int pollution = 1412842341;
    x[0] = pollution;
    y[0] = pollution;

    std::vector<int> ordering(assignedSize);
    std::iota(begin(ordering), end(ordering), 0);

    // send the last nex elements to the next rank
    int nex = 10;
    int nextRank = (thisRank + 1) % nRanks;

    // Note that SendList indices are counted from the offset, not the actual
    // begin of the x,y arrays
    SendList sendList(nRanks);
    // keep all but the last nex elements
    sendList[thisRank].addRange(0, assignedSize - nex);
    // send last nex to nextRank
    sendList[nextRank].addRange(assignedSize - nex, assignedSize);

    reallocate(finalSize, x, y);
    exchangeParticles<T>(sendList, Rank(thisRank), assignedSize, inputOffset, outputOffset, ordering.data(), x.data(),
                         y.data());

    // the reference covers only the assigned range of 64
    std::vector<T> refX(assignedSize, thisRank);
    int incomingRank = (thisRank - 1 + nRanks) % nRanks;
    std::fill(begin(refX) + assignedSize - nex, end(refX), incomingRank);

    auto refY = refX;
    for (auto& yi : refY)
        yi = -yi;

    EXPECT_EQ(x.size(), finalSize);
    EXPECT_EQ(y.size(), finalSize);

    for (std::size_t i = 0; i < refX.size(); ++i)
    {
        EXPECT_EQ(refX[i], x[i + outputOffset]);
        EXPECT_EQ(refY[i], y[i + outputOffset]);
    }
}

TEST(GlobalDomain, exchangeCyclicNeighborsOffsets)
{
    int rank = 0, nRanks = 0;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &nRanks);

    exchangeCyclicNeighborsOffsets<double>(rank, nRanks);
    exchangeCyclicNeighborsOffsets<float>(rank, nRanks);
    exchangeCyclicNeighborsOffsets<int>(rank, nRanks);
}
