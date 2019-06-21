/* Copyright (C) 2013, 2015  Alexander Lamaison <swish@lammy.co.uk>

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

#include "About.hpp"

#include "swish/versions/version.hpp" // release_version

#include <washer/gui/message_box.hpp>
#include <washer/dynamic_link.hpp> // module_path

#include <comet/uuid_fwd.h> // uuid_t

#include <boost/locale.hpp> // translate
#include <boost/filesystem/path.hpp>

#include <cassert> // assert
#include <sstream> // ostringstream
#include <string>

using swish::nse::Command;
using swish::nse::command_site;

using namespace washer::gui::message_box;
using washer::module_path;
using washer::shell::pidl::apidl_t;
using washer::window::window;

using comet::com_ptr;
using comet::uuid_t;

using boost::locale::translate;
using boost::filesystem::path;
using boost::optional;

using std::ostringstream;
using std::string;

// http://stackoverflow.com/a/557859/67013
EXTERN_C IMAGE_DOS_HEADER __ImageBase;

namespace swish {
namespace frontend {
namespace commands {

namespace {

   const uuid_t ABOUT_COMMAND_ID(L"b816a885-5022-11dc-9153-0090f5284f85");

   path installation_path()
   {
       return module_path<char>(((HINSTANCE)&__ImageBase));
   }
}

About::About() :
    Command(
      translate(
        L"Title of command used to show the Swish 'About' box in the "
        L"Explorer Help menu",
        L"About &Swish"), ABOUT_COMMAND_ID,
      translate(
          L"Displays version, licence and copyright information for Swish."))
    {}

    Command::presentation_state About::state(com_ptr<IShellItemArray>,
                                             bool /*ok_to_be_slow*/) const
{
    return presentation_state::enabled;
}

void About::operator()(
    com_ptr<IShellItemArray>, const command_site& site, com_ptr<IBindCtx>)
const
{
    optional<window<wchar_t>> view_window = site.ui_owner();
    if (!view_window)
        return;

    string snapshot = snapshot_version();
    if (snapshot.empty())
    {
        snapshot = translate(
            "Placeholder version if actual version is not known",
            "unknown");
    }

    ostringstream message;
    message
        << "Swish " << release_version().as_string() << "\n"
        << translate(
            "A short description of Swish", "Easy SFTP for Windows Explorer")
        << "\n\n"
        << "Copyright (C) 2006-2013  Alexander Lamaison and contributors.\n\n"
        << "This program comes with ABSOLUTELY NO WARRANTY. This is free "
           "software, and you are welcome to redistribute it under the terms "
           "of the GNU General Public License as published by the Free "
           "Software Foundation, either version 3 of the License, or "
           "(at your option) any later version.\n\n"
        << translate("Title of a version description", "Snapshot:")
        << " " <<  snapshot << "\n"
        << translate("Title for a date and time", "Build time:")
        << " " << build_date() << " " << build_time() << "\n"
        << translate("Title of a filesystem path", "Installation path:")
        << " " << installation_path().string();

   message_box(
        view_window->hwnd(), message.str(),
        translate("Title of About dialog box", "About Swish"), box_type::ok,
        icon_type::information);
}

}}} // namespace swish::frontend::commands
