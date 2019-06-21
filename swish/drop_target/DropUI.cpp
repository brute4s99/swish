/**
    @file

    User-interaction for DropTarget.

    @if license

    Copyright (C) 2010, 2012, 2013  Alexander Lamaison <awl03@doc.ic.ac.uk>

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

#include "DropUI.hpp"

#include "swish/frontend/announce_error.hpp" // announce_last_exception
#include "swish/trace.hpp" // trace

#include <washer/com/ole_window.hpp> // window_from_ole_window
#include <washer/gui/message_box.hpp> // message_box
#include <washer/gui/progress.hpp>
#include <washer/window/window.hpp>
#include <washer/window/window_handle.hpp>

#include <comet/error.h> // com_error
#include <comet/ptr.h> // com_ptr

#include <boost/locale.hpp> // translate, wformat
#include <boost/noncopyable.hpp>
#include <boost/throw_exception.hpp> // BOOST_THROW_EXCEPTION

#include <cassert> // assert
#include <iosfwd> // wstringstream
#include <string>

using swish::frontend::announce_last_exception;
using swish::tracing::trace;

using washer::com::window_from_ole_window;
using namespace washer::gui::message_box;
using washer::gui::progress;
using washer::window::window;
using washer::window::window_handle;

using comet::com_error;
using comet::com_ptr;

using ssh::filesystem::path;

using boost::locale::translate;
using boost::locale::wformat;
using boost::noncopyable;
using boost::optional;

using std::auto_ptr;
using std::wstringstream;
using std::wstring;

namespace swish {
namespace drop_target {

namespace {

    /**
     * Drain any messages in the queue.
     */
    void do_events()
    {
        MSG msg;
        BOOL result;

        while (::PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE))
        {
            result = ::GetMessage(&msg, NULL, 0, 0);
            if (result == 0) // WM_QUIT
            {
                ::PostQuitMessage(msg.wParam);
                break;
            }
            else if (result == -1)
            {
                return;
            }
            else
            {
                ::TranslateMessage(&msg);
                ::DispatchMessage(&msg);
            }
        }
    }


    /**
     * Exception-safe lifetime manager for an IProgressDialog object.
     *
     * Calls StartProgressDialog when created and StopProgressDialog when
     * destroyed.
     */
    class DropProgress : public noncopyable, public Progress
    {
    public:

        DropProgress(
            const optional< window<wchar_t> >& owner, const wstring& title)
            :
        m_inner(create_dialog(owner, title)) {}

        /**
         * Has the user cancelled the operation via the progress dialogue?
         */
        bool user_cancelled()
        {
            return m_inner.user_cancelled();
        }

        // Because we are no longer doing the transfer in a different COM
        // apartment, which would pump messages during the call, the UI blocks
        // on the drop.  That includes not showing the progress dialog.
        //
        // Therefore, we pump outstanding messages every time there is
        // an update.  I don't think this it the right solution, but we can't
        // run the progress dialog in a different thread as that breaks
        // the windows rules.
        //
        // The UI is still not wonderfully responsive because it can only
        // update a little each time the progress is updated.  We may be able
        // to do better once we use libssh2's non-blocking API as then we
        // can pump messages more frequently.

        /**
         * Set the indexth line of the display to the given text.
         */
        void line(DWORD index, const wstring& text)
        {
            m_inner.line(index, text);
            do_events();
        }

        /**
         * Set the indexth line of the display to the given path.
         *
         * Uses the inbuilt path compression.
         */
        void line_path(DWORD index, const wstring& text)
        {
            m_inner.line_compress_paths_if_needed(index, text);
            do_events();
        }

        /**
         * Update the indicator to show current progress level.
         */
        void update(ULONGLONG so_far, ULONGLONG out_of)
        {
            m_inner.update(so_far, out_of);
            do_events();
        }

        /**
         * Force the dialogue window to disappear.
         *
         * Useful, for instance, to temporarily hide the progress display while
         * displaying other dialogues in the middle of the process whose
         * progress is being monitored.
         */
        void hide()
        {
            optional< window<wchar_t> > window = m_inner.window();
            if (window)
                window->enable(false);
            do_events();
        }

        /**
         * Force the dialogue window to appear.
         *
         * Useful to force the window to appear quicker than it normally would,
         * and to redisplay the window after hiding it.
         *
         * @see hide
         */
        void show()
        {
            optional< window<wchar_t> > window = m_inner.window();
            if (window)
                window->enable(true);
            do_events();
        }

    private:

        static progress create_dialog(
            const optional< window<wchar_t> >& owner, const wstring& title)
        {
            return progress(
                owner, title,
                progress::modality::non_modal,
                progress::time_estimation::automatic_time_estimate,
                progress::bar_type::finite,
                progress::minimisable::yes,
                progress::cancellability::cancellable);
        }

        progress m_inner;
    };

    /**
     * Disables a progress window for duration of its scope and reenables
     * after.
     */
    class ScopedDisabler
    {
    public:
        ScopedDisabler(Progress& progress) : m_progress(progress)
        {
            m_progress.hide();
        }

        ~ScopedDisabler()
        {
            m_progress.show();
        }

    private:
        Progress& m_progress;
    };
}

