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
 * @file    msh_vtkreader.cc
 * @brief
 * @author  dalibor
 */


#include <iostream>
#include <vector>
#include "boost/lexical_cast.hpp"

#include "msh_vtkreader.hh"
#include "msh_gmshreader.h" // TODO move exception to base class and remove
#include "system/tokenizer.hh"
#include "system/system.hh"

#include "config.h"
#include <zlib.h>


/*******************************************************************
 * Helper methods
 */
template<typename T>
T read_binary_value(std::istream &data_stream)
{
	T val;
	data_stream.read(reinterpret_cast<char *>(&val), sizeof(val));
	return val;
}


uint64_t read_header_type(VtkMeshReader::DataType data_header_type, std::istream &data_stream)
{
	if (data_header_type == VtkMeshReader::DataType::uint64)
		return read_binary_value<uint64_t>(data_stream);
	else if (data_header_type == VtkMeshReader::DataType::uint32)
		return (uint64_t)read_binary_value<unsigned int>(data_stream);
	else {
		ASSERT(false).error("Unsupported header_type!\n");
		return 0;
	}
}


/*******************************************************************
 * implementation of VtkMeshReader
 */
VtkMeshReader::VtkMeshReader(const FilePath &file_name)
: BaseMeshReader()
{
	current_cache_ = new ElementDataCacheBase();
	f_name_ = (std::string)file_name;
	parse_result_ = doc_.load_file( f_name_.c_str() );
	read_base_vtk_attributes();
	make_header_table();
}



VtkMeshReader::~VtkMeshReader()
{
	delete data_stream_;
}



void VtkMeshReader::read_base_vtk_attributes()
{
	pugi::xml_node node = doc_.child("VTKFile");
	// header type of appended data
	header_type_ = this->get_data_type( node.attribute("header_type").as_string() );
	// data format
	if (header_type_ == DataType::undefined) {
		data_format_ = DataFormat::ascii;
	} else {
		std::string compressor = node.attribute("compressor").as_string();
		if (compressor == "vtkZLibDataCompressor")
			data_format_ = DataFormat::binary_zlib;
		else
			data_format_ = DataFormat::binary_uncompressed;
	}
	// size of node and element vectors
	pugi::xml_node piece_node = node.child("UnstructuredGrid").child("Piece");
	n_nodes_ = piece_node.attribute("NumberOfPoints").as_uint();
	n_elements_ = piece_node.attribute("NumberOfCells").as_uint();
}



unsigned int VtkMeshReader::get_appended_position() {
	unsigned int appended_pos=0;

	{
		// find line by tokenizer
		Tokenizer tok( FilePath(f_name_, FilePath::input_file) );
		if (! tok.skip_to("AppendedData"))
			THROW(GmshMeshReader::ExcMissingSection() << GmshMeshReader::EI_Section("AppendedData") << GmshMeshReader::EI_GMSHFile(f_name_) );
		else {
			appended_pos = tok.get_position().file_position_;
		}
	}

	// find exact position of appended data (starts with '_')
	char c;
	data_stream_->seekg(appended_pos);
	do {
		data_stream_->get(c);
	} while (c!='_');
	appended_pos = data_stream_->tellg();
	delete data_stream_; // close stream

	// reopen stream in binary mode
	data_stream_ = new std::ifstream( f_name_, std::ios_base::in | std::ios_base::binary );

	return appended_pos;
}


VtkMeshReader::DataArrayAttributes VtkMeshReader::create_header(pugi::xml_node node, unsigned int appended_pos)
{
    DataArrayAttributes attributes;
    attributes.field_name = node.attribute("Name").as_string();
    attributes.type_ = this->get_data_type( node.attribute("type").as_string() );
    attributes.n_components = node.attribute("NumberOfComponents").as_uint(1);
    std::string format = node.attribute("format").as_string();
    if (format=="appended") {
        ASSERT(data_format_ != DataFormat::ascii)(attributes.field_name).error("Invalid format of DataArray!");
        attributes.offset_ = node.attribute("offset").as_uint() + appended_pos;
    } else if (format=="ascii") {
    	ASSERT(data_format_ == DataFormat::ascii)(attributes.field_name).error("Invalid format of DataArray!");

    	Tokenizer tok( FilePath(f_name_, FilePath::input_file) );
    	bool is_point = (attributes.field_name=="");
    	std::string found_str = (is_point) ? "<Points>" : "Name=\"" + attributes.field_name + "\"";
		if (! tok.skip_to(found_str))
			THROW(GmshMeshReader::ExcMissingSection() << GmshMeshReader::EI_Section(attributes.field_name) << GmshMeshReader::EI_GMSHFile(f_name_) );
		else {
			if (is_point) tok.skip_to("DataArray");
			attributes.offset_ = tok.get_position().file_position_;
		}
    } else {
        ASSERT(false).error("Unsupported or missing VTK format.");
    }

    return attributes;
}


