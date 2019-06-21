/*  WTL dialog box for keyboard-interactive requests.

    Copyright (C) 2008, 2013  Alexander Lamaison <awl03@doc.ic.ac.uk>

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
*/

#include "KbdInteractiveDialog.h"

#include "swish/utils.hpp"

#include <boost/foreach.hpp> // BOOST_FOREACH
#include <boost/locale.hpp> // translate

#include "wtl.hpp"          // WTL
#include <atlctrls.h>       // WTL control wrappers
#include <atlstr.h> // CString

using swish::utils::Utf8StringToWideString;
using swish::utils::WideStringToUtf8String;
using boost::locale::translate;

using WTL::CStatic;
using WTL::CEdit;
using WTL::CButton;
using WTL::CClientDC;
using WTL::CFontHandle;

using ATL::CString;

using std::pair;
using std::string;
using std::vector;

#define SEPARATION 10
#define MINI_SEPARATION 3
#define RESPONSE_BOX_HEIGHT 22


CKbdInteractiveDialog::CKbdInteractiveDialog(
    const std::string& title, const std::string& instructions,
    const std::vector<std::pair<std::string, bool>>& prompts)
    :
m_title(title), m_instructions(instructions), m_prompts(prompts) {}

vector<string> CKbdInteractiveDialog::GetResponses()
{
    return m_responses;
}

/*----------------------------------------------------------------------------*
 * Event Handlers
 *----------------------------------------------------------------------------*/

LRESULT CKbdInteractiveDialog::OnInitDialog(UINT, WPARAM, LPARAM, BOOL&)
{
    // If server specifies a title use it as the dialogue title
    if (!m_title.empty())
        this->SetWindowText(
            Utf8StringToWideString(m_title).c_str());
    else
        this->SetWindowText(
            translate(L"Keyboard-interactive request").str().c_str());

    // Get size of this dialogue box
    CRect rectDialog;
    this->GetClientRect(rectDialog);

    // Control drawing 'cursor' - increment each time we move down the window
    CPoint point(0,0);

    // Draw instruction label
    CRect rect = _DrawInstruction(rectDialog);
    point.Offset(rect.left, rect.Height()+2*SEPARATION);

    // Draw prompts and response boxes
    typedef pair<string, bool> prompt_pair;
    BOOST_FOREACH(const prompt_pair& prompt, m_prompts)
    {
        CRect rectPrompt = _DrawPrompt(prompt.first, point, rectDialog);

        // Increment point by height of prompt text plus small separation
        point.Offset(0, rectPrompt.Height() + MINI_SEPARATION);

        CRect rectResponse = _DrawResponseBox(
            !prompt.second, point, rectDialog);

        // Increment point by height of response box plus separation
        point.Offset(0, rectResponse.Height() + SEPARATION);
    }

    // Move OK and Cancel below prompts
    CRect rectOKCancel = _DrawOKCancel(point, rectDialog);

    // Expand dialogue downward to include all controls
    rectDialog.bottom = rectOKCancel.bottom + SEPARATION;
    ATLVERIFY( this->ResizeClient(rectDialog.Width(), rectDialog.Height()) );

    // Place dialogue and set focus
    CenterWindow();
    if (m_vecResponseWindows.size() && m_vecResponseWindows[0])
        ::SetFocus(m_vecResponseWindows[0]);

    return 0;
}

LRESULT CKbdInteractiveDialog::OnOK(WORD, WORD wID, HWND, BOOL&)
{
    _ExchangeData();
    EndDialog(wID);
    return 0;
}

LRESULT CKbdInteractiveDialog::OnCancel(WORD, WORD wID, HWND, BOOL&)
{
    EndDialog(wID);
    return 0;
}

/*----------------------------------------------------------------------------*
 * Private functions
 *----------------------------------------------------------------------------*/

CRect CKbdInteractiveDialog::_DrawInstruction(CRect rectDialog)
{
    // Fix instruction text's width to 20px fewer than the dialog and centre
    CRect rect(0, 0, rectDialog.Width()-20, 0);
    rect.OffsetRect(10, 10);

    // Always draw the instruction label even if empty to override resource text
    CStatic instruction(GetDlgItem(IDC_INSTRUCTION));
    instruction.SetWindowText(Utf8StringToWideString(m_instructions).c_str());

    CFontHandle font, fontOld;
    font = instruction.GetFont();

    // Calculate neccessary size of instruction label
    CClientDC dc(instruction);
    fontOld = dc.SelectFont(font);
    dc.DrawText(Utf8StringToWideString(m_instructions).c_str(), -1, rect,
        DT_CALCRECT | DT_WORDBREAK | DT_NOPREFIX);
    dc.SelectFont(fontOld);

    // Set instruction size, position and text
    instruction.MoveWindow(rect);
    instruction.SetWindowText(Utf8StringToWideString(m_instructions).c_str());

    // Return instruction's rectangle
    return rect;
}

