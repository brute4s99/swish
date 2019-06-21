// Copyright 2007, 2008, 2009, 2010, 2011, 2013, 2015, 2016 Alexander Lamaison

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

#include "HostFolder.h"

#include "Registry.h" // For saved connection details
#include "RemoteFolder.h"
#include "swish/debug.hpp"
#include "swish/frontend/UserInteraction.hpp" // CUserInteraction
#include "swish/host_folder/columns.hpp"      // property_key_from_column_index
#include "swish/host_folder/commands/Remove.hpp"
#include "swish/host_folder/commands/Rename.hpp"
#include "swish/host_folder/commands/commands.hpp" // host_folder_commands
#include "swish/host_folder/context_menu_callback.hpp"
#include "swish/host_folder/extract_icon.hpp"
#include "swish/host_folder/host_management.hpp"
#include "swish/host_folder/host_pidl.hpp"       // host_itemid_view,
                                                 // find_host_itemid
                                                 // url_from_host_itemid
#include "swish/host_folder/ViewCallback.hpp"    // CViewCallback
#include "swish/host_folder/host_management.hpp" // RemoveConnectionFromRegistry
#include "swish/host_folder/overlay_icon.hpp"
#include "swish/host_folder/properties.hpp" // property_from_pidl
#include "swish/remotelimits.h"             // Text field limits
#include "swish/shell/shell_item_array.hpp" // shell_item_array_from_folder_items
#include "swish/trace.hpp"                  // trace
#include "swish/windows_api.hpp"            // SHBindToParent

#include <washer/com/catch.hpp>   // WASHER_COM_CATCH_INTERFACE
#include <washer/shell/shell.hpp> // strret_to_string
#include <washer/window/window.hpp>
#include <washer/window/window_handle.hpp>

#include <comet/error.h>      // com_error
#include <comet/smart_enum.h> // make_smart_enumeration

#include <strsafe.h> // For StringCchCopy

#include <boost/locale.hpp> // translate
#include <boost/optional/optional.hpp>
#include <boost/throw_exception.hpp> // BOOST_THROW_EXCEPTION

#include <cassert> // assert
#include <cstring> // memset
#include <memory>
#include <string>
#include <vector>

using ATL::CComPtr;
using ATL::CComObject;

using comet::com_error;
using comet::com_error_from_interface;
using comet::com_ptr;
using comet::make_smart_enumeration;
using comet::throw_com_error;
using comet::variant_t;

using boost::locale::translate;
using boost::optional;
using boost::shared_ptr;

using std::make_shared;
using std::vector;
using std::wstring;

using swish::frontend::CUserInteraction;
using swish::host_folder::CViewCallback;
using swish::host_folder::commands::host_folder_command_provider;
using swish::host_folder::commands::Rename;
using swish::host_folder::commands::Remove;
using swish::host_folder::context_menu_callback;
using swish::host_folder::create_host_itemid;
using swish::host_folder::extract_icon_co;
using swish::host_folder::find_host_itemid;
using swish::host_folder::host_itemid_view;
using swish::host_folder::host_management::FindConnectionInRegistry;
using swish::host_folder::host_management::LoadConnectionsFromRegistry;
using swish::host_folder::host_management::RenameConnectionInRegistry;
using swish::host_folder::overlay_icon;
using swish::host_folder::property_from_pidl;
using swish::host_folder::property_key_from_column_index;
using swish::host_folder::url_from_host_itemid;
using swish::nse::Command;
using swish::shell::shell_item_array_from_folder_items;
using swish::tracing::trace;

using washer::shell::pidl::cpidl_t;
using washer::shell::pidl::apidl_t;
using washer::shell::pidl::pidl_t;
using washer::shell::property_key;
using washer::shell::strret_to_string;
using washer::shell::string_to_strret;
using washer::window::window;
using washer::window::window_handle;

namespace comet
{

template <>
struct comtype<::IEnumIDList>
{
    static const ::IID& uuid() throw()
    {
        return ::IID_IEnumIDList;
    }
    typedef ::IUnknown base;
};

template <>
struct enumerated_type_of<IEnumIDList>
{
    typedef PITEMID_CHILD is;
};

template <>
struct comtype<IQueryAssociations>
{
    static const IID& uuid()
    {
        return IID_IQueryAssociations;
    }
    typedef ::IUnknown base;
};

template <>
struct comtype<::IExtractIconW>
{
    static const ::IID& uuid() throw()
    {
        return ::IID_IExtractIconW;
    }
    typedef ::IUnknown base;
};

/**
 * Copy policy used to create IEnumIDList from cpidl_t.
 */
template <>
struct impl::type_policy<PITEMID_CHILD>
{
    static void init(PITEMID_CHILD& raw_pidl, const cpidl_t& pidl)
    {
        pidl.copy_to(raw_pidl);
    }

