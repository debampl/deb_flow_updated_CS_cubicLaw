/*!
 *
 * Copyright (C) 2007 Technical University of Liberec.  All rights reserved.
 *
 * Please make a following refer to Flow123d on your project site if you use the program for any purpose,
 * especially for academic research:
 * Flow123d, Research Centre: Advanced Remedial Technologies, Technical University of Liberec, Czech Republic
 *
 * This program is free software; you can redistribute it and/or modify it under the terms
 * of the GNU General Public License version 3 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with this program; if not,
 * write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 021110-1307, USA.
 *
 *
 * $Id$
 * $Revision$
 * $LastChangedBy$
 * $LastChangedDate$
 *
 * @file
 * @brief Discontinuous Galerkin method for equation of transport with dispersion.
 *  @author Jan Stebel
 */

#ifndef TRANSPORT_DG_HH_
#define TRANSPORT_DG_HH_

#include "transport_operator_splitting.hh"
#include "la/linsys.hh"
#include "flow/mh_dofhandler.hh"

class Distribution;
template<unsigned int dim, unsigned int spacedim> class DOFHandler;
template<unsigned int dim, unsigned int spacedim> class FEValuesBase;
template<unsigned int dim, unsigned int spacedim> class FiniteElement;


/**
 * @brief Transport with dispersion implemented using discontinuous Galerkin method.
 *
 * TransportDG implements the discontinuous Galerkin method for the transport and diffusion of substances.
 * The concentration @f$ c_i ~[kg/m^3]@f$ of the i-th substance is governed by the advection-diffusion equation
 * @f[
 * 		\partial_t c_i + \mathbf v\cdot\nabla c_i - \mathrm{div}(D\nabla c_i) = F \mbox{ in }\Omega^d,
 * @f]
 * where @f$\mathbf v@f$ is the fluid velocity and @f$\Omega^d@f$ the @f$d@f$-dimensional domain, respectively.
 * The hydrodynamic dispersivity tensor @f$\mathbf D ~[m^2/s]@f$ is given by:
 * @f[
 *		\mathbf D = D_m\mathbf I + |\mathbf v|\left(\alpha_T\mathbf I + (\alpha_L-\alpha_T)\frac{\mathbf v\otimes\mathbf v}{|\mathbf v|^2}\right).
 * @f]
 * The molecular dispersivity @f$D_m~[m^2/s]@f$, as well as the longitudal and transversal dispersivity @f$\alpha_L,~\alpha_T~[m]@f$ are input parameters of the model.
 *
 * For lower dimensions @f$d=1,2@f$ the advection-diffusion equation is multiplied by the fracture cross-cut @f$\delta^d~[m^{3-d}]@f$.
 *
 * The boundary @f$\partial\Omega^d@f$ is divided into three disjoint parts @f$\Gamma^d_D\cup\Gamma^d_N\cup\Gamma^d_F@f$.
 * We prescribe the following boundary conditions:
 * @f{eqnarray*}{
 * 		c_i^d &= c_{iD}^d &\mbox{ on }\Gamma^d_D \mbox{ (Dirichlet)},\\
 * 		\mathbf D^d\nabla c_i^d\cdot\mathbf n &= 0 &\mbox{ on }\Gamma^d_N \mbox{ (Neumann)},
 * @f}
 * The transfer of mass through fractures is described by the transmission conditions on @f$\Gamma^d_F@f$:
 * @f[
 * 		-\mathbf D^d\nabla c_i^d\cdot\mathbf n = \sigma(c_i^d-c_i^{d-1}) + \left\{\begin{array}{cl}0 &\mbox{ if }\mathbf v^d\cdot\mathbf n\ge 0\\\mathbf v^d\cdot\mathbf n(c_i^{d-1}-c_i^d) & \mbox{ if }\mathbf v^d\cdot\mathbf n<0\end{array}\right.,\qquad
 * 		F^{d-1} = (\sigma + |\mathbf v^d\cdot\mathbf n|)(c_i^d-c_i^{d-1}).
 * @f]
 * Here @f$\mathbf n@f$ stands for the unit outward normal vector to @f$\partial\Omega^d@f$.
 * The coefficient @f$\sigma@f$ determines the transfer of mass through fractures due to diffusion.
 *
 * @ingroup transport_mod
 *
 */
