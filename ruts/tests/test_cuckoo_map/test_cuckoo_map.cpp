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

#include <ostream>
#include "ruts/interned_string.h"
#include "ruts/cuckoo_map.h"

using namespace ruts;
using namespace std;

int main() {
  using string_table_type = interned_string_table<0>;
  using string_type = typename string_table_type::value_type;
  using map_type = small_cuckoo_map<string_type, size_t>;

  string_table_type table(100);
  map_type map;

  string_type s1 = table.intern("First");
  cout << "map[" << s1 << "] = " << map[s1] << endl;
  map[s1] = 5;
  cout << "map[" << s1 << "] = " << map[s1] << endl;
  map[s1] += 1;
  cout << "map[" << s1 << "] = " << map[s1] << endl;
  ++map[s1];
  cout << "map[" << s1 << "] = " << map[s1] << endl;
  auto was = map[s1]++;
  cout << "map[" << s1 << "] = " << map[s1] << endl;
  cout << "map[" << s1 << "] was " << was << endl;
  map[s1] *= 2;
  cout << "map[" << s1 << "] = " << map[s1] << endl;
  cout << endl;

  string_type s2 = table.intern("Second");
  cout << "map[" << s2 << "] = " << map[s2] << endl;
  auto munge = map.updater([](size_t old){ return 3*old+1; }, 1);
  map.put(s2, munge);
  cout << "map[" << s2 << "] = " << map[s2] << endl;
  map.put(s2, munge);
  cout << "map[" << s2 << "] = " << map[s2] << endl;
  map.put_new(s2, 0);
  cout << "map[" << s2 << "] = " << map[s2] << endl;

  auto bound = map.guard([](size_t old){ return old < 20; });
  map.put(s2, bound, munge);
  cout << "map[" << s2 << "] = " << map[s2] << endl;
  map.put(s2, bound, munge);
  cout << "map[" << s2 << "] = " << map[s2] << endl;
  map.put(s2, bound, munge);
  cout << "map[" << s2 << "] = " << map[s2] << endl;


}