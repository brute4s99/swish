/**
    @file

    Unit tests for the shell utility functions.

    @if license

    Copyright (C) 2009, 2011, 2012, 2015
    Alexander Lamaison <swish@lammy.co.uk>

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

#include "swish/shell/shell.hpp" // Test subject

// use PidlFormat to inspect DataObjects produces by the Windows Shell
#include "swish/shell_folder/data_object/ShellDataObject.hpp"

#include "test/common_boost/fixtures.hpp"
#include "test/common_boost/helpers.hpp"
#include "test/fixtures/local_sandbox_fixture.hpp"

#include <washer/shell/pidl.hpp> // apidl_t

#include <boost/test/unit_test.hpp>
#include <boost/filesystem.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/throw_exception.hpp> // BOOST_THROW_EXCEPTION

#include <vector>
#include <string>
#include <algorithm> // transform

using swish::shell::ui_object_of_item;
using swish::shell::ui_object_of_items;
using swish::shell::path_from_pidl;
using swish::shell::pidl_from_path;
using swish::shell::data_object_for_files;
using swish::shell::data_object_for_file;
using swish::shell::data_object_for_directory;
using swish::shell_folder::data_object::PidlFormat;

using washer::shell::pidl::apidl_t;

using test::ComFixture;
using test::fixtures::local_sandbox_fixture;

using boost::filesystem::path;
using boost::test_tools::predicate_result;
using boost::shared_ptr;

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

class ShellFunctionFixture : public ComFixture, public local_sandbox_fixture
{
public:
};
}

//
// There are three types of shell function being tested here: those
// that require real filesystem (non-virtual paths), those to do with
// DataObjects specifically and those that are generic with respect to
// both the above (they work on generic objects and take PIDLs instead
// of paths).
// Perhaps these three types should be separated out.
//

#pragma region Shell utility function tests
BOOST_FIXTURE_TEST_SUITE(shell_utility_tests, ShellFunctionFixture)

/**
 * Convert a PIDL to a path.  The path should match the source from which the
 * PIDL was created.
 *
 * Tests path_from_pidl().
 */
BOOST_AUTO_TEST_CASE(convert_pidl_to_path)
{
    path source = new_file_in_local_sandbox();
    shared_ptr<ITEMIDLIST_ABSOLUTE> pidl(
        ::ILCreateFromPathW(source.wstring().c_str()), ::ILFree);

    path path_from_conversion = path_from_pidl(pidl.get());

    BOOST_REQUIRE(equivalent(path_from_conversion, source));
}

/**
 * Make a PIDL from a path.  We should be able to convert the PIDL back to
 * a path that refers to the same item as the original path.
 *
 * Tests pidl_from_path().
 */
BOOST_AUTO_TEST_CASE(convert_path_to_pidl)
{
    path source = new_file_in_local_sandbox();

    shared_ptr<ITEMIDLIST_ABSOLUTE> pidl = pidl_from_path(source);

    vector<wchar_t> buffer(MAX_PATH);
    BOOST_REQUIRE(::SHGetPathFromIDListW(pidl.get(), &buffer[0]));
    BOOST_REQUIRE(equivalent(wstring(&buffer[0]), source));
}

/**
 * Ask the shell for a DataObject 'on' a given file.  This means that the
 * shell should create a DataObject holding a PIDL list format
 * (CFSTR_SHELLIDLIST) with two items in it:
 * - an absolute PIDL to the given file's parent folder
 * - the file's single-item (child) PIDL relative to the parent folder
 *
 * Tests data_object_for_file().
 */
BOOST_AUTO_TEST_CASE(single_item_dataobject)
{
    path source = new_file_in_local_sandbox();

    PidlFormat format(data_object_for_file(source));

    BOOST_REQUIRE_EQUAL(format.pidl_count(), 1U);

    BOOST_REQUIRE(
        pidl_path_equivalence(format.parent_folder(), local_sandbox()));
    BOOST_REQUIRE(pidl_path_equivalence(format.file(0), source));
}

/**
 * Ask the shell for a DataObject 'on' two items in the same folder.
 * This means that the shell should create a DataObject holding a PIDL list
 * format (CFSTR_SHELLIDLIST) with three items in it:
 * - an absolute PIDL to the given files' parent folder
 * - the first file's single-item (child) PIDL relative to the parent folder
 * - the second file's single-item (child) PIDL relative to the parent folder
 *
 * Tests data_object_for_files().
 */
BOOST_AUTO_TEST_CASE(multi_item_dataobject)
{
    vector<path> sources;
    sources.push_back(new_file_in_local_sandbox());
    sources.push_back(new_file_in_local_sandbox());

    PidlFormat format(data_object_for_files(sources.begin(), sources.end()));

    BOOST_REQUIRE_EQUAL(format.pidl_count(), 2U);

    BOOST_REQUIRE(
        pidl_path_equivalence(format.parent_folder(), local_sandbox()));
    BOOST_REQUIRE(pidl_path_equivalence(format.file(0), sources[0]));
    BOOST_REQUIRE(pidl_path_equivalence(format.file(1), sources[1]));
}

/**
 * Ask for an associated object of a given file.  In this case we ask for a
 * DataObject because then we can subject it to the same tests as the
 * data_object_for_file test above.
 *
 * Tests ui_object_of_item().
 */
BOOST_AUTO_TEST_CASE(single_item_ui_object)
{
    path source = new_file_in_local_sandbox();

    PidlFormat format(
        ui_object_of_item<IDataObject>(pidl_from_path(source).get()));

    BOOST_REQUIRE_EQUAL(format.pidl_count(), 1U);

    BOOST_REQUIRE(
        pidl_path_equivalence(format.parent_folder(), local_sandbox()));
    BOOST_REQUIRE(pidl_path_equivalence(format.file(0), source));
}

/**
 * Ask for an associated object of two files in the same folder.  In this case
 * we ask for a DataObject because then we can subject it to the same tests
 * as the data_object_for_files test above.
 *
 * Tests ui_object_of_items().
 */
BOOST_AUTO_TEST_CASE(multi_item_ui_object)
{
    vector<path> sources;
    sources.push_back(new_file_in_local_sandbox());
    sources.push_back(new_file_in_local_sandbox());

    vector<shared_ptr<ITEMIDLIST_ABSOLUTE>> pidls;
    transform(sources.begin(), sources.end(), back_inserter(pidls),
              pidl_from_path);

    PidlFormat format(
        ui_object_of_items<IDataObject>(pidls.begin(), pidls.end()));

    BOOST_REQUIRE_EQUAL(format.pidl_count(), 2U);

    BOOST_REQUIRE(
        pidl_path_equivalence(format.parent_folder(), local_sandbox()));
    BOOST_REQUIRE(pidl_path_equivalence(format.file(0), sources[0]));
    BOOST_REQUIRE(pidl_path_equivalence(format.file(1), sources[1]));
}

BOOST_AUTO_TEST_SUITE_END()
#pragma endregion