void VtkMeshReader::make_header_table()
{
	// open ifstream for find position
	data_stream_ = new std::ifstream( f_name_);

	// data of appended tag
	unsigned int appended_pos;
	if (header_type_==DataType::undefined) { // no AppendedData tag
		appended_pos = 0;
	} else {
		appended_pos = get_appended_position();
	}

	pugi::xml_node node = doc_.child("VTKFile").child("UnstructuredGrid").child("Piece");

	header_table_.clear();

	// create headers of Points and Cells DataArrays
	header_table_["Points"] = create_header( node.child("Points").child("DataArray"), appended_pos );
	header_table_["connectivity"] = create_header( node.child("Cells").find_child_by_attribute("DataArray", "Name", "connectivity"), appended_pos );
	header_table_["offsets"] = create_header( node.child("Cells").find_child_by_attribute("DataArray", "Name", "offsets"), appended_pos );
	header_table_["types"] = create_header( node.child("Cells").find_child_by_attribute("DataArray", "Name", "types"), appended_pos );

	node = node.child("CellData");
	for (pugi::xml_node subnode = node.child("DataArray"); subnode; subnode = subnode.next_sibling("DataArray")) {
		auto header = create_header( subnode, appended_pos );
		header_table_[header.field_name] = header;
	}
}


VtkMeshReader::DataArrayAttributes VtkMeshReader::find_header(double time, std::string field_name)
{
	HeaderTable::iterator table_it = header_table_.find(field_name);

	if (table_it == header_table_.end()) {
		// no data found
        THROW( GmshMeshReader::ExcFieldNameNotFound() << GmshMeshReader::EI_FieldName(field_name) << GmshMeshReader::EI_GMSHFile(f_name_));
	}

	return table_it->second;
}



VtkMeshReader::DataType VtkMeshReader::get_data_type(std::string type_str) {
    // names of types in DataArray section
	static const std::map<std::string, DataType> types = {
			{"Int8",    DataType::int8},
			{"UInt8",   DataType::uint8},
			{"Int16",   DataType::int16},
			{"UInt16",  DataType::uint16},
			{"Int32",   DataType::int32},
			{"UInt32",  DataType::uint32},
			{"Int64",   DataType::int64},
			{"UInt64",  DataType::uint64},
			{"Float32", DataType::float32},
			{"Float64", DataType::float64},
			{"",        DataType::undefined}
	};

	std::map<std::string, DataType>::const_iterator it = types.find(type_str);
	if (it != types.end()) {
	    return it->second;
    } else {
        ASSERT(false).error("Unsupported VTK data type.");
        return DataType::uint32;
    }

}



unsigned int VtkMeshReader::type_value_size(VtkMeshReader::DataType data_type)
{
	static const std::vector<unsigned int> sizes = { 1, 1, 2, 2, 4, 4, 8, 8, 4, 8, 0 };

	return sizes[data_type];
}



