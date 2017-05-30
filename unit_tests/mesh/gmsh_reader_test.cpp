/*
 * gmsh_reader_test.cpp
 *
 *  Created on: Oct 2, 2012
 *      Author: jb
 */

#define FEAL_OVERRIDE_ASSERTS

#include <flow_gtest.hh>
#include <sstream>
#include <string>
#include <mesh_constructor.hh>

#include "system/sys_profiler.hh"

#include "mesh/mesh.h"
#include "io/msh_gmshreader.h"



TEST(GMSHReader, read_mesh_from_stream) {
    Profiler::initialize();
    
    string fname = string(UNIT_TESTS_SRC_DIR) + "/mesh/simplest_cube.msh";
    ifstream ifs( fname.c_str() );
    if (! ifs) cout << "Can not open file!" << endl;
    stringstream ss;
    ss << ifs.rdbuf();

    Mesh * mesh = mesh_constructor();
    GmshMeshReader reader(ss);
	reader.read_physical_names(mesh);
	reader.read_raw_mesh(mesh);

    EXPECT_EQ(9, mesh->n_elements());

    delete mesh;
}


TEST(GMSHReader, read_mesh_from_file) {
    Profiler::initialize();

    // has to introduce some flag for passing absolute path to 'test_units' in source tree
    FilePath::set_io_dirs(".",UNIT_TESTS_SRC_DIR,"",".");
    FilePath mesh_file("mesh/test_input.msh", FilePath::input_file);

    Mesh * mesh = mesh_constructor("{mesh_file=\"mesh/test_input.msh\"}");
    GmshMeshReader reader( mesh->mesh_file() );
	reader.read_physical_names(mesh);
	reader.read_raw_mesh(mesh);

    EXPECT_EQ(118, mesh->n_nodes());
    EXPECT_EQ(216, mesh->n_elements());

    delete mesh;
}
