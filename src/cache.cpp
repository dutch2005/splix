/*
 * 	    cache.cpp                 (C) 2008, Aurélien Croc (AP²C)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 * 
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the
 *  Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 *  $Id$
 * 
 */
#ifndef DISABLE_THREADS
#include <mutex>
#include <condition_variable>
#include <semaphore>
#include <thread>
#include <vector>
#include "sp_semaphore.h"
#include "sp_result.h"

#include "cache.h"
#include <fcntl.h>
#include <stdlib.h>
#include "sp_portable.h"
#include <string.h>
#include <errno.h>
#include "page.h"
#include "errlog.h"
#include <algorithm>

/*
 * Variables internes
 * Internal variables
 */
// Cache controller thread variables
static CachePolicy _policy = EveryPagesIncreasing;
static std::jthread _cacheThread;
static SP::Semaphore _work(0);

// Page request variables
static unsigned long _lastPageRequested = 0, _pageRequested = 0;
static SP::Semaphore _pageAvailable(0);

// Document information
static uint32_t _numberOfPages = 0;
static bool _pageLimitKnown = false;

// Cache variables
static CacheEntry *_waitingList = nullptr, *_lastWaitingList = nullptr;
static std::mutex _waitingListLock;
static uint32_t _pagesInTable = 0;
static std::vector<std::unique_ptr<CacheEntry>> _pages;
static std::mutex _pageTableLock;



static void _cacheControllerThread(std::stop_token stopToken)
{
    DEBUGMSG(_("Cache controller thread loaded and is waiting for a job"));
    while (!stopToken.stop_requested()) {
        // Waiting for a job
        _work.acquire();

        // Does the thread needs to exit?
        if (stopToken.stop_requested())
            break;

        CacheEntry *entry = nullptr;

        // Get the cache entry to store
        {
            std::lock_guard<std::mutex> lock(_waitingListLock);
            entry = _waitingList;
            if (entry) {
                _waitingList = entry->next();
                if (_lastWaitingList == entry)
                    _lastWaitingList = nullptr;
            }
        }

        if (!entry)
            continue;

        // Store the entry in the page table
        {
            std::lock_guard<std::mutex> lock(_pageTableLock);

            // Resize the page table if needed
            if (entry->page()->pageNr() > _pages.size()) {
                _pages.resize(entry->page()->pageNr());
            }

            // Store the page in the table
            _pages[entry->page()->pageNr() - 1] = std::unique_ptr<CacheEntry>(entry);
        }
        _pagesInTable++;

        // Does the main thread need this page?
        if (_pageRequested == entry->page()->pageNr()) {
            _pageAvailable.release();
        }
    }

    DEBUGMSG(_("Cache controller unloaded. See ya"));
}



/*
 * Initialisation et clôture du cache
 * Cache initialization and uninitialization
 */
bool initializeCache()
{
    // Load the cache controller thread
    try {
        _cacheThread = std::jthread(_cacheControllerThread);
    } catch (const std::system_error& e) {
        ERRORMSG(_("Cannot load the cache controller thread: %s"), e.what());
        return false;
    }

    // Initialize the different cache variables
    return true;
}

bool uninitializeCache()
{
    // Stop the cache controller thread
    _cacheThread.request_stop();
    _work.release(); // wake up thread to check for stop
    _cacheThread.join();
 
    // Check if all pages has been read. Otherwise free them
    for (size_t i = 0; i < _pages.size(); i++) {
        if (_pages[i]) {
            ERRORMSG(_("Cache: page %lu hasn't be used!"), static_cast<unsigned long>(i+1));
        }
    }
    _pages.clear();

    return true;
}



/*
 * Enregistrement d'une page dans le cache
 * Register a new page in the cache
 */
void registerPage(std::unique_ptr<Page> page)
{
    auto entry = std::make_unique<CacheEntry>(std::move(page));
    CacheEntry* entryRaw = entry.release();
    {
        std::lock_guard<std::mutex> lock(_waitingListLock);
        if (_lastWaitingList) {
            _lastWaitingList->setNext(entryRaw);
            _lastWaitingList = entryRaw;
        } else {
            _waitingList = entryRaw;
            _lastWaitingList = entryRaw;
        }
    }
    _work.release();
}



/*
 * Extraction d'une page du cache
 * Cache page extraction
 */
SP::Result<std::unique_ptr<Page>> getNextPage()
{
    CacheEntry *entry = nullptr;
    uint32_t nr = 0;

    // Get the next page number
    switch (_policy) {
        case EveryPagesIncreasing:
            nr = static_cast<uint32_t>(_lastPageRequested + 1);
            break;
        case EvenDecreasing:
            if (_lastPageRequested > 2)
                nr = static_cast<uint32_t>(_lastPageRequested - 2);
            else {
                nr = 1;
                setCachePolicy(OddIncreasing);
            }
            break;
        case OddIncreasing:
            if (!_lastPageRequested)
                nr = 1;
            else
                nr = static_cast<uint32_t>(_lastPageRequested + 2);
            break;
    }

    DEBUGMSG(_("Next requested page : %lu (# pages into memory=%lu)"), static_cast<unsigned long>(nr), 
        static_cast<unsigned long>(_pagesInTable));

    // Wait for the page
    while (nr && (!_pageLimitKnown || _numberOfPages >= nr)) {
        {
            std::lock_guard<std::mutex> lock(_pageTableLock);
            if (_pages.size() >= nr && _pages[nr - 1]) {
                
                entry = _pages[nr - 1].get();
                
                // If the background thread encountered an error, propagate it
                if (entry->error() != SP::Error::None) {
                    return SP::Unexpected(entry->error());
                }

                break;
            }
            _pageRequested = nr;
        }
        _pageAvailable.acquire();
    }

    // Extract the page instance
    if (!entry)
        return std::unique_ptr<Page>(nullptr);
    
    _pageTableLock.lock();
    auto entryPtr = std::move(_pages[nr - 1]);
    _pageTableLock.unlock();

    _pagesInTable--;
    _lastPageRequested = nr;
    auto page = entryPtr->releasePage();
    // entryPtr (the CacheEntry) is destroyed here when we return or at end of scope

    // entryPtr (the CacheEntry) is destroyed here when we return or at end of scope

    return page;
}



/*
 * Modification de la politique de gestion du cache
 * Update the cache policy
 */
void setCachePolicy(CachePolicy policy)
{
    _policy = policy;

    // Initialize the lastPageRequested to get the next page number 
    if (policy == EveryPagesIncreasing || policy == OddIncreasing)
        _lastPageRequested = 0;
    else {
        while (!_numberOfPages)
            _pageAvailable.acquire();
        _lastPageRequested = (_numberOfPages & ~0x1) + 2;
    }
}



/*
 * Enregistrer le nombre de pages maximum
 * Set the maximum number of pages
 */
void setNumberOfPages(uint32_t nr)
{
    _numberOfPages = nr;
    _pageLimitKnown = true;
    _pageAvailable.release();
}



/*
 * Gestion des entrées du cache
 * Cache entry management
 */

CacheEntry::CacheEntry(std::unique_ptr<Page> page) : _page(std::move(page)) 
{}

CacheEntry::~CacheEntry()
{
}

#endif /* DISABLE_THREADS */

std::unique_ptr<Page> CacheEntry::releasePage()
{
    return std::move(_page);
}

/* vim: set expandtab tabstop=4 shiftwidth=4 smarttab tw=80 cin enc=utf8: */

