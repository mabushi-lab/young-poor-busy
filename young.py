#!/usr/bin/env python3
"""
young.py -- ICPC Asia 2001 (Hakodate) Problem F, "Young, Poor and Busy",
with Prof. Utsuro's checkpoint (5) and (6) extensions.

A direct port of young.c: same algorithm, same names, same table layout, so the
two read side by side. See young.c and the accompanying notes for the full
reasoning. In brief: time is discretised at train-arrival events; four min-fare
tables cover each person's trip out to a station and their trip back home (the
return tables reuse the outbound DP on a time-reversed timetable); and for each
candidate meeting city a two-pointer sweep takes the cheapest arrive/depart pair
that keeps both people together long enough. The answer is the minimum over all
meeting cities, or 0 if no meeting is possible.

Run:  python3 young.py < sample.txt
      python3 young.py Tokyo Hakodate 08:00 23:00 30 < sample.txt   (checkpoint 5)
"""

import sys

INF = 99_999_999      # larger than any reachable total fare
BIAS = 24 * 60        # time-reversal offset; > any HH:MM in minutes


class Train:
    """One connection: endpoints (city ids), depart/arrive (minutes), fare."""
    __slots__ = ("frm", "to", "dep", "arr", "fare")

    def __init__(self, frm, to, dep, arr, fare):
        self.frm, self.to, self.dep, self.arr, self.fare = frm, to, dep, arr, fare


def parse_hhmm(text):
    """'08:15' -> minutes from 00:00."""
    h, m = text.split(":")
    return int(h) * 60 + int(m)


def make_table(v, org, tv, n_city):
    """
    Fill one DP table for timetable `tv` with origin city `org`.
    Column 0 is the pre-departure state (event -1); column k+1 is the state
    right after event k.  v[st][k+1] = min fare to be at st by event k.
    """
    for row in v:
        row[0] = INF
    v[org][0] = 0                          # start at the origin, no fare paid

    for k, tr in enumerate(tv):
        # nothing changes unless we ride train k
        for i in range(n_city):
            v[i][k + 1] = v[i][k]

        # to ride train k we must already be at its departure city, having
        # arrived there no later than train k leaves
        a = change(tv, k - 1, tr.frm, tr.dep)
        via = tr.fare + v[tr.frm][a + 1]   # a == -1 -> column 0
        if via < v[tr.to][k + 1]:
            v[tr.to][k + 1] = via


def change(tv, p, st, deadline):
    """
    Latest train, scanning events p, p-1, ..., 0, that arrives at station `st`
    no later than `deadline`; its event index, or -1 if none.  (A traveller may
    board a train that departs the same minute they arrived.)
    """
    while p >= 0:
        if tv[p].to == st and tv[p].arr <= deadline:
            return p
        p -= 1
    return -1


def calc_city(city, trains, rtrains, fr1, fr2, to1, to2, meet):
    """
    Cheapest meeting held in `city`.  Returns (min_total_fare, best_a, best_d):
    the event pair attaining the minimum -- best_a the arrival event that starts
    the meeting, best_d the departure event that ends it.  (INF and None indices
    if no feasible meeting exists here.)  We slide a forward and d only toward
    later departures, valid because a larger arrival needs an equal-or-later
    minimum departure.  The window shown to the user is reconstructed afterwards
    by meeting_window.
    """
    n = len(trains)
    if n == 0:
        return INF, None, None

    a, d, best = 0, n - 1, INF
    best_a = best_d = None
    while True:
        dep_time = BIAS - rtrains[d].arr       # real departure time
        stay = dep_time - trains[a].arr
        if stay < meet:                        # window too short ...
            d -= 1
            if d < 0:                          # ... allow a later exit
                break
            continue
        c = (fr1[city][a + 1] + fr2[city][a + 1]
             + to1[city][d + 1] + to2[city][d + 1])
        if c < best:
            best, best_a, best_d = c, a, d
        a += 1                                 # try a later meeting start
        if a >= n:
            break
    return best, best_a, best_d


