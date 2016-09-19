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
 * gcdemo-init.h
 *
 *  Created on: June 21, 2016
 *      Author: uversky
 */

#ifndef GCDEMO_GCDEMO_INIT_H_
#define GCDEMO_GCDEMO_INIT_H_

#include "../gcdemo/gcdemo.h"
#include "../gcdemo/Graph.h"
#include "mpgc/gc.h"
#include <functional>
#include <random>

using namespace std;

/*
 * There could be more than 2^32 users (roughly 4.3 million).
 * Fortunately, the indexing for gc_vectors and gc_arrays is
 * 64-bit, so we can use an unsigned long generator and be okay. 
 *
 * (If you have a social network of 1.845e19 users or more, 
 *  I'm afraid you're out of luck for now.  Sorry, CEO of 
 *  Intergalactic Facebook!)
 */
class InitRNG : public UniformRNG {
private:
  exponential_distribution<float> expDist;
public:
  function<float()> numFriendsFloat;

  /*
   * We assume a default instance of 1 million users with an exponential
   * distribution of friends - as the mean for an exponential distribution
   * is 1/lambda, finding lambda given an desired mean of friends is  easy.
   */
  InitRNG(
    unsigned int numUsers = 1e6,
    float mean = 200
  )
  : UniformRNG(numUsers),
    expDist(1/mean),
    numFriendsFloat{bind(expDist,generator)} {}
};

#endif /* GCDEMO_GCDEMO_INIT_H_ */
