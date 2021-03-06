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
 * Graph.h
 *
 *  Created on: June 1, 2016
 *      Author: uversky
 */

#ifndef GCDEMO_GRAPH_H_
#define GCDEMO_GRAPH_H_

#include "mpgc/gc.h"
#include "mpgc/gc_string.h"
#include "mpgc/gc_vector.h"
#include <algorithm>
#include <atomic>
#include <string>
#include <unordered_set>
#include <vector>

//#define TEST_WEAK_PTRS

using namespace std;
using namespace mpgc;

class User;
class Post;

class Comment : public gc_allocated {
public:
  const gc_ptr<User>                    commentor;
  const gc_ptr<Post>                    parent;
  const gc_string                       commentText;
  const gc_array_ptr<gc_ptr<User>>      tagged;
  gc_ptr<Comment>                       next;

  Comment(gc_token &gc,
          gc_ptr<User> cc = nullptr,
          gc_ptr<Post> pp = nullptr,
          string text = "i am a comment on a post",
          gc_array_ptr<gc_ptr<User>> tags = nullptr)
    : gc_allocated{gc}, commentor(cc), parent(pp), commentText(text), tagged(tags), next(nullptr) {}

  static const auto &descriptor() {
    static gc_descriptor d = 
      GC_DESC(Comment)
      .WITH_FIELD(&Comment::commentor)
      .WITH_FIELD(&Comment::parent)
      .WITH_FIELD(&Comment::commentText)
      .WITH_FIELD(&Comment::tagged)
      .WITH_FIELD(&Comment::next);
    return d;
  }
};

class Post : public gc_allocated {
public:
  const gc_ptr<User>                    poster;
  const gc_string                       postText;
  const gc_array_ptr<gc_ptr<User>>      tagged;
  atomic<gc_array_ptr<gc_ptr<User>>>    subscribers;
  atomic<gc_ptr<Comment>>               comments;

  Post(gc_token &gc, string pp = "i am a default post")
    : gc_allocated{gc},
      poster(nullptr),
      postText(pp),
      tagged{make_gc_array<gc_ptr<User>>(0)},
      subscribers{make_gc_array<gc_ptr<User>>(0)},
      comments{nullptr}
  {}

  Post(gc_token &gc, gc_ptr<User> usr, string pp, const gc_array_ptr<gc_ptr<User>> &tags)
    : gc_allocated{gc},
      poster(usr),
      postText(pp),
      tagged{tags},
      subscribers{make_gc_array<gc_ptr<User>>(1)},
      comments{nullptr}
  {
    //We know that only the poster himself is the only subscriber at the time of creating the post.
    auto localSubs = subscribers.load();
    localSubs[0] = usr;
  }

  static const auto &descriptor() {
    static gc_descriptor d = 
      GC_DESC(Post)
      .WITH_FIELD(&Post::poster)
      .WITH_FIELD(&Post::postText)
      .WITH_FIELD(&Post::tagged)
      .WITH_FIELD(&Post::subscribers)
      .WITH_FIELD(&Post::comments);
    return d;
  }

  void addSubscriber(gc_ptr<User>);
  size_t addSubscribers(const unordered_set<gc_ptr<User>>&);

  size_t addComment(gc_ptr<Comment> c)
  {
    bool cas = false;
    size_t count = 0;
    gc_ptr<Comment> expected = comments;
    do {
      c->next = expected;
      count++;
      cas = comments.compare_exchange_weak(expected, c);
    } while (!cas);
    addSubscriber(c->commentor);
    return count;
    //  Dropping the pointer to the old array on the floor and letting the
    //  garbage collector worry about the cleanup.
  }
};

class User : public gc_allocated {
private:
  static unsigned long nextId() {
    static atomic<unsigned long> _nextId(1);
    return _nextId++;
  }

#ifdef TEST_WEAK_PTRS
  constexpr static std::size_t nr_posts = 64;
  using post_ptr_t = weak_gc_ptr<Post>;
#else
  using post_ptr_t = gc_ptr<Post>;
#endif
public:
  const unsigned long id,
                      feedLength;
  atomic<unsigned long> numSubscribers; // number of times subscribed to posts, even ones that fell off
  gc_string name;
  atomic<gc_array_ptr<post_ptr_t>> feed;
#ifdef TEST_WEAK_PTRS
  atomic<size_t> oldest_post_idx;
  const gc_array_ptr<gc_ptr<Post>> posts;
#endif
  gc_vector<gc_ptr<User>>            friends;

  User(gc_token &gc, unsigned long fl = 200, string nm = "John Doe")
    : gc_allocated{gc},
      id{nextId()},
      feedLength(fl),
      numSubscribers(0),
      name(nm),
      feed{make_gc_array<post_ptr_t>(0)},
#ifdef TEST_WEAK_PTRS
      oldest_post_idx(0),
      posts{make_gc_array<gc_ptr<Post>>(nr_posts)},
#endif
      friends{gc_vector<gc_ptr<User>>()} {}

  static const auto &descriptor() {
    static gc_descriptor d = 
      GC_DESC(User)
      .WITH_FIELD(&User::id)
      .WITH_FIELD(&User::feedLength)
      .WITH_FIELD(&User::numSubscribers)
      .WITH_FIELD(&User::name)
      .WITH_FIELD(&User::feed)
#ifdef TEST_WEAK_PTRS
      .WITH_FIELD(&User::oldest_post_idx)
      .WITH_FIELD(&User::posts)
#endif
      .WITH_FIELD(&User::friends);
    return d;
  }