def meeting_window(m, a, d, trains, rtrains, fr1, fr2, to1, to2, depart, ret):
    """
    True meeting window for the winning city and event pair.

    calc_city's sweep stops at the earliest departure that makes the *summed*
    fare feasible, and that instant can sit on a flat stretch of to1/to2, so it
    understates how long the two are really together.  Instead we read each
    person's optimal inbound and return fares and recover the times behind them:
    the earliest arrival at the city that already reaches the inbound fare, and
    the latest departure that still reaches the return fare.  They overlap from
    the later of the two arrivals to the earlier of the two departures.  A person
    whose home is the meeting city is present all day, from `depart` to `ret`.
    """
    n = len(trains)

    def earliest_arrival(fr):
        v = fr[m][a + 1]
        if fr[m][0] == v:                      # home == m: present from the start
            return depart
        for k in range(n):
            if fr[m][k + 1] == v:
                return trains[k].arr
        return depart

    def latest_departure(to):
        w = to[m][d + 1]
        if to[m][0] == w:                      # home == m: present until the end
            return ret
        for k in range(n):
            if to[m][k + 1] == w:
                return BIAS - rtrains[k].arr
        return ret

    start = max(earliest_arrival(fr1), earliest_arrival(fr2))
    end = min(latest_departure(to1), latest_departure(to2))
    return start, end


def solve(trains, n_city, home1, home2, city_name, depart, ret, meet, out):
    """Solve one dataset and write its answer to `out`."""
    n = len(trains)

    # time-reversed timetable for the return trips
    rtrains = [Train(t.to, t.frm, BIAS - t.arr, BIAS - t.dep, t.fare) for t in trains]
    trains.sort(key=lambda t: t.arr)
    rtrains.sort(key=lambda t: t.arr)

    cols = n + 1
    fr1 = [[INF] * cols for _ in range(n_city)]
    fr2 = [[INF] * cols for _ in range(n_city)]
    to1 = [[INF] * cols for _ in range(n_city)]
    to2 = [[INF] * cols for _ in range(n_city)]

    make_table(fr1, home1, trains, n_city)
    make_table(fr2, home2, trains, n_city)
    make_table(to1, home1, rtrains, n_city)
    make_table(to2, home2, rtrains, n_city)

    best, best_city, best_a, best_d = INF, -1, None, None
    for c in range(n_city):
        cost, a, d = calc_city(c, trains, rtrains, fr1, fr2, to1, to2, meet)
        if cost < best:
            best, best_city, best_a, best_d = cost, c, a, d

    if best >= INF:                            # no feasible meeting
        out.append("0")
    else:                                      # checkpoint 6 output
        start, end = meeting_window(best_city, best_a, best_d, trains, rtrains,
                                    fr1, fr2, to1, to2, depart, ret)
        out.append("%d %s: %02d:%02d - %02d:%02d"
                   % (best, city_name[best_city], start // 60, start % 60,
                      end // 60, end % 60))


def main(argv):
    home1, home2 = "Hakodate", "Tokyo"
    depart, ret, meet = 8 * 60, 18 * 60, 30

    # checkpoint 5: 5 optional args override the fixed defaults
    if len(argv) == 6:
        home1, home2 = argv[1], argv[2]
        depart, ret = parse_hhmm(argv[3]), parse_hhmm(argv[4])
        meet = int(argv[5])
    elif len(argv) != 1:
        sys.stderr.write(
            "usage: %s [home1 home2 depart(HH:MM) return(HH:MM) meet(min)]\n"
            "       (no arguments == Hakodate Tokyo 08:00 18:00 30)\n" % argv[0])
        return 1

    data = sys.stdin.read().split("\n")
    pos = 0
    out = []

    while pos < len(data):
        line = data[pos].strip()
        pos += 1
        if not line:                           # skip stray blank lines
            continue
        n = int(line.split()[0])
        if n == 0:                             # a lone 0 ends the input
            break

        # a fresh city table per dataset; home cities are registered first so
        # they always exist and take ids 0 and 1
        city_name = []
        city_id = {}

        def id_of(name):
            if name not in city_id:
                city_id[name] = len(city_name)
                city_name.append(name)
            return city_id[name]

        h1, h2 = id_of(home1), id_of(home2)

        trains = []
        for _ in range(n):
            if pos >= len(data):
                break
            parts = data[pos].split()
            pos += 1
            if len(parts) != 5:
                continue
            frm, dep_s, to, arr_s, fare_s = parts
            dep, arr = parse_hhmm(dep_s), parse_hhmm(arr_s)
            if dep < depart or arr > ret:      # outside the day window
                continue
            trains.append(Train(id_of(frm), id_of(to), dep, arr, int(fare_s)))

        solve(trains, len(city_name), h1, h2, city_name, depart, ret, meet, out)

    sys.stdout.write("\n".join(out) + ("\n" if out else ""))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
