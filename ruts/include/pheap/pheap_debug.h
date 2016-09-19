/*
 *
 *  Multi Process Garbage Collector
 *  Copyright © 2016 Hewlett Packard Enterprise Development Company LP.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *  As an exception, the copyright holders of this Library grant you permission
 *  to (i) compile an Application with the Library, and (ii) distribute the 
 *  Application containing code generated by the Library and added to the 
 *  Application during this compilation process under terms of your choice, 
 *  provided you also meet the terms and conditions of the Application license.
 *
 */

#ifndef _PHEAP_DEBUG_H_
#define _PHEAP_DEBUG_H_


/* if PHEAPDEBUG
 *   macro defines dout as cout
 * else
 *  "the preprocessor replaces 'dout' with '0 && cout'
 *   note that << has higher precedence than && and
 *   short-circuit evaluation of && makes the whole line evaluate to 0.
 *   Since the 0 is not used the compiler generates no code at all
 *   for that line."
 *
 * author: suspence
 */


#include <iostream>

using namespace std;

#ifdef PHEAPDEBUG
#define dout cout << __FILE__ << ":" << __LINE__ << ": "
#else
namespace {
  struct ignore__ {
    template <typename T>
    void operator =(T&&val) {}
  };
}
#define dout ignore__{} = false && cout
#endif


#endif /* _PHEAP_DEBUG_H_ */
