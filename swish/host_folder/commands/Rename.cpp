/* Copyright (C) 2015  Alexander Lamaison <swish@lammy.co.uk>

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by the
   Free Software Foundation, either version 3 of the License, or (at your
   option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "Rename.hpp"

#include "swish/shell/shell.hpp" // put_view_item_into_rename_mode
#include "swish/shell/parent_and_item.hpp"
#include "swish/shell/shell_item_array.hpp"

#include <washer/shell/services.hpp> // shell_browser, shell_view

#include <comet/error.h> // com_error
#include <comet/ptr.h> // com_ptr
#include <comet/uuid_fwd.h> // uuid_t

#include <boost/locale.hpp> // translate
#include <boost/throw_exception.hpp> // BOOST_THROW_EXCEPTION

#include <cassert> // assert
#include <string>

using swish::nse::Command;
using swish::nse::command_site;
using swish::shell::put_view_item_into_rename_mode;

using washer::shell::pidl::cpidl_t;
using washer::shell::shell_browser;
using washer::shell::shell_view;

using comet::com_error;
using comet::com_ptr;
using comet::uuid_t;

using boost::locale::translate;

using std::wstring;

namespace swish {
namespace host_folder {
namespace commands {

namespace {
    const uuid_t RENAME_COMMAND_ID("b816a883-5022-11dc-9153-0090f5284f85");
}

Rename::Rename() :
    Command(
        translate(L"&Rename SFTP Connection"), RENAME_COMMAND_ID,
        translate(L"Rename an SFTP connection created with Swish."),
        L"shell32.dll,133", translate(L"&Rename SFTP Connection..."),
        translate(L"Rename Connection"))
    {}

    Command::presentation_state
    Rename::state(com_ptr<IShellItemArray> selection,
                  bool /*ok_to_be_slow*/) const
{
    if (!selection)
    {
        // Selection unknown.
        return presentation_state::hidden;
    }

    switch (selection->size())
    {
    case 1:
        return presentation_state::enabled;
    case 0:
        return presentation_state::hidden;
    default:
        // This means multiple items are selected. We disable rather than
        // hide the buttons to let the user know the option exists but that
        // we don't support multi-host renaming.
        return presentation_state::disabled;
    }
}

// This command just puts the item into rename (edit) mode.  When the user
// finishes typing the new name, the shell takes care of performing the rest of
// the renaming process by calling SetNameOf() on the HostFolder
void Rename::operator()(
    com_ptr<IShellItemArray> selection, const command_site& site,
    com_ptr<IBindCtx>)
const
{
    if (selection->size() != 1)
        BOOST_THROW_EXCEPTION(com_error(E_FAIL));

    com_ptr<IShellView> view = shell_view(shell_browser(site.ole_site()));

    com_ptr<IShellItem> item = selection->at(0);
    com_ptr<IParentAndItem> folder_and_pidls = try_cast(item);
    cpidl_t selected_item = folder_and_pidls->item_pidl();

    put_view_item_into_rename_mode(view, selected_item);
}

}}} // namespace swish::host_folder::commands
