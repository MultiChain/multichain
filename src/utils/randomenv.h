// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2014-2020 The Bitcoin Core developers
// Original code was distributed under the MIT software license.
// Copyright (c) 2014-2019 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#ifndef BITCOIN_RANDOMENV_H
#define BITCOIN_RANDOMENV_H

#include <crypto/sha512.h>


#ifndef WIN32
#if defined(__x86_64__) || defined(__amd64__) || defined(__i386__)
#define HAVE_GETCPUID

#include <cpuid.h>

// We can't use cpuid.h's __get_cpuid as it does not support subleafs.
void static inline GetCPUID(uint32_t leaf, uint32_t subleaf, uint32_t& a, uint32_t& b, uint32_t& c, uint32_t& d)
{
#ifdef __GNUC__
    __cpuid_count(leaf, subleaf, a, b, c, d);
#else
  __asm__ ("cpuid" : "=a"(a), "=b"(b), "=c"(c), "=d"(d) : "0"(leaf), "2"(subleaf));
#endif
}

#endif // defined(__x86_64__) || defined(__amd64__) || defined(__i386__)
#endif


/** Gather non-cryptographic environment data that changes over time. */
void RandAddDynamicEnv(CSHA512& hasher);

/** Gather non-cryptographic environment data that does not change over time. */
void RandAddStaticEnv(CSHA512& hasher);

#endif // BITCOIN_RANDOMENV_H
