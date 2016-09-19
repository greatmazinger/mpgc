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

/*
 * pheap_barrier.h
 *
 *  Created on: Sep 20, 2013
 *      Author: evank
 */

#ifndef PHEAP_BARRIER_H_
#define PHEAP_BARRIER_H_


#include "pheap/spin.h"
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <cassert>

namespace pheap {
    class barrier {
    public:
    	void enter_for_mutate();
    	void exit_for_mutate();
    	bool enter_for_sync();
    	void exit_for_sync(bool did_sync);

    	enum state : char { Idle, Mutating, Allowing, Syncing, Unwinding };

    	struct compound_state {
    		uint16_t n_mutate_regions;
    		uint16_t n_sync_regions;
    		state _state;
    	};

    	static_assert(sizeof(compound_state) < 32, "Compound state needs to fit in 64-bit integer");
    	barrier();
    private:
    	std::mutex _mutex;
    	std::condition_variable _mutate_okay;
    	std::condition_variable _sync_okay;
    	std::condition_variable _sync_done;
    	std::atomic<uint64_t> _state;
    	bool update_state(compound_state &expected, const compound_state &val);
    	bool update_state(compound_state &expected, state s, int16_t delta_mutators, int16_t delta_syncs) {
    		compound_state new_state = expected;
    		new_state.n_mutate_regions += delta_mutators;
    		new_state.n_sync_regions += delta_syncs;
    		new_state._state = s;
    		return update_state(expected, new_state);
    	}
    	bool update_state(compound_state &expected, uint16_t delta_mutators, uint16_t delta_syncs) {
    		return update_state(expected, expected._state, delta_mutators, delta_syncs);
    	}
    	void unwind(compound_state &s, bool did_sync);


    	compound_state current();
    };


	class mutate_region {
	public:
		mutate_region(barrier &b) : _barrier(b) {
			b.enter_for_mutate();
		}
		~mutate_region() {
			_barrier.exit_for_mutate();
		}
	private:
		barrier &_barrier;
	};

	class sync_region {
	public:
		sync_region(barrier &b) : _barrier(b), do_sync(b.enter_for_sync()) {
		}
		~sync_region() {
			_barrier.exit_for_sync(do_sync);
		}
		operator bool() const {
			return do_sync;
		}

	private:
		barrier &_barrier;
		const bool do_sync;
	};


	union state_or_int {
		barrier::compound_state _state;
		uint64_t _int;
	};

	inline barrier::barrier() {
		state_or_int sori;
		sori._state = { 0, 0, Idle};
		_state = sori._int;
	}

	inline bool barrier::update_state(compound_state &expected, const compound_state &val) {
		state_or_int u_expected;
		u_expected._state = expected;
		state_or_int u_val;
		u_val._state = val;
		bool res = _state.compare_exchange_strong(u_expected._int, u_val._int);
		if (!res) {
			expected = u_expected._state;
		}
		return res;
	}


	inline barrier::compound_state barrier::current() {
		state_or_int u;
		u._int = _state;
		return u._state;
	}


	inline void barrier::enter_for_mutate() {
		compound_state s = current();
		spin_loop loop;
		while (loop()) {
			switch (s._state) {
			case Idle:
			case Mutating:
				if (update_state(s, Mutating, +1, 0)) {
					return;
				}
				break;
			case Allowing:
			case Syncing:
			case Unwinding:
			{
				if (update_state(s, +1, 0)) {
					std::unique_lock<std::mutex> lck(_mutex);
					// No syncs can now enter or
					// exit, so either they all
					// exited in between our
					// update and our grabbing the
					// lock or we have to wait.
					s = current();
					if (s._state == Mutating) {
						return;
					}
					_mutate_okay.wait(lck, [this](){ return current()._state == Mutating; });
				}
				break;
			}
			}
		}
	}

	inline void barrier::exit_for_mutate() {
		compound_state s = current();
		spin_loop loop;
		while (loop()) {
			assert(s._state == Mutating);
			state next = s.n_mutate_regions > 1 ? Mutating
					: s.n_sync_regions == 0 ? Idle
							: Allowing;
			if (update_state(s, next, -1, 0)) {
				if (next == Allowing) {
					std::unique_lock<std::mutex> lck(_mutex);
					_sync_okay.notify_all();
				}
				return;
			}
		}
	}


}


#endif /* PHEAP_BARRIER_H_ */
