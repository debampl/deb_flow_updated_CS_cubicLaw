/*!
 *
 * Copyright (C) 2007 Technical University of Liberec.  All rights reserved.
 *
 * Please make a following refer to Flow123d on your project site if you use the program for any purpose,
 * especially for academic research:
 * Flow123d, Research Centre: Advanced Remedial Technologies, Technical University of Liberec, Czech Republic
 *
 * This program is free software; you can redistribute it and/or modify it under the terms
 * of the GNU General Public License version 3 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with this program; if not,
 * write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 021110-1307, USA.
 *
 *
 * $Id: time_marks.hh 1974 2012-11-14 13:46:11Z pavel.exner $
 * $Revision: 1974 $
 * $LastChangedBy: pavel.exner $
 * $LastChangedDate: 2012-11-14 14:46:11 +0100 (St, 14 lis 2012) $
 *
 * @file
 * @brief
 *
 *  @author Jan Brezina
 *  Created on: Jun 15, 2011
 *
 */

#ifndef TIME_MARKS_HH_
#define TIME_MARKS_HH_

#include "system/global_defs.h"

/**
 * This class represents one record in the TimeMarks simple database.
 * Class members can not be modified after the item is created.
 */
class TimeMark {
public:

    /**
     *  MarkType is a bitmap where each bit represents one base type such as (strict, Output, Input, ...)
     *  This allow more complex queries through bitwise operations. Also one TimeMark can be shared by more events.
     *  In the context of TimeMarks the MarkType can be either strict or vague. If a TimeGovernor is connected to the TimeMarks object
     *  the  TimeMarks with strict MarkType are used to match exactly their times. Base MarkTypes should be obtained form TimeMarks class
     *  through the TimeMarks::new_mark_type method.
     */
    typedef unsigned long int Type;

    /// Mask that matchs every type of TimeMark.
    static const Type every_type;

    /**
     * Constructor for a TimeMarks::Mark
     * @param time - time of the mark
     * @param type - mark type
     *
     * <b> In order to create a strict TimeMark (at time=0.1) with base type output_type, use:
     * TimeMark( 0.1, output_type | TimeMark::strict)
     */
    TimeMark(double time, Type type) :
        time_(time), mark_type_(type) {}


    /// Getter for mark type.
    inline Type mark_type() const {
        return mark_type_;
    }

    /// Getter for the time of the TimeMark.
    inline double time() const {
        return time_;
    }

    /**
     * Returns true if TimeMark's type has 1 on all positions where mask has 1.
     * @param mask {Select bits that should be 1 for matching mark types.
     */

    inline bool match_mask(const TimeMark::Type &mask) const {
        return ( mask & (~mark_type_) ) == 0;
    }

    /// Add more bits that a mark satisfies.
    inline void add_to_type(const TimeMark::Type &type) {
        mark_type_ |= type;
    }

    /// Comparison of time marks according to their time.
    bool operator<(const TimeMark& second) const
      { return time_ < second.time(); }


private:
    double time_;
    Type mark_type_;
};

/**
 * Output operator for TimeMark class.
 */
ostream& operator<<(ostream& stream, const TimeMark &marks);




/***************************************************************************************/



/**
 * Iterator into the TimeMarks of particular mask. This is always const iterator, i.e. it points to const TimeMark.
 */
class TimeMarksIterator {
public:
    TimeMarksIterator(const vector<TimeMark> &marks,const  vector<TimeMark>::const_iterator &it, const TimeMark::Type &mask)
    : marks_(marks), it_(it), mask_(mask) {}

    TimeMarksIterator &operator=(const TimeMarksIterator &it)
    {ASSERT(&marks_ == &it.marks_, "Can not assign TimeMarks::iterator of different container.\n");
     it_=it.it_;
     mask_=it.mask_;
     return *this;
    }
    /// Prefix increment. Skip non-matching marks.
    TimeMarksIterator &operator++()
    { while ( it_ != marks_.begin() && ! (++it_) -> match_mask(mask_) ); return (*this); }

    /// Prefix decrement. Skip non-matching marks.
    TimeMarksIterator &operator--()
    { while ( it_ != marks_.end() && ! (--it_) -> match_mask(mask_) ); return (*this); }


    ///  * dereference operator
    inline const TimeMark & operator *() const
            { return *it_; }

    /// -> dereference operator
    inline const TimeMark * operator ->() const
            { return &(*(it_)); }

    inline bool operator ==(const TimeMarksIterator &other) const
        {return it_ == other.it_; }

    inline bool operator !=(const TimeMarksIterator &other) const
            {return it_ != other.it_; }

    TimeMark::Type mask()
    { return mask_; }
private:
    const vector<TimeMark> &marks_;
    vector<TimeMark>::const_iterator it_;
    TimeMark::Type mask_;
};


/***************************************************************************************/
class TimeGovernor;


