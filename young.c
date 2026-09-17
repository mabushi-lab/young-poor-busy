/*
 * young.c -- ICPC Asia 2001 (Hakodate) Problem F, "Young, Poor and Busy",
 * with Prof. Utsuro's checkpoint (5) and (6) extensions.
 *
 * Two people leave their home cities, meet somewhere for at least `meet`
 * minutes, then each head home, all inside one [depart, return] window. We want
 * the smallest total train fare, or 0 if it can't be done.
 *
 * The idea: fares only ever change when some train arrives, so time is
 * discretised at arrival events. From there we build four min-fare tables (each
 * person's trip out to a station, and their trip back home from it) and, for
 * every possible meeting city, take the cheapest arrive/depart pair that still
 * keeps the two together long enough. The return tables come from running the
 * outbound DP on a time-reversed timetable. The full derivation is in the
 * accompanying notes.
 *
 * Build:  cc -O2 -Wall -o young young.c
 * Run:    ./young < sample.txt
 *         ./young Tokyo Hakodate 08:00 23:00 30 < sample.txt   (checkpoint 5)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_CITY 100      /* problem guarantees <= 100 cities              */
#define MAX_CONN 2001     /* <= 2000 trains, +1 column for the event -1    */
#define NAME_LEN 20       /* city names are <= 16 alphabetic chars         */
#define LINE_LEN 256
#define INF 99999999      /* larger than any reachable total fare          */
#define BIAS (24 * 60)    /* time-reversal offset; > any HH:MM in minutes  */

/* ---- one train connection ---- */
typedef struct {
    int from, to;   /* city ids                       */
    int dep, arr;   /* minutes from 00:00             */
    int fare;
} Train;

/* ---- per-dataset state ---- */
static char city_name[MAX_CITY][NAME_LEN];
static int  n_city;

static Train trains[MAX_CONN];    /* forward timetable, sorted by arrival   */
static Train rtrains[MAX_CONN];   /* time-reversed timetable, same sort     */
static int   n_conn;              /* trains kept after windowing            */

static int fr1[MAX_CITY][MAX_CONN];   /* person 1 outbound  */
static int fr2[MAX_CITY][MAX_CONN];   /* person 2 outbound  */
static int to1[MAX_CITY][MAX_CONN];   /* person 1 return    */
static int to2[MAX_CITY][MAX_CONN];   /* person 2 return    */

/* ---- run-time parameters (checkpoint 5: overridable on the command line) ---- */
static int g_depart = 8 * 60;    /* earliest one may leave home            */
static int g_return = 18 * 60;   /* latest one must be home again          */
static int g_meet   = 30;        /* required minutes together              */

/* Map a city name to a small integer id, registering it on first sight. */
static int city_id(const char *name)
{
    int i;
    for (i = 0; i < n_city; i++)
        if (strcmp(name, city_name[i]) == 0)
            return i;
    strcpy(city_name[n_city], name);
    return n_city++;
}

/* Order trains by arrival time (used for both timetables). */
static int cmp_arr(const void *a, const void *b)
{
    return ((const Train *)a)->arr - ((const Train *)b)->arr;
}

/*
 * Latest train, scanning events p, p-1, ..., 0, that arrives at station `st`
 * no later than `deadline`; returns its event index, or -1 if none.
 * (A traveller may board a train that departs the same minute they arrived.)
 */
static int change(const Train *tv, int p, int st, int deadline)
{
    while (p >= 0) {
        if (tv[p].to == st && tv[p].arr <= deadline)
            return p;
        p--;
    }
    return -1;
}

/*
 * Fill one DP table for timetable `tv` with origin city `org`.
 * Column 0 is the pre-departure state (event -1); column k+1 is the state
 * right after event k.  v[st][k+1] is the min fare to be at st by event k.
 */
