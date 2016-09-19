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
 * gcdemo-init.cpp
 *
 * Created on: June 9, 2016
 *     Author: uversky
 */

#include "mpgc/gc.h"
#include "gcdemo-init.h"

#include <atomic>
#include <array>
#include <chrono>
#include <cmath>
#include <getopt.h>
#include <fstream>
#include <iostream>
#include <locale>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

using namespace std;
using namespace mpgc;

const unsigned long _DEFAULT_NUM_USERS = 1e6,
                    _DEFAULT_NUM_ITERS = 10000;
const unsigned int  _DEFAULT_FEED_LENGTH = 250,
                    _DEFAULT_NUM_TOTAL_THREADS = 1,
                    _DEFAULT_MEAN = 200;
static const string _DEFAULT_NAME = "com.hpe.gcdemo.users";

void show_usage() {
   cerr << "usage: ./gcdemo-init [options]\n\n"
        << "Initialize a randomized graph of users and their friendships in the persistent heap."
        << "\n\n"
        << "Options:\n"
        << "-i, --" << underline("i") << "ters <i>\n"
        << "  Specifies the total number of action iterations across all processes and threads.\n"
        << "  An action involves picking a random user and then having that user either\n"
        << "    comment or post, based on the probability specified by -r.\n"
        << "  Default: " << _DEFAULT_NUM_ITERS << ".\n"
        << "-f , --" << underline("f") << "orce\n"
        << "  If a graph already exists with the given key, force its removal and re-creation.\n"
        << "  Otherwise, a graph existing under the given name will display a prompt asking to\n"
        << "  either delete the old graph or abort the initialization so the clients can use\n"
        << "  the old graph.\n"
        << "-h, --" << underline("h") << "elp\n"
        << "  Display this message.\n"
        << "-l, --feed-" << underline("l") << "ength <l>\n"
        << "  Specifies the maximum length of the user's feed.  The user's feed acts as a queue,\n"
        << "  so the <l>+1th post will make the earliest post drop off the feed.\n"
        << "  Default: " << _DEFAULT_FEED_LENGTH << ".\n"
        << "-m, --" << underline("m") << "ean <m>\n"
        << "  Users' friendships follow an exponential distribution.  This argument sets the mean\n"
        << "  for that distribution, i.e. the average number of friends that a user might have.\n"
        << "  Default: " << _DEFAULT_MEAN << ".\n"
        << "-n, --" << underline("n") << "ame <n>\n"
        << "  Specify the name (i.e. key) used to locate the graph in the persistent heap.\n"
        << "  Default: \'" << _DEFAULT_NAME << "\'.\n"
        << "-s, --" << underline("s") << "kip-init\n"
        << "  Setting this flag skips the graph initialization phase; persistent counters are\n"
        << "  cleared but no other steps are taken.\n"
        << "-t, --total-num-" << underline("t") << "hreads <t>\n"
        << "  Specifies the total number of threads that will run concurrently after the\n"
        << "  initialization process completes.  This setting is used to properly reset a\n"
        << "  flag in the persistent heap.\n"
        << "  Default: " << _DEFAULT_NUM_TOTAL_THREADS << ".\n"
        << "-u, --num-" << underline("u") << "sers <u>\n"
        << "  Initialize graph with <u> users.\n"
        << "  Default: " << _DEFAULT_NUM_USERS << ".\n";
}

