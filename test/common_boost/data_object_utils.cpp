/**
    @file

    Helper functions for tests that involve DataObjects.

    @if license

    Copyright (C) 2009, 2012  Alexander Lamaison <awl03@doc.ic.ac.uk>

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

#include "data_object_utils.hpp"

#include "swish/shell/shell.hpp"

#include <washer/shell/shell.hpp> // bind_to_handler_object

#include <comet/error.h> // com_error

#include <boost/shared_ptr.hpp>
#include <boost/throw_exception.hpp>  // BOOST_THROW_EXCEPTION
#include <boost/numeric/conversion/cast.hpp>  // numeric_cast
#include <boost/system/system_error.hpp>  // system_error, get_system_category

#include <string>
#include <vector>

using swish::shell::pidl_from_path;
using swish::shell::ui_object_of_items;

using washer::shell::bind_to_handler_object;

using boost::filesystem::path;
using boost::shared_ptr;
using boost::numeric_cast;
using boost::system::get_system_category;
using boost::system::system_error;

using comet::com_error_from_interface;
using comet::com_ptr;

using std::wstring;
using std::vector;

namespace {

    /**
     * Return the path of the currently running executable.
     */
    path get_module_path(HMODULE hmodule=NULL)
    {
        vector<wchar_t> buffer(MAX_PATH);
        unsigned long len = ::GetModuleFileNameW(
            hmodule, &buffer[0], numeric_cast<unsigned long>(buffer.size()));

        if (len == 0)
            BOOST_THROW_EXCEPTION(
                system_error(::GetLastError(), get_system_category()));

        return wstring(&buffer[0], len);
    }

}

namespace test {
namespace data_object_utils {

/**
 * Create a zip archive containing two files that we can use as a source
 * of 'virtual' namespace items.
 *
 * Virtual namespace items are not real files on disk and instead are
 * simulated by an IShellFolder implementation.  This is how Swish
 * itself presents its 'files' to Explorer.  The ZIP-file browser in
 * Windows 2000 and later does the same thing to give access to the
 * files inside a .zip.  We're going to use one of these to test our
 * shell data object wrapper with virtual items.
 */
path create_test_zip_file(const path& in_directory)
{
    path source = get_module_path().parent_path()
        / L"test_zip_file.zip";
    path destination = in_directory / L"test_zip_file.zip";

    copy_file(source, destination);

    return destination;
}

/**
 * Return a DataObject with the contents of a zip file.
 */
com_ptr<IDataObject> data_object_for_zipfile(const path& zip_file)
{
    shared_ptr<ITEMIDLIST_ABSOLUTE> zip_pidl = pidl_from_path(zip_file);
    com_ptr<IShellFolder> zip_folder =
        bind_to_handler_object<IShellFolder>(zip_pidl.get());

    com_ptr<IEnumIDList> enum_items;
    HRESULT hr = zip_folder->EnumObjects(
        NULL, SHCONTF_FOLDERS | SHCONTF_NONFOLDERS, enum_items.out());
    if (FAILED(hr))
        BOOST_THROW_EXCEPTION(com_error_from_interface(zip_folder, hr));

    enum_items->Reset();

    vector<shared_ptr<ITEMIDLIST_ABSOLUTE> > pidls;
    while (hr == S_OK)
    {
        PITEMID_CHILD pidl;
        hr = enum_items->Next(1, &pidl, NULL);
        if (hr == S_OK)
        {
            shared_ptr<ITEMID_CHILD> child_pidl(pidl, ::ILFree);

            pidls.push_back(
                shared_ptr<ITEMIDLIST_ABSOLUTE>(
                    ::ILCombine(zip_pidl.get(), child_pidl.get()),
                    ::ILFree));
        }
    }

    return ui_object_of_items<IDataObject>(pidls.begin(), pidls.end());
}

}} // namespace test::data_object_utils