static void make_table(int v[][MAX_CONN], int org, const Train *tv)
{
    int k, i, a, via;

    for (i = 0; i < n_city; i++)
        v[i][0] = INF;
    v[org][0] = 0;                       /* start at the origin, no fare paid */

    for (k = 0; k < n_conn; k++) {
        for (i = 0; i < n_city; i++)
            v[i][k + 1] = v[i][k];       /* nothing changes unless we ride train k */

        /* to ride train k we must already be at its departure city, having
         * arrived there no later than train k leaves */
        a   = change(tv, k - 1, tv[k].from, tv[k].dep);
        via = tv[k].fare + v[tv[k].from][a + 1];   /* a == -1 -> column 0 */
        if (via < v[tv[k].to][k + 1])
            v[tv[k].to][k + 1] = via;
    }
}

/* Build the time-reversed timetable and sort both by arrival time. */
static void prepare_data(void)
{
    int i;
    for (i = 0; i < n_conn; i++) {
        rtrains[i].from = trains[i].to;          /* swap endpoints ...        */
        rtrains[i].to   = trains[i].from;
        rtrains[i].dep  = BIAS - trains[i].arr;   /* ... and mirror the clock  */
        rtrains[i].arr  = BIAS - trains[i].dep;
        rtrains[i].fare = trains[i].fare;
    }
    qsort(trains,  n_conn, sizeof(Train), cmp_arr);
    qsort(rtrains, n_conn, sizeof(Train), cmp_arr);
}

/*
 * Cheapest meeting held in `city`.  Returns the min total fare (INF if none)
 * and records the event pair that attains it: *best_a is the arrival event that
 * begins the meeting, *best_d the departure event that ends it.  We slide a
 * forward and d only toward later departures, valid because a larger arrival
 * needs an equal-or-later minimum departure.  The window shown to the user is
 * reconstructed afterwards by meeting_window (see the note there).
 */
static int calc_city(int city, int *best_a, int *best_d)
{
    int a = 0, d = n_conn - 1, best = INF;

    if (n_conn == 0)
        return INF;

    for (;;) {
        int dep_time = BIAS - rtrains[d].arr;    /* real departure time  */
        int stay     = dep_time - trains[a].arr;
        int c;

        if (stay < g_meet) {                     /* window too short ...  */
            if (--d < 0)                         /* ... allow a later exit */
                break;
            continue;
        }
        c = fr1[city][a + 1] + fr2[city][a + 1]
          + to1[city][d + 1] + to2[city][d + 1];
        if (c < best) {
            best    = c;
            *best_a = a;
            *best_d = d;
        }
        if (++a >= n_conn)                       /* try a later meeting start */
            break;
    }
    return best;
}

/*
 * True meeting window for the winning city and event pair.
 *
 * calc_city's sweep stops at the earliest departure that makes the *summed*
 * fare feasible, and that instant can sit on a flat stretch of to1/to2 (a fare
 * that will not improve until some much later train), so it understates how
 * long the two are really together.  Instead we read each person's optimal
 * inbound and return fares and recover the actual times behind them: the
 * earliest arrival at the city that already reaches the inbound fare, and the
 * latest departure that still reaches the return fare.  The pair overlaps from
 *   start = the later of the two arrivals   to   end = the earlier of the two
 * departures.  A person whose home *is* the meeting city is there the whole day,
 * from g_depart to g_return.
 */
static void meeting_window(int m, int a, int d, int *start, int *end)
{
    int v1 = fr1[m][a + 1], v2 = fr2[m][a + 1];   /* each person's inbound fare */
    int w1 = to1[m][d + 1], w2 = to2[m][d + 1];   /* each person's return fare  */
    int arr1, arr2, dep1, dep2, k;

    arr1 = g_depart;                              /* home1 == m: present from the start */
    if (fr1[m][0] != v1)
        for (k = 0; k < n_conn; k++)
            if (fr1[m][k + 1] == v1) { arr1 = trains[k].arr; break; }
    arr2 = g_depart;
    if (fr2[m][0] != v2)
        for (k = 0; k < n_conn; k++)
            if (fr2[m][k + 1] == v2) { arr2 = trains[k].arr; break; }
    *start = (arr1 > arr2) ? arr1 : arr2;

    dep1 = g_return;                              /* home1 == m: present until the end */
    if (to1[m][0] != w1)
        for (k = 0; k < n_conn; k++)
            if (to1[m][k + 1] == w1) { dep1 = BIAS - rtrains[k].arr; break; }
    dep2 = g_return;
    if (to2[m][0] != w2)
        for (k = 0; k < n_conn; k++)
            if (to2[m][k + 1] == w2) { dep2 = BIAS - rtrains[k].arr; break; }
    *end = (dep1 < dep2) ? dep1 : dep2;
}

