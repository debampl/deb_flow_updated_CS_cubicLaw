/*!
 *
﻿ * Copyright (C) 2015 Technical University of Liberec.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License version 3 as published by the
 * Free Software Foundation. (http://www.gnu.org/licenses/gpl-3.0.en.html)
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.
 *
 *
 * @file    assembly_dg.hh
 * @brief
 */

#ifndef ASSEMBLY_DG_HH_
#define ASSEMBLY_DG_HH_

#include "transport/advection_diffusion_model.hh"
#include "transport/transport_dg.hh"
#include "fem/mapping_p1.hh"
#include "fem/fe_p.hh"
#include "fem/fe_rt.hh"
#include "fem/fe_values.hh"
#include "quadrature/quadrature_lib.hh"
#include "coupling/balance.hh"


/**
 * Base (non-templated) class of container for Finite element and related objects.
 */
class AssemblyDGBase
{
public:
    virtual ~AssemblyDGBase() {}

    virtual void initialize() = 0;

    virtual void assemble_mass_matrix(DHCellAccessor cell) = 0;

    virtual void assemble_volume_integrals(DHCellAccessor cell) = 0;
};


/**
 * Auxiliary container class for Finite element and related objects of given dimension.
 */
template <unsigned int dim, class Model>
class AssemblyDG : public AssemblyDGBase
{
public:
    typedef typename TransportDG<Model>::EqData EqDataDG;

    /// Constructor.
    AssemblyDG(std::shared_ptr<EqDataDG> data, unsigned int fe_order, AdvectionDiffusionModel &adm)
    : fe_(new FE_P_disc<dim>(fe_order)), fe_low_(new FE_P_disc<dim-1>(fe_order)),
      fe_rt_(new FE_RT0<dim>), fe_rt_low_(new FE_RT0<dim-1>),
      quad_(new QGauss<dim>(2*fe_order)), quad_low_(new QGauss<dim-1>(2*fe_order)),
      mapping_(new MappingP1<dim,3>), mapping_low_(new MappingP1<dim-1,3>),
      model_(adm), data_(data), fv_rt_(*mapping_, *quad_, *fe_rt_, update_values | update_gradients),
      fe_values_(*mapping_, *quad_, *fe_, update_values | update_gradients | update_JxW_values | update_quadrature_points) {

        ndofs_ = fe_->n_dofs();
        qsize_ = quad_->size();
        dof_indices_.resize(ndofs_);
    }

    /// Destructor.
    ~AssemblyDG() {
        delete fe_;
        delete fe_low_;
        delete fe_rt_;
        delete fe_rt_low_;
        delete quad_;
        delete quad_low_;
        delete mapping_;
        delete mapping_low_;
    }

    /// Getter for FE_P_disc.
    inline FiniteElement<dim> *fe() const {
        return fe_;
    }

    /// Getter for FE_P_disc of lower dim.
    inline FiniteElement<dim-1> *fe_low() const {
        return fe_low_;
    }

    /// Getter for FE_RT0.
    inline FiniteElement<dim> *fe_rt() const {
        return fe_rt_;
    }

    /// Getter for FE_RT0 of lower dim.
    inline FiniteElement<dim-1> *fe_rt_low() const {
        return fe_rt_low_;
    }

    /// Getter for quadrature.
    inline Quadrature<dim> *quad() const {
        return quad_;
    }

    /// Getter for quadrature of lower dim.
    inline Quadrature<dim-1> *quad_low() const {
        return quad_low_;
    }

    /// Getter for mapping.
    inline MappingP1<dim,3> *mapping() const {
        return mapping_;
    }

    /// Getter for mapping of lower dim.
    inline MappingP1<dim-1,3> *mapping_low() const {
        return mapping_low_;
    }

