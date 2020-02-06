/*
 * field_value_cache_test.cpp
 *
 *  Created on: Dec 03, 2019
 *      Author: David Flanderka
 *
 *  Tests FieldValueCache and ElementCacheMap classes
 */

#define TEST_USE_MPI
#define FEAL_OVERRIDE_ASSERTS
#include <flow_gtest_mpi.hh>
#include <mesh_constructor.hh>
#include "arma_expect.hh"

#include "fields/eval_points.hh"
#include "fields/eval_subset.hh"
#include "fields/field_value_cache.hh"
#include "fields/field_values.hh"
#include "quadrature/quadrature.hh"
#include "quadrature/quadrature_lib.hh"
#include "fem/dofhandler.hh"
#include "fem/dh_cell_accessor.hh"
#include "mesh/mesh.h"
#include "system/sys_profiler.hh"


class FieldValueCacheTest : public testing::Test, public ElementCacheMap {

public:
    FieldValueCacheTest() {
        FilePath::set_io_dirs(".",UNIT_TESTS_SRC_DIR,"",".");
        Profiler::initialize();
        PetscInitialize(0,PETSC_NULL,PETSC_NULL,PETSC_NULL);

        mesh_ = mesh_full_constructor("{mesh_file=\"mesh/cube_2x1.msh\"}");
        dh_ = std::make_shared<DOFHandlerMultiDim>(*mesh_);

        eval_points = std::make_shared<EvalPoints>();
        Quadrature *q_bulk = new QGauss(3, 2);
        Quadrature *q_side = new QGauss(2, 2);
        bulk_eval = eval_points->add_bulk<3>(*q_bulk );
        edge_eval = eval_points->add_edge<3>(*q_side );
        this->init(eval_points);
    }

    ~FieldValueCacheTest() {}

    Mesh * mesh_;
    std::shared_ptr<DOFHandlerMultiDim> dh_;

    std::shared_ptr<EvalPoints> eval_points;
    std::shared_ptr<BulkIntegral> bulk_eval;
    std::shared_ptr<EdgeIntegral> edge_eval;
};


TEST_F(FieldValueCacheTest, field_value_cache) {
    FieldValueCache<double, double> value_cache(1, 1);
    value_cache.init(eval_points, ElementCacheMap::n_cached_elements);
    EXPECT_EQ(value_cache.n_cache_points(), eval_points->max_size()*ElementCacheMap::n_cached_elements);

    DHCellAccessor dh_cell(dh_.get(), 2);
    this->add(dh_cell);
    for( DHCellSide cell_side : dh_cell.side_range() )
      if ( cell_side.n_edge_sides() >= 2 )
        for( DHCellSide edge_side : cell_side.edge_sides() ) {
            this->add(edge_side);
        }

    this->prepare_elements_to_update(mesh_);

    // mark used points
    this->mark_used_eval_points(dh_cell, bulk_eval->get_subset_idx(), 4);
    for( DHCellSide cell_side : dh_cell.side_range() )
      if ( cell_side.n_edge_sides() >= 2 )
    	for( DHCellSide edge_side : cell_side.edge_sides() ) {
            this->mark_used_eval_points(edge_side.cell(), edge_eval->get_subset_idx(), 3, 3*edge_side.side_idx());
        }
    this->create_elements_points_map();

    // set value
    EXPECT_EQ(this->points_in_cache_, 16);
    Armor::ArmaMat<double, 1, 1> const_val{0.5};
    for (unsigned int i=0; i<this->points_in_cache_; ++i) value_cache.data().set(i) = const_val;
    this->clear_elements_to_update();

    // check value
    dh_cell = (*this)(dh_cell);
    for(BulkPoint q_point: bulk_eval->points(dh_cell)) {
        auto point_val = value_cache.template get_value<1,1>(*this, dh_cell, q_point.eval_point_idx());
    	EXPECT_ARMA_EQ( point_val, const_val );
    }
    for ( DHCellSide cell_side : dh_cell.side_range() )
      if ( cell_side.n_edge_sides() >= 2 )
        for( DHCellSide edge_side : cell_side.edge_sides() )
            for ( EdgePoint q_point : edge_eval->points(edge_side) ) {
                auto edge_cell = (*this)(edge_side.cell());
                auto point_val = value_cache.template get_value<1,1>(*this, edge_cell, q_point.eval_point_idx());
                EXPECT_ARMA_EQ( point_val, const_val );
            }
}


