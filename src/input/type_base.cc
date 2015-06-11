/*
 * input_type.cc
 *
 *  Created on: Mar 29, 2012
 *      Author: jb
 */



#include <limits>
#include <ios>
#include <map>
#include <vector>
#include <string>
#include <iomanip>

#include "system/system.hh"

#include <boost/type_traits.hpp>
#include <boost/tokenizer.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/make_shared.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/functional/hash.hpp>


#include "type_base.hh"
#include "type_record.hh"
#include "type_output.hh"
#include "type_repository.hh"
#include <boost/algorithm/string.hpp>


namespace Input {
namespace Type {

using namespace std;



/*******************************************************************
 * implementation of TypeBase
 */



TypeBase::TypeBase() {}



TypeBase::TypeBase(const TypeBase& other) {}



TypeBase::~TypeBase() {}


bool TypeBase::is_valid_identifier(const string& key) {
  namespace ba = boost::algorithm;
  return ba::all( key, ba::is_lower() || ba::is_digit() || ba::is_any_of("_") );
}


string TypeBase::desc() const {
    stringstream ss;
    ss << OutputText(this,1);
    return ss.str();
}



void TypeBase::lazy_finish() {
	Input::TypeRepository<Record>::getInstance().finish();
	Input::TypeRepository<AbstractRecord>::getInstance().finish();
	Input::TypeRepository<Selection>::getInstance().finish();

}



void TypeBase::add_attribute(std::string name, json_string val) {
	ASSERT(this->is_closed(), "Attribute can be add only to closed type: '%s'.\n", this->type_name().c_str());
	if (validate_json(val)) {
		attributes_[name] = val;
	} else {
		xprintf(PrgErr, "Invalid JSON format of attribute '%s'.\n", name.c_str());
	}
}


void TypeBase::print_json(ostream& stream) {
	stream << "{" << endl;
	for (std::map<std::string, json_string>::iterator it=attributes_.begin(); it!=attributes_.end(); ++it) {
        if (it != attributes_.begin()) {
        	stream << "," << endl;
        }
		stream << "\"" << it->first << "\" : " << it->second;
	}
	stream << endl << "}";
}


bool TypeBase::validate_json(json_string str) {
	boost::algorithm::trim(str);
	return ( (str[0] == '{' && str[str.length()-1] == '}')
			|| (str[0] == '[' && str[str.length()-1] == ']')
			|| (str[0] == '\"' && str[str.length()-1] == '\"') );
}


std::string TypeBase::format_hash( TypeBase::TypeHash hash) {
    stringstream ss;
    ss << std::hex << hash;
    return ss.str();
}


void TypeBase::add_basic_attributes() {
    this->add_attribute("id", "\"" + format_hash(this->content_hash()) + "\"");
    this->add_attribute("name", "\"" + this->type_name() + "\"");
    this->add_attribute("full_name", "\"" + this->full_type_name() + "\""); //obsolete
}


std::string TypeBase::escape_description(std::string desc) {
    return boost::regex_replace(desc, boost::regex("\\n"), "\\\\n");
}







std::ostream& operator<<(std::ostream& stream, const TypeBase& type) {
    return ( stream << OutputText(&type, 1) );
}



/**********************************************************************************
 * implementation of Type::Array
 */

TypeBase::TypeHash Array::content_hash() const
{
	TypeHash seed=0;
    boost::hash_combine(seed, type_name());
    boost::hash_combine(seed, data_->lower_bound_);
    boost::hash_combine(seed, data_->upper_bound_);
    boost::hash_combine(seed, data_->type_of_values_->content_hash() );
    return seed;
}


bool Array::finish() {
	return data_->finish();
}



bool Array::ArrayData::finish()
{
	if (finished) return true;

	return (finished = const_cast<TypeBase *>( type_of_values_.get() )->finish() );
}



string Array::type_name() const {
    return "array_of_" + data_->type_of_values_->type_name();
}

string Array::full_type_name() const {
	return "array_of_" + data_->type_of_values_->type_name();
}



bool Array::operator==(const TypeBase &other) const    {
    return  typeid(*this) == typeid(other) &&
              (*data_->type_of_values_ == static_cast<const Array *>(&other)->get_sub_type() );
}



bool Array::valid_default(const string &str) const {
    if ( this->match_size( 1 ) ) {
        return get_sub_type().valid_default( str );
    } else {
        THROW( ExcWrongDefault() << EI_DefaultStr( str ) << EI_TypeName(type_name()));
    }
}


/**********************************************************************************
 * implementation and explicit instantiation of Array constructor template
 */

template <class ValueType>
Array::Array(const ValueType &type, unsigned int min_size, unsigned int max_size)
: data_(boost::make_shared<ArrayData>(min_size, max_size))
{
    // ASSERT MESSAGE: The type of declared keys has to be a class derived from TypeBase.
    BOOST_STATIC_ASSERT( (boost::is_base_of<TypeBase, ValueType >::value) );
    ASSERT( min_size <= max_size, "Wrong limits for size of Input::Type::Array, min: %d, max: %d\n", min_size, max_size);
    ASSERT( type.is_closed(), "Sub-type '%s' of Input::Type::Array must be closed!", type.type_name().c_str());

	boost::shared_ptr<const TypeBase> type_copy = boost::make_shared<ValueType>(type);
	data_->type_of_values_ = type_copy;

	// Add attributes of array to map.
	add_attribute("input_type", "\"Array\"");
	add_attribute("id", "\"" + format_hash(this->content_hash()) + "\"");
	stringstream ss;
	ss << "[\"" << min_size << "\", \"" << max_size << "\"]";
	add_attribute("range", ss.str());
	add_attribute("subtype", "\"" + format_hash(data_->type_of_values_->content_hash()) + "\"");
}

// explicit instantiation

#define ARRAY_CONSTRUCT(TYPE) \
template Array::Array(const TYPE &type, unsigned int min_size, unsigned int max_size)

ARRAY_CONSTRUCT(String);
ARRAY_CONSTRUCT(Integer);
ARRAY_CONSTRUCT(Double);
ARRAY_CONSTRUCT(Bool);
ARRAY_CONSTRUCT(FileName);
ARRAY_CONSTRUCT(Selection);
ARRAY_CONSTRUCT(Array);
ARRAY_CONSTRUCT(Record);
ARRAY_CONSTRUCT(AbstractRecord);


/**********************************************************************************
 * implementation of Type::Scalar ... and descendants.
 */

string Scalar::full_type_name() const {
    return type_name();
}

/**********************************************************************************
 * implementation of Type::Bool
 */


Bool::Bool()
{
	// Add attributes of bool to map.
	add_attribute("input_type", "\"Bool\"");
	add_basic_attributes();
}


TypeBase::TypeHash Bool::content_hash() const
{
	TypeHash seed=0;
    boost::hash_combine(seed, type_name());
    return seed;
}


bool Bool::valid_default(const string &str) const {
    from_default(str);
    return true;
}



bool Bool::from_default(const string &str) const {
    if (str == "true" )  {
        return true;
    } else
    if (str == "false") {
        return false;
    } else {
        THROW( ExcWrongDefault() << EI_DefaultStr( str ) << EI_TypeName(type_name()));
    }
}


string Bool::type_name() const {
    return "Bool";
}


/**********************************************************************************
 * implementation of Type::Integer
 */

Integer::Integer(int lower_bound, int upper_bound)
: lower_bound_(lower_bound), upper_bound_(upper_bound)
{
	// Add attributes of integer to map.
	add_attribute("input_type", "\"Integer\"");
	add_basic_attributes();
	stringstream ss;
	ss << "[\"" << lower_bound_ << "\", \"" << upper_bound_ << "\"]";
	add_attribute("range", ss.str());
}



TypeBase::TypeHash Integer::content_hash() const
{
	TypeHash seed=0;
    boost::hash_combine(seed, type_name());
    boost::hash_combine(seed, lower_bound_);
    boost::hash_combine(seed, upper_bound_);
    return seed;
}



bool Integer::match(int value) const {
    return ( value >=lower_bound_ && value <= upper_bound_);
}



int Integer::from_default(const string &str) const {
    std::istringstream stream(str);
    int value;
    stream >> value;

    if (stream && stream.eof() && match(value)) {
        return value;
    } else {
        THROW( ExcWrongDefault() << EI_DefaultStr( str ) << EI_TypeName(type_name()));
    }
}



bool Integer::valid_default(const string &str) const
{
    from_default(str);
    return true;
}



string Integer::type_name() const {
    return "Integer";
}


/**********************************************************************************
 * implementation of Type::Double
 */


Double::Double(double lower_bound, double upper_bound)
: lower_bound_(lower_bound), upper_bound_(upper_bound)
{
	// Add attributes of double to map.
	add_attribute("input_type", "\"Double\"");
	add_basic_attributes();
	stringstream ss;
	ss << "[\"" << lower_bound_ << "\", \"" << upper_bound_ << "\"]";
	add_attribute("range", ss.str());
}


TypeBase::TypeHash Double::content_hash() const
{
	TypeHash seed=0;
    boost::hash_combine(seed, type_name());
    boost::hash_combine(seed, lower_bound_);
    boost::hash_combine(seed, upper_bound_);
    return seed;
}



bool Double::match(double value) const {
    return ( value >=lower_bound_ && value <= upper_bound_);
}



double Double::from_default(const string &str) const {
    std::istringstream stream(str);
    double value;
    stream >> value;

    if (stream && stream.eof() && match(value)) {
        return value;
    } else {
        THROW( ExcWrongDefault() << EI_DefaultStr( str ) << EI_TypeName(type_name()));
    }
}



bool Double::valid_default(const string &str) const
{
    from_default(str);
    return true;
}




string Double::type_name() const {
    return "Double";
}


/**********************************************************************************
 * implementation of Type::FileName
 */

FileName::FileName(enum ::FilePath::FileType type)
: type_(type)
{
	// Add attributes of filename to map.
	add_attribute("input_type", "\"FileName\"");
	add_basic_attributes();
	switch (type_) {
	case ::FilePath::input_file:
		add_attribute("file_mode", "\"input\"");
		break;
	case ::FilePath::output_file:
		add_attribute("file_mode", "\"output\"");
		break;
	}
}


TypeBase::TypeHash FileName::content_hash() const
{
	TypeHash seed=0;
    boost::hash_combine(seed, type_name());
    boost::hash_combine(seed, type_);
    return seed;
}





string FileName::type_name() const {
    switch (type_) {
    case ::FilePath::input_file:
        return "FileName_input";
    case ::FilePath::output_file:
        return "FileName_output";
    default:
        return "FileName";
    }
}



bool FileName::match(const string &str) const {
    return (type_ == ::FilePath::input_file) || (str[0] != DIR_DELIMITER); // output files can not be absolute
}


/**********************************************************************************
 * implementation of Type::String
 */


String::String()
{
	// Add attributes of string to map.
	add_attribute("input_type", "\"String\"");
	add_basic_attributes();
}


TypeBase::TypeHash String::content_hash() const
{
	TypeHash seed=0;
    boost::hash_combine(seed, type_name());
    return seed;
}



string String::type_name() const {
    return "String";
}




bool String::valid_default(const string &str) const {
    if (! match(str)) {
        THROW( ExcWrongDefault() << EI_DefaultStr( str ) << EI_TypeName(type_name()));
    }
    return true;
}



string String::from_default(const string &str) const {
    valid_default(str);
    return str;
}



bool String::match(const string &str) const {
    return true;
}



} // closing namespace Type
} // closing namespace Input



