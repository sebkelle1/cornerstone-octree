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
 * \brief implements a bounding bounding box for floating point coordinates and integer indices
 *
 * \author Sebastian Keller <sebastian.f.keller@gmail.com>
 */

#pragma once


namespace sphexa
{

/*! \brief normalize a spatial length w.r.t. to a min/max range
 *
 * @tparam T
 * @param d
 * @param min
 * @param max
 * @return
 */
template<class T>
static inline T normalize(T d, T min, T max) { return (d - min) / (max - min); }

/*! \brief stores the coordinated bounds
 *
 * Needs a slightly different behavior in the PBC case than the existing BBox
 * to manage morton code based octrees.
 *
 * \tparam T floating point type
 */
template<class T>
class Box
{
public:

    Box(T xyzMin, T xyzMax, bool hasPbc = false) :
        limits{xyzMin, xyzMax, xyzMin, xyzMax, xyzMin, xyzMax}, pbc{hasPbc, hasPbc, hasPbc}
    {}

    Box(T xmin, T xmax, T ymin, T ymax, T zmin, T zmax,
        bool pbcX = false, bool pbcY = false, bool pbcZ = false)
    : limits{xmin, xmax, ymin, ymax, zmin, zmax}, pbc{pbcX, pbcY, pbcZ}
    {}

    T xmin() const { return limits[0]; }
    T xmax() const { return limits[1]; }
    T ymin() const { return limits[2]; }
    T ymax() const { return limits[3]; }
    T zmin() const { return limits[4]; }
    T zmax() const { return limits[5]; }

    [[nodiscard]] bool pbcX() const { return pbc[0]; }
    [[nodiscard]] bool pbcY() const { return pbc[1]; }
    [[nodiscard]] bool pbcZ() const { return pbc[2]; }

private:

    friend bool operator==(const Box<T>& a, const Box<T>& b)
    {
        return    a.limits[0] == b.limits[0]
               && a.limits[1] == b.limits[1]
               && a.limits[2] == b.limits[2]
               && a.limits[3] == b.limits[3]
               && a.limits[4] == b.limits[4]
               && a.limits[5] == b.limits[5]
               && a.pbc[0] == b.pbc[0]
               && a.pbc[1] == b.pbc[1]
               && a.pbc[2] == b.pbc[2];
    }

    T limits[6];
    bool pbc[3];
};

//! \brief simple pair that's usable in both CPU and GPU code
template<class T>
class pair
{
public:

    pair(T first, T second) : data{first, second} {}

          T& operator[](int i)       { return data[i]; }
    const T& operator[](int i) const { return data[i]; }

private:

    friend bool operator==(const pair& a, const pair& b)
    {
        return a.data[0] == b.data[0] && a.data[1] == b.data[1];
    }

    friend bool operator<(const pair& a, const pair& b)
    {
        bool c0 = a.data[0] < b.data[0];
        bool e0 = a.data[0] == b.data[0];
        bool c1 = a.data[1] < b.data[1];
        return c0 || (e0 && c1);
    }

    T data[2];
};

} // namespace sphexa