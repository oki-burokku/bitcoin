// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2017 The Bitcoin developers
// Copyright (c) 2020 Oki Burokku
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_MAXBLOCKWEIGHT_H
#define BITCOIN_MAXBLOCKWEIGHT_H

#include "consensus/consensus.h"
#include "consensus/params.h"
#include "script/script.h"

class CBlockHeader;
class CBlockIndex;
class CChainParams;

//==Implements BIPBBB functionailty.

// called when a new block is received on the blockchain. Checks for a change in
// block weight multiplier whenever a work adjustment occurs.
extern void GetNextBlockWeightMultiplier(const CBlockIndex* pindexNew, const CChainParams& chainParams);

// Parse a blocks coinbase string and return the BIPBBB Vote, or 0 if no vote found.
extern uint32_t BlockWeightFindVote(const std::string& coinbase);

#endif // BITCOIN_MAXBLOCKWEIGHT_H