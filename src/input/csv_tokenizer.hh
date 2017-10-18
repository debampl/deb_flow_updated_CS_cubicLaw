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
 * @file    csv_tokenizer.hh
 * @brief
 */

#ifndef CSV_TOKENIZER_HH_
#define CSV_TOKENIZER_HH_

#include <system/tokenizer.hh>

/**
 * @brief Simple class for parsing CSV files.
 *
 * CSV tokenizer use backslash '\\' as the escape character, double quotas '"' as quotation
 * character, and comma ',' as the separator of tokens.
 */
class CSVTokenizer : public Tokenizer {
public:
    /**
     * Opens a file given by file path @p fp. And construct the CSV tokenizer over the
     * input stream for this file.
     */
	CSVTokenizer(const FilePath &fp);

	/**
     * @brief Construct the CSV tokenizer over given input stream @p in.
     *
     * @code CSVTokenizer( ifstream("my_file") );
     *
     */
	CSVTokenizer(std::istream &in);

	/**
	 * @brief Get count of active lines (without head lines) in CSV file.
	 *
	 * Return count of active lines and set position of tokenizer to first active lines.
	 *
	 * @param n_head_lines number of head lines given by user
	 */
	unsigned int get_n_lines(unsigned int n_head_lines);

	/// Cast token on actual position to integer value and return its.
	int get_int_val();

	/// Cast token on actual position to double value and return its.
	double get_double_val();

	/// Return string value of token on actual position.
	std::string get_string_val();
};

#endif /* CSV_TOKENIZER_HH_ */
