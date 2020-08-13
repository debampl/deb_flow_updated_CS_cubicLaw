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
 * @file    field_value_cache.cc
 * @brief
 * @author  David Flanderka
 */

#include <limits>
#include "fields/field_value_cache.impl.hh"
#include "fields/field_values.hh"
#include "fields/eval_points.hh"
#include "fields/eval_subset.hh"
#include "fem/dh_cell_accessor.hh"
#include "mesh/accessors.hh"


/******************************************************************************
 * Implementation of ElementCacheMap methods
 */

const unsigned int ElementCacheMap::undef_elem_idx = std::numeric_limits<unsigned int>::max();
const unsigned int ElementCacheMap::simd_size_double = 4;


ElementCacheMap::ElementCacheMap()
: elm_idx_(ElementCacheMap::n_cached_elements, ElementCacheMap::undef_elem_idx),
  ready_to_reading_(false), element_eval_points_map_(nullptr), eval_point_data_(0),
  regions_starts_(2*ElementCacheMap::regions_in_chunk,ElementCacheMap::regions_in_chunk),
  element_starts_(2*ElementCacheMap::elements_in_chunk,ElementCacheMap::elements_in_chunk) {}


ElementCacheMap::~ElementCacheMap() {
    if (element_eval_points_map_!=nullptr) {
        delete element_eval_points_map_;
    }
}


void ElementCacheMap::init(std::shared_ptr<EvalPoints> eval_points) {
    this->eval_points_ = eval_points;
    unsigned int ep_data_size = ElementCacheMap::n_cached_elements * eval_points_->max_size();
    eval_point_data_.resize(ep_data_size);
    element_eval_points_map_ = new int [ep_data_size];
    this->clear_element_eval_points_map(); // use loop
}


void ElementCacheMap::prepare_elements_to_update() {
    std::sort(eval_point_data_.begin(), eval_point_data_.end());
    unsigned int last_region_idx = -1;
    unsigned int last_element_idx = -1;
    unsigned int i_pos=0; // position in eval_point_data_
    unsigned int points_in_cache = 0; // index to cache in table element_eval_points_map_

    // Erase element data of previous step
    regions_starts_.reset();
    element_starts_.reset();
    regions_to_map_.clear();
    element_to_map_.clear();
    std::fill(elm_idx_.begin(), elm_idx_.end(), ElementCacheMap::undef_elem_idx);

    for (auto it=eval_point_data_.begin(); it!=eval_point_data_.end(); ++it, ++i_pos) {
        if (it->i_element_ != last_element_idx) { // new element
            if (it->i_reg_ != last_region_idx) { // new region
                regions_to_map_[it->i_reg_] = regions_starts_.temporary_size();
                regions_starts_.push_back( element_starts_.temporary_size() );
                last_region_idx = it->i_reg_;
            }
			elm_idx_[element_to_map_.size()] = it->i_element_;
            element_to_map_[it->i_element_] = element_starts_.temporary_size();
            element_starts_.push_back(i_pos);
            last_element_idx = it->i_element_;
        }
        set_element_eval_point(element_to_map_.find(it->i_element_)->second, it->i_eval_point_, points_in_cache);
        points_in_cache++;
    }
    regions_starts_.push_back( element_starts_.temporary_size() );
    element_starts_.push_back(i_pos);
    regions_starts_.make_permanent();
    element_starts_.make_permanent();
}


void ElementCacheMap::start_elements_update() {
	ready_to_reading_ = false;
}

void ElementCacheMap::finish_elements_update() {
	ready_to_reading_ = true;
}

void ElementCacheMap::clear_element_eval_points_map() {
	/// optimize loops, set to ElementCacheMap::unused_point only items stored in eval_point_data_
	ASSERT_PTR_DBG(element_eval_points_map_);
    unsigned int size = this->eval_points_->max_size();
	for (unsigned int i_elm=0; i_elm<ElementCacheMap::n_cached_elements; ++i_elm)
	    for (unsigned int i_point=0; i_point<size; ++i_point)
	        set_element_eval_point(i_elm, i_point, ElementCacheMap::unused_point);
}


DHCellAccessor & ElementCacheMap::cache_map_index(DHCellAccessor &dh_cell) const {
	ASSERT_DBG(ready_to_reading_);
	unsigned int elm_idx = dh_cell.elm_idx();
	std::map<unsigned int, unsigned int>::const_iterator it = element_to_map_.find(elm_idx);
	if ( it != element_to_map_.end() ) dh_cell.set_element_cache_index( it->second );
	else dh_cell.set_element_cache_index( ElementCacheMap::undef_elem_idx );
    return dh_cell;
}

