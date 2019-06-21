/* Copyright (C) 2010, 2011, 2013, 2015
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

#ifndef SWISH_NSE_COMMAND_HPP
#define SWISH_NSE_COMMAND_HPP
#pragma once

#include "swish/nse/command_site.hpp"

#include <washer/shell/pidl.hpp> // apidl_t

#include <comet/ptr.h>      // com_ptr
#include <comet/uuid_fwd.h> // uuid_t

#include <boost/function.hpp>     // function
#include <boost/preprocessor.hpp> // creating variadic pass-through contructors

#include <ObjIdl.h>   // IDataObject, IBindCtx
#include <shobjidl.h> // IExplorerCommandProvider, IShellItemArray

#include <string>

namespace swish
{
namespace nse
{

class Command
{
public:
    Command(const std::wstring& title, const comet::uuid_t& guid,
            const std::wstring& tool_tip = std::wstring(),
            const std::wstring& icon_descriptor = std::wstring(),
            const std::wstring& menu_title = std::wstring(),
            const std::wstring& webtask_title = std::wstring());

    virtual ~Command()
    {
    }

    /**
     * Invoke to perform the command.
     *
     * Concrete commands will provide their implementation by overriding
     * this method.
     *
     * NOTE: If commands need access to the view's window, to use as a UI owner,
     * they need to get this from the command site parameter.  If the owner
     * window is not available from the site, the command must not show UI.
     *
     * @param selection    IShellItemArray holding items on which to perform the
     *                     command.  This may be NULL in which case the
     *                     command should only execute if it makes sense to
     *                     do so regardless of selected items.
     */
    virtual void operator()(comet::com_ptr<IShellItemArray> selection,
                            const command_site& site,
                            comet::com_ptr<IBindCtx> bind_ctx) const = 0;

    // For any of them methods that take a data_object, the implementation
    // does what is appropriate for a situation where selection information
    // is not available.
    // This differs from the situation where it is known that no objects
    // are selected.  In that case, a data_object is provided, but it renders
    // no items.

    /** @name Attributes. */
    // @{
    const comet::uuid_t& guid() const;
    virtual std::wstring title(comet::com_ptr<IShellItemArray> selection) const;
    virtual std::wstring
    tool_tip(comet::com_ptr<IShellItemArray> selection) const;
    virtual std::wstring
    icon_descriptor(comet::com_ptr<IShellItemArray> selection) const;

    /** @name Optional title variants. */
    // @{
    virtual std::wstring
    menu_title(comet::com_ptr<IShellItemArray> selection) const;
    virtual std::wstring
    webtask_title(comet::com_ptr<IShellItemArray> selection) const;
    // @}

    // @}

    /** @name State. */
    // @{
    enum class presentation_state
    {
        enabled,
        disabled,
        hidden
    };

    virtual presentation_state state(comet::com_ptr<IShellItemArray> selection,
                                     bool ok_to_be_slow) const = 0;
    // @}

private:
    std::wstring m_title;
    comet::uuid_t m_guid;
    std::wstring m_tool_tip;
    std::wstring m_icon_descriptor;
    std::wstring m_menu_title;
    std::wstring m_webtask_title;
};

#ifndef COMMAND_ADAPTER_CONSTRUCTOR_MAX_ARGUMENTS
#define COMMAND_ADAPTER_CONSTRUCTOR_MAX_ARGUMENTS 10
#endif

#define COMMAND_ADAPTER_VARIADIC_CONSTRUCTOR(N, classname, initialiser)        \
    BOOST_PP_EXPR_IF(N, template <BOOST_PP_ENUM_PARAMS(N, typename A)>)        \
    explicit classname(BOOST_PP_ENUM_BINARY_PARAMS(N, A, a))                   \
        : initialiser(BOOST_PP_ENUM_PARAMS(N, a))                              \
    {                                                                          \
    }

// TODO: This class should be changed to use aggregation rather than
// inheritance because the latter can lead to a stack overflow if the
// superclass calls title() in its implementation of webtask_title()
// At the moment, other parts of the code rely on being able to pass
// commands with extra methods and have them available
template <typename CommandImpl>
class WebtaskCommandTitleAdapter : public CommandImpl
{
public:
// Define pass-through contructors with variable numbers of arguments
#define BOOST_PP_LOCAL_MACRO(N)                                                \
    COMMAND_ADAPTER_VARIADIC_CONSTRUCTOR(N, WebtaskCommandTitleAdapter,        \
                                         CommandImpl)

#define BOOST_PP_LOCAL_LIMITS (0, COMMAND_ADAPTER_CONSTRUCTOR_MAX_ARGUMENTS)
#include BOOST_PP_LOCAL_ITERATE()

    virtual std::wstring title(comet::com_ptr<IShellItemArray> selection) const
    {
        return webtask_title(selection);
    }
};

#undef COMMAND_ADAPTER_CONSTRUCTOR_MAX_ARGUMENTS
#undef COMMAND_ADAPTER_VARIADIC_CONSTRUCTOR
}
} // namespace swish::nse

#endif