class TransportDG : public TransportBase
{
public:

	class EqData : public TransportBase::TransportEqData {
	public:

		EqData();
		RegionSet read_boundary_list_item(Input::Record rec);

		Field<3, FieldValue<3>::Vector> disp_l;     ///< Longitudal dispersivity (for each substance).
		Field<3, FieldValue<3>::Vector> disp_t;     ///< Transversal dispersivity (for each substance).
		Field<3, FieldValue<3>::Vector> diff_m;     ///< Molecular diffusivity (for each substance).
		Field<3, FieldValue<3>::Vector> sigma_c;    ///< Transition parameter for diffusive transfer on fractures (for each substance).
		Field<3, FieldValue<3>::Vector> dg_penalty; ///< Penalty enforcing inter-element continuity of solution (for each substance).

	};

    /**
     * @brief Constructor.
     * @param init_mesh         computational mesh
     * @param in_rec            input record
     */
    TransportDG(Mesh &init_mesh, const Input::Record &in_rec);

    /**
     * @brief Declare input record type for the equation TransportDG.
     */
    static Input::Type::Record input_type;

    /**
     * @brief Computes the solution in one time instant.
     */
	void update_solution();

	/**
	 * @brief Returns the serialized solution array.
	 * @param vector the solution
	 * @param size   size of the array
	 */
	void get_solution_vector(double * &vector, unsigned int &size);

	/**
	 * @brief Returns the (possibly) parallel solution vector.
	 * @param vector
	 */
	void get_parallel_solution_vector(Vec &vector);

	/**
	 * @brief Updates the velocity field which determines some coefficients of the transport equation.
	 * 
         * @param dh mixed hybrid dof handler
         * 
	 * (So far it does not work since the flow module returns a vector of zeros.)
	 * @param velocity_vector Input array of velocity values.
	 */
	virtual void set_velocity_field(const MH_DofHandler &dh);

	/**
	 * @brief Postprocesses the solution and writes to output file.
	 */
	void output_data();

    /**
     * @brief Sets pointer to data of other equations.
     * TODO: there should be also passed the sigma parameter between dimensions
     * @param cross_section is pointer to cross_section data of Darcy flow equation
     */
	void set_eq_data(Field< 3, FieldValue<3>::Scalar >* cross_section);

	/**
	 * @brief Getter for field data.
	 */
	virtual EqData *get_data() { return &data; }

	/**
	 * @brief Getter for number of substances.
	 */
	unsigned int n_substances() { return n_subst; };

	/**
	 * @brief Getter for substance names.
	 * @return Vector of substance names.
	 */
	vector<string> &substance_names() { return subst_names; };

	/**
	 * @brief Destructor.
	 */
	~TransportDG();

private:

	/**
	 * @brief Assembles the mass matrix.
	 *
	 * The routine just calls templated method assemble_mass_matrix() for each
	 * space dimension.
	 */
	void assemble_mass_matrix();

	/**
	 * @brief Assembles the mass matrix for the given dimension.
	 *
	 * The DOF handler and FiniteElement objects of specified dimension
     * must be passed as arguments.
	 * @param dh DOF handler.
	 * @param fe FiniteElement
	 */
	template<unsigned int dim>
	void assemble_mass_matrix(DOFHandler<dim,3> *dh, FiniteElement<dim,3> *fe);

	/**
	 * @brief Assembles the stiffness matrix.
	 *
	 * This routine just calls assemble_volume_integrals(), assemble_fluxes_boundary(),
	 * assemble_fluxes_element_element() and assemble_fluxes_element_side() for each
	 * space dimension.
	 */
	void assemble_stiffness_matrix();