void
addFriends(UserGraphPtr userPtr, unsigned int mean)
{
  UserGraph users = *userPtr;
  const unsigned long numUsers = users.size();
  InitRNG userrng(numUsers, mean);

  // Create reciprocal friendships probabilistically
  unsigned long r = 0;

  // placeholder value to deduce type for finding
  auto jj = find(users[0]->friends.begin(), users[0]->friends.end(), users[0]);

  for (unsigned long i = 0; i < numUsers; i++) {
    // Due to the fact that we add friendships reciprocally, the number of
    // iterations here actually needs to be halved in order to get a mean 
    // number of friends equivalent to the mu specified.
    //
    // Further, random::exponential_distribution yields a positive real value,
    // so we coerce into a non-negative integer value via ceil(...).
    auto u = users[i];
    unsigned int numFriends = ceil(userrng.numFriendsFloat() / 2);
    for (unsigned int j = 0; j < numFriends; j++) {
      do {
        // Find a random user that is not me and is not already my friend.
        // Make sure to abort early if we've exhausted all of the possible users.
        r = userrng.randElt();
        jj = find(u->friends.begin(), u->friends.end(), users[r]);
      } while ((r == u->id || jj != u->friends.end()) && u->friends.size() < numUsers - 1);

      // Reciprocally become friends
      auto v = users[r];

      u->friends.push_back(v);
      v->friends.push_back(u);
    }

    advanceProgressBar(i, numUsers);
  }
}

/*
 * populateUsers(numUsers, mu, sigma, feedLength)
 * Create a vector of <numUsers> users.  For each user, add random 
 *  reciprocal friendships via random generation to get a normal
 *  distribution for the number of friends for each user.  Each user 
 *  has a feed of maximum length <feedLength>.
 */
UserGraphPtr
populateUsers(const unsigned long numUsers, unsigned int mean, unsigned int feedLength)
{
  InitRNG userrng(numUsers, mean);
  auto users = UserGraph(numUsers);
  
  // Read names from name file
  vector<string> names;
  string nameStr = "../../../src/tools/gcdemo-init/usernames.txt";
  ifstream f;
  string line;

  cout << "Reading names..." << flush;
  f.open(nameStr);
  while (getline(f, line))
    names.push_back(line);
  f.close();
  cout << "Done.\n" << flush;

  cout << "Populating graph with users..." << flush;
  // Populate vector with randomly-named users
  UniformRNG namerng(names.size());
  users.reserve(numUsers);
  for (unsigned long i = 0; i < numUsers; i++) {
    users[i] = make_gc<User>(feedLength, names[namerng.randElt()]);
  }
  cout << "Done.\n" << flush;

  cout << "Generating friendships (this step might take a while)...\n" << flush;
  displayProgressBarHeader();
  UserGraphPtr usersPtr = make_gc<WrappedUserGraph>(users);
  addFriends(usersPtr, mean);
  cout << "\nDone." << endl;

  cout << "Calculating statistics for friends per user..." << endl;
  double avgFriends = 0;
  unsigned long maxFriends = 0, minFriends = numUsers;
  gc_string minName, maxName;
  unsigned long minId, maxId;
  for (auto u : users) {
    unsigned long nf = u->friends.size();
    if (nf > maxFriends) {
      maxFriends = nf;
      maxName = u->name;
      maxId = u->id;
    }
    if (nf < minFriends) {
      minFriends = nf;
      minName = u->name;
      minId = u->id;
    }
    avgFriends += nf;
  }
  avgFriends /= users.size();
  cout << "  Avg: " << avgFriends << endl;
  cout << "  Min: User " << minName << " [id: " << minId << "] has " << minFriends
       << " friends." << endl;
  cout << "  Max: User " << maxName << " [id: " << maxId << "] has " << maxFriends
       << " friends." << endl;
  
  return usersPtr;
}

