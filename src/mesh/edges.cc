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
 * @file    edges.cc
 * @ingroup mesh
 * @brief   
 */

#include "system/system.hh"
#include "mesh/sides.h"
#include "mesh/mesh.h"



Edge::Edge()
: n_sides(0),
  side_(NULL)
{
}


SideIter Edge::side(const unsigned int i) const
{
    return side_[i];
}

//-----------------------------------------------------------------------------
// vim: set cindent:
