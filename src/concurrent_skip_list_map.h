/*
 * Copyright (©) 2015 Nate Rosenblum
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef SRC_CONCURRENT_SKIP_LIST_MAP_H_
#define SRC_CONCURRENT_SKIP_LIST_MAP_H_

#include <atomic>

#include <assert.h> // XXX

#include "hazard_pointers.h"
#include "thread_local_random.h"

namespace ccmetrics {

template<typename Key, typename Value>
class ConcurrentSkipListMap {
public:
    typedef Key key_type;
    typedef Value value_type;

    ConcurrentSkipListMap();
    ~ConcurrentSkipListMap();

    /**
     * Inserts a `key, value` tuple into the map, returning true if
     * the pair was inserted or false if an entry with the matching key
     * already existed.
     */
    bool insert(Key const& key, Value const& value);

    /**
     * Find a matching value, returning true if found.
     */
    // TODO: consider optional?
    bool find(Key const& key, Value *value);

    /** @return whether the key exists. */
    bool exists(Key const& key);

    /** @return whether the key existed (and thus was erased). */
    bool erase(Key const& key);

    void printLevel(int i) {
        Node *n = head_->next_[i];
        while (n) {
            n = n->next_[i];
        }
    }
private:
    static const int kMaxHeight = 12; // XXX explain choice of max height

    /** Returns a random level in [0, kMaxHeight - 1]. */
    static int randomLevel();

    struct Node {
        const Key key;
        const Value value;
        const uint8_t height;
        std::atomic<Node*> *next_;

        ~Node() {
            delete [] next_;
        }

        // Returns true if the node has been marked dead
        bool dead() const {
            return 0 != (reinterpret_cast<uintptr_t>(next_[0].load(std::memory_order_acquire)) & 0x1);
        }
    };

    struct NewHPFunctor {
        typename HazardPointers<Node, 4>::pointer_type* operator()(void) const {
            return smr_.hazards.allocate();
        }
    };
    static struct SMR {
        HazardPointers<Node, 4> hazards;
        ThreadLocal<typename HazardPointers<Node, 4>::pointer_type,
            NewHPFunctor> hp;
    } smr_;
    static void retireHazard(void *h) {
        smr_.hazards.retire(reinterpret_cast<
            typename decltype(smr_.hazards)::pointer_type*>(h));
    }

    /** @return a new node suitable for inclusion in `height` lists. */
    static Node* mkNode(int height, Key const& key, Value const& value);

    /** Release a node allocated with `mkNode`. */
    static void freeNode(Node *node) {
        delete node;
    }

    /**
     * Result type for find operation. The find operation returns a consistent
     * snapshot of the list state with the following elements:
     *
     *  - `cur`  is the node with key >= the input key
     *  - `prev` is the node immediately preceding cur
     *  - `next` is `cur->next`, the node following cur
     *  - `match` indicates whether cur->key == key, saving a comparison
     *
     * Node order refers to entries in the ordered set (the level-0 list).
     *
     * Each of these is guarded with a hazard pointer, ensuring that object
     * lifetime continues across the return from `find`.
     */
    struct FindResult {
        Node *prev; // hp2
        Node *cur;  // hp1
        Node *next; // hp0
        bool match;
    };

    /** Takes a snapshot for nodes around `key`; see above. */
    FindResult find(Key const& key);

    Node *head_;
    int height_; // XXX atomic

    ConcurrentSkipListMap(ConcurrentSkipListMap const&) = delete;
    ConcurrentSkipListMap& operator=(ConcurrentSkipListMap const&) = delete;
};

template<typename Key, typename Value>
typename ConcurrentSkipListMap<Key, Value>::SMR
ConcurrentSkipListMap<Key, Value>::smr_{{},
    {ConcurrentSkipListMap<Key, Value>::NewHPFunctor(),
     ConcurrentSkipListMap<Key, Value>::retireHazard}};

template<typename Key, typename Value>
typename ConcurrentSkipListMap<Key, Value>::Node*
ConcurrentSkipListMap<Key, Value>::mkNode(
        int height, Key const& key, Value const& value) {
    Node* ret = new Node({key, value, (uint8_t) height, nullptr});
    ret->next_ = new std::atomic<Node*>[height];
    memset(ret->next_, 0, height * sizeof(*ret->next_));
    return ret;
}

template<typename T>
T* mark(T *ptr) {
    return reinterpret_cast<T*>(reinterpret_cast<uintptr_t>(ptr) | 0x1);
}

template<typename T>
bool marked(T *ptr) {
    return 0 != (reinterpret_cast<uintptr_t>(ptr) & 0x1);
}

template<typename T>
T* clear(T *ptr) {
    return reinterpret_cast<T*>(reinterpret_cast<uintptr_t>(ptr) & ~0x1);
}

template<typename Key, typename Value>
ConcurrentSkipListMap<Key, Value>::ConcurrentSkipListMap()
        : head_(mkNode(kMaxHeight, Key(), Value())), height_(0) {
}

