// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Original code was distributed under the MIT software license.
// Copyright (c) 2014-2017 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#ifndef BITCOIN_INIT_H
#define BITCOIN_INIT_H

#include <string>

class CWallet;
struct mc_WalletTxs;

namespace boost
{
class thread_group;
} // namespace boost

extern CWallet* pwalletMain;
extern mc_WalletTxs* pwalletTxsMain;


void StartShutdown();
bool ShutdownRequested();
void Shutdown();

/* MCHN START */
#ifndef STDOUT_FILENO
#define STDIN_FILENO 0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2
#endif
/* MCHN END */



bool AppInit2(boost::thread_group& threadGroup,int OutputPipe=STDOUT_FILENO);

/** The help message mode determines what help message to show */
enum HelpMessageMode {
    HMM_BITCOIND,
    HMM_BITCOIN_QT
};

/** Help for options shared between UI and daemon (for -help) */
std::string HelpMessage(HelpMessageMode mode);
/** Returns licensing information (for -version) */
std::string LicenseInfo();

#endif // BITCOIN_INIT_H
