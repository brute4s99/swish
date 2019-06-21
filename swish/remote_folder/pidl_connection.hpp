/**
    @file

    Relates PIDLs to SFTP connections.

    @if license

    Copyright (C) 2007, 2008, 2009, 2010, 2011, 2013
    Alexander Lamaison <awl03@doc.ic.ac.uk>

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

#ifndef SWISH_REMOTE_FOLDER_PIDL_CONNECTION_HPP
#define SWISH_REMOTE_FOLDER_PIDL_CONNECTION_HPP
#pragma once

#include "swish/connection/connection_spec.hpp"
#include "swish/provider/sftp_provider.hpp"

#include <washer/shell/pidl.hpp> // apidl_t

#include <comet/ptr.h>

#include <boost/shared_ptr.hpp>

#include <string>

namespace swish {
namespace remote_folder {

/**
 * Converts a host PIDL into a connection specification.
 */
swish::connection::connection_spec connection_from_pidl(
    const washer::shell::pidl::apidl_t& pidl);

/**
 * Creates lazy-connecting provider primed to connect for given PIDL.
 *
 * The session will be created from the information stored in this
 * folder's PIDL, @a pidl, if connection is required.
 */
boost::shared_ptr<swish::provider::sftp_provider> provider_from_pidl(
    const washer::shell::pidl::apidl_t& pidl,
    comet::com_ptr<ISftpConsumer> consumer,
    const std::string& task_name);

}} // namespace swish::remote_folder

#endif
