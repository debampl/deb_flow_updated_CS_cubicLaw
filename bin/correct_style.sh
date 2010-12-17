#!/bin/bash
# 
# Copyright (C) 2007 Technical University of Liberec.  All rights reserved.
#
# Please make a following refer to Flow123d on your project site if you use the program for any purpose,
# especially for academic research:
# Flow123d, Research Centre: Advanced Remedial Technologies, Technical University of Liberec, Czech Republic
#
# This program is free software; you can redistribute it and/or modify it under the terms
# of the GNU General Public License version 3 as published by the Free Software Foundation.
# 
# This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; 
# without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See the GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License along with this program; if not,
# write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 021110-1307, USA.
#
# $Id$
# $Revision$
# $LastChangedBy$
# $LastChangedDate$
#

FILE=${1:-*.c *.cpp *.h *.hpp *.cc *.hh}
ASO=
# K/R style
ASO="$ASO --style=kr"
# convert tabs to spaces
ASO="$ASO --convert-tabs"	
# do not break one line blocks
ASO="$ASO -- one-line=keep-blocks"
# Insert empty lines around unrelated blocks, labels, classes, ...
#ASO=$ASO --break-blocks

ARTISTIC_STYLE_OPTIONS=$ASO

for f in $FILE;
do 
	if test -f $f 
	then
		astyle $f
	fi
done