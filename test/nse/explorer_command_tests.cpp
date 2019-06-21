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

#include "swish/nse/explorer_command.hpp" // test subject

#include "swish/nse/Command.hpp" // Command

#include "test/common_boost/helpers.hpp"
#include <boost/test/unit_test.hpp>

#include <comet/error.h> // com_error
#include <comet/ptr.h> // com_ptr
#include <comet/uuid_fwd.h> // uuid_t

#include <boost/shared_ptr.hpp> // shared_ptr

#include <string>

using swish::nse::CExplorerCommandProvider;
using swish::nse::CExplorerCommand;
using swish::nse::Command;
using swish::nse::command_site;
using comet::com_error;
using comet::com_ptr;
using comet::uuidof;
using comet::uuid_t;

using boost::shared_ptr;

using std::wstring;

namespace {

    struct TestCommand : public Command
    {
        TestCommand(
            const wstring& title, const uuid_t& guid,
            const wstring& tool_tip=wstring(),
            const wstring& icon_descriptor=wstring())
        : Command(title, guid, tool_tip, icon_descriptor) {}

        presentation_state state(com_ptr<IShellItemArray>, bool) const
        {
            return presentation_state::enabled;
        }

        void operator()(
            com_ptr<IShellItemArray>, const command_site&,
            com_ptr<IBindCtx>)
        const
        {} // noop
    };

    const uuid_t DUMMY_GUID_1("002F9D5D-DB85-4224-9097-B1D06E681252");
    const uuid_t DUMMY_GUID_2("3BDC0E76-2D94-43c3-AC33-ED629C24AA70");

    struct DummyCommand1 : public TestCommand
    {
        DummyCommand1() : TestCommand(
            L"command_1", DUMMY_GUID_1, L"tool-tip-1") {}
    };

    struct DummyCommand2 : public TestCommand
    {
        DummyCommand2() : TestCommand(
            L"command_2", DUMMY_GUID_2, L"tool-tip-2") {}
    };

    CExplorerCommandProvider::ordered_commands dummy_commands()
    {
        CExplorerCommandProvider::ordered_commands commands;
        commands.push_back(new CExplorerCommand<DummyCommand1>());
        commands.push_back(new CExplorerCommand<DummyCommand2>());
        return commands;
    }
}

BOOST_AUTO_TEST_SUITE(explorer_command_tests)

BOOST_AUTO_TEST_CASE( create_empty_provider )
{
    com_ptr<IExplorerCommandProvider> commands = new CExplorerCommandProvider(
        CExplorerCommandProvider::ordered_commands());
    BOOST_REQUIRE(commands);

    // Test GetCommands
    com_ptr<IEnumExplorerCommand> enum_commands;
    BOOST_REQUIRE_OK(
        commands->GetCommands(
            NULL, uuidof(enum_commands.in()),
            reinterpret_cast<void**>(enum_commands.out())));

    com_ptr<IExplorerCommand> command;
    BOOST_REQUIRE_EQUAL(enum_commands->Next(1, command.out(), NULL), S_FALSE);

    // Test GetCommand
    BOOST_REQUIRE_EQUAL(
        commands->GetCommand(
            GUID_NULL, uuidof(command.in()),
            reinterpret_cast<void**>(command.out())),
        E_FAIL);
}

BOOST_AUTO_TEST_CASE( commands )
{
    com_ptr<IExplorerCommandProvider> commands = new CExplorerCommandProvider(
        dummy_commands());
    BOOST_REQUIRE(commands);

    // Test GetCommands
    com_ptr<IEnumExplorerCommand> enum_commands;
    BOOST_REQUIRE_OK(
        commands->GetCommands(
            NULL, uuidof(enum_commands.in()),
            reinterpret_cast<void**>(enum_commands.out())));

    com_ptr<IExplorerCommand> command;
    uuid_t guid;

    BOOST_REQUIRE_OK(enum_commands->Next(1, command.out(), NULL));
    BOOST_REQUIRE_OK(command->GetCanonicalName(guid.out()));
    BOOST_REQUIRE_EQUAL(guid, DUMMY_GUID_1);

    BOOST_REQUIRE_OK(enum_commands->Next(1, command.out(), NULL));
    BOOST_REQUIRE_OK(command->GetCanonicalName(guid.out()));
    BOOST_REQUIRE_EQUAL(guid, DUMMY_GUID_2);

    BOOST_REQUIRE_EQUAL(enum_commands->Next(1, command.out(), NULL), S_FALSE);

    // Test GetCommand
    BOOST_REQUIRE_OK(
        commands->GetCommand(
            DUMMY_GUID_2, uuidof(command.in()),
            reinterpret_cast<void**>(command.out())));
    BOOST_REQUIRE_OK(command->GetCanonicalName(guid.out()));
    BOOST_REQUIRE_EQUAL(guid, DUMMY_GUID_2);

    BOOST_REQUIRE_OK(
        commands->GetCommand(
            DUMMY_GUID_1, uuidof(command.in()),
            reinterpret_cast<void**>(command.out())));
    BOOST_REQUIRE_OK(command->GetCanonicalName(guid.out()));
    BOOST_REQUIRE_EQUAL(guid, DUMMY_GUID_1);

    BOOST_REQUIRE_EQUAL(
        commands->GetCommand(
            GUID_NULL, uuidof(command.in()),
            reinterpret_cast<void**>(command.out())),
        E_FAIL);
}

