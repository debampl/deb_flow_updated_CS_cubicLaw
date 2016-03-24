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
 * @file    multi_field.impl.hh
 * @brief   
 */

#ifndef MULTI_FIELD_IMPL_HH_
#define MULTI_FIELD_IMPL_HH_


#include "multi_field.hh"
#include "fields/field_algo_base.hh"
#include "input/input_exception.hh"

namespace it = Input::Type;

/******************************************************************************************
 * Implementation of MultiField<...>
 */

template<int spacedim, class Value>
MultiField<spacedim, Value>::MultiField(bool bc)
: FieldCommon()
{
	static_assert(Value::NRows_ == 1 && Value::NCols_ == 1, "");
	this->multifield_ = true;
    this->shared_->bc_ = bc;
}



template<int spacedim, class Value>
it::Instance MultiField<spacedim,Value>::get_input_type() {
	ASSERT(false, "This method can't be used for MultiField");

	it::Abstract abstract = it::Abstract();
	it::Instance inst = it::Instance( abstract, std::vector<it::TypeBase::ParameterPair>() );
	return inst;
}


template<int spacedim, class Value>
it::Array MultiField<spacedim,Value>::get_multifield_input_type() {
	it::Array type = it::Array( SubFieldBaseType::get_input_type_instance(shared_->input_element_selection_), 1);
	return type;
}


template<int spacedim, class Value>
bool MultiField<spacedim, Value>::set_time(
		const TimeStep &time, LimitSide limit_side)
{
	// initialization of Multifield for first call
	if (sub_fields_.size() == 0) {
	    setup_components();
	}

	// set time for sub fields
	set_time_result_ = TimeStatus::constant;
	is_jump_time_=false;
	for( SubFieldType &field : sub_fields_) {
            if (field.set_time(time, limit_side))
                set_time_result_ = TimeStatus::changed;
            is_jump_time_ = is_jump_time_ ||  field.is_jump_time();
	}
    return (set_time_result_ == TimeStatus::changed);
}



template<int spacedim, class Value>
void MultiField<spacedim, Value>::set_mesh(const Mesh &mesh) {
	// test if mesh is not set
	if (shared_->mesh_ && shared_->mesh_ != &mesh) {
		THROW(ExcFieldMeshDifference() << EI_Field(name()) );
	}

    shared_->mesh_ = &mesh;
}


template<int spacedim, class Value>
void MultiField<spacedim, Value>::copy_from(const FieldCommon & other) {
	if (typeid(other) == typeid(*this)) {
		auto  const &other_field = dynamic_cast<  MultiField<spacedim, Value> const &>(other);
		this->operator=(other_field);
	} else if (typeid(other) == typeid(SubFieldType)) {
		auto  const &other_field = dynamic_cast<  SubFieldType const &>(other);
		sub_fields_.resize(1);
		sub_fields_[0] = other_field;
	}
}



template<int spacedim, class Value>
void MultiField<spacedim, Value>::output(std::shared_ptr<OutputTime> stream)
{
	// currently we cannot output boundary fields
	if (!is_bc())
		stream->register_data(this->output_type(), *this);
}





template<int spacedim, class Value>
bool MultiField<spacedim, Value>::is_constant(Region reg) {
	bool const_all=true;
	for(auto &field : sub_fields_) const_all = const_all && field.is_constant(reg);
	return const_all;
}