int main(int argc, char ** argv)
{
  // For legibility's sake, long numbers should print with comma separators.
  // This is accomplished by setting the locale for our output streams - here
  // we set to "", or the local machine's current language settings.
  cout.imbue(locale(""));
  cerr.imbue(locale(""));

  // Boilerplate for option processing
  struct option long_options[] = {
    {"force",             no_argument,       0, 'f'},
    {"help",              no_argument,       0, 'h'},
    {"iters",             required_argument, 0, 'i'},
    {"feed-length",       required_argument, 0, 'l'},
    {"mean",              required_argument, 0, 'm'},
    {"name",              required_argument, 0, 'n'},
    {"skip-init",         no_argument,       0, 's'},
    {"total-num-threads", required_argument, 0, 't'},
    {"num-users",         required_argument, 0, 'u'},
    {0,                   0,                 0,  0 }
  };

  unsigned long numUsers   = _DEFAULT_NUM_USERS,
                iters      = _DEFAULT_NUM_ITERS;
  unsigned int  feedLength = _DEFAULT_FEED_LENGTH,
                totalThreads = _DEFAULT_NUM_TOTAL_THREADS,
                mean       = _DEFAULT_MEAN;
  string        prName     = _DEFAULT_NAME;
  bool          forceClear = false,
                skipInit   = false;

  int opt;

  while((opt = getopt_long(argc, argv, "fhi:l:m:n:st:u:", long_options, nullptr)) != -1) {
    switch(opt) {
      case 'f': forceClear = true;
                break;
      case 'h': show_usage();
                return 0;
      case 'i': iters = atol(optarg);
                break;
      case 'l': feedLength = atoi(optarg);
                break;
      case 'm': mean = atoi(optarg);
                break;
      case 'n': prName = optarg;
                break;
      case 's': skipInit = true;
                break;
      case 't': totalThreads = atoi(optarg);
                break;
      case 'u': numUsers = atol(optarg);
                break;
      case '?': show_usage();
                return -1;
    }
  }

  // More argument checking boilerplate...
  bool checkFailed = false;
  if ((checkFailed = (numUsers == 0)))
    cerr << "Number of users must be a positive integer.\n";
  else if ((checkFailed = (iters == 0)))
    cerr << "Number of iterations must be a positive integer.\n";
  else if ((checkFailed = (feedLength == 0)))
    cerr << "Feed length must be a positive integer.\n";
  else if ((checkFailed = (mean == 0)))
    cerr << "Mean must be a positive integer.\n";
  else if ((checkFailed = (totalThreads == 0)))
    cerr << "Total number of threads must be a positive integer.\n";

  if (checkFailed) {
    show_usage();
    return -1;
  }
  // Argument checking complete.

  // Clear persistent heap counters
  cout << "Clearing persistent heap counters..." << flush;
  string phIterCtrName        = prName + ".iterCtr";
  string phRunningThreadsName = prName + ".runningThreads";
  string phMaxThreadsName     = prName + ".maxThreads";
  string phUsersNotifiedName  = prName + ".usersNotified";
  string phMissesName         = prName + ".misses";
  string phTotalWindowsName   = prName + ".totalWindows";

  AtomicULPtr phIterCtrPtr =
    persistent_roots().lookup<gc_wrapped<atomic<unsigned long>>>(phIterCtrName);
  AtomicUIPtr phRunningThreadsPtr =
    persistent_roots().lookup<gc_wrapped<atomic<unsigned int>>>(phRunningThreadsName);
  AtomicUIPtr phMaxThreadsPtr =
    persistent_roots().lookup<gc_wrapped<atomic<unsigned int>>>(phMaxThreadsName);
  AtomicULPtr phUsersNotifiedPtr =
    persistent_roots().lookup<gc_wrapped<atomic<unsigned long>>>(phUsersNotifiedName);
  AtomicULPtr phMissesPtr =
    persistent_roots().lookup<gc_wrapped<atomic<unsigned long>>>(phMissesName);
  AtomicULPtr phTotalWindowsPtr =
    persistent_roots().lookup<gc_wrapped<atomic<unsigned long>>>(phTotalWindowsName);

  if (phIterCtrPtr == nullptr) {
    auto iterCtrPtr = make_gc<gc_wrapped<atomic<unsigned long>>>(0);
    persistent_roots().store(phIterCtrName, iterCtrPtr);
  }
  else {
    phIterCtrPtr->store(0);
  }
  if (phRunningThreadsPtr == nullptr) {
    auto runningThreadsPtr = make_gc<gc_wrapped<atomic<unsigned int>>>(0);
    persistent_roots().store(phRunningThreadsName, runningThreadsPtr);
  }
  else {
    phRunningThreadsPtr->store(0);
  }
  if (phMaxThreadsPtr == nullptr) {
    auto maxThreadsPtr = make_gc<gc_wrapped<atomic<unsigned int>>>(totalThreads);
    persistent_roots().store(phMaxThreadsName, maxThreadsPtr);
  }
  else {
    phMaxThreadsPtr->store(totalThreads);
  }
  if (phUsersNotifiedPtr == nullptr) {
    auto notifiedPtr = make_gc<gc_wrapped<atomic<unsigned long>>>(0);
    persistent_roots().store(phUsersNotifiedName, notifiedPtr);
  }
  else {
    phUsersNotifiedPtr->store(0);
  }
  if (phMissesPtr == nullptr) {
    auto missesPtr = make_gc<gc_wrapped<atomic<unsigned long>>>(0);
    persistent_roots().store(phMissesName, missesPtr);
  }
  else {
    phMissesPtr->store(0);
  }
  if (phTotalWindowsPtr == nullptr) {
    auto totalWindowsPtr = make_gc<gc_wrapped<atomic<unsigned long>>>(0);
    persistent_roots().store(phTotalWindowsName, totalWindowsPtr);
  }
  else {
    phTotalWindowsPtr->store(0);
  }

  cout << "Done." << endl;

  if (skipInit) {
    cout << "Skipping graph initialization step." << endl;
    return 0;
  }

  cout << "Initializing social network graph with the following parameters:" << endl
       << "  Force new graph?          " << (forceClear ? "Yes" : "No") << endl
       << "  Feed length:              " << feedLength << endl
       << "  Mean # friends:           " << mean << endl
       << "  Name in persistent heap:  " << '\'' << prName << '\'' << endl
       << "  Number of users:          " << numUsers << endl
       << "  Total number of threads:  " << totalThreads << endl
       << endl;

  // See if the graph already exists
  UserGraphPtr userPtr = persistent_roots().lookup<WrappedUserGraph>(prName);
  string confirm = "n";
  if (userPtr != nullptr) {
    if (!forceClear) {
      cout << "Found a graph with this key in the persistent heap! Delete? (y/N): ";
      cin >> confirm;
      if (confirm.length() == 0 || (confirm[0] != 'y' && confirm[0] != 'Y')) {
        cout << "Keeping existing graph." << endl;
        cout << "Graph initialization complete." << endl;
        return 0;
      }
      else  // confirmation began with 'y' or 'Y'
        cout << "Deleting existing graph." << endl;
    }
    else {
      cout << "Force clear set - deleting existing graph." << endl;
    }
    persistent_roots().remove(prName);
  }

  // The first time a heap is used for benchmarking, it should be in a steady state.
  // Set a flag to ensure that this happens on the first benchmarking run.
  //string ssFlagStartName = prName + ".ssflag-start";
  //string ssFlagEndName   = prName + ".ssflag-done";
  //persistent_roots().store(ssFlagStartName, make_gc<gc_wrapped<atomic<bool>>>(false));
  //persistent_roots().store(ssFlagEndName,   make_gc<gc_wrapped<atomic<bool>>>(false));
  string ssFeedsFullName = prName + ".ssFeeds-Full";
  string ssFeedsFullCtrName = prName + ".ssFeeds-Full-Counter";
  persistent_roots().store(ssFeedsFullName, make_gc_array<atomic<bool>>(numUsers));
  persistent_roots().store(ssFeedsFullCtrName, make_gc<gc_wrapped<atomic<unsigned long>>>(0));

  // Populate user graph with randomly-named users
  chrono::steady_clock::time_point t1 = chrono::steady_clock::now();
  UserGraphPtr users = populateUsers(numUsers, mean, feedLength);
  persistent_roots().store(prName, users);
  chrono::steady_clock::time_point t2 = chrono::steady_clock::now();
  auto d = chrono::duration_cast<chrono::milliseconds>(t2 - t1);

  cout << "Graph stored in persistent heap under the name \'" << prName << "\'." << endl;
  cout << "Graph initialization complete in " << d.count() << " ms." << endl;

  return 0;
}