  bool isFeedFull() const {
    return feed.load()->size() == feedLength;
  }

  bool isFeedEmpty() const {
    return feed.load()->size() != 0;
  }

#ifdef TEST_WEAK_PTRS
  static bool weak_pred(const post_ptr_t &post) {
    return !post.expired();
  }
#endif

  size_t pushToFeed(gc_ptr<Post> p) {
    bool cas = false;
    gc_array_ptr<post_ptr_t> newFeed = nullptr;

    size_t count;
#ifdef TEST_WEAK_PTRS
    if (p->poster == this) {
      count = oldest_post_idx++;
      while (count >= nr_posts) {
        count++;
        oldest_post_idx.compare_exchange_strong(count, 0);
        count = oldest_post_idx++;
      }
      posts[count] = p;
    }
#endif
    count = 0;
    gc_array_ptr<post_ptr_t> localFeed = feed;
    while (!cas) {
      const bool   isEmpty = localFeed.size() == 0;
      const bool   isFull  = localFeed.size() >= feedLength;
      const size_t newSize = min(localFeed.size() + 1, feedLength);

      if (newFeed.size() != newSize) {
        newFeed = make_gc_array<post_ptr_t>(newSize);
      }

      if (!isEmpty) {
#ifdef TEST_WEAK_PTRS
        copy_if(localFeed.begin() + (isFull ? 1 : 0),  // drop earliest post if full
                localFeed.end(), newFeed.begin(), weak_pred);
#else
        copy(localFeed.begin() + (isFull ? 1 : 0),
             localFeed.end(), newFeed.begin());
#endif
      }
      newFeed->back() = p;
      count++;
      cas = feed.compare_exchange_strong(localFeed, newFeed);
    }
    return count;
  }

  size_t bumpInFeed(gc_ptr<Post> p) {
    bool cas = false;
    size_t count = 0;
    gc_array_ptr<post_ptr_t> newFeed = nullptr;
    gc_array_ptr<post_ptr_t> localFeed = feed;
    while (!cas) {
      // If the post is in the feed; bump it; otherwise add a new post.
#ifdef TEST_WEAK_PTRS
      bool isPostFound = false;
      auto iter = localFeed.begin();
      while (iter != localFeed.end()) {
        gc_ptr<Post> temp = iter->lock();
        if (temp == p) {
          isPostFound = true;
          break;
        }
        iter++;
      }
#else
      auto iter = find(localFeed.begin(), localFeed.end(), p);
      bool isPostFound = iter != localFeed.end();
#endif
      if (isPostFound) {
        // Bump an existing post
        if (newFeed.size() != localFeed.size()) {
          newFeed = make_gc_array<post_ptr_t>(localFeed.size());
        }
#ifdef TEST_WEAK_PTRS
        auto it = copy_if(localFeed.begin(), iter, newFeed.begin(), weak_pred);  // [begin, i)
        copy_if(iter + 1, localFeed.end(), it, weak_pred); // Skip the post
#else
        auto it = copy(localFeed.begin(), iter, newFeed.begin());  // [begin, i)
        copy(iter + 1, localFeed.end(), it); // Skip the post
#endif
      }
      else {
        // Add a new post
        const bool isEmpty = localFeed.size() == 0;
        const bool isFull  = localFeed.size() >= feedLength;
        const size_t newSize = min(localFeed.size() + 1, feedLength);
        if (newFeed.size() != newSize) {
          newFeed = make_gc_array<post_ptr_t>(newSize);
        }
        if (!isEmpty) {
#ifdef TEST_WEAK_PTRS
          copy_if(localFeed.begin() + (isFull ? 1 : 0),  // drop earliest post if full
                  localFeed.end(), newFeed.begin(), weak_pred);
#else
          copy(localFeed.begin() + (isFull ? 1 : 0),  // drop earliest post if full
                  localFeed.end(), newFeed.begin());
#endif
        }
      }
      newFeed->back() = p;  // Add it back in at the end
      count++;
      cas = feed.compare_exchange_strong(localFeed, newFeed);
    }
    return count;
  }
};

void Post::addSubscriber(gc_ptr<User> u)
{
  bool cas = false;
  while (!cas) {
    gc_array_ptr<gc_ptr<User>> localSubs = subscribers;
    unordered_set<gc_ptr<User>> s;
    s.reserve(localSubs.size() + 1);
    s.insert(localSubs.begin(), localSubs.end());
    s.insert(u);
    auto newSubs = make_gc_array<gc_ptr<User>>(s.begin(), s.end());
    cas = subscribers.compare_exchange_strong(localSubs, newSubs);
  }
  u->numSubscribers++;
}

size_t Post::addSubscribers(const unordered_set<gc_ptr<User>> &newSubscribers)
{
  bool cas = false;
  size_t count = 0;
  while (!cas) {
    gc_array_ptr<gc_ptr<User>> localSubs = subscribers;
    unordered_set<gc_ptr<User>> s(newSubscribers);
    s.insert(localSubs.begin(), localSubs.end());

    auto newSubs = make_gc_array<gc_ptr<User>>(s.begin(), s.end());
    cas = subscribers.compare_exchange_strong(localSubs, newSubs);
    count++;
  }

  for (auto u : newSubscribers) {
    u->numSubscribers++;
  }
  return count;
}

#endif /* GCDEMO_GRAPH_H */