    static void clear(PITEMID_CHILD& raw_pidl)
    {
        ::CoTaskMemFree(raw_pidl);
    }
};

template <>
struct comtype<::IShellIconOverlay>
{
    static const ::IID& uuid() throw()
    {
        return ::IID_IShellIconOverlay;
    }
    typedef ::IUnknown base;
};
}

/*--------------------------------------------------------------------------*/
/*      Functions implementing IShellFolder via folder_error_adapter.       */
/*--------------------------------------------------------------------------*/

/**
 * Create an IEnumIDList which enumerates the items in this folder.
 *
 * @implementing folder_error_adapter
 *
 * @param hwnd   Optional window handle used if enumeration requires user
 *               input.
 * @param flags  Flags specifying which types of items to include in the
 *               enumeration. Possible flags are from the @c SHCONT enum.
 *
 * @retval S_FALSE if the are no matching items to enumerate.
 */
IEnumIDList* CHostFolder::enum_objects(HWND hwnd, SHCONTF flags)
{
    UNREFERENCED_PARAMETER(hwnd); // No UI required to access registry

    // This folder only contains folders
    if (!(flags & SHCONTF_FOLDERS) ||
        (flags & (SHCONTF_NETPRINTERSRCH | SHCONTF_SHAREABLE)))
        return NULL;

    // Load connections from HKCU\Software\Swish\Connections
    return make_smart_enumeration<IEnumIDList>(
               make_shared<vector<cpidl_t>>(LoadConnectionsFromRegistry()))
        .detach();
}

/**
 * Convert path string relative to this folder into a PIDL to the item.
 *
 * @implementing folder_error_adapter
 *
 * @todo  Handle the attributes parameter.  Should just return
 * GetAttributesOf() the PIDL we create but it is a bit hazy where the
 * host PIDL's responsibilities end and the remote PIDL's start because
 * of the path embedded in the host PIDL.
 */
PIDLIST_RELATIVE CHostFolder::parse_display_name(HWND hwnd, IBindCtx* bind_ctx,
                                                 const wchar_t* display_name,
                                                 ULONG* attributes_inout)
{
    trace(__FUNCTION__ " called (display_name=%s)") % display_name;

    // The string we are trying to parse should be of the form:
    //    sftp://username@hostname:port/path

    wstring strDisplayName(display_name);
    if (strDisplayName.empty())
    {
        PIDLIST_RELATIVE pidl;
        root_pidl().copy_to(pidl);
        return pidl;
    }

    // Must start with sftp://
    if (strDisplayName.substr(0, 7) != L"sftp://")
        BOOST_THROW_EXCEPTION(com_error(E_FAIL));

    // Must have @ to separate username from hostname
    wstring::size_type nAt = strDisplayName.find_first_of(L'@', 7);
    if (nAt == wstring::npos)
        BOOST_THROW_EXCEPTION(com_error(E_FAIL));

    // Must have : to separate hostname from port number
    wstring::size_type nColon = strDisplayName.find_first_of(L':', 7);
    if (nAt == wstring::npos)
        BOOST_THROW_EXCEPTION(com_error(E_FAIL));
    if (nColon <= nAt)
        BOOST_THROW_EXCEPTION(com_error(E_FAIL));

    // Must have / to separate port number from path
    wstring::size_type nSlash = strDisplayName.find_first_of(L'/', 7);
    if (nAt == wstring::npos)
        BOOST_THROW_EXCEPTION(com_error(E_FAIL));
    if (nColon <= nAt)
        BOOST_THROW_EXCEPTION(com_error(E_FAIL));

    wstring strUser = strDisplayName.substr(7, nAt - 7);
    wstring strHost = strDisplayName.substr(nAt + 1, nColon - (nAt + 1));
    wstring strPort = strDisplayName.substr(nColon + 1, nAt - (nSlash + 1));
    wstring strPath = strDisplayName.substr(nSlash + 1);
    if (strUser.empty() || strHost.empty() || strPort.empty() ||
        strPath.empty())
        BOOST_THROW_EXCEPTION(com_error(E_FAIL));

    int nPort = _wtoi(strPort.c_str());
    if (nPort < MIN_PORT || nPort > MAX_PORT)
        BOOST_THROW_EXCEPTION(com_error(E_FAIL));

    // Create child PIDL for this path segment
    cpidl_t pidl = create_host_itemid(strHost, strUser, strPath, nPort);

    com_ptr<IShellFolder> subfolder;
    bind_to_object(pidl.get(), bind_ctx, subfolder.iid(),
                   reinterpret_cast<void**>(subfolder.out()));

    wchar_t wszPath[MAX_PATH];
    ::StringCchCopyW(wszPath, ARRAYSIZE(wszPath), strPath.c_str());

    pidl_t pidl_path;
    HRESULT hr = subfolder->ParseDisplayName(hwnd, bind_ctx, wszPath, NULL,
                                             pidl_path.out(), attributes_inout);
    if (FAILED(hr))
        throw_com_error(subfolder.get(), hr);

    pidl_t pidl_out = root_pidl() + pidl_path;
    return pidl_out.detach();
}