template<typename T>
typename ElementDataCache<T>::ComponentDataPtr VtkMeshReader::get_element_data( std::string field_name, double time,
		unsigned int n_entities, unsigned int n_components, bool &actual, std::vector<int> const & el_ids, unsigned int component_idx)
{
	DataArrayAttributes data_attr = this->find_header(time, field_name);
	if ( !current_cache_->is_actual(time, field_name) ) {
    	unsigned int size_of_cache; // count of vectors stored in cache

	    // check that the header is valid, try to correct
	    if (this->n_elements_ != n_entities) {
	    	WarningOut().fmt("In file '{}', '$ElementData' section for field '{}'.\nWrong number of entities: {}, using {} instead.\n",
	                f_name_, field_name, this->n_elements_, n_entities);
	        // actual_header.n_entities=n_entities;
	    }

	    if (n_components == 1) {
	    	// read for MultiField to 'n_comp' vectors
	    	// or for Field if ElementData contains only one value
	    	size_of_cache = data_attr.n_components;
	    }
	    else {
	    	// read for Field if more values is stored to one vector
	    	size_of_cache = 1;
	    	if (data_attr.n_components != n_components) {
	    		WarningOut().fmt("In file '{}', '$ElementData' section for field '{}'.\nWrong number of components: {}, using {} instead.\n",
	    				f_name_, field_name, data_attr.n_components, n_components);
	    		data_attr.n_components=n_components;
	    	}
	    }

	    // create vector of shared_ptr for cache
	    typename ElementDataCache<T>::CacheData data_cache;
		switch (data_format_) {
			case DataFormat::ascii: {
				data_cache = parse_ascii_data<T>( size_of_cache, n_components, this->n_elements_, data_attr.offset_ );
				break;
			}
			case DataFormat::binary_uncompressed: {
				ASSERT_PTR(data_stream_).error();
				data_cache = parse_binary_data<T>( size_of_cache, n_components, this->n_elements_, data_attr.offset_,
						data_attr.type_ );
				break;
			}
			case DataFormat::binary_zlib: {
				ASSERT_PTR(data_stream_).error();
				data_cache = parse_compressed_data<T>( size_of_cache, n_components, this->n_elements_, data_attr.offset_,
						data_attr.type_);
				break;
			}
			default: {
				ASSERT(false).error(); // should not happen
				break;
			}
		}

	    LogOut().fmt("time: {}; {} entities of field {} read.\n",
	    		time, n_read_, field_name);

	    actual = true; // use input header to indicate modification of @p data buffer

	    // set new cache
	    delete current_cache_;
	    current_cache_ = new ElementDataCache<T>(time, field_name, data_cache);
	}

	if (component_idx == std::numeric_limits<unsigned int>::max()) component_idx = 0;
	return static_cast< ElementDataCache<T> *>(current_cache_)->get_component_data(component_idx);
}


template<typename T>
typename ElementDataCache<T>::CacheData VtkMeshReader::parse_ascii_data(unsigned int size_of_cache, unsigned int n_components,
		unsigned int n_entities, unsigned int data_pos)
{
    unsigned int idx, i_row;
    n_read_ = 0;

    typename ElementDataCache<T>::CacheData data_cache = ElementDataCache<T>::create_data_cache(size_of_cache, n_components*n_entities);

	Tokenizer tok( FilePath(f_name_, FilePath::input_file ) );
	tok.set_position( Tokenizer::Position(data_pos, 0, 0) );
	tok.next_line();
	for (i_row = 0; i_row < n_entities; ++i_row) {
    	for (unsigned int i_vec=0; i_vec<size_of_cache; ++i_vec) {
    		idx = i_row * n_components;
    		std::vector<T> &vec = *( data_cache[i_vec].get() );
    		for (unsigned int i_col=0; i_col < n_components; ++i_col, ++idx) {
    			vec[idx] = boost::lexical_cast<T>(*tok);
    			++tok;
    		}
    	}
        n_read_++;
	}

	return data_cache;
}


template<typename T>
typename ElementDataCache<T>::CacheData VtkMeshReader::parse_binary_data(unsigned int size_of_cache, unsigned int n_components,
		unsigned int n_entities, unsigned int data_pos, VtkMeshReader::DataType value_type)
{
    unsigned int idx, i_row;
    n_read_ = 0;

    typename ElementDataCache<T>::CacheData data_cache = ElementDataCache<T>::create_data_cache(size_of_cache, n_components*n_entities);
    data_stream_->seekg(data_pos);
	uint64_t data_size = read_header_type(header_type_, *data_stream_) / type_value_size(value_type);
	ASSERT_EQ(size_of_cache*n_components*n_entities, data_size).error();

	for (i_row = 0; i_row < n_entities; ++i_row) {
    	for (unsigned int i_vec=0; i_vec<size_of_cache; ++i_vec) {
    		idx = i_row * n_components;
    		std::vector<T> &vec = *( data_cache[i_vec].get() );
    		for (unsigned int i_col=0; i_col < n_components; ++i_col, ++idx) {
    			vec[idx] = read_binary_value<T>(*data_stream_);
    		}
    	}
        n_read_++;
	}

	return data_cache;
}


