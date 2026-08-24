# Extension threading contract

`PlayerbotExtensionRegistry` invokes every extension hook synchronously. The registry does not isolate,
serialize, or schedule extension work. Time spent in a hook is time taken from the loop that called it.

This contract applies to every extension, including Economy, Social, telemetry, recovery, and future
modules.

## Hook ownership

1. `OnWorldUpdate` executes from the Playerbots world update and is world thread only.

2. `OnBotUpdate` executes immediately after the bot AI update. It inherits the caller and may run on a
   map worker. It must not assume world thread ownership.

3. `HandleBotEvent` executes inline at the event producer. It inherits the caller and may run on a bot or
   map worker. It must not assume world thread ownership.

Other hooks are also synchronous unless their declaration documents a stronger guarantee. An extension
must treat an undocumented caller as an owner thread that cannot be blocked.

## Hook budget

Every hook must be bounded and nonblocking. A hook may validate live state, copy a small immutable value
snapshot, and attempt one bounded queue admission. A hook must not perform network, file, or database
input and output. It must not wait on a future, mutex, condition variable, or worker. It must not run an
unbounded scan, build a large serialized snapshot, or retry until work is accepted.

Moving expensive work to another thread is not sufficient by itself. The handoff must preserve game
object ownership and place explicit limits on memory, latency, and commit work.

## Safe worker handoff

Cross thread work uses three stages.

1. Owner thread capture. Validate live objects while their owner thread is current. Copy only identifiers,
   scalar facts, bounded strings, and immutable value objects needed by the computation.

2. Bounded worker compute. Submit the value snapshot to a queue with a fixed capacity and an explicit
   admission policy. Worker code computes from values only. It must not dereference `Player*`,
   `PlayerbotAI*`, `Unit*`, `Map*`, or any other raw game pointer.

3. Owner thread commit. Drain a bounded number of results on the owning world or map thread. Resolve
   identifiers again, validate current state, then apply the result. A worker must never commit directly
   to live game state.

Payloads must not contain raw game pointers, references, iterators, or views into owner managed storage.
Copying a pointer into a queue does not transfer ownership.

## Capacity and backpressure

Every ingress queue and completion queue needs a fixed capacity. Admission must have a documented policy
for a full queue, such as rejection, safe coalescing, or dropping the oldest replaceable observation.
Admission must remain bounded even under sustained overload.

Each owner thread drain needs both an item budget and a time budget. Exhausting either budget defers the
remaining work to a later tick. Deferred work must not trigger a busy retry loop, and one bot or scope
must not monopolize the drain.

## Generation and deadline fencing

Each submitted task must carry the stable owner identity, a generation or revision, and a deadline when
the result can become obsolete. The owner thread commit discards a result when the owner disappeared,
the generation changed, the deadline expired, or the current state no longer satisfies the captured
preconditions.

Bot removal, logout, map transfer, module disablement, configuration generation changes, and shutdown
must invalidate affected pending work. Cancellation is advisory. Commit validation is the authority.

## Shutdown

Shutdown proceeds in a defined order.

1. Stop accepting new work.

2. Signal worker cancellation and wake blocked workers.

3. Join workers within a bounded shutdown budget.

4. Discard or owner thread commit only results that remain valid under the shutdown policy.

5. Unregister hooks and destroy shared state only after callbacks and workers can no longer reach it.

Detached workers are not permitted. A module must not wait forever for network or provider work during
shutdown.

## Required metrics

Threaded extensions expose enough low cost telemetry to distinguish healthy load from overload. At
minimum, record current and peak queue depth, configured capacity, accepted work, full queue rejection,
coalesced or dropped work, stale or expired results, oldest pending age, completed work, failed work,
drain budget exhaustion, and bounded execution duration.

Metrics collection must itself remain bounded. Do not serialize every queued payload or rebuild a full
snapshot on every tick solely to report queue state.

## Conformance checklist

An extension is conforming only when all five statements are true.

1. Every synchronous hook has a measured bounded path and performs no blocking input and output.

2. Every cross thread payload is pointer free, immutable after admission, and fenced by identity,
   generation, and deadline.

3. Every queue has a fixed capacity, explicit backpressure, and a bounded owner thread drain.

4. Shutdown stops admission, joins workers within a bound, and prevents late commits.

5. Queue pressure, stale work, failures, and drain duration are observable without expensive per tick
   serialization.
