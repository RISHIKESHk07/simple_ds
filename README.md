# simple_data_store

## Introduction

Hello there! This is my implementation of a data store for another project (a simple web server), mainly for session management and to act as a cache backend with an LSM-tree storage engine. That is the short explanation for why this project is going to take a while to implement. The main reason is that I am writing as many tree implementations as possible to test which one performs the best, while also learning more about different tree-based data structures.

Since an LSM tree is not actually a tree data structure, I will keep that aside for now. Instead, I will focus on AVL Trees, Red-Black Trees, B-Trees, B+ Trees, B* Trees, Treaps, and Skip Lists. I will try to implement all of these (hopefully I can keep up). Below is the current status.

I do realize that my implementations may not be as fast as theoretical or production-grade implementations, so it might take quite some time before this project reaches that level. However, I find it much more interesting to learn by building everything myself.

I also came across a research paper by the amazing Sun Yihan that discusses parallel processing of self-balancing trees and augmented trees, along with the PAM library. My main goal here is to use these ideas to build a simple but different style of key-value store, with an in-memory database as the initial application, and then extend it further in the future if needed.

(DONE CHECKLIST)

* AVL Trees: INS/DEL/JOIN/SPLIT/UNION & RANDOMIZED TESTING

**NOTE:** I realized that my AVL implementation is somewhat sloppy and reckless. It is not strictly **O(log N)** as it should be; in several places it is closer to **O(log² N)**, or even **O(N log N)**, because of excessive recomputation of node heights. The proper solution would be to maintain node heights correctly instead of repeatedly recomputing them. I got carried away while writing it, and there are several places where I could have updated heights much more consistently. Hopefully I will not make the same mistake when implementing the remaining data structures, and I will come back later to clean this implementation up. My main reason for this project is to learn rather than chase perfection (I wish I were that good).

* RB Trees: INS/DEL/JOIN/SPLIT/UNION & RANDOMIZED TESTING

**NOTE:** This implementation is done properly, but it could use better formatting and additional comments. It is probably better to refer to the Wikipedia article if you want to understand the actual structure.

* B-Trees and B+ Trees: INS/DEL/RANGE SCAN & RANDOMIZED TESTING

**NOTE:** I am using an order-4 B-tree here. The Wikipedia article is a good reference, but I found simpler tutorials much easier to follow when implementing the deletion cases.

* Skiplist : INS/DEL/RANGE SCAN/INDEX LOOKUP using SPAN & RANDOMIZED TESTING

**NOTE:** This is a simple skiplist implementation which does not use quasi randomization for masking our links , and uses a simple standard probabilistic generator (p=0.5) , so this could be tweaked for better performance but something which more of experimental fine tuning , not something which guartnees better performance. Skiplist is used for ZSET implementation alongside unordered-map.

## Some notes on analysis

* Coming to our analysis, both AVL Trees and Red-Black Trees are self-balancing trees, and you can tell just from their algorithms that neither of them is going to give your CPU much of a break while maintaining balance. These solutions are rigid and strict about preserving their balancing invariants. They always aim for the best possible tree height, which is great theoretically, but when we start thinking about sub-second performance on tens of thousands of nodes or more, this becomes difficult because every insertion or deletion may require revalidation and rebalancing whenever an invariant is violated.

  The same idea also applies to weighted trees such as Treaps. I will not be implementing Treaps, as I do not currently see much benefit over the other structures. The main difference is that, instead of explicitly storing and maintaining a height attribute like AVL Trees, they rely on randomly assigned priorities (weights) to maintain balance.

  So, you might ask why I am implementing these structures at all. The main reason is to complete my understanding of why these trees were developed in the first place, how we transitioned to more modern solutions, and why algorithms that look elegant in theory can become much more complicated when implemented in practice, sometimes resulting in solutions that are far from satisfactory.

  Before I start praising M-ary trees—more specifically B-Trees and B+ Trees—it is worth mentioning that we can significantly improve the performance of self-balancing trees by using join-based algorithms. These algorithms allow bulk operations to be executed in parallel while also supporting logarithmic-time joins between trees. Using **join**, **split**, and **union**, we can accelerate many operations and make these data structures much more competitive with newer alternatives.

  Another advantage is that join-based algorithms are generally simpler to implement and naturally support concurrent execution without requiring complicated recursive balancing logic or patchwork code.

  The idea of join-based algorithms comes from **Sun Yihan's CMU thesis**, which presents excellent work on parallel tree algorithms. The thesis introduces **P-Trees**, which combine persistent trees with join-based algorithms through path copying. (I have a separate blog post that discusses this in more detail.)

  Using these ideas, we can build a data store that is more than just a simple key-value lookup. While a standard B-Tree is already sufficient for basic key-value storage, B+ Trees make range queries much more efficient and provide a better foundation for concurrent implementations from the beginning.

  One important limitation of traditional B-Trees is that updates may propagate all the way to the root during insertion or deletion. As a result, the affected path generally needs to be locked, increasing write amplification and making concurrency more difficult.

