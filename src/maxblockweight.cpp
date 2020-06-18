// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2017 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "maxblockweight.h"

#include "chain.h"
#include "chainparams.h"
#include "logging.h"
#include "validation.h"
#include <boost/lexical_cast.hpp>
#include <string>

void GetNextBlockWeightMultiplier(const CBlockIndex* pindexNew, const CChainParams& chainParams)
{
    static bool firsttime = true;

    // Is the bip enabled at all?
    static bool enabled = gArgs.IsArgSet("-enableBIPBBB");
    if (!firsttime && !enabled) {
        // LogPrintf("-enableBIPBBB is not enabled (S)\n");
        return;
    }

    //LogPrintf("GetNextBlockWeightMultiplier()\n");
    int height = pindexNew->nHeight;
    const Consensus::Params& consensusParams = chainParams.GetConsensus();
    unsigned int currentBlockWeightMultiplier = BlockWeightMultiplier();
    int64_t DIA = consensusParams.DifficultyAdjustmentInterval();

    // if this is the first new tip after startup, then try and find the
    // start of the previous difficulty adjustment.
    bool isfirsttime = firsttime;
    if (firsttime) {
        firsttime = false;

        uint32_t blockmaxweightmultiplier = gArgs.GetArg("-blockmaxweightmultiplier", 0);
        if ((blockmaxweightmultiplier > 0) && (blockmaxweightmultiplier < 100000)) {
            BlockWeightMultiplier(blockmaxweightmultiplier);
            LogPrintf("Extracted Arg -blockmaxweightmultiplier=%d\n", blockmaxweightmultiplier);
            return;
        }

        // Is the bip enabled at all?
        if (!enabled) {
            LogPrintf("BIPBBB is not enabled. (-enableBIPBBB)\n");
            return;
        }

        bool scan = gArgs.IsArgSet("-scanblockmaxweightmultiplier");
        if (scan) {
            LogPrintf("GetNextBlockWeightMultiplier(Scan Set)\n");
            while (height && (((height + 1) % DIA != 0))) {
                if (!pindexNew->pprev) {
                    LogPrintf("GetNextBlockWeightMultiplier(FIRSTTIME %d -- BAIL)\n", height);
                    return; // start of the chain, cannot go back in time far enough.
                }
                pindexNew = pindexNew->pprev;
                height = pindexNew->nHeight;
            }
        }
        LogPrintf("GetNextBlockWeightMultiplier(FIRSTTIME %d)\n", height);
    }

    // Only change once per difficulty adjustment interval
    if ((height + 1) % DIA != 0) {
        // LogPrintf("GetNextBlockWeightMultiplier(BAILING DAI)\n");
        return; // try again on a new tip.
    }

    CBlock block;
    std::vector<uint32_t> votes;
    uint32_t novote, nochangevote, upvote, downvote;
    novote = nochangevote = upvote = downvote = 0;
    for (int64_t i = 0; i < DIA; i++) {
        bool x = ReadBlockFromDisk(block, pindexNew, consensusParams);
        if (x) {
            //LogPrintf("GetNextBlockWeightMultiplier(loaded block from disk!!!!)\n");
            if (block.vtx[0]->IsCoinBase()) {
                //LogPrintf("GetNextBlockWeightMultiplier(vtx 0 is coinbase!!!!)\n");

                // extract coinbase message
                CScript coinbase = block.vtx[0]->vin[0].scriptSig;

                // Skip encoded height if found at start of coinbase
                CScript expect = CScript() << pindexNew->nHeight;
                int searchStart = coinbase.size() >= expect.size() &&
                                          std::equal(expect.begin(), expect.end(), coinbase.begin()) ?
                                      expect.size() :
                                      0;
                std::string s(coinbase.begin() + searchStart, coinbase.end());

                uint32_t vote = BlockWeightFindVote(s);
                if (!vote) {
                    vote = currentBlockWeightMultiplier;
                    novote++;
                    LogPrintf(".");
                } else if (vote == currentBlockWeightMultiplier) {
                    nochangevote++;
                    LogPrintf("-");
                } else if (vote < currentBlockWeightMultiplier) {
                    downvote++;
                    LogPrintf("<");
                } else if (vote > currentBlockWeightMultiplier) {
                    upvote++;
                    LogPrintf(">");
                }
                if (((i + 1) % 63) == 0) {
                    LogPrintf("\n");
                }

                votes.push_back(vote);
            } else {
                LogPrintf("GetNextBlockWeightMultiplier(Bad coinbase marker!!!)\n");
                return;
            }
        } else {
            LogPrintf("GetNextBlockWeightMultiplier(Could not load block from disk!!!!)\n");
            return;
        }

        // go to the previous item in chain.
        pindexNew = pindexNew->pprev;
        if (!pindexNew) {
            LogPrintf("GetNextBlockWeightMultiplier(!!!!bad pprev)\n");
            return;
        }
    }
    LogPrintf("\n");

    LogPrintf("novote       = %d (%g)\n", novote, (novote * 100.00 / DIA));
    LogPrintf("nochangevote = %d (%g)\n", nochangevote, (nochangevote * 100.00 / DIA));
    LogPrintf("upvote       = %d (%g)\n", upvote, (upvote * 100.00 / DIA));
    LogPrintf("downvote     = %d (%g)\n", downvote, (downvote * 100.00 / DIA));

    size_t lowerat = ((75 * DIA) / 100) - 1;
    size_t raiseat = ((25 * DIA) / 100);

    // We now have a set of block weight votes for the last retaget period.
    // so we can sort them and find out if we should increase or decrease the
    // block multiplier.
    std::sort(votes.begin(), votes.end());
    uint32_t lowerValue = votes.at(lowerat);
    uint32_t raiseValue = votes.at(raiseat);

    // if this is the first time here (either from a scan or a command line)
    // then allow the multiplier to be moved by more then 1.
    if (!isfirsttime) {
        if (raiseValue > currentBlockWeightMultiplier) {
            raiseValue = currentBlockWeightMultiplier + 1;
        } else if (lowerValue < currentBlockWeightMultiplier) {
            lowerValue = currentBlockWeightMultiplier - 1;
        }
    }

    assert(lowerValue >= 1);          // minimal vote supported is 1
    assert(lowerValue >= raiseValue); // lowerValue comes from a higher sorted position

    if (raiseValue > currentBlockWeightMultiplier) {
        BlockWeightMultiplier(raiseValue);
    } else if (lowerValue < currentBlockWeightMultiplier) {
        BlockWeightMultiplier(lowerValue);
    }

    if (currentBlockWeightMultiplier != BlockWeightMultiplier()) {
        LogPrintf("BlockWeightMultiplier RETARGET!\n");
        LogPrintf("Before: %d\n", currentBlockWeightMultiplier);
        LogPrintf("After:  %d\n", BlockWeightMultiplier());
    } else {
        LogPrintf("BlockWeightMultiplier RETARGET unmoved %d!\n", currentBlockWeightMultiplier);
    }
}

uint32_t BlockWeightFindVote(const std::string& coinbase)
{
    uint32_t vote = 0;
    static const std::string BIPMarker("/BIPBBB/X");

    if (coinbase.length() < BIPMarker.length() + 1) {
        return vote; // coinbase string too small
    }

    size_t BIPMarkerStart = coinbase.find(BIPMarker);
    if (BIPMarkerStart == std::string::npos) {
        // No BIPBBB Marker in string
        return vote;
    }

    size_t BIPMarkerEnd = BIPMarkerStart + BIPMarker.length();
    size_t EndMultiplier = coinbase.find('/', BIPMarkerEnd);
    if (EndMultiplier == std::string::npos) {
        // No end Marker in string
        return vote;
    }

    if (EndMultiplier - BIPMarkerEnd > 5) {
        // max 5 digits in vote, max 99999
        return vote;
    }

    for (size_t i = BIPMarkerEnd; i < EndMultiplier; i++) {
        char vx = coinbase[i];
        if (isdigit(vx) == 0) {
            vote = 0; // Bad multiplier value - not a number!
            return vote;
        }
        vote = (vote * 10) + (vx - '0');
    }

    return vote;
}
