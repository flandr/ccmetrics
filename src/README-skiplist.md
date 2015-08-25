# Concurrent skip list notes

Observations used for the distribution estimates in the `Timer` metric are
stored in stored as an ordered map of `key, value` pairs. We use a skip list
for this map structure, because it admits a lock-free implementation. Although
a concurrent implementation of the base list is straightforward, updating the
index lists while maintaining safe memory reclamation in the face of
concurrent updates is highly complicated.

## Incomplete towers during insertion

Marking the towers top-down can cause partitioning at a list level, reducing
the effectiveness of the index lists:

T1, start linking node 1 w/ an incomplete tower:

```
+-+                              +-+
| +----------------------------> | |
+-+                              +-+

+-+                       +-+    +-+
| +--+                    |-|--> | |
+-+  |                    +-+    +-+
     v

+-+                       +-+    +-+
| +---------------------> | |--> | |
+-+                       +-+    +-+

(1)                       (9)    (10)
```

T2, fully link node 5, picking up the incomplete references from node 1

```
+-+        +-+                   +-+
| +------> | |-----------------> | |
+-+        +-+                   +-+

+-+        +-+            +-+    +-+
| +--+     | |--+         |-|--> | |
+-+  |     +-+  |         +-+    +-+
     v          v

+-+        +-+            +-+    +-+
| +--------| |----------> | |--> | |
+-+        +-+            +-+    +-+

(1)        (5)            (9)    (10)
```

T3 finishing linking node1, cutting off the middle level of node 5.

```
+-+        +-+                   +-+
| +------> | |-----------------> | |
+-+        +-+                   +-+
     +----------------+
+-+  |     +-+        |   +-+    +-+
| +--+     | |--+     +-> |-|--> | |
+-+        +-+  |         +-+    +-+
                v

+-+        +-+            +-+    +-+
| +--------| |----------> | |--> | |
+-+        +-+            +-+    +-+

(1)        (5)            (9)    (10)
```

This is not a correctness issue ; the invariants for the level-0 list are
preserved. It is confusing, tho.

Ideally we would form towers bottom-up, preventing the observation of the
lower levels of partially-completed towers. Unfortunately there is no way to
do this with a finite set of hazard pointers, except with n\*log(n) cost of
repeated calls to find.

This situation does compromise the effectiveness of the index lists. More
experience is needed to determine whether it is a problem.