* Coming to B-Trees and B+ Trees, both structures are very similar. The biggest advantage of B+ Trees is that they support efficient sequential traversal through the linked leaf nodes. Since modern storage devices work much better with sequential access patterns, this makes range scans extremely efficient while also improving cache locality.

  The biggest strength of B-Trees is that they can store many keys within a single page. Compared to binary search trees, where pointers consume a significant amount of memory, this results in a much denser tree with a smaller height and consequently fewer page accesses during lookups.

  Now let's talk about the B* Tree, which is essentially a denser version of the B+ Tree. At first glance, it sounds strictly better because denser nodes imply shorter trees. However, the downside is that insertions and deletions become much more expensive due to increased write amplification. Traditional B+ Trees intentionally leave some free space in internal nodes to accommodate future updates, whereas B* Trees maintain much higher occupancy, causing node splits and redistributions to occur more frequently. This is one of the main reasons they are not commonly used in modern database systems, although they are still an excellent exercise for understanding the limits of balanced tree design.

  MassTree and Bw-Tree are two other important variants that I plan to discuss in more detail later.

  Coming to Skip Lists, they are probabilistic data structures that can achieve performance comparable to balanced trees while requiring much simpler algorithms. Instead of maintaining rigid balancing invariants like AVL Trees or Red-Black Trees, Skip Lists rely on randomization to maintain their structure, making them significantly easier to implement. This simplicity is one of the reasons they are commonly used as memtable implementations in key-value stores such as Redis variants and DragonflyDB.Another interesting advantage to skiplists is their concurrency implmentation is also quite easy to work out because , unlike trees the amount of node which we need to lock for consistency is quite less and less contented for , mainly if see the predecessors required in skiplists are spread out , throughout data structure , but incase of trees we will always try to hold locks of parents until the root itself as write amplification can go al the way to root , so when multiple writes occur to the same section (not exactly same point ) their will be alots of contention for locks and retries . These are solved using Masstree implementation for trees which use a optimic locking framework along with tries to make sure concurrency is viable while also maintaing vache coherence of long strings .

* Some ideas I want to note down here. In Redis, we typically use an ART or a hash table as the primary in-memory data structure, and persistence is achieved by flushing updates to an Append-Only File (AOF).

  The main bottlenecks are:

  (a) I/O flushing can be expensive, although dedicated I/O threads help reduce this overhead.

  (b) Snapshotting through `fork()` can lead to significant memory consumption and, in extreme cases, Out-Of-Memory (OOM) failures because of copy-on-write behavior.

  (c) Scaling does not improve simply by purchasing better hardware. Eventually, we need to scale horizontally, and even then, newly added nodes are often not utilized to their full potential.

  If we model a distributed system using **N** nodes with performance following a random variable of **(mean, variance)**, then the overall expected mean becomes **N × mean**, while the expected variance scales approximately with **√N × variance**. In theory, replacing those nodes with a single machine that provides equivalent aggregate resources should provide similar performance. However, when additional compute is required later, scaling the distributed system requires proportionally more resources than simply upgrading a single machine. DragonflyDB discusses these trade-offs quite well.

  Redis has been optimized for many years, so some of these problems may no longer be as significant in practice.

  Coming back to this project, my goal is to build a shared-nothing architecture where each CPU core owns its own memtable. These independent memtables will eventually feed immutable tables into a single LSM-tree layer through sequential I/O, where compaction will take place.

  This will be the architecture used for the AVL Tree, Red-Black Tree, and B+ Tree implementations in Version 1.

  In Version 2, I plan to move towards an MVCC-based architecture that supports concurrent reads and writes while compaction is taking place. This should also provide a much better foundation for parallel execution.

