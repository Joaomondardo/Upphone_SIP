/*
 * Copyright (C) 2011-2026 MicroSIP (http://www.microsip.org)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#pragma once

#include "const.h"

#define _GLOBAL_DPI
// #define _GLOBAL_DTLS // desativado: pjproject compilado sem OpenSSL

#define STR_SZ 256

#define _GLOBAL_WIDTH 162

//#ifndef _GLOBAL_RINGIN_WIDTH
#define _GLOBAL_RINGIN_WIDTH 180
//#endif

#define _GLOBAL_DIALER_WIDTH 162

#define _GLOBAL_ICON "res\\microsip.ico"
#define _GLOBAL_ICON_INACTIVE "res\\tray_inactive.ico"

#define _GLOBAL_HEIGHT1 0
#define _GLOBAL_HEIGHT2 _GLOBAL_HEIGHT1+16

#define _GLOBAL_HEIGHT3 _GLOBAL_HEIGHT2+23

#define _GLOBAL_HEIGHT4 _GLOBAL_HEIGHT3

#define _GLOBAL_HEIGHT_FINAL _GLOBAL_HEIGHT4

#define _GLOBAL_LOGO_HEIGHT 0
#define _GLOBAL_HEIGHT 192 + _GLOBAL_LOGO_HEIGHT + _GLOBAL_HEIGHT_FINAL

#define _GLOBAL_TAB_WIDTH 47

#define _GLOBAL_ACCT_OFFSET_LEFT 0

#define IDD_CALLS_OFFSET_INITIAL _GLOBAL_HEIGHT - 17
#define IDD_CALLS_OFFSET_LISTVIEW1 IDD_CALLS_OFFSET_INITIAL
#define IDD_CALLS_OFFSET_LISTVIEW2 IDD_CALLS_OFFSET_LISTVIEW1

#define IDD_CALLS_OFFSET_LISTVIEW IDD_CALLS_OFFSET_LISTVIEW2+2


// Upphone: mesma lista fixa de AccountSettings::SettingsLoad (settings.cpp), com G.729
#define _GLOBAL_CODECS_ENABLED "PCMA/8000/1 PCMU/8000/1 G729/8000/1"

#define _GLOBAL_SETT_DENYINC_DEFAULT "button"
#define _GLOBAL_SETT_AA_DEFAULT "button"

#define _GLOBAL_MENU_WEBSITE "https://upphone.com.br/"
#define _GLOBAL_MENU_DOWNLOADS "https://upphone.com.br/"
#define _GLOBAL_MENU_ISSUES "https://upphone.com.br/"
#define _GLOBAL_MENU_HELP "https://upphone.com.br/"
#define _GLOBAL_HELP_WEBSITE "https://upphone.com.br/"
// Upphone: new versions are the GitHub releases of this repository (build/release.ps1 publishes them).
// The app reads the version from the API and always opens the fixed release page, never a link from the response.
#define _GLOBAL_UPDATES_API "https://api.github.com/repos/Joao-Mondardo-Upp/Upphone_SIP/releases/latest"
#define _GLOBAL_UPDATES_PAGE "https://github.com/Joao-Mondardo-Upp/Upphone_SIP/releases/latest"
// Upphone: provisioning servers of the Upp platform, chosen by the first letter of the activation code
// (P production, H homologation). Development (D) needs the address in the installer (/SERVIDOR=) or in
// Upphone.ini, [Provisioning] server=. See docs/PROVISIONAMENTO.md.
#define _GLOBAL_PROVISIONING_PROD "https://provisionamento.upphone.com.br"
#define _GLOBAL_PROVISIONING_HOMOLOG "https://homolog.provisionamento.upphone.com.br"
#define _GLOBAL_TECH_DETAILS

#define _GLOBAL_EC_DEFAULT "1"

#define _GLOBAL_NAME_NICE "Upphone SIP"

#define _GLOBAL_NAME_VISIBLE _GLOBAL_NAME_NICE

#define _GLOBAL_CALL_PICKUP "**"

#define _GLOBAL_SHORTCUTS
#define _GLOBAL_SHORTCUTS_QTY 8
#define _GLOBAL_SHORTCUTS_ROWS 8

#define MACRO_ENABLE_LOCAL_ACCOUNT (accountSettings.enableLocalAccount || !accountSettings.accountId)

#define _GLOBAL_SUBSCRIBE

#ifndef LVS_EX_AUTOSIZECOLUMNS
#define LVS_EX_AUTOSIZECOLUMNS 0x10000000
#endif

#define _GLOBAL_FEATURE_CODES_CP_DEFAULT "**"
#define _GLOBAL_FEATURE_CODES_CP_ENABLE 1

// Upphone palette: accent is fixed, the rest follows the Windows light/dark setting
#ifndef RC_INVOKED
#include "Theme.h"
#endif
#define _GLOBAL_BRAND_ACCENT RGB(228, 103, 55)
#define _GLOBAL_BRAND_ACCENT_HOT RGB(240, 128, 84)
#define _GLOBAL_THEME_BG ThemeColor(THEME_BG)
#define _GLOBAL_THEME_SURFACE ThemeColor(THEME_SURFACE)
#define _GLOBAL_THEME_SURFACE_HOT ThemeColor(THEME_SURFACE_HOT)
#define _GLOBAL_THEME_BORDER ThemeColor(THEME_BORDER)
#define _GLOBAL_THEME_TEXT ThemeColor(THEME_TEXT)
#define _GLOBAL_THEME_TEXT2 ThemeColor(THEME_TEXT2)
#define _GLOBAL_THEME_DANGER ThemeColor(THEME_DANGER)
#define _GLOBAL_THEME_WARNING ThemeColor(THEME_WARNING)
#define _GLOBAL_THEME_SUCCESS ThemeColor(THEME_SUCCESS)
#define _GLOBAL_DIALER_CALL_COLOR _GLOBAL_BRAND_ACCENT
#define _GLOBAL_DIALER_END_COLOR RGB(255, 59, 48)