    /// Initialize auxiliary vectors and other data members
    void initialize() override {
        local_matrix_.resize(ndofs_*ndofs_);
        local_retardation_balance_vector_.resize(ndofs_);
        local_mass_balance_vector_.resize(ndofs_);
        velocity_.resize(qsize_);
        sources_sigma_.resize(model_.n_substances(), std::vector<double>(qsize_));

        mm_coef_.resize(qsize_);
        ret_coef_.resize(model_.n_substances());
        for (unsigned int sbi=0; sbi<model_.n_substances(); sbi++)
        {
            ret_coef_[sbi].resize(qsize_);
        }
    }

    /// Assemble integral over element
    void assemble_mass_matrix(DHCellAccessor cell) override
    {
        ASSERT_EQ_DBG(cell.dim(), dim).error("Dimension of element mismatch!");
        ElementAccessor<3> elm = cell.elm();

        fe_values_.reinit(elm);
        cell.get_dof_indices(dof_indices_);

        model_.compute_mass_matrix_coefficient(fe_values_.point_list(), elm, mm_coef_);
        model_.compute_retardation_coefficient(fe_values_.point_list(), elm, ret_coef_);

        for (unsigned int sbi=0; sbi<model_.n_substances(); ++sbi)
        {
            // assemble the local mass matrix
            for (unsigned int i=0; i<ndofs_; i++)
            {
                for (unsigned int j=0; j<ndofs_; j++)
                {
                    local_matrix_[i*ndofs_+j] = 0;
                    for (unsigned int k=0; k<qsize_; k++)
                        local_matrix_[i*ndofs_+j] += (mm_coef_[k]+ret_coef_[sbi][k])*fe_values_.shape_value(j,k)*fe_values_.shape_value(i,k)*fe_values_.JxW(k);
                }
            }

            for (unsigned int i=0; i<ndofs_; i++)
            {
                local_mass_balance_vector_[i] = 0;
                local_retardation_balance_vector_[i] = 0;
                for (unsigned int k=0; k<qsize_; k++)
                {
                    local_mass_balance_vector_[i] += mm_coef_[k]*fe_values_.shape_value(i,k)*fe_values_.JxW(k);
                    local_retardation_balance_vector_[i] -= ret_coef_[sbi][k]*fe_values_.shape_value(i,k)*fe_values_.JxW(k);
                }
            }

            model_.balance()->add_mass_matrix_values(model_.get_subst_idx()[sbi], elm.region().bulk_idx(), dof_indices_, local_mass_balance_vector_);
            data_->ls_dt[sbi]->mat_set_values(ndofs_, &(dof_indices_[0]), ndofs_, &(dof_indices_[0]), &(local_matrix_[0]));
            VecSetValues(data_->ret_vec[sbi], ndofs_, &(dof_indices_[0]), &(local_retardation_balance_vector_[0]), ADD_VALUES);
        }
    }

