/**
    @file

    COM Exception handler.

    @if licence

    Copyright (C) 2009  Alexander Lamaison <awl03@doc.ic.ac.uk>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

    @endif
*/

#include <atl.hpp>     // For CAtlException

#include <ComDef.h>    // For _com_error

#include <exception>   // For std::exception and std::bad_alloc

#define catchCom()            \
catch (const _com_error& e)   \
{                             \
	return e.Error();         \
}                             \
catch (const std::bad_alloc&) \
{                             \
	return E_OUTOFMEMORY;     \
}                             \
catch (const std::exception&) \
{                             \
	return E_UNEXPECTED;      \
}                             \
catch (const ATL::CAtlException& e)\
{                             \
	return e;                 \
}