/**
 * @brief This class is a collection of time marks to manage various events occurring during simulation time.
 *
 * <b> TimeMark and their types </b>
 * One TimeMark consists of time and type (TimeMark::Type) see the constructor TimeMark::TimeMark.
 * The type of mark is bitmap where individual bits corresponds to some base event types like changing a BC, output solution, coupling time with another
 * equation and so on. Base types can be combined by bitwise or (operator|).
 *
 * There is one particular base mark type TimeMark::strict.
 * Only marks with this type are considered as fixed times by a TimeGovernor which is connected to particular TimeMarks object.
 *
 * <b> TimeMarks collection</b>
 * TimeMarks collect marks of various types and provides methods for iterating over stored marks. You can selectively access only marks matching given
 * type mask. See TimeMark::match_mask.
 *
 * You can add one new mark through method add or add evenly spaced marks of same type by TimeMarks::add_time_marks.
 *
 * You can allocate new TimeMark::Type in the context of one TimeMarks object by TimeMarks::new_mark_type and TimeMarks::new_strict_mark_type.
 *
 * For a given TimeGovernor (not necessarily connected one) you can ask about existence of mark in current time interval (TimeMarks::is_current) and iterate
 * around current time (TimeMarks::next and TimeMarks::last).
 *
 * In most cases there will be only one TimeMarks object for the whole solved problem and used by TimeGovernors of individual equations. However
 * this is not necessary.
 */
class TimeMarks {

public:
    /// Iterator class for iteration over time marks of particular type. This is always const_iterator.
    typedef TimeMarksIterator iterator;

    /**
     * Default constructor.
     */
    TimeMarks();

    /**
     * Add a new base mark within the context of the particular TimeMarks instance.
     * User should keep the returned value (MarkType is basically a bitmap) for further queries and
     * TimeMark insertions. ATTENTION: You can not use the TimeMark::Type with other TimeMarks instance!
     */
    TimeMark::Type new_mark_type();

    /// Predefined base TimeMark type that is taken into account by the TimeGovernor.
    inline TimeMark::Type type_fixed_time()
    { return type_fixed_time_;}

    /// Predefined base TimeMark type for output times.
    inline TimeMark::Type type_output()
    { return type_output_;}

    /// Predefined base TimeMark type for times when the boundary condition is changed.
    inline TimeMark::Type type_bc_change()
    { return type_bc_change_;}


    /**
     * Basic method for inserting TimeMarks.
     * @param time    Time of the TimeMark.
     * @param type    MarkType or their combinations.
     */
    void add(const TimeMark &mark);

    /**
     * Method for creating and inserting equally spaced TimeMarks.
     * @param time    Time of the first TimeMark.
     * @param dt      Interval for further TimeMarks.
     * @param end_time  No marks after the end_time.
     * @param type    MarkType or their combinations.
     *
     * Current lazy implementation have complexity O(m*n) where m is number of inserted time marks and n number of time marks in the array.
     * TODO: O(n+m) implementation
     */
    void add_time_marks(double time, double dt, double end_time, TimeMark::Type type);

    /**
     * Find the last time mark matching given mask, and returns true if it is in the time interval of
     * current time step.
     */
    bool is_current(const TimeGovernor &tg, const TimeMark::Type &mask) const;

    /**
     * Return the first TimeMark with time strictly greater then tg.time() that match the mask.
     * The time governor tg  is used also for time comparisons.
     *
     * @param tg    the time governor
     * @param mask  mask of marks to iterate on
     *
     * TODO: have also method which accepts double (time) instead of the whole TimeGovernor.
     * and compare without safety.
     */
    TimeMarks::iterator next(const TimeGovernor &tg, const TimeMark::Type &mask) const;

    /**
     * Return the last TimeMark with time less or equal to tg.time() that match the mask.
     * The time governor tg  is used also for time comparisons.
     * @param tg    the time governor
     * @param mask  mask of marks to iterate on
     */
    TimeMarks::iterator last(const TimeGovernor &tg, const TimeMark::Type &mask) const;

    /// Iterator for the begging mimics container-like  of TimeMarks
    TimeMarks::iterator begin() const
    {return TimeMarksIterator(marks_, marks_.begin(), TimeMark::every_type); }

    /// Iterator for the end mimics container-like  of TimeMarks
    TimeMarks::iterator end() const
        {return TimeMarksIterator(marks_, --marks_.end(), TimeMark::every_type); }

    /// Friend output operator.
    friend ostream& operator<<(ostream& stream, const TimeMarks &marks);

private:

    /// MarkType that will be used at next new_time_mark() call.
    TimeMark::Type next_mark_type_;

    /// TimeMarks list sorted according to the their time.
    vector<TimeMark> marks_;

    /// Predefined types.
    TimeMark::Type type_fixed_time_;
    TimeMark::Type type_output_;
    TimeMark::Type type_bc_change_;
};

/**
 * Output operator for TimeMarks database.
 */
ostream& operator<<(ostream& stream, const  TimeMarks &marks);


#endif /* TIME_MARKS_HH_ */