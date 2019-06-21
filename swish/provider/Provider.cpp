// Copyright 2008, 2009, 2010, 2011, 2012, 2013, 2016 Alexander Lamaison

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

#include "Provider.hpp"

#include "swish/connection/authenticated_session.hpp"
#include "swish/connection/session_manager.hpp" // session_reservation
#include "swish/provider/libssh2_sftp_filesystem_item.hpp"
#include "swish/provider/sftp_filesystem_item.hpp"
#include "swish/remotelimits.h"
#include "swish/trace.hpp" // trace

#include <comet/bstr.h>     // bstr_t
#include <comet/datetime.h> // datetime_t
#include <comet/enum.h>     // stl_enumeration
#include <comet/error.h>    // com_error
#include <comet/ptr.h>      // com_ptr
#include <comet/server.h>   // simple_object for STL holder with AddRef lifetime
#include <comet/stream.h>   // adapt_stream_pointer

#include <ssh/filesystem.hpp> // directory_iterator
#include <ssh/stream.hpp>     // ofstream, ifstream

#include <boost/filesystem/path.hpp>          // path
#include <boost/iterator/filter_iterator.hpp> // make_filter_iterator
#include <boost/system/system_error.hpp>      // system_error, system_category
#include <boost/throw_exception.hpp>          // BOOST_THROW_EXCEPTION

#include <cassert> // assert
#include <exception>
#include <stdexcept> // invalid_argument
#include <string>
#include <type_traits>
#include <vector> // to hold listing

using swish::connection::authenticated_session;
using swish::connection::session_reservation;
using swish::tracing::trace;

using comet::adapt_stream_pointer;
using comet::bstr_t;
using comet::com_error;
using comet::com_ptr;
using comet::datetime_t;
using comet::stl_enumeration;

using boost::make_filter_iterator;
namespace errc = boost::system::errc;
using boost::system::system_category;
using boost::system::system_error;

using ssh::filesystem::directory_iterator;
using ssh::filesystem::file_attributes;
using ssh::filesystem::fstream;
using ssh::filesystem::ifstream;
using ssh::filesystem::ofstream;
using ssh::filesystem::overwrite_behaviour;
using ssh::filesystem::path;
using ssh::filesystem::sftp_filesystem;
using ssh::filesystem::sftp_file;

using std::exception;
using std::invalid_argument;
using std::string;
using std::wstring;
using std::make_shared;
using std::make_unique;
using std::vector;

namespace swish
{
namespace provider
{

class provider
{
public:
    explicit provider(session_reservation&& session_ticket);

    directory_listing listing(const path& directory);

    comet::com_ptr<IStream> get_file(const path& file_path,
                                     std::ios_base::openmode open_mode);

    VARIANT_BOOL rename(com_ptr<ISftpConsumer> consumer, const path& from_path,
                        const path& to_path);

    void remove_all(const path& path);

    void create_new_directory(const path& path);

    const path resolve_link(const path& path);