CRect CKbdInteractiveDialog::_DrawPrompt(
    const string& prompt, CPoint point, CRect rectDialog)
{
    // Use same font as instruction label
    CStatic instruction(GetDlgItem(IDC_INSTRUCTION));
    CFontHandle font, fontOld;
    font = instruction.GetFont();

    // Prompt label
    CStatic prompt_label;
    prompt_label.Create(*this, NULL, NULL, 
        WS_VISIBLE | WS_CHILD | SS_WORDELLIPSIS | SS_NOPREFIX);

    // Fix prompt text's width to 20px fewer than the dialog
    CRect rect(0, 0, rectDialog.Width()-20, 0);

    // Calculate necessary (vertical) size of prompt label
    CClientDC dcPrompt(prompt_label);
    fontOld = dcPrompt.SelectFont(font);
    dcPrompt.DrawText(
        Utf8StringToWideString(prompt).c_str(), -1, rect,
        DT_CALCRECT | DT_WORD_ELLIPSIS | DT_NOPREFIX);
    dcPrompt.SelectFont(fontOld);

    // Set prompt size, position, font and text
    rect.OffsetRect(point);
    prompt_label.MoveWindow(rect);
    prompt_label.SetFont(font);
    prompt_label.SetWindowText(Utf8StringToWideString(prompt).c_str());

    // Return prompt's rectangle
    return rect;
}

CRect CKbdInteractiveDialog::_DrawResponseBox(
    bool fHideResponse, CPoint point, CRect rectDialog)
{
    // Use same font as instruction label
    CStatic instruction(GetDlgItem(IDC_INSTRUCTION));
    CFontHandle font = instruction.GetFont();

    // Response text box
    CEdit edit;
    DWORD dwWindowFlags = WS_VISIBLE | WS_CHILD | WS_TABSTOP | ES_AUTOHSCROLL;
    if (fHideResponse)
    {
        dwWindowFlags |= ES_PASSWORD;
    }
    DWORD dwWindowFlagsEx = WS_EX_CLIENTEDGE;
    edit.Create(*this, NULL, NULL, dwWindowFlags, dwWindowFlagsEx);
    m_vecResponseWindows.push_back(edit); // Push onto list of response boxes

    // Fix response box's width to 20px fewer than the dialog
    CRect rect(0, 0, rectDialog.Width()-20, RESPONSE_BOX_HEIGHT);

    // Set response size, position and font
    rect.MoveToXY(point.x, point.y);
    edit.MoveWindow(rect);
    edit.SetFont(font);

    // Return response box's rectangle
    return rect;
}

CRect CKbdInteractiveDialog::_DrawOKCancel(
    CPoint point, CRect rectDialog)
{
    CButton btnOK(GetDlgItem(IDOK)), btnCancel(GetDlgItem(IDCANCEL));

    CRect rectOK, rectCancel;
    ATLVERIFY( btnOK.GetClientRect(rectOK) );
    ATLVERIFY( btnCancel.GetClientRect(rectCancel) );
    rectCancel.MoveToXY(
        rectDialog.right - rectCancel.Width() - SEPARATION,
        point.y + SEPARATION);
    rectOK.MoveToXY(
        rectDialog.right - rectCancel.Width() - rectOK.Width() - 2*SEPARATION,
        point.y + SEPARATION);
    ATLVERIFY( btnOK.MoveWindow(rectOK) );
    ATLVERIFY( btnCancel.MoveWindow(rectCancel) );

    // Return the union of the two buttons' rectangles
    return rectOK | rectCancel;
}

/**
 * Copy data from response Win32 edit boxes into member variable.
 *
 * This is necessary as the dialogue and its text boxed are destroyed when OK
 * or Cancel is clicked.  Therefore, this function must be called in the
 * OK button click event handler.  The responses can be retrieved from the
 * member variable using the GetResonses() function after the
 * dialogue window has been destroyed.
 */
void CKbdInteractiveDialog::_ExchangeData()
{
    m_responses.clear();

    for each (HWND hwnd in m_vecResponseWindows)
    {
        CEdit edit(hwnd);
        CString strResponse;
        edit.GetWindowText(strResponse);
        m_responses.push_back(WideStringToUtf8String(strResponse.GetString()));
    }
}
