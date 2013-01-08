/*
 * field_base.hh
 *
 *  Created on: Aug 31, 2012
 *      Author: jb
 */

/**
 * TODO:
 * - better tests:
 *   - common set of quantities with different kind of values (scalar, vector, tensor, discrete, ..),
 *     common points and elements for evaluation
 *   - for individual Field implementations have:
 *     - different input
 *     - possibly different EPETCT_EQ tests, but rather have majority common
 *
 *
 */

#include <fields/field_all.hh>

#ifndef FIELD_BASE_HH_
#define FIELD_BASE_HH_

#include <string>

#include "input/input_type.hh"
#include "input/accessors.hh"

#include "mesh/accessors.hh"
#include "mesh/point.hh"

#include "fields/field_values.hh"


/// Result type have sense only for larger Value types like vectors and tensors.
typedef enum  {
    result_zero,    // zero scalar, vector, or tensor
    result_one,     // unit scalar (1.0), identity tensor
    result_other
} FieldResult;



/**
 * Base class for space-time function classes.
 */
template <int spacedim, class Value>
class FieldBase {
public:
       // expose template parameters
       typedef Value ValueType;
       static const unsigned int spacedim_=spacedim;



       /**
        * Kind of default constructor , with possible setting of the initial time.
        * Fields that returns variable size vectors accepts number of components @p n_comp.
        */
       FieldBase(const double init_time=0.0, unsigned int n_comp=0);

       /**
        * Returns template parameters as string in order to distinguish Fields Input::Type name.
        */
       static std::string template_name();

       /**
        * This static method gets accessor to abstract record with function input,
        * dispatch to correct constructor and initialize appropriate function object from the input.
        * Returns pointer to  FunctionBase<>.
        */
       static FieldBase<spacedim, Value> *function_factory(Input::AbstractRecord rec,
               double init_time=0.0, unsigned int n_comp=0);

       /**
        *  Function can provide way to initialize itself from the input data.
        */
       virtual void init_from_input(Input::Record rec);

       /**
        * Set new time value. Some Fields may and some may not implement time dependent values and
        * possibly various types of interpolation. There can not be unified approach to interpolation (at least not on this abstraction level)
        * since some fields (FieldFormula, FieldPython) provides naturally time dependent functions other fields like (FieldConstant, ...), however,
        * can be equipped by various time interpolation schemes. In future, we obviously need interpolation of variable order so that
        * we can use ODE integrators of higher order.
        */
       virtual void set_time(double time);

       /**
        * Special field values spatially constant. Could allow optimization of tensor multiplication and
        * tensor or vector addition. field_result_ should be set in constructor and in set_time method of particular Field implementation.
        */
        FieldResult field_result() const
        { return field_result_;}

       /**
        * Method for getting some information about next time where the function change its character.
        * Used to add appropriate TimeMarks.
        * TODO: think what kind of information we may need, is the next time value enough?
        */
       virtual double next_change_time()
       { ASSERT(0, "Not implemented yet."); }

       /**
        * Returns one value in one given point @p on an element given by ElementAccessor @p elm.
        * It returns reference to he actual value in order to avoid temporaries for vector and tensor values.
        *
        * This method just call the later one @p value(Point, ElementAccessor, Value) and drops the FieldResult.
        *
        * Usual implementation of this method fills @p member r_value_ through unified envelope @p value_ as general tensor
        * and then returns member @p r_value_. However, some particular Fields may have result stored elsewhere, in such a case
        * the reference to the result can be returned directly without using the member @p value_. Keeping such wide space for optimization
        * has drawback in slow generic implementation of the @p value_list method that fills whole vector of values for vector of points.
        * Its generic implementation has to copy all values instead of directly store them into the vector of result values.
        *
        * So the best practice when implementing @p value and @value_list methods in particular FieldBase descendant is
        * implement some thing like value(point, elm, Value::return_type &value) and using
        *  s having in part
        *
        */
       virtual typename Value::return_type &value(const Point<spacedim> &p, ElementAccessor<spacedim> &elm)=0;

       /**
        * Pure virtual method. At least this has to be implemented by descendants.
        * Returns one value in one given point. ResultType can be used to avoid some costly calculation if the result is trivial.
        */
       //virtual FieldResult value(const Point<spacedim> &p, ElementAccessor<spacedim> &elm, typename Value::return_type &value) =0;

       /**
        * Returns std::vector of scalar values in several points at once. The base class implements
        * trivial implementation using the @p value(,,) method. This is not optimal as it involves lot of virtual calls,
        * but this overhead can be negligible for more complex fields as Python of Formula.
        */
       virtual void value_list(const std::vector< Point<spacedim> >  &point_list, ElementAccessor<spacedim> &elm,
                          std::vector<typename Value::return_type>  &value_list)=0;


       /**
        * Declaration of input type.
        */
       static Input::Type::AbstractRecord input_type;


protected:
       /// Actual time level
       double time_;
       /// Last value, prevents passing large values (vectors) by value.
       Value value_;
       typename Value::return_type r_value_;
       /// Indicator of particular values (zero, one) constant over space.
       FieldResult field_result_;
};




#endif /* FUNCTION_BASE_HH_ */