/**
 * Retrieve the display name for the specified file object or subfolder.
 *
 * @implementing folder_error_adapter
 */
STRRET CHostFolder::get_display_name_of(PCUITEMID_CHILD pidl, SHGDNF flags)
{
    if (::ILIsEmpty(pidl))
        BOOST_THROW_EXCEPTION(com_error(E_INVALIDARG));

    wstring name;

    if (flags & SHGDN_FORPARSING)
    {
        if (!(flags & SHGDN_INFOLDER))
        {
            // Bind to parent
            com_ptr<IShellFolder> parent;
            PCUITEMID_CHILD pidlThisFolder = NULL;
            HRESULT hr = swish::windows_api::SHBindToParent(
                root_pidl().get(), parent.iid(),
                reinterpret_cast<void**>(parent.out()), &pidlThisFolder);

            if (FAILED(hr))
                BOOST_THROW_EXCEPTION(com_error(hr));

            STRRET strret;
            std::memset(&strret, 0, sizeof(strret));
            hr = parent->GetDisplayNameOf(pidlThisFolder, flags, &strret);
            if (FAILED(hr))
                throw_com_error(parent.get(), hr);

            name = strret_to_string<wchar_t>(strret, pidlThisFolder) + L'\\';
        }

        name += url_from_host_itemid(pidl, true);
    }
    else if (flags == SHGDN_NORMAL || flags & SHGDN_FORADDRESSBAR)
    {
        name = url_from_host_itemid(pidl, false);
    }
    else if (flags == SHGDN_INFOLDER || flags & SHGDN_FOREDITING)
    {
        name = host_itemid_view(pidl).label();
    }
    else
    {
        UNREACHABLE;
        BOOST_THROW_EXCEPTION(com_error(E_INVALIDARG));
    }

    return string_to_strret(name);
}

namespace
{

void notify_shell_that_rename_occurred(const apidl_t& old_pidl,
                                       const apidl_t& new_pidl)
{
    ::SHChangeNotify(SHCNE_RENAMEFOLDER, SHCNF_IDLIST, old_pidl.get(),
                     new_pidl.get());
}
}

/**
 * Rename item.
 */
PITEMID_CHILD CHostFolder::set_name_of(HWND /*hwnd*/, PCUITEMID_CHILD pidl,
                                       const wchar_t* new_label,
                                       SHGDNF /*flags*/)
{
    wstring from_label = host_itemid_view(pidl).label();
    RenameConnectionInRegistry(from_label, new_label);
    optional<cpidl_t> connection = FindConnectionInRegistry(new_label);
    if (connection)
    {
        notify_shell_that_rename_occurred(root_pidl() + pidl,
                                          root_pidl() + *connection);
        return connection->detach();
    }
    else
    {
        return NULL;
    }
}

/**
 * Returns the attributes for the items whose PIDLs are passed in.
 *
 * @implementing folder_error_adapter
 */
void CHostFolder::get_attributes_of(UINT pidl_count,
                                    PCUITEMID_CHILD_ARRAY pidl_array,
                                    SFGAOF* attributes_inout)
{
    com_ptr<IShellItemArray> selection =
        shell_item_array_from_folder_items(this, pidl_count, pidl_array);

    DWORD dwAttribs = 0;
    dwAttribs |= SFGAO_FOLDER;
    dwAttribs |= SFGAO_HASSUBFOLDER;

    // This adds a 'rename' item to the default context menu that SetNameOf
    // directly on the IShellFolder
    Rename rename_command;
    if (rename_command.state(selection, false) ==
        Command::presentation_state::enabled)
    {
        dwAttribs |= SFGAO_CANRENAME;
    }

    // This adds an 'delete' item to the default context menu that calls the
    // menu handler with ID DFM_CMD_DELETE
    Remove remove_command(root_pidl());
    if (remove_command.state(selection, false) ==
        Command::presentation_state::enabled)
    {
        dwAttribs |= SFGAO_CANDELETE;
    }

    *attributes_inout &= dwAttribs;
}