template<typename Key, typename Value>
ConcurrentSkipListMap<Key, Value>::~ConcurrentSkipListMap() {
    Node *n = head_;
    while (n) {
        Node *d = n;
        n = n->next_[0].load(std::memory_order_relaxed);
        freeNode(d);
    }
}

// The find method has two responsibilities:
//
//  1. Acquring a consistent snapshot over the <prev, cur, next> tuple
//     described in the FindResult comments
//  2. Deleting nodes that have been marked dead
//
// This is a variation on the algorithm presented by Maged Michael for
// concurrent list-base sets; the extensions add skip list indexing but do not
// change the basic algorithm or its safety properties.
template<typename Key, typename Value>
typename ConcurrentSkipListMap<Key, Value>::FindResult
ConcurrentSkipListMap<Key, Value>::find(Key const& key) {
    Node *prev, *cur, *next;

    auto& hp = *smr_.hp;

try_again:
    cur = nullptr;
    next = nullptr;
    prev = head_;

    // Unnecessary but assists in asserting the loop invariant
    hp.setHazard(2, prev); // hp2

    for (int i = height_; i >= 0; --i) {
        // Each iteration of the loop body is a minor variation on MM's
        // Find algorithm (SMR variant). Changes are called out in comments.
        // At the top of the loop body, we hold hp2 == prev and hp1 == cur

        cur = hp.loadAndSetHazard(prev->next_[i], 1); // hp1
        if (marked(cur)) {
            // Inconsistent prev *and* you have no protection from the hazard
            // pointer. Shoot again.
            goto try_again;
        }

        for (;;) {
            if (!cur) {
                break; // Break out to main index level traversal
            }

            if (!hp.loadAndSetHazardOrFail(cur->next_[i], 0, &next)) { // hp0
                // Inconsistent read of cur.next: either (1) cur marked or
                // (2) node inserted after cur. Try again.
                // XXX actually we don't give a shit about (2). should just
                // unconditionally acquire a hazard.
                goto try_again;
            }

            if (prev->next_[i] != cur) {
                // Either (1) prev marked or (2) node inserted after prev
                goto try_again;
            }

            // TODO: if we marked up from 0 we could just use cur->dead()
            bool cur_dead = marked(next) || cur->dead();

            if (!cur_dead) {
                if (cur->key >= key) {
                    // XXX on equality, short-circuit here?
                    break;
                }
                prev = cur;
                hp.setHazard(2, prev); // hp2
            } else {
                // Unlink at this level. Once dead, a node's next pointers
                // cannot be changed, so relaxed order loading is ok
                Node *nexti = cur->next_[i].load(std::memory_order_relaxed);
                if (!prev->next_[i].compare_exchange_strong(cur,
                        clear(nexti))) {
                    // CAS failure indicates (1) insertion after prev on this
                    // level or (2) concurrent unlink of cur on this level.
                    // Restart.
                    goto try_again;
                }
                // Successfully unlinked. If this was level-0, then the node
                // has been fully unlinked and can be retired
                if (i == 0) {
                    // Dump your own hazard pointer before retiring
                    hp.clearHazard(1);
                    hp.retireNode(cur);
                }
                cur = nullptr;
                // Move to the next level
                break;
            }

            cur = next;

            assert(!(((uintptr_t)cur) & 0x1));

            hp.setHazard(1, clear(cur)); // hp1
        }
    }

    return FindResult{prev, cur, next, cur ? cur->key == key : false};
}

// The Delete algorithm in MM acquires a consistent list snapshot via Find and
// then attempts to mark the node dead & unlink from the list. Failure of
// either operation will trigger a new call to Find, which is responsible for
// ensuring that dead nodes get removed from the list (repeating if necessary).
//
// In a skip list, we need to unlink the index nodes as well. This is a bit of
// a headache; with SMR it is prohibitively expensive to acquire a consistent
// snapshot over the index nodes, (HP storage cost linear in height of the
// index), and the window of the snapshot is comparatively long, increasing
// the likelihood of an intermediate modification & restart. In this algorithm,
// we execute the marking phase of erase (observing that the restart on
// mutation of cur->next is unnecessary) and then follow it by invocation of
// Find, which removes marked nodes on both the index and level-0 lists. This
// is directly analogous to the contended case of MM's Delete (without index
// lists, of course). Hence erase costs at minimum 2xFind.
//
// TODO: we could instead implement a Find-for-erase that implements index
// unlinking on the descent path.
template<typename Key, typename Value>
bool ConcurrentSkipListMap<Key, Value>::erase(Key const& key) {
    bool marked_0 = false;
    const auto result = find(key);
    if (!result.match) {
        return false;
    }

    // Mark the node dead at all index levels. In MM's Delete algorithm, CAS
    // failure here triggers a restart & new Find. We observe that marking a
    // node does not require a consistent <prev, cur, next> snapshot,
    // so the algorithm is tolerant to CAS failure; we simply reload
    // the `next_i` pointer at each level. Since we are not accessing
    // `next_i`, we needn't hold a hazard pointer on it.
    for (int i = result.cur->height - 1; i >= 0; --i) {
        Node *nexti = result.cur->next_[i];
        while (!result.cur->next_[i].compare_exchange_strong(nexti,
                mark(nexti))) {
            nexti = result.cur->next_[i];
        }
        if (i == 0 && nexti == clear(nexti)) {
            // CAS on list 0 was linearization point for erase; this ensures
            // that only one of 2+ concurrent erase invocations can return
            // true. Only one thread can succeed in marking next0.
            marked_0 = true;
        }
    }

    // Node is marked dead. We don't have a consistent snapshot at every
    // level with which to unlink the node from the list, so we invoke
    // find again to free the deleted node.
    const auto result2 = find(key);
    assert(!result2.match || result2.cur != result.cur);

    auto& hp = *smr_.hp;
    hp.clearHazard(0);
    hp.clearHazard(1);
    hp.clearHazard(2);

    return marked_0;
}

