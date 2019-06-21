/**
    @file

    Management functions for host entries saved in the registry.

    @if license

    Copyright (C) 2009, 2011, 2015  Alexander Lamaison <swish@lammy.co.uk>

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

#ifndef SWISH_HOST_FOLDER_HOST_MANAGEMENT_HPP
#define SWISH_HOST_FOLDER_HOST_MANAGEMENT_HPP
#pragma once

#include <washer/shell/pidl.hpp> // cpidl_t

#include <boost/optional/optional.hpp>

#include <string>
#include <vector>

namespace swish {
namespace host_folder {
namespace host_management {

std::vector<washer::shell::pidl::cpidl_t> LoadConnectionsFromRegistry();

void AddConnectionToRegistry(
    std::wstring label, std::wstring host, int port,
    std::wstring username, std::wstring path);

boost::optional<washer::shell::pidl::cpidl_t> FindConnectionInRegistry(
    const std::wstring& label);

void RemoveConnectionFromRegistry(std::wstring label);

void RenameConnectionInRegistry(
    const std::wstring& from_label, const std::wstring& to_label);

bool ConnectionExists(std::wstring label);

}}} // namespace swish::host_folder::host_management

#endif