/*--------------------------------------------------------------------------*/
/*     Functions implementing IShellFolder2 via folder2_error_adapter.      */
/*--------------------------------------------------------------------------*/

/**
 * Convert column index to matching PROPERTYKEY, if any.
 *
 * @implementing folder2_error_adapter
 */
SHCOLUMNID CHostFolder::map_column_to_scid(UINT column_index)
{
    return property_key_from_column_index(column_index).get();
}

/*--------------------------------------------------------------------------*/
/*                    Functions implementing IShellIconOverlay              */
/*--------------------------------------------------------------------------*/

STDMETHODIMP CHostFolder::GetOverlayIndex(PCUITEMID_CHILD item, int* index)
{
    try
    {
        overlay_icon overlay(item);

        if (overlay.has_overlay())
        {
            *index = overlay.index();
            return S_OK;
        }
        else
        {
            return S_FALSE;
        }
    }
    WASHER_COM_CATCH_INTERFACE(IShellIconOverlay)
}

STDMETHODIMP CHostFolder::GetOverlayIconIndex(PCUITEMID_CHILD item,
                                              int* icon_index)
{
    try
    {
        overlay_icon overlay(item);

        if (overlay.has_overlay())
        {
            *icon_index = overlay.icon_index();
            return S_OK;
        }
        else
        {
            return S_FALSE;
        }
    }
    WASHER_COM_CATCH_INTERFACE(IShellIconOverlay)
}

/*--------------------------------------------------------------------------*/
/*                     CFolder NVI internal interface.                      */
/* These method implement the internal interface of the CFolder abstract    */
/* class                                                                    */
/*--------------------------------------------------------------------------*/

/**
 * Return the folder's registered CLSID
 *
 * @implementing CFolder
 */
CLSID CHostFolder::clsid() const
{
    return __uuidof(this);
}

/**
 * Sniff PIDLs to determine if they are of our type.  Throw if not.
 *
 * @implementing CFolder
 */
void CHostFolder::validate_pidl(PCUIDLIST_RELATIVE pidl) const
{
    if (pidl == NULL)
        BOOST_THROW_EXCEPTION(com_error(E_POINTER));

    if (!host_itemid_view(pidl).valid())
        BOOST_THROW_EXCEPTION(com_error(E_INVALIDARG));
}

namespace
{

com_ptr<ISftpConsumer> consumer_factory(HWND hwnd)
{
    return new CUserInteraction(hwnd);
}
}

/**
 * Create and initialise new folder object for subfolder.
 *
 * @implementing CFolder
 *
 * Create CRemoteFolder initialised with its root PIDL.  CHostFolders
 * don't have any other types of subfolder.
 */
CComPtr<IShellFolder> CHostFolder::subfolder(const cpidl_t& pidl)
{
    CComPtr<IShellFolder> folder =
        CRemoteFolder::Create((root_pidl() + pidl).get(), consumer_factory);
    ATLENSURE_THROW(folder, E_NOINTERFACE);

    return folder;
}

/**
 * Return a property, specified by PROERTYKEY, of an item in this folder.
 */
variant_t CHostFolder::property(const property_key& key, const cpidl_t& pidl)
{
    return property_from_pidl(pidl, key);
}

/*--------------------------------------------------------------------------*/
/*                    CSwishFolder internal interface.                      */
/* These method override the (usually no-op) implementations of some        */
/* in the CSwishFolder base class                                           */
/*--------------------------------------------------------------------------*/

/**
 * Create a toolbar command provider for the folder.
 */
CComPtr<IExplorerCommandProvider>
    CHostFolder::command_provider(HWND /*owning_hwnd*/)
{
    TRACE("Request: IExplorerCommandProvider");

    return host_folder_command_provider(root_pidl()).get();
}

/**
 * Create an icon extraction helper object for the selected item.
 *
 * @implementing CSwishFolder
 */
CComPtr<IExtractIconW> CHostFolder::extract_icon_w(HWND hwnd_view,
                                                   PCUITEMID_CHILD pidl)
{
    optional<window<wchar_t>> owning_view;
    if (hwnd_view)
        owning_view = window<wchar_t>(window_handle::foster_handle(hwnd_view));

    return new extract_icon_co(owning_view, pidl);
}

