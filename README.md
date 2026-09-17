# young-poor-busy

Solutions to ICPC Asia Regional 2001 (Hakodate) Problem F, "Young, Poor and Busy",
in C and Python, with two extensions.

## The problem

Two people live in different cities on a rail network. Each leaves home no earlier
than a given departure time, travels to some city, meets the other there for at
least a required number of minutes, and is back home by a given return time.
Find the smallest total fare for the pair, or `0` if no such meeting is possible.

## Approach

Fares only change when a train arrives, so time is discretised at arrival events
rather than at minutes.

For each dataset the program builds four minimum-fare tables by dynamic
programming over those events:

- each person's cheapest way to **reach** a given city by a given event
- each person's cheapest way to **get home** from a given city, leaving at a given event

The return tables come from running the same outbound DP over a time-reversed
timetable, with the endpoints swapped and the clock mirrored, so one routine
covers both directions.

Every city is then a candidate meeting point. For each one, a two-pointer sweep
over (arrival event, departure event) finds the cheapest pair that still leaves
the two together long enough. The best over all cities is the answer.

## Extensions

**Configurable parameters.** The home cities, departure time, return time and
required meeting length can be passed on the command line rather than being fixed:

```
./young Tokyo Hakodate 08:00 23:00 30 < sample.txt
```

With no arguments the original defaults apply: `Hakodate Tokyo 08:00 18:00 30`.

**Meeting window in the output.** Instead of printing the fare alone, the program
reports the meeting city and the interval during which both people are actually
there:

```
11000 Morioka: 13:35 - 14:05
```

Recovering that interval is not just a matter of reading off the event pair the
cost sweep settles on. That instant can sit on a flat stretch of the return-fare
table — a fare that will not improve until some much later train — which
understates the real overlap. The program instead takes each person's optimal
inbound and return fares, recovers the earliest arrival and latest departure that
attain them, and intersects the two presences.

## Build and run

```
cc -O2 -Wall -o young young.c
./young < sample.txt

python3 young.py < sample.txt
```

Expected output for the bundled sample:

```
11000 Morioka: 13:35 - 14:05
0
11090 Morioka: 11:04 - 14:49
```

## Files

| File | |
|---|---|
| `young.c` | C implementation |
| `young.py` | Python implementation |
| `sample.txt` | Sample datasets |
| `output.txt` | Expected output, default and custom parameters |

## Verification

Verified against the Aizu Online Judge:
[Problem 1229 — Young, Poor and Busy](https://judge.u-aizu.ac.jp/onlinejudge/description.jsp?id=1229)
