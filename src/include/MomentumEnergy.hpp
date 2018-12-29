#pragma once

#include <vector>
#include "kernels.hpp"

namespace sphexa
{

template<typename T = double, typename ArrayT = std::vector<T>>
class MomentumEnergy
{
public:
	MomentumEnergy(const T K = compute_3d_k(5.0)) : K(K) {}

	void compute(const std::vector<int> &clist, const std::vector<std::vector<int>> &neighbors, 
		const ArrayT &x, const ArrayT &y, const ArrayT &z, const ArrayT &h,
		const ArrayT &vx, const ArrayT &vy, const ArrayT &vz, 
		const ArrayT &ro, const ArrayT &p, const ArrayT &c, const ArrayT &m,
		ArrayT &grad_P_x, ArrayT &grad_P_y, ArrayT &grad_P_z, ArrayT &du)
	{
		int n = clist.size();

		const T gradh_i = 1.0;
		const T gradh_j = 1.0;

		#pragma omp parallel for
		for(int pi=0; pi<n; pi++)
		{
			int i = clist[pi];

			T momentum_x = 0.0, momentum_y = 0.0, momentum_z = 0.0, energy = 0.0;
			
			for(unsigned int j=0; j<neighbors[pi].size(); j++)
			{
				// retrive the id of a neighbor
	        	int nid = neighbors[pi][j];
	        	if(nid == i) continue;

		        // calculate the scalar product rv = rij * vij
		        T r_ijx = (x[i] - x[nid]);
		        T r_ijy = (y[i] - y[nid]);
		        T r_ijz = (z[i] - z[nid]);

		        T v_ijx = (vx[i] - vx[nid]);
		        T v_ijy = (vy[i] - vy[nid]);
		        T v_ijz = (vz[i] - vz[nid]);

		        T rv = r_ijx * v_ijx + r_ijy * v_ijy + r_ijz * v_ijz;

		        T r_square = (r_ijx * r_ijx) + (r_ijy * r_ijy) + (r_ijz * r_ijz);

		        T viscosity_ij = artificial_viscosity(ro[i], ro[nid], h[i], h[nid], c[i], c[nid], rv, r_square);
		        
		        // if(isnan(viscosity_ij))
	        	//     printf("ERROR::MomentumEnergy::artificial_viscosity(%d %d) c_i %f c_j %f h_i %f h_j %f rv %f r_square %f ro_i %f ro_j %f\n", 
	        	//     	i, nid, c[i], c[nid], h_i, h_j, rv, r_square, ro_i, ro_j);

		        T r_ij = sqrt(r_square);
		        T rv_i = r_ij / h[i];
		        T rv_j = r_ij / h[nid];

		        T derivative_kernel_i = wharmonic_derivative(rv_i, h[i], K);
		        T derivative_kernel_j = wharmonic_derivative(rv_j, h[nid], K);
		        
		        T grad_v_kernel_x_i = r_ijx * derivative_kernel_i;
		        T grad_v_kernel_x_j = r_ijx * derivative_kernel_j;
		        T grad_v_kernel_y_i = r_ijy * derivative_kernel_i;
		        T grad_v_kernel_y_j = r_ijy * derivative_kernel_j;
		        T grad_v_kernel_z_i = r_ijz * derivative_kernel_i;
		        T grad_v_kernel_z_j = r_ijz * derivative_kernel_j;
				
				momentum_x +=  (p[i]/(gradh_i * ro[i] * ro[i]) * grad_v_kernel_x_i) 
					+ (p[nid]/(gradh_j * ro[nid] * ro[nid]) * grad_v_kernel_x_j) 
					+ viscosity_ij * (grad_v_kernel_x_i + grad_v_kernel_x_j)/2.0;
		        momentum_y +=  (p[i]/(gradh_i * ro[i] * ro[i]) * grad_v_kernel_y_i) 
		        	+ (p[nid]/(gradh_j * ro[nid] * ro[nid]) * grad_v_kernel_y_j) 
		        	+ viscosity_ij * (grad_v_kernel_y_i + grad_v_kernel_y_j)/2.0;
		        momentum_z +=  (p[i]/(gradh_i * ro[i] * ro[i]) * grad_v_kernel_z_i) 
		        	+ (p[nid]/(gradh_j * ro[nid] * ro[nid]) * grad_v_kernel_z_j) 
		        	+ viscosity_ij * (grad_v_kernel_z_i + grad_v_kernel_z_j)/2.0;
		    	
		    	energy +=  m[nid] * (1 + 0.5 * viscosity_ij) * (v_ijx * grad_v_kernel_x_i + v_ijy * grad_v_kernel_y_i + v_ijz * grad_v_kernel_z_i);
		    }

		    if(std::isnan(momentum_x) || std::isnan(momentum_y) || std::isnan(momentum_z))
	    		printf("ERROR::MomentumEnergy(%d) MomentumEnergy (%f %f %f)\n", i, momentum_x, momentum_y, momentum_z);

	    	du[i] =  energy * (-p[i]/(gradh_i * ro[i] * ro[i]));

	    	if(std::isnan(du[i]))
	    		printf("ERROR:Energy du %f energy %f p_i %f gradh_i %f ro_i %f\n", du[i], energy, p[i], gradh_i, ro[i]);

		    grad_P_x[i] = momentum_x * m[i];
		    grad_P_y[i] = momentum_y * m[i];
		    grad_P_z[i] = momentum_z * m[i];
		}
	}

private:
	const T K;
};

}