	/**
	 * @brief Assembles the volume integrals into the stiffness matrix.
	 *
	 * The DOF handler and FiniteElement objects of specified dimension
	 * must be passed as arguments.
	 * @param dh DOF handler.
	 * @param fe FiniteElement.
	*/
	template<unsigned int dim>
	void assemble_volume_integrals(DOFHandler<dim,3> *dh, FiniteElement<dim,3> *fe);

	/**
	 * @brief Assembles the right hand side due to volume sources.
	 *
	 * This method just calls set_sources() for each space dimension.
	 */
	void set_sources();

	/**
	 * @brief Assembles the right hand side vector due to volume sources.
	 *
	 * The DOF handler and FiniteElement objects of specified dimension
	 * must be passed as arguments.
	 * @param dh DOF handler.
	 * @param fe Finite element.
	 */
	template<unsigned int dim>
	void set_sources(DOFHandler<dim,3> *dh, FiniteElement<dim,3> *fe);

	/**
	 * @brief Assembles the fluxes on the boundary.
	 *
	 * The DOF handler and FiniteElement objects of specified dimension
	 * must be passed as arguments.
	 * @param dh DOF handler.
	 * @param dh_sub DOF handler for sides.
	 * @param fe FiniteElement.
	 * @param fe_sub FiniteElement for sides.
	 */
	template<unsigned int dim>
	void assemble_fluxes_boundary(DOFHandler<dim,3> *dh, DOFHandler<dim-1,3> *dh_sub, FiniteElement<dim,3> *fe, FiniteElement<dim-1,3> *fe_sub);

	/**
	 * @brief Assembles the fluxes between elements of the same dimension.
	 *
	 * The DOF handler and FiniteElement objects of specified dimension
	 * must be passed as arguments.
	 * @param dh DOF handler.
	 * @param dh_sub DOF handler for sides.
	 * @param fe FiniteElement.
	 * @param fe_sub FiniteElement for sides.
	 */
	template<unsigned int dim>
	void assemble_fluxes_element_element(DOFHandler<dim,3> *dh, DOFHandler<dim-1,3> *dh_sub, FiniteElement<dim,3> *fe, FiniteElement<dim-1,3> *fe_sub);

	/**
	 * @brief Assembles the fluxes between elements of different dimensions.
	 *
	 * The DOF handler and FiniteElement objects of specified dimension
	 * must be passed as arguments.
	 * @param dh DOF handler.
	 * @param dh_sub DOF handler for sides.
	 * @param fe FiniteElement.
	 * @param fe_sub FiniteElement for sides.
	 */
	template<unsigned int dim>
	void assemble_fluxes_element_side(DOFHandler<dim,3> *dh, DOFHandler<dim-1,3> *dh_sub, FiniteElement<dim,3> *fe, FiniteElement<dim-1,3> *fe_sub);


	/**
	 * @brief Assembles the r.h.s. components corresponding to the Dirichlet boundary conditions.
	 *
	 * The routine just calls templated method set_boundary_condition() for each space dimension.
	 */
	void set_boundary_conditions();

	/**
	 * @brief Assembles the r.h.s. components corresponding to the Dirichlet boundary conditions
	 * for a given space dimension.
	 *
	 * @param dh DOF handler.
     * @param fe FiniteElement.
	 */
	template<unsigned int dim>
	void set_boundary_conditions(DOFHandler<dim,3> *dh, FiniteElement<dim,3> *fe);

	/**
	 * @brief Calculates the velocity field on a given @p dim dimensional cell.
	 *
	 * @param cell     The cell.
	 * @param velocity The computed velocity field (at quadrature points).
	 * @param fv       The FEValues class providing the quadrature points
	 *                 and the shape functions for velocity.
	 */
	template<unsigned int dim>
	void calculate_velocity(typename DOFHandler<dim,3>::CellIterator cell, std::vector<arma::vec3> &velocity, FEValuesBase<dim,3> &fv);

