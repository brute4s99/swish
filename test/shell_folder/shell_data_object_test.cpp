/**
    @file

    Unit tests for the ShellDataObject wrapper.

    @if license

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

#include "swish/shell_folder/data_object/ShellDataObject.hpp" // Test subject
#include "swish/shell_folder/data_object/StorageMedium.hpp"   // Test subject
#include "swish/shell/shell.hpp" // data_object_for_*

#include "test/fixtures/local_sandbox_fixture.hpp"
#include "test/common_boost/fixtures.hpp"
#include "test/common_boost/helpers.hpp"
#include "test/common_boost/data_object_utils.hpp" // DataObject zip stuff

#include <boost/test/unit_test.hpp>
#include <boost/filesystem.hpp>

#include <algorithm>
#include <vector>
#include <string>

using swish::shell_folder::data_object::ShellDataObject; // test subject
using swish::shell_folder::data_object::PidlFormat;      // test subject
using swish::shell_folder::data_object::StorageMedium;   // test subject
using swish::shell::data_object_for_file;
using swish::shell::data_object_for_directory;

using washer::shell::pidl::apidl_t;

using test::ComFixture;
using test::data_object_utils::create_test_zip_file;
using test::data_object_utils::data_object_for_zipfile;
using test::fixtures::local_sandbox_fixture;

using boost::filesystem::path;
using boost::test_tools::predicate_result;

using std::vector;
using std::wstring;

namespace
{ // private

/**
 * Check that a PIDL and a filesystem path refer to the same item.
 */
predicate_result pidl_path_equivalence(apidl_t pidl, path path)
{
    vector<wchar_t> name(MAX_PATH);
    ::SHGetPathFromIDListW(pidl.get(), &name[0]);

    if (!equivalent(path, &name[0]))
    {
        predicate_result res(false);
        res.message() << "Different items [" << wstring(&name[0])
                      << " != " << path.string() << "]";
        return res;
    }

    return true;
}

class DataObjectFixture : public ComFixture, public local_sandbox_fixture
{
};
}

#pragma region StorageMedium tests
BOOST_AUTO_TEST_SUITE(storage_medium_tests)

/**
 * Create and destroy an instance of the StorageMedium helper object.
 * Check a few members to see if they are initialsed properly.
 */
BOOST_AUTO_TEST_CASE(storage_medium_lifecycle)
{
    {
        StorageMedium medium;

        BOOST_REQUIRE(medium.empty());
        BOOST_REQUIRE_EQUAL(medium.get().hGlobal, static_cast<HGLOBAL>(NULL));
        BOOST_REQUIRE_EQUAL(medium.get().pstm, static_cast<IStream*>(NULL));
        BOOST_REQUIRE_EQUAL(medium.get().pUnkForRelease,
                            static_cast<IUnknown*>(NULL));
    }
}

BOOST_AUTO_TEST_SUITE_END()
#pragma endregion

#pragma region ShellDataObject tests
BOOST_FIXTURE_TEST_SUITE(shell_data_object_tests, DataObjectFixture)

/**
 * Detecting the CF_HDROP format for a filesystem item.
 *
 * The shell data object should always have this format for items that are
 * backed by a real filesystem (i.e. not virtual).  This is a test of whether
 * we can recognise that or not.
 */
BOOST_AUTO_TEST_CASE(cf_hdrop_format)
{
    path file = new_file_in_local_sandbox();
    ShellDataObject data_object(data_object_for_file(file).get());

    BOOST_REQUIRE(data_object.has_hdrop_format());
}

/**
 * Detecting the CF_HDROP format for virtual items.
 *
 * A data object should not have this format for virtual items as they have no
 * filesystem path.  This is a test of whether we can recognise that or not.
 */
BOOST_AUTO_TEST_CASE(cf_hdrop_format_virtual)
{
    path zip_file = create_test_zip_file(local_sandbox());
    ShellDataObject data_object(data_object_for_zipfile(zip_file).get());

    BOOST_REQUIRE(!data_object.has_hdrop_format());
}

/**
 * Detecting the CFSTR_SHELLIDLIST format for a filesystem item.
 *
 * The shell data object should always have this format.  This is a test
 * of whether we can recognise that or not.
 *
 * @todo  Unset the format and test a negative result.
 */
BOOST_AUTO_TEST_CASE(cfstr_shellidlist_format)
{
    path file = new_file_in_local_sandbox();
    ShellDataObject data_object(data_object_for_file(file).get());

    BOOST_REQUIRE(data_object.has_pidl_format());
}

/**
 * Detecting the CFSTR_SHELLIDLIST format for virtual items.
 *
 * The shell data object should always have this format.  This is a test
 * of whether we can recognise that or not.
 *
 * @todo  Unset the format and test a negative result.
 */
