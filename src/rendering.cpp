/*
 * 	    rendering.cpp             (C) 2006-2008, Aurélien Croc (AP²C)
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
#include "rendering.h"
#include "qpdl.h"
#include "page.h"
#include "cache.h"
#include "errlog.h"
#include "colors.h"
#include "request.h"
#include "printer.h"
#include "compress.h"
#include "document.h"

#ifndef DISABLE_THREADS
#include <pthread.h>
#include "sp_semaphore.h"

static Document document;
static Semaphore _lock;
static bool _returnState=true;

/*
 * This function is executed by each compression thread
 * It compress each page, page by page and store them
 * into the cache
 */
static void *_compressPage(void* data)
{
    const Request *request = static_cast<const Request *>(data);
    bool rotateEvenPages;
    rotateEvenPages = request->duplex() == Request::ManualLongEdge;
    do {
        // Load the page
        std::unique_ptr<Page> page;
        {
            _lock.lock();
            page = document.getNextRawPage(*request);
            _lock.unlock();
        }
        if (!page) {
            setNumberOfPages(document.numberOfPages());
            break;
        }

        // Make rotation on even pages for ManualLongEdge duplex mode
        if (rotateEvenPages && !(page->pageNr() % 2)) {
            page->rotate();
        }

        // Apply some colors optimizations
#ifndef DISABLE_BLACKOPTIM
        applyBlackOptimization(page.get());
#endif /* DISABLE_BLACKOPTIM */

        // Compress the page
        if (compressPage(*request, page.get())) {
            DEBUGMSG(_("Page %lu has been compressed and is ready for "
                "rendering"), static_cast<unsigned long>(page->pageNr()));
        } else {
            ERRORMSG(_("Error while compressing the page. Check the previous "
                "message. Trying to print the other pages."));
            page->setEmpty();
            _returnState = false;
        }
        registerPage(std::move(page));
    } while (true);

    DEBUGMSG(_("Compression thread: work done. See ya"));

    return NULL;
}

bool render(Request& request)
{
    bool manualDuplex=false, checkLastPage=false, lastPage=false;
    pthread_t threads[THREADS];
    std::unique_ptr<Page> page;

    // Load the document
    if (!document.load(request)) {
        ERRORMSG(_("Error while rendering the request. Check the previous "
            "message"));
        return false;
    }

    // Load the compression threads
    for (unsigned int i=0; i < THREADS; i++) {
        if (pthread_create(&threads[i], nullptr, _compressPage, static_cast<void*>(&request))) {
            ERRORMSG(_("Cannot load compression threads. Operation aborted."));
            return false;
        }
    }

    // Prepare the manual duplex
    if (request.duplex() == Request::ManualLongEdge || 
        request.duplex() == Request::ManualShortEdge) {
            manualDuplex = true;
            setCachePolicy(EvenDecreasing);
    }

    //Load the first page
    /*
     * NOTE: To prevent printer timeout, PJL header must be sent when the first
     * page to render is available (which is very quickly for normal request but
     * can take very long time if a big document in manual duplex is printed).
     */
    page = getNextPage();

    // Prevent troubles if the last page is an odd page (in manual duplex mode)
    if (manualDuplex && document.numberOfPages() % 2)
        checkLastPage = true;

    // Send the PJL Header
    if (page)
        request.printer()->sendPJLHeader(request, page->compression(),
                             page->xResolution(), page->yResolution() );
    else
        request.printer()->sendPJLHeader(request, 0, 0, 0);

    // Render the whole document
    while (page) {
        if (checkLastPage && document.numberOfPages() == page->pageNr())
            lastPage = true;
        if (!page->isEmpty()) {
            if (!renderPage(request, page.get(), lastPage)) {
                ERRORMSG(_("Error while rendering the page. Check the previous "
                    "message. Trying to print the other pages."));
                _returnState = false;
            }
            fprintf(stderr, "PAGE: %lu %lu\n", static_cast<unsigned long>(page->pageNr()), static_cast<unsigned long>(page->copiesNr()));
        }
        page = getNextPage();
    }

    // Send the PJL footer
    request.printer()->sendPJLFooter(request);

    // Wait for threads to be finished
    for (unsigned int i=0; i < THREADS; i++) {
        void *result;

        if (pthread_join(threads[i], &result))
            ERRORMSG(_("An error occurred while waiting the end of a thread"));
    }

    return _returnState;
}


#else /* DISABLE_THREADS */

bool render(Request& request)
{
    Document document;
    std::unique_ptr<Page> page;

    // Load the document
    if (!document.load(request)) {
        ERRORMSG(_("Error while rendering the request. Check the previous "
            "message"));
        return false;
    }

    // Get first Page
    page = document.getNextRawPage(request);

    // Send the PJL Header
    if (page)
        request.printer()->sendPJLHeader(request, page->compression(),
                             page->xResolution(), page->yResolution() );
    else
        request.printer()->sendPJLHeader(request, 0, 0, 0);

    // Send each page
    while (page) {
#ifndef DISABLE_BLACKOPTIM
        applyBlackOptimization(page.get());
#endif /* DISABLE_BLACKOPTIM */
        if (compressPage(request, page.get())) {
            if (!renderPage(request, page.get()))
                ERRORMSG(_("Error while rendering the page. Check the previous "
                            "message. Trying to print the other pages."));
        } else
            ERRORMSG(_("Error while compressing the page. Check the previous "
                "message. Trying to print the other pages."));
        fprintf(stderr, "PAGE: %lu %lu\n", static_cast<unsigned long>(page->pageNr()), static_cast<unsigned long>(page->copiesNr()));
        page = document.getNextRawPage(request);
    }

    // Send the PJL footer
    request.printer()->sendPJLFooter(request);

    return true;
}

#endif /* DISABLE_THREADS */

/* vim: set expandtab tabstop=4 shiftwidth=4 smarttab tw=80 cin enc=utf8: */