	/**
	 * @brief Calculates the velocity divergence on a given @p dim dimensional cell.
	 *
	 * @param cell       The cell.
	 * @param divergence The computed divergence (at quadrature points).
	 * @param fv         The FEValues class providing the quadrature points
	 *                   and the shape functions for velocity.
	 */
	template<unsigned int dim>
	void calculate_velocity_divergence(typename DOFHandler<dim,3>::CellIterator cell, std::vector<double> &divergence, FEValuesBase<dim,3> &fv);

	/**
	 * @brief Calculates the dispersivity (diffusivity) tensor from the velocity field.
	 *
	 * @param K        The computed dispersivity tensor.
	 * @param velocity The velocity field (at quadrature points).
	 * @param Dm       Molecular diffusivities.
	 * @param alphaL   Longitudal dispersivities.
	 * @param alphaT   Transversal dispersivities.
	 * @param porosity  Porosities.
	 * @param cross_cut Cross-cuts of higher dimension.
	 */
	void calculate_dispersivity_tensor(std::vector<arma::mat33> &K, std::vector<arma::vec3> &velocity,
			vector<double> &Dm, vector<double> &alphaL, vector<double> &alphaT, vector<double> &porosity,
			vector<double> &cross_cut);

	/**
	 * @brief Sets up some parameters of the DG method for two sides of an edge.
	 *
	 * @param edg					The edge.
	 * @param s1				Side 1.
	 * @param s2				Side 2.
	 * @param n_points			Number of quadrature points.
	 * @param K					Dispersivity tensor.
	 * @param normal_vector		Normal vector to side 0 of the neighbour
	 * 							(assumed constant along the side).
	 * @param alpha				Penalty parameter that influences the continuity
	 * 							of the solution (large value=more continuity).
	 * @param gamma				Computed penalty parameters.
	 * @param omega				Computed weights.
	 * @param transport_flux	Computed flux from side 1 to side 2.
	 */

	void set_DG_parameters(const Edge *edg,
	        const int s1,
	        const int s2,
	        const unsigned int n_points,
	        const std::vector< std::vector<arma::mat33> > &K,
	        const arma::vec3 &normal_vector,
	        const std::vector<std::vector<double> > &Dm,
	        const vector<double> &alpha,
	        double &gamma,
	        double *omega,
	        double &transport_flux);

	/**
	 * @brief Sets up parameters of the DG method on a given boundary edge.
	 *
	 * Assumption is that the edge consists of only 1 side.
	 * @param edge				The edge.
	 * @param n_points			Number of quadrature points.
	 * @param K					Dispersivity tensor.
	 * @param normal_vector		Normal vector (assumed constant along the edge).
	 * @param alpha				Penalty parameter that influences the continuity
	 * 							of the solution (large value=more continuity).
	 * @param gamma				Computed penalty parameters.
	 * @param omega				Computed weights.
	 */
	void set_DG_parameters_boundary(const Edge *edge,
	            const unsigned int n_points,
	            const std::vector<arma::mat33> &K,
	            const arma::vec3 &normal_vector,
	            const double alpha,
	            const double Dm,
	            double &gamma,
	            double *omega);


	/**
	 * @brief Sets the initial condition.
	 */
	void set_initial_condition();

	template<unsigned int dim>
	void prepare_initial_condition(DOFHandler<dim,3> *dh, FiniteElement<dim,3> *fe);

	/**
	 * @brief Calculates flux through boundary of each region.
	 *
	 * This actually calls calc_fluxes<dim>() for each space dimension.
	 * @param bcd_balance       Total fluxes.
	 * @param bcd_plus_balance  Incoming fluxes.
	 * @param bcd_minus_balance Outgoing fluxes.
	 */
	void calc_fluxes(vector<vector<double> > &bcd_balance, vector<vector<double> > &bcd_plus_balance, vector<vector<double> > &bcd_minus_balance);