/* Solve one dataset and print the answer. */
static void solve(int home1, int home2)
{
    int c, cost, best = INF, best_city = -1, best_a = 0, best_d = 0, a, d;

    prepare_data();
    make_table(fr1, home1, trains);
    make_table(fr2, home2, trains);
    make_table(to1, home1, rtrains);
    make_table(to2, home2, rtrains);

    for (c = 0; c < n_city; c++) {
        cost = calc_city(c, &a, &d);
        if (cost < best) {
            best = cost; best_city = c; best_a = a; best_d = d;
        }
    }

    if (best >= INF) {                           /* no feasible meeting */
        printf("0\n");
    } else {                                     /* checkpoint 6 output */
        int start, end;
        meeting_window(best_city, best_a, best_d, &start, &end);
        printf("%d %s: %02d:%02d - %02d:%02d\n", best, city_name[best_city],
               start / 60, start % 60, end / 60, end % 60);
    }
}

int main(int argc, char **argv)
{
    char home1[NAME_LEN] = "Hakodate";
    char home2[NAME_LEN] = "Tokyo";
    char line[LINE_LEN];

    /* checkpoint 5: 5 optional args override the fixed defaults */
    if (argc == 6) {
        int h, m;
        strncpy(home1, argv[1], NAME_LEN - 1); home1[NAME_LEN - 1] = '\0';
        strncpy(home2, argv[2], NAME_LEN - 1); home2[NAME_LEN - 1] = '\0';
        if (sscanf(argv[3], "%d:%d", &h, &m) == 2) g_depart = h * 60 + m;
        if (sscanf(argv[4], "%d:%d", &h, &m) == 2) g_return = h * 60 + m;
        g_meet = atoi(argv[5]);
    } else if (argc != 1) {
        fprintf(stderr,
            "usage: %s [home1 home2 depart(HH:MM) return(HH:MM) meet(min)]\n"
            "       (no arguments == Hakodate Tokyo 08:00 18:00 30)\n", argv[0]);
        return 1;
    }

    while (fgets(line, sizeof line, stdin)) {
        int n, i, id1, id2;

        if (sscanf(line, "%d", &n) != 1)         /* skip stray blank lines */
            continue;
        if (n == 0)                              /* a lone 0 ends the input */
            break;

        n_city = 0;
        n_conn = 0;
        id1 = city_id(home1);                    /* home cities always exist */
        id2 = city_id(home2);

        for (i = 0; i < n; i++) {
            char a[NAME_LEN], b[NAME_LEN];
            int dh, dm, ah, am, fare, dep, arr;

            if (!fgets(line, sizeof line, stdin))
                break;
            if (sscanf(line, "%s %d:%d %s %d:%d %d",
                       a, &dh, &dm, b, &ah, &am, &fare) != 7)
                continue;

            dep = dh * 60 + dm;
            arr = ah * 60 + am;
            if (dep < g_depart || arr > g_return)   /* outside the day window */
                continue;

            trains[n_conn].from = city_id(a);
            trains[n_conn].to   = city_id(b);
            trains[n_conn].dep  = dep;
            trains[n_conn].arr  = arr;
            trains[n_conn].fare = fare;
            n_conn++;
        }

        solve(id1, id2);
    }
    return 0;
}