template<typename Key, typename Value>
int ConcurrentSkipListMap<Key, Value>::randomLevel() {
    int64_t r = ThreadLocalRandom::current().next();
    // P = 0.5, K = 0. See Pugh's cookbook.
    int level = 0;
    while (level < kMaxHeight - 1 && ((r >>= 1) & 1) != 0) {
        ++level;
    }
    return level;
}

template<typename Key, typename Value>
bool ConcurrentSkipListMap<Key, Value>::find(Key const& key, Value *value) {
    auto result = find(key);
    if (result.match) {
        *value = result.cur->value;
    }

    auto& hp = *smr_.hp;
    hp.clearHazard(0);
    hp.clearHazard(1);
    hp.clearHazard(2);

    return result.match;
}

template<typename Key, typename Value>
bool ConcurrentSkipListMap<Key, Value>::exists(Key const& key) {
    FindResult res = find(key);

    auto& hp = *smr_.hp;
    hp.clearHazard(0);
    hp.clearHazard(1);
    hp.clearHazard(2);

    return res.match;
}

template<typename Key, typename Value>
bool ConcurrentSkipListMap<Key, Value>::insert(Key const& key,
        Value const& value) {
    int level = randomLevel();
    if (level > height_) {
        // Clamp growth to +1
        height_ = level = height_ + 1;
    }

    FindResult result = find(key);
    if (result.match) {
        return false;
    }

    auto& hp = *smr_.hp;

    // We're going to publish this value before we finish working with
    // it (linking it in to the index lists); this means we'll need to
    // hold a hazard pointer throughout
    Node *n = mkNode(level + 1, key, value);
    hp.setHazard(3, n);

    for (;;) {
        n->next_[0].store(result.cur);
        if (result.prev->next_[0].compare_exchange_strong(result.cur, n)) {
            break;
        }

        // A possibly intervening entry was inserted. It is always correct
        // to repeat the search from the root down. If the predecessor was
        // not *removed* then it's safe to search forward; this is a possible
        // future optimization.

        result = find(key);
        if (result.match) {
            hp.clearHazard(0);
            hp.clearHazard(1);
            hp.clearHazard(2);
            hp.clearHazard(3);
            // Node was never visible; needn't retire via HP
            freeNode(n);
            return false;
        }
    }

    // At this point we have completed MM's Insert algorithm. We now
    // build up the index entries, following the structure of `find`.
    //
    // Concurrent modification of the list can lead to CAS failures here.
    // Because the index lists are an optimization only, this initial
    // implementation simply bails out of index creation on the first
    // inconsistency. This only impacts performance, not correctness.
    Node *prev = head_;
    Node *next = nullptr;
    for (int i = std::max(level, height_); i > 0; --i) {
        Node *cur = hp.loadAndSetHazard(prev->next_[i], 1); // hp1
        if (marked(cur)) {
            goto exit;
        }

        while (cur && cur->key < key) {
            // Don't care about spurious failures due to insertion after `cur`.
            next = hp.loadAndSetHazard(cur->next_[i], 0);
            if (marked(next)) {
                goto exit;
            }

            if (prev->next_[i] != cur) {
                // XXX prev changed; you have to restart the search in
                // this case. Should we actually retry, or just give up
                // index insertion? It's ok to fail.
                // XXX double check that it's ok to fail
                goto exit;
            }

            prev = cur;
            hp.setHazard(2, prev); // hp2
            cur = next;
            hp.setHazard(1, cur); // hp1
        }

        if (i <= level) {
            n->next_[i].store(cur);
            if (!prev->next_[i].compare_exchange_strong(cur, n)) {
                // XXX insertion after prev. Either retry the whole
                // thing or abort. XXX
                goto exit;
            }
        }
        if (n->dead()) {
            // We were concurrently erased; just give up and get out.
            goto exit;
        }
    }
exit:

    hp.clearHazard(0);
    hp.clearHazard(1);
    hp.clearHazard(2);
    hp.clearHazard(3);

    return true;
}

} // ccmetrics namespace

#endif // SRC_CONCURRENT_KIP_LIST_MAP_H_