	/**
	 * @brief Calculates flux through boundary of each region.
	 * @param bcd_balance       Total fluxes.
	 * @param bcd_plus_balance  Incoming fluxes.
	 * @param bcd_minus_balance Outgoing fluxes.
	 * @param dh                DOF handler.
	 * @param fe                Finite element.
	 */
	template<unsigned int dim>
	void calc_fluxes(vector<vector<double> > &bcd_balance, vector<vector<double> > &bcd_plus_balance, vector<vector<double> > &bcd_minus_balance,
			DOFHandler<dim,3> *dh, FiniteElement<dim,3> *fe);

	/**
	 * @brief Calculates volume sources for each region.
	 *
	 * This method actually calls calc_elem_sources<dim>() for each space dimension.
	 * @param src_balance Vector of sources per region.
	 */
	void calc_elem_sources(vector< vector<double> > &src_balance);

	/**
	 * @brief Calculates volume sources for each region.
	 * @param src_balance Vector of sources per region.
	 * @param dh          DOF handler.
	 * @param fe          Finite element.
	 */
	template<unsigned int dim>
	void calc_elem_sources(vector< vector<double> > &src_balance, DOFHandler<dim,3> *dh, FiniteElement<dim,3> *fe);



	/// @name Physical parameters
	// @{

	/// Field data for model parameters.
	EqData data;

	/// Number of transported substances.
	int n_subst;

	/// Names of transported substances.
	std::vector<string> subst_names;

	/// True if sorption is considered.
	bool sorption;

	/// True if dual porosity is considered.
	bool dual_porosity;
	// @}


	/// @name Parameters of the numerical method
	// @{
	/// Penalty parameters.
	std::vector<std::vector<double> > gamma;

	/**
	 * @brief Tolerance to detect Dirichlet or Neumann boundary.
	 *
	 * This is used in the following test:
	 *   if (flux_through_side / |flux_through_all_sides| > -tol_switch_dirichlet_neumann)
	 *      set Neumann;
	 *   else
	 *      set Dirichlet;
	 */
    const double tol_switch_dirichlet_neumann;

	// @}



	/// @name Solution of algebraic system
	// @{

	/// Vector of right hand side.
	Vec *rhs;

	/// The stiffness matrix.
	Mat *stiffness_matrix;

	/// The mass matrix.
	Mat mass_matrix;

	/// Distribution of the solution vectors to the processors.
	Distribution *distr;

	/// Linear algebra system for the transport equation.
	LinSys **ls;

	/// Linear algebra system for the time derivative (actually it is used only for handling the matrix structures).
	LinSys *ls_dt;

	/// Solver for the linear algebraic system.
	struct Solver *solver;

	// @}


	/// @name Output to file
	// @{

	/// Array for storing the output solution data.
	vector<double*> output_solution;

	/// Class for handling the solution output.
	OutputTime *transport_output;

	/// Time marks for writing the output.
	TimeMark::Type output_mark_type;

	// @}


	/// @name Finite element objects
	// @{

	/// DOF handler for 1D problem.
	DOFHandler<1,3> *dof_handler1d;

	/// DOF handler for 2D problem.
	DOFHandler<2,3> *dof_handler2d;

	/// DOF handler for 3D problem.
	DOFHandler<3,3> *dof_handler3d;

	/// Finite element for 1D.
	FiniteElement<1,3> *fe1d;

	/// Finite element for 2D.
	FiniteElement<2,3> *fe2d;

	/// Finite element for 3D.
	FiniteElement<3,3> *fe3d;

	// @}


	/// @name Other
	// @{

    /// Indicates whether the fluxes have changed in the last time step.
    bool flux_changed;

    /// Indicates whether matrices have been preallocated.
    bool allocation_done;

    const MH_DofHandler * mh_dh;

    // @}
};





#endif /* TRANSPORT_DG_HH_ */