namespace {

    const GUID TEST_GUID =
        { 0x1621a875, 0x1252, 0x4bde,
        { 0xb7, 0x69, 0x70, 0xa9, 0x5f, 0x49, 0x7c, 0x5f } };

    struct HostCommand : public Command
    {
        HostCommand() : Command(L"title", TEST_GUID, L"tool-tip") {}

        presentation_state state(com_ptr<IShellItemArray>, bool) const
        {
            return presentation_state::enabled;
        }

        void operator()(
            com_ptr<IShellItemArray>, const command_site&, com_ptr<IBindCtx>)
        const
        {
            throw com_error(E_ABORT);
        }
    };

    com_ptr<IExplorerCommand> host_command()
    {
        com_ptr<IExplorerCommand> command =
            new CExplorerCommand<HostCommand>();

        BOOST_REQUIRE(command);
        return command;
    }
}


/**
 * GetTitle returns the string given in the constructor.
 */
BOOST_AUTO_TEST_CASE( title )
{
    com_ptr<IExplorerCommand> command = host_command();

    wchar_t* ret_val;
    BOOST_REQUIRE_OK(command->GetTitle(NULL, &ret_val));

    shared_ptr<wchar_t> title(ret_val, ::CoTaskMemFree);
    BOOST_REQUIRE_EQUAL(title.get(), L"title");
}

/**
 * GetIcon returns the expected empty string as it wasn't set in the
 * constructor.
 */
BOOST_AUTO_TEST_CASE( icon )
{
    com_ptr<IExplorerCommand> command = host_command();

    wchar_t* ret_val;
    BOOST_REQUIRE_OK(command->GetIcon(NULL, &ret_val));

    shared_ptr<wchar_t> icon(ret_val, ::CoTaskMemFree);
    BOOST_REQUIRE_EQUAL(icon.get(), L"");
}

/**
 * GetToolTip returns the string given in the constructor.
 */
BOOST_AUTO_TEST_CASE( tool_tip )
{
    com_ptr<IExplorerCommand> command = host_command();

    wchar_t* ret_val;
    BOOST_REQUIRE_OK(command->GetToolTip(NULL, &ret_val));

    shared_ptr<wchar_t> tip(ret_val, ::CoTaskMemFree);
    BOOST_REQUIRE_EQUAL(tip.get(), L"tool-tip");
}

/**
 * GetCanonicalName returns the test GUID given in the constructor.
 */
BOOST_AUTO_TEST_CASE( guid )
{
    com_ptr<IExplorerCommand> command = host_command();

    GUID guid;
    BOOST_REQUIRE_OK(command->GetCanonicalName(&guid));
    BOOST_REQUIRE_EQUAL(uuid_t(guid), uuid_t(TEST_GUID));
}

/**
 * GetFlags returns ECF_DEFAULT (0).
 */
BOOST_AUTO_TEST_CASE( flags )
{
    com_ptr<IExplorerCommand> command = host_command();

    EXPCMDFLAGS flags;
    BOOST_REQUIRE_OK(command->GetFlags(&flags));
    BOOST_REQUIRE_EQUAL(flags, 0U);
}

/**
 * GetState returns ECS_ENABLED (0).
 */
BOOST_AUTO_TEST_CASE( state )
{
    com_ptr<IExplorerCommand> command = host_command();

    EXPCMDSTATE flags;
    BOOST_REQUIRE_OK(command->GetState(NULL, false, &flags));
    BOOST_REQUIRE_EQUAL(flags, 0U);
}

/**
 * Invoke returns error that matches exception thrown by throwing_function
 * passed to constructor.
 */
BOOST_AUTO_TEST_CASE( invoke )
{
    com_ptr<IExplorerCommand> command = host_command();

    BOOST_REQUIRE_EQUAL(command->Invoke(NULL, NULL), E_ABORT);
}


namespace {

    const GUID TEST_GUID2 =
        { 0xae4792b2, 0x3b35, 0x4c07,
        { 0x9a, 0x96, 0x2f, 0x33, 0xc5, 0x56, 0xdb, 0x4a } };

    struct CommandNeedingSite : public Command
    {
        CommandNeedingSite() : Command(L"title", TEST_GUID2, L"tool-tip") {}

        presentation_state state(com_ptr<IShellItemArray>, bool) const
        {
            return presentation_state::enabled;
        }

        void operator()(
            com_ptr<IShellItemArray>, const command_site&, com_ptr<IBindCtx>)
        const
        {
            throw com_error(E_ABORT);
        }

        void set_site(com_ptr<IUnknown> ole_site) {}
    };

}

/**
 * A CExplorerCommand must support IObjectWithSite.
 */
BOOST_AUTO_TEST_CASE( support_ole_site )
{
    com_ptr<IExplorerCommand> command =
        new CExplorerCommand<CommandNeedingSite>();

    com_ptr<IObjectWithSite> object_with_site = try_cast(command);
    BOOST_REQUIRE(object_with_site);

    BOOST_REQUIRE_OK(object_with_site->SetSite(NULL));
}

BOOST_AUTO_TEST_SUITE_END();