    sftp_filesystem_item stat(const path& path, bool follow_links);

private:
    session_reservation m_ticket;
};

CProvider::CProvider(session_reservation&& session_ticket)
{
    m_provider = make_unique<provider>(std::move(session_ticket));
}

directory_listing CProvider::listing(const path& directory)
{
    return m_provider->listing(directory);
}

comet::com_ptr<IStream> CProvider::get_file(const path& file_path,
                                            std::ios_base::openmode open_mode)
{
    return m_provider->get_file(file_path, open_mode);
}

VARIANT_BOOL CProvider::rename(ISftpConsumer* consumer, const path& from_path,
                               const path& to_path)
{
    return m_provider->rename(consumer, from_path, to_path);
}

void CProvider::remove_all(const path& path)
{
    m_provider->remove_all(path);
}

void CProvider::create_new_directory(const path& path)
{
    m_provider->create_new_directory(path);
}

path CProvider::resolve_link(const path& path)
{
    return m_provider->resolve_link(path);
}

sftp_filesystem_item CProvider::stat(const path& path, bool follow_links)
{
    return m_provider->stat(path, follow_links);
}

/**
 * Create libssh2-based data provider.
 */
provider::provider(session_reservation&& ticket) : m_ticket(std::move(ticket))
{
}

namespace
{

bool not_special_file(const sftp_file& file)
{
    return file.path().filename() != "." && file.path().filename() != "..";
}
}

/**
* Retrieves a file listing, @c ls, of a given directory.
*
* @param directory  Absolute path of the directory to list.
*/
directory_listing provider::listing(const path& directory)
{
    if (directory.empty())
        BOOST_THROW_EXCEPTION(com_error(E_INVALIDARG));

    sftp_filesystem& channel = m_ticket.session().get_sftp_filesystem();

    vector<sftp_filesystem_item> files;
    transform(
        make_filter_iterator(not_special_file,
                             channel.directory_iterator(directory)),
        make_filter_iterator(not_special_file, channel.directory_iterator()),
        back_inserter(files),
        libssh2_sftp_filesystem_item::create_from_libssh2_file);

    return files;
}

com_ptr<IStream> provider::get_file(const path& file_path,
                                    std::ios_base::openmode mode)
{
    if (file_path.empty())
        BOOST_THROW_EXCEPTION(invalid_argument("File cannot be empty"));

    sftp_filesystem& channel = m_ticket.session().get_sftp_filesystem();

    if (mode & std::ios_base::out && mode & std::ios_base::in)
    {
        auto stream_pointer =
            std::make_shared<fstream>(channel, file_path, mode);
        return adapt_stream_pointer(stream_pointer,
                                    file_path.filename().wstring());
    }
    else if (mode & std::ios_base::out)
    {
        return adapt_stream_pointer(
            std::make_shared<ofstream>(boost::ref(channel), file_path, mode),
            file_path.filename().wstring());
    }
    else if (mode & std::ios_base::in)
    {
        return adapt_stream_pointer(
            std::make_shared<ifstream>(boost::ref(channel), file_path, mode),
            file_path.filename().wstring());
    }
    else
    {
        BOOST_THROW_EXCEPTION(
            std::invalid_argument("Stream must be input, output or both"));
    }
}

namespace
{

/**
 * Rename file or directory and overwrite any obstruction non-atomically.
 *
 * This involves renaming the obstruction at the target to a temporary file,
 * renaming the source file to the target and then deleting the renamed
 * obstruction.  As this is not an atomic operation it is possible to fail
 * between any of these stages and is not a prefect solution.  It may, for
 * instance, leave the temporary file behind.
 *
 * @param from
 *     Absolute path of the file or directory to be renamed.
 * @param to
 *     Absolute path to rename `from` to.
 *
 * @throws  ssh_error if the operation fails.
 */
void rename_non_atomic_overwrite(authenticated_session& session,
                                 const string& from, const string& to)
{
    string temporary = to + ".swish_rename_temp";

    rename(session.get_sftp_filesystem(), to, temporary,
           overwrite_behaviour::prevent_overwrite);

    try
    {
        rename(session.get_sftp_filesystem(), from, to,
               overwrite_behaviour::prevent_overwrite);
    }
    catch (const exception&)
    {
        // Rename failed, rename our temporary back to its old name
        try
        {
            rename(session.get_sftp_filesystem(), from, to,
                   overwrite_behaviour::prevent_overwrite);
        }
        catch (const exception&)
        { /* Suppress to avoid nested exception */
        }

        throw;
    }

    // We ignore any failure to clean up the temporary backup as the rename
    // has succeeded, whether or not cleanup fails.
    //
    // XXX: We could inform the user of this here.  Might make UI
    // separation messy though.
    try
    {
        remove_all(session.get_sftp_filesystem(), temporary);
    }
    catch (const exception&)
    {
    }
}

/**
 * Retry renaming after seeking permission to overwrite the obstruction at
 * the target.
 *
 * If this fails the file or directory really can't be renamed and the error
 * message from libssh2 is returned in @a error_out.
 *
 * @param pConsumer
 *     Callback for user confirmation.
 *
 * @param previous_error
 *     Error code of the previous rename attempt in order to determine if an
 *     overwrite has any chance of being successful.
 *
 * @param from
 *     Absolute path of the file or directory to be renamed.
 *
 * @param to
 *     Absolute path to rename @a from to.
 *
 * @returns `true` if the the rename operation succeeds as a result of
 *           retrying it,
 *          `false` if the the rename operation needed user permission for
 *           something and the user chose to abort the renaming.
 *
 * @throws `previous_error` if the situation is not caused by an obstruction
 *         at the target.  Retrying renaming is not going to help here.
 *
 * @bug  The strings aren't converted from UTF-8 to UTF-16 before displaying
 *       to the user.  Any unicode filenames will produce gibberish in the
 *       confirmation dialogues.
 */
bool rename_retry_with_overwrite(authenticated_session& session,
                                 ISftpConsumer* pConsumer,
                                 const system_error& previous_error,
                                 const string& from, const string& to)
{
    assert(previous_error.code() && "Previous attempt succeeded; why retry?");

    if (previous_error.code() == errc::file_exists)
    {
        HRESULT hr =
            pConsumer->OnConfirmOverwrite(bstr_t(from).in(), bstr_t(to).in());
        if (FAILED(hr))
            return false;

        // Attempt rename again this time allowing it to atomically overwrite
        // any obstruction.
        // This will only work on a server supporting SFTP version 5 or above.

        try
        {
            rename(session.get_sftp_filesystem(), from, to,
                   overwrite_behaviour::atomic_overwrite);
            return true;
        }
        catch (const system_error& e)
        {
            if (e.code() == errc::operation_not_supported)
            {
                rename_non_atomic_overwrite(session, from, to);
                return true;
            }
            else
            {
                throw;
            }
        }
    }
    else
    {
        // The failure is an unspecified one. This isn't the end of the world.
        // SFTP servers < v5 (i.e. most of them) return this error code if the
        // file already exists as they don't explicitly support overwriting.
        // We need to stat() the file to find out if this is the case and if
        // the user confirms the overwrite we will have to explicitly delete
        // the target file first (via a temporary) and then repeat the rename.
        //
        // NOTE: this is not a perfect solution due to the possibility
        // for race conditions.

        // We used to test for FX_FAILURE here, because that's what OpenSSH
        // returns, but changed it because the v3 standard (v5 handled above)
        // doesn't promise any particular error code so we might as well
        // treat them all this way.

        if (exists(session.get_sftp_filesystem(), to))
        {
            HRESULT hr = pConsumer->OnConfirmOverwrite(bstr_t(from).in(),
                                                       bstr_t(to).in());
            if (FAILED(hr))
                return false;

            rename_non_atomic_overwrite(session, from, to);
            return true;
        }
        else
        {
            // Rethrow the last exception because it wasn't caused by an
            // obstruction.
            //
            // RACE CONDITION: It might have been caused by an obstruction
            // which was then cleared by the time we did the existence check
            // above.  The result it just that we would fail when we could
            // have succeeded.  Such an edge case that it doesn't matter.
            throw previous_error;
        }
    }
}
}

/**
 * Renames a file or directory.
 *
 * The source and target file or directory must be specified using absolute
 * paths for the remote filesystem.  The results of passing relative paths are
 * not guaranteed (though, libssh2 seems to default to operating in the home
 * directory) and may be dangerous.
 *
 * If a file or folder already exists at the target path, @a to_path,
 * we inform the front-end consumer through a call to OnConfirmOverwrite.
 * If confirmation is given, we attempt to overwrite the
 * obstruction with the source path, @a from_path, and if successful we
 * return @c VARIANT_TRUE.  This can be used by the caller to decide whether
 * or not to update a directory view.
 *
 * @remarks
 * Due to the limitations of SFTP versions 4 and below, most servers will not
 * allow atomic overwrite.  We attempt to do this non-atomically by:
 * -# appending @c ".swish_renaming_temp" to the obstructing target's filename
 * -# renaming the source file to the old target name
 * -# deleting the renamed target
 * If step 2 fails, we try to rename the temporary file back to its old name.
 * It is possible that this last step may fail, in which case the temporary file
 * would remain in place.  It could be recovered by manually renaming it back.
 *
 * @warning
 * If either of the paths are not absolute, this function may cause files
 * in whichever directory libssh2 considers 'current' to be renamed or deleted
 * if they happen to have matching filenames.
 *
 * @param consumer   UI callback.
 * @param from_path  Absolute path of the file or directory to be renamed.
 * @param to_path    Absolute path that @a from_path should be renamed to.
 *
 * @returns  Whether or not we needed to overwrite an existing file or
 *           directory at the target path.
 */
VARIANT_BOOL provider::rename(com_ptr<ISftpConsumer> consumer, const path& from,
                              const path& to)
{
    if (from.empty())
        BOOST_THROW_EXCEPTION(com_error(E_INVALIDARG));
    if (to.empty())
        BOOST_THROW_EXCEPTION(com_error(E_INVALIDARG));

    // NOP if filenames are equal
    if (from == to)
        return VARIANT_FALSE;

    // Attempt to rename old path to new path
    try
    {
        ssh::filesystem::rename(m_ticket.session().get_sftp_filesystem(), from,
                                to, overwrite_behaviour::prevent_overwrite);

        // Rename was successful without overwrite
        return VARIANT_FALSE;
    }
    catch (const system_error& e)
    {
        if (rename_retry_with_overwrite(m_ticket.session(), consumer.get(), e,
                                        from, to))
        {
            return VARIANT_TRUE;
        }
        else
        {
            BOOST_THROW_EXCEPTION(com_error(E_ABORT));
        }
    }
}

void provider::remove_all(const path& target)
{
    if (target.empty())
        BOOST_THROW_EXCEPTION(com_error(E_INVALIDARG));

    ssh::filesystem::remove_all(m_ticket.session().get_sftp_filesystem(),
                                target);
}

void provider::create_new_directory(const path& path)
{
    if (path.empty())
        BOOST_THROW_EXCEPTION(com_error(
            "Cannot create a directory without a name", E_INVALIDARG));

    create_directory(m_ticket.session().get_sftp_filesystem(), path);
}

const path provider::resolve_link(const path& path)
{
    sftp_filesystem& channel = m_ticket.session().get_sftp_filesystem();
    bstr_t target = channel.canonical_path(path).wstring();

    return target.detach();
}

/**
 * Get the details of a file by path.
 *
 * The item returned by this function doesn't include a long entry or
 * owner and group names as string (these being derived from the long entry).
 */
sftp_filesystem_item provider::stat(const path& path, bool follow_links)
{
    sftp_filesystem& channel = m_ticket.session().get_sftp_filesystem();

    file_attributes stat_result =
        channel.attributes(path, follow_links != FALSE);

    return libssh2_sftp_filesystem_item::create_from_libssh2_attributes(
        path, stat_result);
}
}
} // namespace swish::provider