template<int spacedim, class Value>
void MultiField<spacedim, Value>::setup_components() {
	unsigned int comp_size = this->shared_->comp_names_.size();
	string full_name;
	ASSERT(comp_size, "Vector of component names is empty!\n");
	ASSERT(this->shared_->mesh_, "Mesh is not set!\n");

    sub_fields_.reserve( comp_size );
    for(unsigned int i_comp=0; i_comp < comp_size; i_comp++)
    {
    	if (this->shared_->comp_names_[i_comp].length() == 0)
    		full_name = name();
    	else {
    		full_name = this->shared_->comp_names_[i_comp] + "_" + name();
    	}

    	sub_fields_.push_back( SubFieldType(i_comp, name(), full_name, is_bc()) );
    	sub_fields_[i_comp].units( units() );
    	sub_fields_[i_comp].set_mesh( *(shared_->mesh_) );
//     	sub_fields_[i_comp].set_limit_side(this->limit_side_);
        sub_fields_[i_comp].input_selection(shared_->input_element_selection_);
    	sub_fields_[i_comp].add_factory( std::make_shared<MultiFieldFactory>(i_comp) );

    	if (this->shared_->input_default_!="") {
    		sub_fields_[i_comp].shared_->input_default_ = this->shared_->input_default_;
    	}

    	sub_fields_[i_comp].flags_ = this->flags_;
    	sub_fields_[i_comp].set_input_list(this->full_input_list_);
    }
}



template<int spacedim, class Value>
void MultiField<spacedim,Value>::set_input_list(const Input::Array &list) {
    if (! flags().match(FieldFlag::declare_input)) return;

    // Check sizes of Arrays defined MultiField in field descriptors
    for (Input::Iterator<Input::Record> it = list.begin<Input::Record>();
					it != list.end();
					++it) {
    	Input::Array mf_array;
    	if ( it->opt_val(this->input_name(), mf_array) ) {
    		unsigned int comp_size = this->shared_->comp_names_.size();
    		if (mf_array.size() != 1 && mf_array.size() != comp_size)
    			THROW( Exc_InvalidMultiFieldSize() << EI_MultiFieldName(this->input_name())
    					<< EI_Size(mf_array.size()) << EI_ExpectedSize(comp_size) << list.ei_address() );
    	}
    }
    
    this->full_input_list_ = list;
    
    // Save the full array for future use in FieldCommon::mark_input_times().
    list.copy_to(shared_->input_list_);
}


template<int spacedim, class Value>
typename MultiField<spacedim, Value>::MultiFieldValue::return_type MultiField<spacedim, Value>::value(const Point &p, const ElementAccessor<spacedim> &elm) const {
    typename MultiFieldValue::return_type ret(size(), 1);
    for (unsigned int i_comp=0; i_comp < size(); i_comp++) {
    	ret(i_comp, 0) = sub_fields_[i_comp].value(p,elm);
    }

    return ret;
}



template<int spacedim, class Value>
void MultiField<spacedim, Value>::value_list(const std::vector< Point >  &point_list, const  ElementAccessor<spacedim> &elm,
                   std::vector<typename MultiFieldValue::return_type>  &value_list) const {
	ASSERT_EQUAL( point_list.size(), value_list.size() );
	for(unsigned int i=0; i< point_list.size(); i++) {
		value_list[i]=this->value(point_list[i], elm);
	}
}



template<int spacedim, class Value>
typename Field<spacedim,Value>::FieldBasePtr MultiField<spacedim, Value>::MultiFieldFactory::create_field(Input::Record descriptor_rec, const FieldCommon &field) {
	Input::Array multifield_arr;
	if (descriptor_rec.opt_val(field.input_name(), multifield_arr))
	{
		//ASSERT(multifield_arr.size() == 1 || multifield_arr.size() == field.n_comp(),
		//		"Invalid size of Array defined for MultiField '%s'!\n", field.input_name().c_str());
		unsigned int position = 0;
		auto it = multifield_arr.begin<Input::AbstractRecord>();
		if (multifield_arr.size() > 1)
			while (index_ != position) {
				++it; ++position;
			}

		typename Field<spacedim,Value>::FieldBasePtr field_algo_base = Field<spacedim,Value>::FieldBaseType::function_factory( (*it), field.n_comp() );
		field_algo_base->set_component_idx(index_);
		return field_algo_base;
	}

	return NULL;
}



template<int spacedim, class Value>
bool MultiField<spacedim, Value>::MultiFieldFactory::is_active_field_descriptor(const Input::Record &in_rec, const std::string &input_name) {
	return in_rec.find<Input::Array>(input_name);
}



#endif /* MULTI_FIELD_IMPL_HH_ */
