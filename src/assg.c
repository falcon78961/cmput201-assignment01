#include <stdio.h>
#include <string.h>

int main(int argc, char *argv[])
{
    FILE *list_fp, *sched_fp, *tap_fp, *out_fp;

    char fname[256];
    char buf[256];
    char route[30];
    char dirc;

    char tap_type[1000];
    int tap_stop[1000];
    int tap_mo[1000], tap_day[1000], tap_yr[1000];
    int tap_hr[1000], tap_min[1000];
    char ap[3];

    int n;
    int mo, d, yr;
    int hr, mn;
    int sid;
    char t;

    int i, j, k;
    int in_stop, out_stop;
    int dur, dur_h, dur_m;
    int matches, pick;
    int has_in, has_out;
    int num_stops;
    int cur;

    char sname[150];
    char in_name[150], out_name[150];

    char match_route[230][30];
    char match_dir[230];
    char match_in[230][150];
    char match_out[230][150];

    int top_stop, top_cnt, freq;
    int is_guess;

    if (argc != 3)
    {
        printf("Invalid number of arguments\n");
        return 1;
    }

    list_fp = fopen(argv[1], "r");
    if (list_fp == NULL)
    {
        printf("Could not open list file\n");
        return 1;
    }

    tap_fp = fopen(argv[2], "r");
    if (tap_fp == NULL) {
        printf("Could not open tap file\n");
        fclose(list_fp);
        return 1;
    }

    out_fp = fopen("output.txt", "w");
    if (out_fp == NULL) {
        printf("Could not create output.txt\n");
        fclose(list_fp);
        fclose(tap_fp);
        return 1;
    }

    if (fscanf(tap_fp, "%d", &n) != 1 || n < 0 || n > 1000) {
        printf("Invalid data format\n");
        return 0;
    }

    /* grab every tap first, sort out the trips after */
    for (i = 0; i < n; i++)
    {
        if (fscanf(tap_fp, "%d-%d-%d,%d:%d %2s,%c,",
                   &mo, &d, &yr, &hr, &mn, ap, &t) != 7)
        {
            printf("Invalid data format\n");
            return 0;
        }

        sid = 0;
        if (t != 'M' && fscanf(tap_fp, "%d", &sid) != 1)
        {
            printf("Invalid data format\n");
            return 0;
        }

        /* 12hr -> 24hr */
        if (ap[0] == 'P' && hr != 12) hr += 12;
        if (ap[0] == 'A' && hr == 12) hr = 0;

        tap_type[i] = t;
        tap_stop[i] = sid;
        tap_mo[i]   = mo;
        tap_day[i]  = d;
        tap_yr[i]   = yr;
        tap_hr[i]   = hr;
        tap_min[i]  = mn;
    }

    fclose(tap_fp);

    /* file is newest-first so go from the bottom up */
    for (i = n - 1; i > 0; i--)
    {
        if (tap_type[i] != 'E') continue;
        if (tap_type[i-1] != 'X' && tap_type[i-1] != 'M') continue;

        in_stop = tap_stop[i];
        out_stop = tap_stop[i-1];
        is_guess = 0;

        /* no exit tap, so guess from past trips at the same hour */
        if (tap_type[i-1] == 'M')
        {
            is_guess = 1;
            top_stop = 0;
            top_cnt = 0;

            for (j = 0; j < n - 1; j++) {
                if (tap_type[j] == 'X' && tap_type[j+1] == 'E' &&
                    tap_stop[j+1] == in_stop && tap_hr[j+1] == tap_hr[i])
                {
                    freq = 0;
                    for (k = 0; k < n - 1; k++) {
                        if (tap_type[k] == 'X' && tap_type[k+1] == 'E' &&
                            tap_stop[k+1] == in_stop &&
                            tap_hr[k+1] == tap_hr[i] &&
                            tap_stop[k] == tap_stop[j])
                            freq++;
                    }

                    // ties go to the smaller stop id
                    if (freq > top_cnt || (freq == top_cnt && tap_stop[j] < top_stop)) {
                        top_cnt = freq;
                        top_stop = tap_stop[j];
                    }
                }
            }

            if (top_cnt == 0) {
                printf("Could not guess missing exit\n");
                continue;
            }
            out_stop = top_stop;
        }

        /* which schedules have both stops? */
        rewind(list_fp);
        matches = 0;

        while (fscanf(list_fp, "%255s", fname) == 1)
        {
            sched_fp = fopen(fname, "r");
            if (sched_fp == NULL) continue;

            if (fgets(buf, sizeof(buf), sched_fp) == NULL) {
                fclose(sched_fp);
                continue;
            }

            if (sscanf(buf, "%29s %*[^,], %d Stops", route, &num_stops) != 2) {
                fclose(sched_fp);
                continue;
            }

            has_in = 0;
            has_out = 0;

            for (j = 0; j < num_stops; j++)
            {
                if (fgets(buf, sizeof(buf), sched_fp) == NULL) break;
                if (sscanf(buf, "%d,%149[^\n]", &cur, sname) != 2) continue;

                if (cur == in_stop) {
                    has_in = 1;
                    strcpy(in_name, sname);
                }
                if (cur == out_stop) {
                    has_out = 1;
                    strcpy(out_name, sname);
                }
            }
            fclose(sched_fp);

            if (has_in && has_out && matches < 230)
            {
                strcpy(match_route[matches], route);
                dirc = '?';

                /* pull direction and route out of the filename */
                for (j = 0; fname[j] != '\0'; j++)
                {
                    if (fname[j] == '_' &&
                        (fname[j+1] == 'E' || fname[j+1] == 'W' ||
                         fname[j+1] == 'N' || fname[j+1] == 'S'))
                        dirc = fname[j+1];

                    if (fname[j] == 'r' && fname[j+1] == 'o' &&
                        fname[j+2] == 'u' && fname[j+3] == 't' &&
                        fname[j+4] == 'e' && fname[j+5] == '_')
                        sscanf(fname + j + 6, "%29[^_]", match_route[matches]);
                }

                match_dir[matches] = dirc;
                strcpy(match_in[matches], in_name);
                strcpy(match_out[matches], out_name);
                matches++;
            }
        }

        if (matches == 0) {
            printf("Invalid stop ID\n");
            continue;
        }

        pick = 1;

        // more than one route fits, let the user choose
        if (matches > 1)
        {
            printf("Ambiguous trip detected:\n");
            printf("Entry: %d (%s)\n", in_stop, match_in[0]);
            printf("Exit: %d (%s)\n", out_stop, match_out[0]);
            printf("Possible routes:\n");

            for (j = 0; j < matches; j++)
                printf("%d. %s %c\n", j + 1, match_route[j], match_dir[j]);

            printf("Select route (1-%d): \n", matches);
            scanf("%d", &pick);

            if (pick < 1 || pick > matches) pick = 1;
        }
        pick--;

        /* minutes on the bus, wrap past midnight */
        dur = (tap_hr[i-1] * 60 + tap_min[i-1]) - (tap_hr[i] * 60 + tap_min[i]);
        if (dur < 0) dur += 1440;
        dur_h = dur / 60;
        dur_m = dur % 60;

        fprintf(out_fp, "%s %c,%s,%s,", match_route[pick], match_dir[pick],
                match_in[pick], match_out[pick]);
        fprintf(out_fp, "%04d-%02d-%02dT%02d:%02d:00,",
                tap_yr[i], tap_mo[i], tap_day[i], tap_hr[i], tap_min[i]);
        fprintf(out_fp, "%04d-%02d-%02dT%02d:%02d:00,",
                tap_yr[i-1], tap_mo[i-1], tap_day[i-1], tap_hr[i-1], tap_min[i-1]);
        fprintf(out_fp, "%d:%02d:00", dur_h, dur_m);

        if (is_guess) fprintf(out_fp, ",Guessed");
        fprintf(out_fp, "\n");
    }

    fclose(list_fp);
    fclose(out_fp);
    return 0;
}