## WHAT I WANT AT THE END OF THE DAY!!!

A simple key-value store (hopefully with most Redis features) that makes full use of my hardware, since I cannot afford to build my own cloud infrastructure right now (a far-fetched dream that will probably never happen...).

The long-term goal is to use this project as the foundation for a proper disk-based distributed database system, similar in spirit to Pebble, RocksDB, Cassandra, ScyllaDB, and many others.

## V1 SPECIFICATIONS:

In Version 1, we will be building a multi-core, shared-nothing architecture where each core owns its own memtable. These memtables will communicate with one another through SPSC ring buffers for multi-key lookups and range scans. This keeps the database architecture relatively simple, similar to Redis, while allowing us to explore the possibility of maximizing hardware utilization without relying heavily on mutexes.

The reason for adopting a control-plane design is that the alternative—a shared-memory architecture with a global view of the entire database—would require extensive global locking whenever individual cores attempt to publish their local commits to the shared state. This would significantly reduce concurrency, effectively turning the system into a glorified sequential update engine rather than a truly parallel one.

To make a shared-memory architecture worthwhile, we would need MVCC, which is planned for Version 2.

```text
                 [ Client Session ]
                          |
  ========================v========================  <- Core Thread 0 Boundary

  |  COORDINATION PLANE                           |
  |  - RESP Parser & Command Analyzer             |
  |  - Key Router (Hash Slot Mapping)             |
  ========================|========================
           /              |              \
   [Local Execution]      |        [To Core 1 SPSC Ring]

          |               |                  |
          |               v                  v
  ========v=======  ==============  =================  <- Core Thread 1 Boundary

  | STORAGE PLANE  |  | SPSC Ring  |  | STORAGE PLANE  |
  | - Local in-mem |  | (Cross-Core|  | - Local in-mem |
  |   List / LSM   |  |  Channel)  |  |   List / LSM   |
  ================    ==============  =================
```

**Write Path:**
Write → In-memory data structure (AVL Trees, Red-Black Trees, Skip Lists, or B+ Trees) → WAL → LSM-tree Engine

**Read Path:**
In-memory data structure → LSM-tree Engine (if required)

**Replication:**
WAL updates are streamed to replica instances to maintain consistency through a synchronization mechanism.

**Recovery:**
In the event of a failure, one of the replicas should be promoted to become the new primary.

**Network Protocol:**
RESP

## V2 SPECIFICATIONS:

In Version 2, we will embrace MVCC along with join-based tree algorithms. The architecture will also change significantly. The control plane introduced in Version 1 will be removed and replaced with a unified data representation layer that combines the local views of all cores into a consistent global representation before flushing data into the LSM-tree engine.

**Write Path:**
Write → In-memory data structure → Unified representation → WAL → LSM-tree Engine

**Read Path:**
In-memory data structure → LSM-tree Engine (if required)

**Replication:**
WAL updates are streamed to replica instances to maintain consistency through a synchronization mechanism.

**Recovery:**
One of the replicas should be capable of becoming the new primary after a failure.

**Network Protocol:**
RESP

## DESIGN OPTIONS:

We will maintain multiple memtables and compact them before flushing them into the LSM-tree. The number of memtables will generally correspond to the number of CPU cores available. The key-value space can then be partitioned across these memtables, allowing for natural load balancing while preserving the shared-nothing design.

The main long-term goal is to replace the traditional hash table used by Redis with a parallelizable, MVCC-based solution built on P-Trees.

For cache eviction, we will use the **2Q** replacement policy, as it provides a better balance than a traditional LRU policy.

The WAL will support both replication and recovery by allowing updates to be streamed to replicas while also serving as persistent on-disk storage in the event of failures.

The LSM-tree engine will initially use a tiered compaction strategy. In the future, I would like to experiment with adaptive compaction algorithms, although existing research suggests that they may not always provide significant practical improvements. A Bloom filter will also be used to reduce unnecessary disk lookups.

## Benchmarking:

Testing will be performed on JSON-based inputs that are converted into RESP commands. The initial benchmark dataset will consist of approximately 100 lines of session data stored in a simple `{key: value}` format.

