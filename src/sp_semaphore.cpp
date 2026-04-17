/*
 * 	    semaphore.cpp             (C) 2007-2008, Aurélien Croc (AP²C)
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
#include "sp_semaphore.h"

#ifndef DISABLE_THREADS

/*
 * Constructeur - Destructeur
 * Init - Uninit
 */
/*
 * Constructeur - Destructeur
 * Init - Uninit
 */
Semaphore::Semaphore()
{
    _counter = 1;
}

Semaphore::Semaphore(unsigned long counter)
{
    _counter = counter;
}

Semaphore::~Semaphore()
{
}



/*
 * Utilisation de la sémaphore en mutex
 * Semaphore used in mutex mode
 */
void Semaphore::lock()
{
    _lock.lock();
}

void Semaphore::unlock()
{
    _lock.unlock();
}



/*
 * Gestion du compteur de la semaphore
 * Semaphore counter management
 */
Semaphore& Semaphore::operator --(int)
{
    std::unique_lock<std::mutex> lock(_lock);
    _cond.wait(lock, [this]{ return _counter > 0; });
    _counter--;

    return *this;
}

Semaphore& Semaphore::operator ++(int)
{
    std::lock_guard<std::mutex> lock(_lock);
    _counter++;
    _cond.notify_one();

    return *this;
}


#endif /* DISABLE_THREADS */

/* vim: set expandtab tabstop=4 shiftwidth=4 smarttab tw=80 cin enc=utf8: */