DropUI::DropUI(const optional< window<wchar_t> >& owner) : m_owner(owner) {}

/**
 * Does user give permission to overwrite remote target file?
 */
bool DropUI::can_overwrite(const path& target)
{
    if (!m_owner)
        return false;

    wstringstream message;
    message << wformat(translate(
        L"This folder already contains a file named '{1}'."))
        % target.filename();
    message << "\n\n";
    message << translate(L"Would you like to replace it?");

    // If the caller has already displayed the progress dialog, we must
    // force-hide it as it gets in the way of other UI
    ScopedDisabler disable_progress(*m_progress);

    button_type::type button = message_box(
        (m_owner) ? m_owner->hwnd() : NULL,
        message.str(), translate(L"Confirm File Replace"),
        box_type::yes_no_cancel, icon_type::question);
    switch (button)
    {
    case button_type::yes:
        return true;
    case button_type::no:
        return false;
    case button_type::cancel:
    default:
        BOOST_THROW_EXCEPTION(com_error(E_ABORT));
    }
}

void DropUI::handle_last_exception()
{
    // Only report errors with a dialog if we are given a window we
    // can use as a dialogue owner.  We can assume if the caller
    // didn't give us one, they don't want UI.
    if (m_owner)
    {
        announce_last_exception(
            m_owner->hwnd(), translate(L"Unable to transfer files"),
            translate(
                L"You might not have permission to write to this "
                L"directory."));
    }

    throw;
}

namespace {

class DummyProgress : public Progress
{
public:
    virtual bool user_cancelled()
    {
        return false;
    };

    virtual void line(DWORD, const std::wstring&) {}
    virtual void line_path(DWORD, const std::wstring&) {}
    virtual void update(ULONGLONG, ULONGLONG) {}
    virtual void hide() {}
    virtual void show() {}
};

}

/**
 * Pass ownership of a progress display scope to caller.
 *
 * We hang on to the progress dialog so that we can hide it if and when we
 * show other dialogs (something the built-in Explorer FTP extension doesn't
 * do and really should).
 *
 * The caller gets a Progress object whose lifetime determines when the dialog
 * is started and ended.  When it goes out of scope the dialog is stopped and
 * disappears.  In other words, the progress dialog is safely stopped even
 * if an exception is thrown.
 */
auto_ptr<Progress> DropUI::progress()
{
    auto_ptr<Progress> p;
    if (m_owner)
    {
        p = auto_ptr<Progress>(
            new DropProgress(m_owner, translate(L"Progress", L"Copying...")));
    }
    else
    {
        p = auto_ptr<Progress>(new DummyProgress());
    }

    // HACK: we keep a raw copy of the pointer so we can hide the progress
    // if needed later when displaying the confirm-overwrite box.  There
    // has got to be a safer way to do this.
    m_progress = p.get();

    return p;
}

}} // namespace swish::drop_target