TEST_F(FieldValueCacheTest, element_cache_map) {
    const ElementCacheMap::UpdateCacheHelper &update_cache_data = this->update_cache_data();

    // Test of 2 elements on same region
    DHCellAccessor dh_cell1(dh_.get(), 1);
    DHCellAccessor dh_cell2(dh_.get(), 2);
    this->add(dh_cell1);
    this->add(dh_cell2);
    EXPECT_EQ(update_cache_data.added_elements_.size(), 2);

    this->prepare_elements_to_update(mesh_);
    EXPECT_EQ(update_cache_data.region_element_map_.size(), 1);
    EXPECT_TRUE(update_cache_data.region_element_map_.find(1)!=update_cache_data.region_element_map_.end());
    EXPECT_EQ(update_cache_data.region_element_map_.find(1)->second.size(), 2);

    this->create_elements_points_map();
    EXPECT_EQ(update_cache_data.region_cache_indices_map_.size(), 1);
    this->clear_elements_to_update();
    EXPECT_EQ(update_cache_data.added_elements_.size(), 0);
    EXPECT_EQ(update_cache_data.region_element_map_.size(), 0);

    dh_cell1 = (*this)(dh_cell1);
    EXPECT_EQ(dh_cell1.element_cache_index(), 1);

    // Test of edge connectivity
    for( DHCellSide cell_side : dh_cell2.side_range() )
        if ( cell_side.n_edge_sides() >= 2 )
            for( DHCellSide edge_side : cell_side.edge_sides() ) {
            	this->add(edge_side);
            }
    EXPECT_EQ(update_cache_data.added_elements_.size(), 3);
    this->prepare_elements_to_update(mesh_);
    EXPECT_EQ(update_cache_data.region_element_map_.size(), 1);
    EXPECT_TRUE(update_cache_data.region_element_map_.find(1)!=update_cache_data.region_element_map_.end());
    EXPECT_EQ(update_cache_data.region_element_map_.find(1)->second.size(), 3);

    for( DHCellSide cell_side : dh_cell2.side_range() )
        if ( cell_side.n_edge_sides() >= 2 )
            for( DHCellSide edge_side : cell_side.edge_sides() ) {
                this->mark_used_eval_points(edge_side.cell(), edge_eval->get_subset_idx(), 3, 3*edge_side.side_idx());
            }
    this->create_elements_points_map();
    EXPECT_EQ(update_cache_data.region_cache_indices_map_.size(), 1);
    EXPECT_EQ(update_cache_data.region_cache_indices_range_[0], 0);
    EXPECT_EQ(update_cache_data.region_cache_indices_range_[1], 12);
    this->clear_elements_to_update();
    dh_cell2 = (*this)(dh_cell2);
    EXPECT_EQ(dh_cell2.element_cache_index(), 1);

    // Test of 3 elements on 2 different regions
    DHCellAccessor dh_cell3(dh_.get(), 3);
    DHCellAccessor dh_cell6(dh_.get(), 6);
    this->add(dh_cell1);
    this->add(dh_cell3);
    this->add(dh_cell6);
    EXPECT_EQ(update_cache_data.added_elements_.size(), 3);

    this->prepare_elements_to_update(mesh_);
    EXPECT_EQ(update_cache_data.region_element_map_.size(), 2);
    EXPECT_TRUE(update_cache_data.region_element_map_.find(1)!=update_cache_data.region_element_map_.end());
    EXPECT_EQ(update_cache_data.region_element_map_.find(1)->second.size(), 2);
    EXPECT_EQ(update_cache_data.region_element_map_.find(3)->second.size(), 1);

    this->create_elements_points_map();
    EXPECT_EQ(update_cache_data.region_cache_indices_map_.size(), 2);
    this->clear_elements_to_update();
    dh_cell1 = (*this)(dh_cell1);
    EXPECT_EQ(dh_cell1.element_cache_index(), 2);
}
