// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Original code was distributed under the MIT software license.
// Copyright (c) 2014-2017 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#include "ui/noui.h"

#include "ui/ui_interface.h"
#include "utils/util.h"

#include <cstdio>
#include <stdint.h>
#include <string>

static bool noui_ThreadSafeMessageBox(const std::string& message, const std::string& caption, unsigned int style)
{
    bool fSecure = style & CClientUIInterface::SECURE;
    style &= ~CClientUIInterface::SECURE;

    std::string strCaption;
    // Check for usage of predefined caption
    switch (style) {
    case CClientUIInterface::MSG_ERROR:
        strCaption += _("Error");
        break;
    case CClientUIInterface::MSG_WARNING:
        strCaption += _("Warning");
        break;
    case CClientUIInterface::MSG_INFORMATION:
        strCaption += _("Information");
        break;
    default:
        strCaption += caption; // Use supplied caption (can be empty)
    }

    if (!fSecure)
        LogPrintf("%s: %s\n", strCaption, message);
    fprintf(stderr, "%s: %s\n", strCaption.c_str(), message.c_str());
    return false;
}

static void noui_InitMessage(const std::string& message)
{
    LogPrintf("init message: %s\n", message);
}

void noui_connect()
{
    // Connect bitcoind signal handlers
    uiInterface.ThreadSafeMessageBox.connect(noui_ThreadSafeMessageBox);
    uiInterface.InitMessage.connect(noui_InitMessage);
}