/**
 * Create a file association handler for host items.
 *
 * @implementing CSwishFolder
 *
 * We don't need to look at the PIDLs as all host items are the same.
 */
CComPtr<IQueryAssociations>
    CHostFolder::query_associations(HWND /*hwnd*/, UINT /*cpidl*/,
                                    PCUITEMID_CHILD_ARRAY /*apidl*/)
{
    TRACE("Request: IQueryAssociations");

    CComPtr<IQueryAssociations> spAssoc;
    HRESULT hr = ::AssocCreate(CLSID_QueryAssociations, IID_PPV_ARGS(&spAssoc));
    ATLENSURE_SUCCEEDED(hr);

    // Get CLSID in {DWORD-WORD-WORD-WORD-WORD.DWORD} form
    LPOLESTR posz;
    ::StringFromCLSID(__uuidof(CHostFolder), &posz);
    shared_ptr<OLECHAR> clsid(posz, ::CoTaskMemFree);
    posz = NULL;

    // Initialise default assoc provider to use Swish CLSID key for data.
    // This is necessary to pick up properties and TileInfo etc.
    hr = spAssoc->Init(0, clsid.get(), NULL, NULL);
    if (FAILED(hr))
        BOOST_THROW_EXCEPTION(com_error_from_interface(spAssoc.p, hr));

    return spAssoc;
}

namespace
{

HRESULT CALLBACK menu_callback(IShellFolder* folder, HWND hwnd_view,
                               IDataObject* selection, UINT message_id,
                               WPARAM wparam, LPARAM lparam)
{
    CRemoteFolder* remote_folder = static_cast<CRemoteFolder*>(folder);

    context_menu_callback callback(remote_folder->root_pidl());
    return callback(hwnd_view, selection, message_id, wparam, lparam);
}
}

/**
 * Create a context menu for the selected items.
 *
 * @implementing CSwishFolder
 */
CComPtr<IContextMenu> CHostFolder::context_menu(HWND hwnd, UINT cpidl,
                                                PCUITEMID_CHILD_ARRAY apidl)
{
    TRACE("Request: IContextMenu");
    assert(cpidl > 0);

    // Get keys associated with filetype from registry.
    // We only take into account the item that was right-clicked on
    // (the first array element) even if this was a multi-selection.
    //
    // This article says that we don't need to specify the keys:
    // http://groups.google.com/group/microsoft.public.platformsdk.shell/
    // browse_thread/thread/6f07525eaddea29d/
    // but we do for the context menu to appear in versions of Windows
    // earlier than Vista.
    HKEY* akeys;
    UINT ckeys;
    ATLENSURE_THROW(
        SUCCEEDED(CRegistry::GetHostFolderAssocKeys(&ckeys, &akeys)),
        E_UNEXPECTED // Might fail if registry is corrupted
        );

    CComPtr<IShellFolder> spThisFolder = this;
    ATLENSURE_THROW(spThisFolder, E_OUTOFMEMORY);

    // Create default context menu from list of PIDLs
    CComPtr<IContextMenu> spMenu;
    HRESULT hr = ::CDefFolderMenu_Create2(root_pidl().get(), hwnd, cpidl, apidl,
                                          spThisFolder, menu_callback, ckeys,
                                          akeys, &spMenu);
    if (FAILED(hr))
        BOOST_THROW_EXCEPTION(com_error(hr));

    return spMenu;
}

/**
 * Create a data object for the selected items.
 *
 * @implementing CSwishFolder
 */
CComPtr<IDataObject> CHostFolder::data_object(HWND /*hwnd*/, UINT cpidl,
                                              PCUITEMID_CHILD_ARRAY apidl)
{
    TRACE("Request: IDataObject");
    assert(cpidl > 0);

    // A DataObject is required in order for the call to
    // CDefFolderMenu_Create2 (above) to succeed on versions of Windows
    // earlier than Vista

    CComPtr<IDataObject> spdo;
    HRESULT hr = ::CIDLData_CreateFromIDArray(
        root_pidl().get(), cpidl,
        reinterpret_cast<PCUIDLIST_RELATIVE_ARRAY>(apidl), &spdo);
    if (FAILED(hr))
        BOOST_THROW_EXCEPTION(com_error(hr));

    return spdo;
}

/**
 * Create an instance of our Shell Folder View callback handler.
 *
 * @implementing CSwishFolder
 */
CComPtr<IShellFolderViewCB> CHostFolder::folder_view_callback(HWND /*hwnd*/)
{
    return new CViewCallback(root_pidl());
}
