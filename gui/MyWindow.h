#ifndef MYWINDOW_H_INCLUDED
#define MYWINDOW_H_INCLUDED
#include "../stdafx.h"
#include "../CLibretro.h"
#include "DropFileTarget.h"
#include "DlgTabCtrl.h"
#include "utf8conv.h"
#include <cmath>
#include "PropertyGrid.h"
#include <iostream>
#include <string>
#include <sstream>
#include <iomanip>
#include "../3rdparty/ini.h"
#include "../3rdparty/libretro.h"
#include <shlwapi.h>
#include "../3rdparty/cmdline.h"

using namespace std;
using namespace utf8util;

#include "Options.h"
#include "../gitver.h"
class CAboutDlg : public CDialogImpl<CAboutDlg>
{
    CHyperLink website;
    CStatic version_number;
    CStatic builddate;
    CEdit greets;
public:
    enum { IDD = IDD_ABOUT };
    BEGIN_MSG_MAP(CAboutDlg)
        MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialogView1)
        COMMAND_ID_HANDLER(IDOK, OnCancel)
    END_MSG_MAP()

    CAboutDlg() { }

    LRESULT OnCancel(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
    {
        EndDialog(wID);
        return 0;
    }


    LRESULT OnInitDialogView1(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
    {
        //CString verinf0 = SVN_REVISION;
        CString date = "Built on " __DATE__ " at " __TIME__ " (GMT+10)";
        CString verinfo = "einwegger�t public build " GIT_VERSION;

        CString greetz;

        CenterWindow();

        greets = GetDlgItem(IDC_CREDITS);
        builddate = GetDlgItem(IDC_BUILDDATE);
        builddate.SetWindowText(date);
        version_number = GetDlgItem(IDC_APPVER);
        version_number.SetWindowText(verinfo);

        greetz += "einwegger�t\r\n";
        greetz += "-----------\r\n";
        greetz += "Keys: \r\n";
        greetz += "F1 : Load Savestate\r\n";
        greetz += "F2 : Save Savestate\r\n";
        greetz += "F3 : Reset\r\n";
        greetz += "-----------\r\n";
        greetz += "Commandline variables:\r\n";
        greetz += "-r (game filename)\r\n";
        greetz += "-c (core filename)\r\n";
        greetz += "-q : Per-game configuration\r\n";
        greetz += "\n";
        greetz += "Example: einweggerat.exe -r somerom.sfc  -c snes9x_libretro.dll\r\n";
        greetz += "-----------\r\n";
        greetz += "Greetz:\r\n";
        greetz += "Higor Eur�pedes\r\n";
        greetz += "Andre Leiradella\r\n";
        greetz += "Daniel De Matteis\r\n";
        greetz += "Andr�s Su�rez\r\n";
        greetz += "Brad Parker\r\n";
        greetz += "Chris Snowhill\r\n";
        greetz += "Hunter Kaller\r\n";
        greetz += "Alfred Agrell\r\n";

        greets.SetWindowText(greetz);
        website.SubclassWindow(GetDlgItem(IDC_LINK));
        website.SetHyperLink(_T("http://rebote.net"));
        return TRUE;
    }
};


class CMyWindow : public CFrameWindowImpl<CMyWindow>, public CDropFileTarget<CMyWindow>, public CMessageFilter
{
public:
    DECLARE_FRAME_WND_CLASS(_T("emu_wtl"), NULL);
    BEGIN_MSG_MAP_EX(CMyWindow)
        MESSAGE_HANDLER(WM_CREATE, OnCreate)
        MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
        MESSAGE_HANDLER(WM_SIZE, OnSize)
        MESSAGE_HANDLER(WM_SETCURSOR,OnSetCursor)
        COMMAND_ID_HANDLER(ID_OPTIONS, OnOptions)
        COMMAND_ID_HANDLER_EX(IDC_EXIT, OnFileExit)
        COMMAND_ID_HANDLER(ID_ABOUT, OnAbout)
        COMMAND_ID_HANDLER(ID_LOADSAVESTATE, OnLoadState)
        COMMAND_ID_HANDLER(ID_SAVESTATE, OnSaveState)
        COMMAND_ID_HANDLER(ID_FILE_OPENROM, OnFileOpen)
        COMMAND_ID_HANDLER(ID_RESET, OnReset)
        CHAIN_MSG_MAP(CFrameWindowImpl<CMyWindow>)
        CHAIN_MSG_MAP(CDropFileTarget<CMyWindow>)
    END_MSG_MAP()

    struct libretro_core {
        std::tstring core_system; //filename
        std::tstring core_exts; //filename
        std::tstring core_path;
    };

    CLibretro *emulator;
    TCHAR rom_path[MAX_PATH + 1];
    TCHAR core_path[MAX_PATH + 1];
    input*    input_device;
    HACCEL    m_haccelerator;
    std::vector<libretro_core> cores;

    LRESULT OnLoadState(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
    {
        CHAR szFileName[MAX_PATH];
        LPCTSTR sFiles =
            L"Savestates (*.state)\0*.state\0"
            L"All Files (*.*)\0*.*\0\0";
        CFileDialog dlg(TRUE, NULL, NULL, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT, sFiles);
        if (dlg.DoModal() == IDOK)
        {
            emulator->savestate(dlg.m_szFileName);
            // do stuff
        }

        return 0;
    }



    LRESULT OnSetCursor(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
    {
        POINT pos;
        if (LOWORD(lParam) == HTCLIENT)
        {
            SetCursor(NULL);
            return true;
        }
        return DefWindowProc(uMsg, wParam, lParam);
    }

    LRESULT OnReset(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
    {
        emulator->reset();

        return 0;
    }

    LRESULT OnSaveState(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
    {
        CHAR szFileName[MAX_PATH];
        LPCTSTR sFiles =
            L"Savestates (*.state)\0*.state\0"
            L"All Files (*.*)\0*.*\0\0";
        CFileDialog dlg(FALSE, L"*.state", NULL, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT, sFiles);
        if (dlg.DoModal() == IDOK)
        {
            emulator->savestate(dlg.m_szFileName, true);
            // do stuff
        }
        return 0;
    }

    BOOL PreTranslateMessage(MSG* pMsg)
    {
        if (m_haccelerator != NULL)
        {
            if (::TranslateAccelerator(m_hWnd, m_haccelerator, pMsg))
                return TRUE;
        }

        return CWindow::IsDialogMessage(pMsg);
    }

    LRESULT OnSize(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
    {
        if (!emulator->isEmulating)
        {
            PAINTSTRUCT ps;
            HDC pDC = BeginPaint(&ps);
            RECT rect;
            GetClientRect(&rect);
            // I don't really want it black but this
            // will do for testing the concept:
            HBRUSH hBrush = (HBRUSH)::GetStockObject(BLACK_BRUSH);
            ::FillRect(pDC, &rect, hBrush);
            ::ValidateRect(m_hWnd, &rect);
            EndPaint(&ps);
        }
        return 0;
    }

    void CreateFolder(TCHAR* path)
    {
        TCHAR filename[MAX_PATH] = { 0 };
        GetCurrentDirectory(MAX_PATH, filename);
        PathAppend(filename, path);
        if (!CreateDirectory(filename, NULL))
        {
            return;
        }
    }

    void LoadPlugins(void)
    {
        int plugin_count = 0;
        TCHAR core_filename[MAX_PATH] = { 0 };
        GetCurrentDirectory(MAX_PATH, core_filename);
        PathAppend(core_filename, L"cores");
        TCHAR spec[_MAX_PATH];
        wsprintf(spec, L"%s\\*.dll", core_filename);
        WIN32_FIND_DATA data;
        HANDLE h = FindFirstFile(spec, &data);
        if (h != INVALID_HANDLE_VALUE) {
            do {
                TCHAR fname[_MAX_PATH];
                wsprintf(fname, L"%s\\%s", core_filename, data.cFileName);
                HMODULE hModule = LoadLibrary(fname);
                if (hModule) {
                    typedef void(*retro_get_system_info)(struct retro_system_info *info);
                    retro_get_system_info getinfo;
                    struct retro_system_info system = { 0 };
                    getinfo = (retro_get_system_info)GetProcAddress(hModule, "retro_get_system_info");
                    if (getinfo)
                    {
                        getinfo(&system);
                        libretro_core entry;
                        entry.core_system = utf16_from_utf8(system.library_name);
                        entry.core_exts = utf16_from_utf8(system.valid_extensions);
                        entry.core_path = fname;
                        cores.push_back(entry);
                    }

                    FreeLibrary(hModule);
                }
            } while (FindNextFile(h, &data));
            FindClose(h);
        }
    }

    LRESULT OnCreate(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
    {
        CMessageLoop* pLoop = _Module.GetMessageLoop();
        ATLASSERT(pLoop != NULL);
        bHandled = FALSE;
        input_device = input::GetInstance(GetModuleHandle(NULL), m_hWnd);
        emulator = CLibretro::GetInstance(m_hWnd);
        m_haccelerator = AtlLoadAccelerators(IDR_ACCELERATOR1);
        pLoop->AddMessageFilter(this);
        RegisterDropTarget();
        SetRedraw(FALSE);
        CreateFolder(L"cores");
        CreateFolder(L"system");
        LoadPlugins();
        return 0;
    }

    void start(TCHAR* rom_filename, TCHAR* core_filename, bool specifics, bool threads)
    {
        TCHAR core_filename2[MAX_PATH] = { 0 };
        GetCurrentDirectory(MAX_PATH, core_filename2);
        PathAppend(core_filename2, L"cores");
        bool set = SetDllDirectory(core_filename2);
        if (lstrcmp(core_filename, L"") == 0 || lstrcmp(rom_filename, L"") == 0)
        {
            CAboutDlg dlg;
            dlg.DoModal();
            DestroyWindow();
            return;
        }
        if (!emulator->loadfile(rom_filename, core_filename, specifics))
            DestroyWindow();
    }

    void DoFrame()
    {
        if (emulator->isEmulating)emulator->run();
        else
        {
            PAINTSTRUCT ps;
            HDC pDC = BeginPaint(&ps);
            RECT rect;
            GetClientRect(&rect);
            // I don't really want it black but this
            // will do for testing the concept:
            HBRUSH hBrush = (HBRUSH)::GetStockObject(BLACK_BRUSH);
            ::FillRect(pDC, &rect, hBrush);
            ::ValidateRect(m_hWnd, &rect);
            EndPaint(&ps);
        }
    }



    void ProcessFile(LPCTSTR lpszPath)
    {
        tstring ext_filter;
        tstring extensions;
        TCHAR* ext = PathFindExtensionW(lpszPath);
        int selected_core = 0;
        int found_core = 0;
        std::vector<tstring> core_paths;
        if (!cores.size())
        {
            MessageBox(L"No libretro cores found.", L"Error", MB_ICONSTOP);
            return;
        }
        for (int i = 0; i < cores.size(); i++)
        {
            tstring extensions = cores[i].core_exts;
            size_t found = extensions.find(ext + 1);
            if (found != std::string::npos)
            {
                found_core++;
                selected_core = i;
                ext_filter += cores[i].core_system;
                core_paths.push_back(cores[i].core_path);
                extensions = cores[i].core_exts;
                ext_filter.push_back('\0');
                ext_filter += L";*.";
                while (extensions.find(L"|") != std::string::npos)
                    extensions.replace(extensions.find(L"|"), 1, L";*.");
                ext_filter += extensions;
                ext_filter.push_back('\0');
            }
        }
        ext_filter.push_back('\0');
        ext_filter.push_back('\0');

        if (found_core == 1)
        {
            start((TCHAR*)lpszPath, (TCHAR*)cores[selected_core].core_path.c_str(), false, false);
            return;
        }
        CFileDialog dlg(TRUE, NULL, NULL, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT, ext_filter.c_str());
        dlg.m_ofn.lpstrTitle = L"Load ROM/image file with chosen core";
        if (dlg.DoModal() == IDOK)
            start(dlg.m_szFileName, (TCHAR*)core_paths[dlg.m_ofn.nFilterIndex - 1].c_str(), false, false);
    }

    LRESULT OnDestroy(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
    {
        if (emulator)emulator->kill();
        if (input_device)input_device->close();
        CMessageLoop* pLoop = _Module.GetMessageLoop();
        ATLASSERT(pLoop != NULL);
        pLoop->RemoveMessageFilter(this);
        bHandled = FALSE;
        return 0;
    }

    LRESULT OnOptions(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
    {

        if (emulator->isEmulating)
        {
            COptions dlg;
            dlg.DoModal();
        }
        else
            MessageBox(L"No game/libretro core running.", L"Error", MB_ICONSTOP);

        return 0;
    }

    void OnFileExit(UINT uCode, int nID, HWND hwndCtrl)
    {
        DestroyWindow();
        return;
    }

    LRESULT OnFileOpen(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
    {
        tstring ext_filter;
        tstring extensions;
        if (!cores.size())
        {
            MessageBox(L"No libretro cores found.", L"Error", MB_ICONSTOP);
            return 0;
        }
        for (int i = 0; i < cores.size(); i++)
        {
            ext_filter += cores[i].core_system;
            extensions = cores[i].core_exts;
            ext_filter.push_back('\0');
            ext_filter += L"*.";
            while (extensions.find(L"|") != std::string::npos)
                extensions.replace(extensions.find(L"|"), 1, L";*.");
            ext_filter += extensions;
            ext_filter.push_back('\0');
        }
        ext_filter.push_back('\0');
        ext_filter.push_back('\0');
        CFileDialog dlg(TRUE, NULL, NULL, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT, ext_filter.c_str());
        dlg.m_ofn.lpstrTitle = L"Load ROM/image file";
        if (dlg.DoModal() == IDOK)
        {
            start((TCHAR*)dlg.m_szFileName, (TCHAR*)cores[dlg.m_ofn.nFilterIndex - 1].core_path.c_str(), false, false);
        }
        return 0;
    }

    LRESULT OnAbout(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
    {
        CAboutDlg dlg;
        dlg.DoModal();

        return 0;
    }
};

class CEmuMessageLoop : public CMessageLoop
{
public:
    int Run(CMyWindow & gamewnd)
    {
        BOOL bDoIdle = TRUE;
        int nIdleCount = 0;
        BOOL bRet = FALSE;

        while (m_msg.message != WM_QUIT)
        {
            if (PeekMessage(&m_msg, 0, 0, 0, PM_REMOVE))
            {
                TranslateMessage(&m_msg);
                DispatchMessage(&m_msg);
            }
            else
            {
                gamewnd.DoFrame();
            }
        }

        return (int)m_msg.wParam;
    }
};


#endif  // ndef MYWINDOW_H_INCLUDED
