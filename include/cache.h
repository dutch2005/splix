/*
 * 	    cache.h                   (C) 2008, Aurélien Croc (AP²C)
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
#ifndef _CACHE_H_
#define _CACHE_H_

#include <memory>
#include <string>
#include <cstdint>

class Page;

/**
  * List all the different cache policy available.
  */
enum CachePolicy {
    /** All pages are needed with page number increasing. */
    EveryPagesIncreasing,
    /** Only even pages are needed with page number decreasing. */
    EvenDecreasing, 
    /** Only odd pages are needed with page number increasing. */
    OddIncreasing,
};


/**
  * Initialize the cache mechanism and load the cache controller thread.
  * @return TRUE if the initialization succeed. Otherwise it returns FALSE.
  */
extern bool initializeCache();

/**
  * Uninitialize the cache mechanism and unload the cache controller thread.
  * @return TRUE if the uninitialization succeed. Otherwise it returns FALSE.
  */
extern bool uninitializeCache();


/**
  * Extract the next page (depending on the curernt cache policy)
  * @return the instance of the page. Otherwise it returns empty unique_ptr if 
  *         no page are found.
  */
extern std::unique_ptr<Page> getNextPage();

/**
  * Register a new page in the cache.
  * @param page the page instance to register in the cache
  */
extern void registerPage(std::unique_ptr<Page> page);

/**
  * Set the new cache policy.
  * @param policy the new cache policy
  */
extern void setCachePolicy(CachePolicy policy);

/**
  * Set the number of pages in the document.
  * @param nr the number of pages
  */
extern void setNumberOfPages(uint32_t nr);


/**
  * @brief This class represent a cache entry to store a page.
  * To preserve memory a swapping mechanism is present.
  */
class CacheEntry {
    protected:
        std::unique_ptr<Page>   _page;
        CacheEntry*             _previous;
        CacheEntry*             _next;
        std::string             _tempFile;

    public:
        /**
          * Initialize the cache entry instance.
          * @param page the page instance associated to this entry.
          */
        CacheEntry(std::unique_ptr<Page> page);
        /**
          * Destroy the cache entry instance.
          */
        virtual ~CacheEntry();

    public:
        /**
          * Set the next cache entry.
          * @param entry the next cache entry
          */
        void                    setNext(CacheEntry *entry) {_next = entry;}
        /**
          * Set the previous cache entry.
          * @param entry the previous cache entry
          */
        void                    setPrevious(CacheEntry *entry) 
                                    {_previous = entry;}
        /**
          * Swap the page instance on the disk.
          * @return TRUE if the page has been successfully swapped. Otherwise it
          *         returns FALSE.
          */
        bool                    swapToDisk();
        /**
          * Restore a previously swapped page into memory.
          * @return TRUE if the page has been successfully restored. Otherwise 
          *         it returns FALSE.
          */
        bool                    restoreIntoMemory();

        /**
          * @return the page instance pointer.
          */
        Page*                   page() const {return _page.get();}
        /**
          * @return the unique_ptr to the page.
          */
        std::unique_ptr<Page>   releasePage();
        /**
          * @return the next instance.
          */
        CacheEntry*             next() const {return _next;}
        /**
          * @return the previous instance.
          */
        CacheEntry*             previous() const {return _previous;}
        /**
          * @return TRUE if the page is currently swapped on disk. Otherwise
          *         returns FALSE.
          */
        bool                    isSwapped() const 
                                    {return !_tempFile.empty();}
};
#endif /* _CACHE_H_ */

/* vim: set expandtab tabstop=4 shiftwidth=4 smarttab tw=80 cin enc=utf8: */