    /// Assembles the volume integrals into the stiffness matrix.
    void assemble_volume_integrals(DHCellAccessor cell) override
    {
        ASSERT_EQ_DBG(cell.dim(), dim).error("Dimension of element mismatch!");
        if (!cell.is_own()) return;

        ElementAccessor<3> elm = cell.elm();

        fe_values_.reinit(elm);
        fv_rt_.reinit(elm);
        cell.get_dof_indices(dof_indices_);

        calculate_velocity(elm, velocity_, fv_rt_);
        model_.compute_advection_diffusion_coefficients(fe_values_.point_list(), velocity_, elm, data_->ad_coef, data_->dif_coef);
        model_.compute_sources_sigma(fe_values_.point_list(), elm, sources_sigma_);

        // assemble the local stiffness matrix
        for (unsigned int sbi=0; sbi<model_.n_substances(); sbi++)
        {
            for (unsigned int i=0; i<ndofs_; i++)
                for (unsigned int j=0; j<ndofs_; j++)
                    local_matrix_[i*ndofs_+j] = 0;

            for (unsigned int k=0; k<qsize_; k++)
            {
                for (unsigned int i=0; i<ndofs_; i++)
                {
                    arma::vec3 Kt_grad_i = data_->dif_coef[sbi][k].t()*fe_values_.shape_grad(i,k);
                    double ad_dot_grad_i = arma::dot(data_->ad_coef[sbi][k], fe_values_.shape_grad(i,k));

                    for (unsigned int j=0; j<ndofs_; j++)
                        local_matrix_[i*ndofs_+j] += (arma::dot(Kt_grad_i, fe_values_.shape_grad(j,k))
                                                  -fe_values_.shape_value(j,k)*ad_dot_grad_i
                                                  +sources_sigma_[sbi][k]*fe_values_.shape_value(j,k)*fe_values_.shape_value(i,k))*fe_values_.JxW(k);
                }
            }
            data_->ls[sbi]->mat_set_values(ndofs_, &(dof_indices_[0]), ndofs_, &(dof_indices_[0]), &(local_matrix_[0]));
        }
    }

private:
	/**
	 * @brief Calculates the velocity field on a given cell.
	 *
	 * @param cell     The cell.
	 * @param velocity The computed velocity field (at quadrature points).
	 * @param fv       The FEValues class providing the quadrature points
	 *                 and the shape functions for velocity.
	 */
    void calculate_velocity(const ElementAccessor<3> &cell, vector<arma::vec3> &velocity,
                            FEValuesBase<dim,3> &fv)
    {
        ASSERT_EQ_DBG(cell->dim(), dim).error("Element dimension mismatch!");

        velocity.resize(fv.n_points());
        arma::mat map_mat = mapping_->element_map(cell);
        vector<arma::vec3> point_list;
        point_list.resize(fv.n_points());
        for (unsigned int k=0; k<fv.n_points(); k++)
        	point_list[k] = mapping_->project_unit_to_real(RefElement<dim>::local_to_bary(fv.get_quadrature()->point(k)), map_mat);
        model_.velocity_field_ptr()->value_list(point_list, cell, velocity);
    }


    FiniteElement<dim> *fe_;            ///< Finite element for the solution of the advection-diffusion equation.
    FiniteElement<dim-1> *fe_low_;      ///< Finite element for the solution of the advection-diffusion equation (dim-1).
    FiniteElement<dim> *fe_rt_;         ///< Finite element for the water velocity field.
    FiniteElement<dim-1> *fe_rt_low_;   ///< Finite element for the water velocity field (dim-1).
    Quadrature<dim> *quad_;             ///< Quadrature used in assembling methods.
    Quadrature<dim-1> *quad_low_;       ///< Quadrature used in assembling methods (dim-1).
    MappingP1<dim,3> *mapping_;         ///< Auxiliary mapping of reference elements.
    MappingP1<dim-1,3> *mapping_low_;   ///< Auxiliary mapping of reference elements (dim-1).

    /// Reference to model (we must use common ancestor of concentration and heat model)
    AdvectionDiffusionModel &model_;

    /// Data object shared with TransportDG
    std::shared_ptr<EqDataDG> data_;

    unsigned int ndofs_;                                      ///< Number of dofs
    unsigned int qsize_;                                      ///< Size of FEValues quadrature
    FEValues<dim,3> fv_rt_;                                   ///< FEValues of object (of RT0 finite element type)
    FEValues<dim,3> fe_values_;                               ///< FEValues of object (of P disc finite element type)

    vector<LongIdx> dof_indices_;                             ///< Vector of global DOF indices
    vector<PetscScalar> local_matrix_;                        ///< Helper vector for assemble methods
    vector<PetscScalar> local_retardation_balance_vector_;    ///< Helper vector for assemble mass matrix.
    vector<PetscScalar> local_mass_balance_vector_;           ///< Same as previous.
    vector<arma::vec3> velocity_;                             ///< Velocity results.
    vector<vector<double> > sources_sigma_;                   ///< Helper vectors for assemble volume integrals.

	/// Mass matrix coefficients.
	vector<double> mm_coef_;
	/// Retardation coefficient due to sorption.
	vector<vector<double> > ret_coef_;
};



#endif /* FE_VALUE_HANDLER_HH_ */
