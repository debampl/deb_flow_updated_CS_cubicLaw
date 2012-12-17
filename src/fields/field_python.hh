/*
 * field_python.hh
 *
 *  Created on: Aug 30, 2012
 *      Author: jb
 */
// TODO: make FieldPython dummy class if we do not have python, so that we
// need not optional code elsewhere

#include "fields/field_all.hh"

#ifndef FIELD_PYTHON_HH_
#define FIELD_PYTHON_HH_


#include "system/system.hh"
#include "system/python_loader.hh"
#include "fields/field_base.hh"
#include "mesh/point.hh"

#include <string>
using namespace std;

/**
 *
 * This class assumes field python field with @p spacedim arguments containing coordinates of the given point.
 * The field should return  a tuple representing a vector value (possibly of size one for scalar fields)
 *
 * TODO:
 * - use rather only one argument - tuple representing the whole point
 * - time fields
 * - set some parameter in the python module
 * - for fields with one component allow python fields returning the value directly
 *
 */
template <int spacedim, class Value>
class FieldPython : public FieldBase<spacedim, Value>
{
public:

    FieldPython(const double init_time=0.0, unsigned int n_comp=0);

    static Input::Type::Record &get_input_type();

    virtual void init_from_input( Input::Record rec);

    /**
     * Set the file and field to be called.
     * TODO: use FilePath
     */
    void set_python_field_from_file( const FilePath &file_name, const string &func_name);

    /**
     * Set the source in a string and name of the field to be called.
     */
    void set_python_field_from_string( const string &python_source, const string &func_name);

    using FieldBase<spacedim,Value>::value;

    /**
     * Returns one value in one given point. ResultType can be used to avoid some costly calculation if the result is trivial.
     */
    virtual FieldResult value(const Point<spacedim> &p, ElementAccessor<spacedim> &elm, typename Value::return_type &value);

    /**
     * Returns std::vector of scalar values in several points at once.
     */
//    virtual void value_list (const std::vector< Point<spacedim> >  &point_list, ElementAccessor<spacedim> &elm,
//                       std::vector<Value>  &value_list,
//                       std::vector<FieldResult> &result_list);


    virtual ~FieldPython();

private:
    /**
     * Common part of set_python_field_from_* methods
     */
    void set_func(const string &func_name);

#ifdef HAVE_PYTHON
    PyObject *p_func_;
    PyObject *p_module_;
    mutable PyObject *p_args_;
    mutable PyObject *p_value_;
#endif // HAVE_PYTHON

};



#endif /* FUNCTION_PYTHON_HH_ */