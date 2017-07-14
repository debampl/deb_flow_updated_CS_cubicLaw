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
 * @file    reader_instances.hh
 * @brief   
 */

#ifndef READER_INSTANCES_HH_
#define READER_INSTANCES_HH_


#include "io/msh_basereader.hh"
#include "mesh/mesh.h"
#include "system/file_path.hh"

#include <memory>


class GmshMeshReader;


/**
 * Auxiliary class to map filepaths to instances of readers.
 */
class ReaderInstance {
public:
	struct ReaderData {
		std::shared_ptr<GmshMeshReader> reader_;
		std::shared_ptr<Mesh> mesh_;
	};

	typedef std::map< string, ReaderData > ReaderTable;

	/**
	 * Returns reader of given FilePath.
	 */
	static std::shared_ptr<GmshMeshReader> get_reader(const FilePath &file_path);

	/**
	 * Returns mesh of given FilePath.
	 */
	static std::shared_ptr<Mesh> get_mesh(const FilePath &file_path);

private:
	/// Returns singleton instance
	static ReaderInstance * instance();

	/// Constructor
	ReaderInstance() {};

	/// Returns instance of given FilePath. If reader doesn't exist, creates new ReaderData object.
	static ReaderData get_instance(const FilePath &file_path);

	/// Table of readers
	ReaderTable reader_table_;
};


class ReaderInstances {
public:
	typedef std::map< string, std::shared_ptr<BaseMeshReader> > ReaderTable;

	/// Returns singleton instance
	static ReaderInstances * instance();

	/**
	 * Returns mesh reader of get filepath. If reader doesn't exist, creates its.
	 */
	std::shared_ptr<BaseMeshReader> get_reader(const FilePath &file_name);

private:
	/// Constructor
	ReaderInstances() {};

	/// Table of readers
	ReaderTable reader_table_;
};


#endif /* READER_INSTANCES_HH_ */