BOOST_AUTO_TEST_CASE(cfstr_shellidlist_format_virtual)
{
    path zip_file = create_test_zip_file(local_sandbox());
    ShellDataObject data_object(data_object_for_zipfile(zip_file).get());

    BOOST_REQUIRE(data_object.has_pidl_format());
}

/**
 * Detecting the CFSTR_FILEDESCRIPTOR format for virtual items.
 *
 * This format is expected for data objects holding virtual items.  This is a
 * test of whether we can recognise that or not.
 */
BOOST_AUTO_TEST_CASE(cf_file_group_descriptor_format_virtual)
{
    path zip_file = create_test_zip_file(local_sandbox());
    ShellDataObject data_object(data_object_for_zipfile(zip_file).get());

    BOOST_REQUIRE(data_object.has_file_group_descriptor_format());
}

BOOST_AUTO_TEST_SUITE_END()
#pragma endregion

#pragma region PidlFormat tests
BOOST_FIXTURE_TEST_SUITE(pidl_format_tests, DataObjectFixture)

/**
 * Get a PIDL from a shell data object.
 *
 * Create the DataObject with one item, the test file in the sandbox.  Get
 * the item from the data object as a PIDL and check that it can be resolved
 * back the to filename from which the data object was created.
 */
BOOST_AUTO_TEST_CASE(cfstr_shellidlist_item)
{
    path file = new_file_in_local_sandbox();
    PidlFormat format(data_object_for_file(file));

    BOOST_REQUIRE_EQUAL(format.pidl_count(), 1U);

    apidl_t pidl = format.file(0);

    BOOST_REQUIRE(pidl_path_equivalence(pidl, file));
}

/**
 * Get a PIDL's parent from a shell data object.
 *
 * Create the DataObject with one item, the test file in the sandbox.  Get
 * the parent folder of this item (the sandbox) from the data object as a
 * PIDL and check that it can be resolved back the sandbox's path.
 */
BOOST_AUTO_TEST_CASE(cfstr_shellidlist_parent)
{
    path file = new_file_in_local_sandbox();
    PidlFormat format(data_object_for_file(file));

    BOOST_REQUIRE_EQUAL(format.pidl_count(), 1U);

    apidl_t folder_pidl = format.parent_folder();

    BOOST_REQUIRE(pidl_path_equivalence(folder_pidl, file.parent_path()));
}

/**
 * Try to get a non-existent PIDL from the data object.
 *
 * Create the DataObject with one item.  Attempt to get the @b second item
 * from the data object.  As there is no second item this should fail by
 * throwing an range_error exception.
 */
BOOST_AUTO_TEST_CASE(cfstr_shellidlist_item_fail)
{
    path file = new_file_in_local_sandbox();
    PidlFormat format(data_object_for_file(file));

    BOOST_REQUIRE_EQUAL(format.pidl_count(), 1U);
    BOOST_REQUIRE_THROW(format.file(1), std::range_error);
}

/**
 * Get PIDLs from a shell data object with more than one.
 *
 * Create the DataObject with three items, test files in the sandbox.  Get
 * the items from the data object as PIDLs and check that they can be resolved
 * back the to the filenames from which the data object was created.
 */
BOOST_AUTO_TEST_CASE(cfstr_shellidlist_multiple_items)
{
    vector<path> files;
    files.push_back(new_file_in_local_sandbox());
    files.push_back(new_file_in_local_sandbox());
    files.push_back(new_file_in_local_sandbox());
    sort(files.begin(), files.end());

    PidlFormat format(data_object_for_directory(local_sandbox()));

    BOOST_REQUIRE_EQUAL(format.pidl_count(), 3U);

    apidl_t pidl1 = format.file(0);
    apidl_t pidl2 = format.file(1);
    apidl_t pidl3 = format.file(2);

    BOOST_REQUIRE(pidl_path_equivalence(pidl1, files[0]));
    BOOST_REQUIRE(pidl_path_equivalence(pidl2, files[1]));
    BOOST_REQUIRE(pidl_path_equivalence(pidl3, files[2]));

    BOOST_REQUIRE_THROW(format.file(4), std::range_error);
}

/**
 * The format should respond sensibly even if created with a NULL pointer.
 *
 * The behaviour should act as though this NULL pointer were an empty
 * DataObject; this is the meaning the shell gives it - for instance when
 * nothing is selected in a folder a NULL pointer is returned as the
 * DataObject.
 */
BOOST_AUTO_TEST_CASE(null_dataobject)
{
    PidlFormat format(NULL);

    BOOST_REQUIRE_EQUAL(format.pidl_count(), 0U);
    BOOST_REQUIRE_THROW(format.file(0), std::range_error);
    BOOST_REQUIRE_THROW(format.relative_file(0), std::range_error);
    BOOST_REQUIRE_THROW(format.parent_folder(), std::logic_error);
}

BOOST_AUTO_TEST_SUITE_END()
#pragma endregion
