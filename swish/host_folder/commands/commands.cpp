/* Copyright (C) 2010, 2011, 2012, 2015
   Alexander Lamaison <swish@lammy.co.uk>

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

#include "commands.hpp"

#include "swish/host_folder/commands/Add.hpp"
#include "swish/host_folder/commands/CloseSession.hpp"
#include "swish/host_folder/commands/LaunchAgent.hpp"
#include "swish/host_folder/commands/Remove.hpp"
#include "swish/host_folder/commands/Rename.hpp"
#include "swish/nse/explorer_command.hpp" // CExplorerCommand*
#include "swish/nse/task_pane.hpp" // CUIElementErrorAdapter

#include <comet/server.h> // simple_object
#include <comet/smart_enum.h> // make_smart_enumeration

#include <boost/locale.hpp> // translate
#include <boost/make_shared.hpp> // make_shared

#include <cassert> // assert
#include <string>
#include <utility> // make_pair
#include <vector>

using swish::nse::CExplorerCommand;
using swish::nse::CExplorerCommandProvider;
using swish::nse::CUICommand;
using swish::nse::CUIElementErrorAdapter;
using swish::nse::Command;
using swish::nse::IEnumUICommand;
using swish::nse::IUICommand;
using swish::nse::IUIElement;
using swish::nse::WebtaskCommandTitleAdapter;

using washer::shell::pidl::apidl_t;

using comet::com_ptr;
using comet::make_smart_enumeration;
using comet::simple_object;

using boost::locale::translate;
using boost::make_shared;
using boost::shared_ptr;

using std::make_pair;
using std::vector;
using std::wstring;

namespace swish {
namespace host_folder {
namespace commands {

com_ptr<IExplorerCommandProvider> host_folder_command_provider(
    const apidl_t& folder_pidl)
{
    CExplorerCommandProvider::ordered_commands commands;
    commands.push_back(new CExplorerCommand<Add>(folder_pidl));
    commands.push_back(new CExplorerCommand<Remove>(folder_pidl));
    commands.push_back(new CExplorerCommand<Rename>());
    commands.push_back(new CExplorerCommand<CloseSession>());
    commands.push_back(new CExplorerCommand<LaunchAgent>(folder_pidl));
    return new CExplorerCommandProvider(commands);
}

class CSftpTasksTitle : public simple_object<CUIElementErrorAdapter>
{
public:

    virtual std::wstring title(
        const comet::com_ptr<IShellItemArray>& /*items*/) const
    {
        return translate(L"SFTP Tasks");
    }

    virtual std::wstring icon(
        const comet::com_ptr<IShellItemArray>& /*items*/) const
    {
        return L"shell32.dll,-9";
    }

    virtual std::wstring tool_tip(
        const comet::com_ptr<IShellItemArray>& /*items*/) const
    {
        return translate(
            L"These tasks help you manage Swish SFTP connections.");
    }
};

std::pair<com_ptr<IUIElement>, com_ptr<IUIElement> >
host_folder_task_pane_titles(const apidl_t& /*folder_pidl*/)
{
    return make_pair(new CSftpTasksTitle(), com_ptr<IUIElement>());
}

std::pair<com_ptr<IEnumUICommand>, com_ptr<IEnumUICommand> >
host_folder_task_pane_tasks(const apidl_t& folder_pidl)
{
    typedef shared_ptr< vector< com_ptr<IUICommand> > > shared_command_vector;
    shared_command_vector commands =
        make_shared< vector< com_ptr<IUICommand> > >();

    commands->push_back(
        new CUICommand<WebtaskCommandTitleAdapter<Add>>(folder_pidl));
    commands->push_back(
        new CUICommand<WebtaskCommandTitleAdapter<Remove>>(folder_pidl));
    commands->push_back(
        new CUICommand<WebtaskCommandTitleAdapter<Rename>>());
    commands->push_back(
        new CUICommand<WebtaskCommandTitleAdapter<CloseSession>>());
    commands->push_back(
        new CUICommand<WebtaskCommandTitleAdapter<LaunchAgent>>(folder_pidl));

    com_ptr<IEnumUICommand> e =
        make_smart_enumeration<IEnumUICommand>(commands);

    return make_pair(e, com_ptr<IEnumUICommand>());
}

}}} // namespace swish::host_folder::commands