template<typename T>
typename ElementDataCache<T>::CacheData VtkMeshReader::parse_compressed_data(unsigned int size_of_cache, unsigned int n_components,
		unsigned int n_entities, unsigned int data_pos, VtkMeshReader::DataType value_type)
{
	data_stream_->seekg(data_pos);
	uint64_t n_blocks = read_header_type(header_type_, *data_stream_);
	uint64_t u_size = read_header_type(header_type_, *data_stream_);
	uint64_t p_size = read_header_type(header_type_, *data_stream_);

	std::vector<uint64_t> block_sizes;
	block_sizes.reserve(n_blocks);
	for (uint64_t i = 0; i < n_blocks; ++i) {
		block_sizes.push_back( read_header_type(header_type_, *data_stream_) );
	}

	stringstream decompressed_data;
	uint64_t decompressed_data_size = 0;
	for (uint64_t i = 0; i < n_blocks; ++i) {
		uint64_t decompressed_block_size = (i==n_blocks-1 && p_size>0) ? p_size : u_size;
		uint64_t compressed_block_size = block_sizes[i];

		std::vector<char> data_block(compressed_block_size);
		data_stream_->read(&data_block[0], compressed_block_size);

		std::vector<char> buffer(decompressed_block_size);

		// set zlib object
		z_stream strm;
		strm.zalloc = 0;
		strm.zfree = 0;
		strm.next_in = (Bytef *)(&data_block[0]);
		strm.avail_in = compressed_block_size;
		strm.next_out = (Bytef *)(&buffer[0]);
		strm.avail_out = decompressed_block_size;

		// decompression of data
		inflateInit(&strm);
		inflate(&strm, Z_NO_FLUSH);
		inflateEnd(&strm);

		// store decompressed data to stream
		decompressed_data << std::string(buffer.begin(), buffer.end());
		decompressed_data_size += decompressed_block_size;
	}

    unsigned int idx, i_row;
    n_read_ = 0;

    typename ElementDataCache<T>::CacheData data_cache = ElementDataCache<T>::create_data_cache(size_of_cache, n_components*n_entities);
	uint64_t data_size = decompressed_data_size / type_value_size(value_type);
	ASSERT_EQ(size_of_cache*n_components*n_entities, data_size).error();

	for (i_row = 0; i_row < n_entities; ++i_row) {
    	for (unsigned int i_vec=0; i_vec<size_of_cache; ++i_vec) {
    		idx = i_row * n_components;
    		std::vector<T> &vec = *( data_cache[i_vec].get() );
    		for (unsigned int i_col=0; i_col < n_components; ++i_col, ++idx) {
    			vec[idx] = read_binary_value<T>(decompressed_data);
    		}
    	}
        n_read_++;
	}

	return data_cache;
}



// explicit instantiation of template methods
#define VTK_READER_GET_ELEMENT_DATA(TYPE) \
template typename ElementDataCache<TYPE>::ComponentDataPtr VtkMeshReader::get_element_data<TYPE>(std::string field_name, double time, \
	unsigned int n_entities, unsigned int n_components, bool &actual, std::vector<int> const & el_ids, unsigned int component_idx); \
template typename ElementDataCache<TYPE>::CacheData VtkMeshReader::parse_ascii_data<TYPE>(unsigned int size_of_cache, \
	unsigned int n_components, unsigned int n_entities, unsigned int data_pos); \
template typename ElementDataCache<TYPE>::CacheData VtkMeshReader::parse_binary_data<TYPE>(unsigned int size_of_cache, \
	unsigned int n_components, unsigned int n_entities, unsigned int data_pos, VtkMeshReader::DataType value_type); \
template typename ElementDataCache<TYPE>::CacheData VtkMeshReader::parse_compressed_data<TYPE>(unsigned int size_of_cache, \
	unsigned int n_components, unsigned int n_entities, unsigned int data_pos, VtkMeshReader::DataType value_type); \
template TYPE read_binary_value<TYPE>(std::istream &data_stream)

VTK_READER_GET_ELEMENT_DATA(int);
VTK_READER_GET_ELEMENT_DATA(unsigned int);
VTK_READER_GET_ELEMENT_DATA(double);

template uint64_t read_binary_value<uint64_t>(std::istream &data_stream);
