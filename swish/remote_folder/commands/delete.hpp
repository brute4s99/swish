/**
    @file

    Swish remote folder commands.

    @if license

    Copyright (C) 2011, 2013  Alexander Lamaison <awl03@doc.ic.ac.uk>

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

#ifndef SWISH_REMOTE_FOLDER_COMMANDS_DELETE_HPP
#define SWISH_REMOTE_FOLDER_COMMANDS_DELETE_HPP
#pragma once

#include "swish/provider/sftp_provider.hpp" // sftp_provider, ISftpConsumer

#include <comet/ptr.h> // com_ptr

#include <boost/function.hpp>
#include <boost/shared_ptr.hpp>

#include <shobjidl.h> // IShellItemArray

namespace swish {
namespace remote_folder {
namespace commands {

class Delete
{
    typedef boost::function<
        boost::shared_ptr<swish::provider::sftp_provider>(
        comet::com_ptr<ISftpConsumer>, const std::string& task_name)
    > my_provider_factory;

    typedef boost::function<comet::com_ptr<ISftpConsumer>(HWND)>
        my_consumer_factory;

public:
    Delete(
        my_provider_factory provider_factory,
        my_consumer_factory consumer_factory);

    void operator()(
        HWND hwnd_view, comet::com_ptr<IShellItemArray> selection) const;

private:
    my_provider_factory m_provider_factory;
    my_consumer_factory m_consumer_factory;
};

}}} // namespace swish::remote_folder::commands

#endif
