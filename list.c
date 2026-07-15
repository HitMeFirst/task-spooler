/*
    Task Spooler - a task queue system for the unix user
    Copyright (C) 2007-2009  Lluís Batlle i Rossell

    Please find the license in the provided COPYING file.
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include "main.h"

/* From jobs.c */
extern int busy_slots;
extern int max_slots;

static char *shorten(char *line, int len) {
    char *newline = (char *) malloc((len + 1) * sizeof(char));
    if (strlen(line) <= len)
        strcpy(newline, line);
    else {
        snprintf(newline, len - 4, "%s", line);
        strcat(newline, "...");
    }
    return newline;
}

char *joblistdump_headers() {
    char *line;

    line = malloc(600);
    snprintf(line, 600, "#!/bin/sh\n# - task spooler (ts) job dump\n"
                        "# This file has been created because a SIGTERM killed\n"
                        "# your queue server.\n"
                        "# The finished commands are listed first.\n"
                        "# The commands running or to be run are stored as you would\n"
                        "# probably run them. Take care - some quotes may have got"
                        " broken\n\n");

    return line;
}

char *joblist_headers_with_output_width(int output_width) {
    char *line;
    int maxlen;

    if (output_width < 20)
        output_width = 20;

    maxlen = 100 + output_width;
    line = malloc(maxlen);
#ifndef CPU
    snprintf(line, maxlen, "%-4s %-10s %-*s %-8s %-6s %-5s %s [run=%i/%i]\n",
             "ID",
             "State",
             output_width,
             "Output",
             "E-Level",
             "Time",
             "GPUs",
             "Command",
             busy_slots,
             max_slots);
#else
    snprintf(line, maxlen, "%-4s %-10s %-*s %-8s %-6s %s [run=%i/%i]\n",
             "ID",
             "State",
             output_width,
             "Output",
             "E-Level",
             "Time",
             "Command",
             busy_slots,
             max_slots);
#endif
    return line;
}

char *joblist_headers() {
    return joblist_headers_with_output_width(20);
}

char *jobgpulist_header() {
    return "ID   GPU-IDs\n";
}

static int max(int a, int b) {
    return a > b ? a : b;
}

static const char *ofilename_shown(const struct Job *p, int shorten_output) {
    const char *output_filename;

    if (p->state == SKIPPED) {
        output_filename = "(no output)";
    } else if (p->store_output) {
        if (p->state == QUEUED || p->state == ALLOCATING) {
            output_filename = "(file)";
        } else {
            if (p->output_filename == 0)
                /* This may happen due to concurrency
                 * problems */
                output_filename = "(...)";
            else
                output_filename = shorten_output ? shorten(p->output_filename, 20)
                                                 : p->output_filename;
        }
    } else
        output_filename = "stdout";

    return output_filename;
}
int joblist_output_width(const struct Job *p, int shorten_output) {
    int width = strlen(ofilename_shown(p, shorten_output));
    return width < 20 ? 20 : width;
}


static char *print_noresult(const struct Job *p, int shorten_output, int output_width) {
    const char *jobstate;
    const char *output_filename;
    int maxlen;
    char *line;
    /* 20 chars should suffice for a string like "[int,int,..]&& " */
    char dependstr[20] = "";
    int cmd_len;
    char timestr[20] = "";

    jobstate = jstate2string(p->state);
    output_filename = ofilename_shown(p, shorten_output);
    if (output_width < 20)
        output_width = 20;
    if (p->state == RUNNING && p->info.start_time.tv_sec != 0) {
        float running_ms = pinfo_time_until_now(&p->info);
        char *unit = time_rep(&running_ms);
        snprintf(timestr, sizeof(timestr), "%5.2f%s", running_ms, unit);
    }

    maxlen = 4 + 1 + 10 + 1 + output_width + 1 + 8 + 1
             + 25 + 1 + 5 + 1 + strlen(p->command) + 20; /* 20 is the margin for errors */

    if (p->label)
        maxlen += 3 + strlen(p->label);
    if (p->depend_on_size) {
        maxlen += sizeof(dependstr);
        int pos = 0;
        if (p->depend_on[0] == -1)
            pos += snprintf(&dependstr[pos], sizeof(dependstr), "[ ");
        else
            pos += snprintf(&dependstr[pos], sizeof(dependstr), "[%i", p->depend_on[0]);

        for (int i = 1; i < p->depend_on_size; i++) {
            if (p->depend_on[i] == -1)
                pos += snprintf(&dependstr[pos], sizeof(dependstr), ", ");
            else
                pos += snprintf(&dependstr[pos], sizeof(dependstr), ",%i", p->depend_on[i]);
        }
        pos += snprintf(&dependstr[pos], sizeof(dependstr), "]&& ");
    }

    line = (char *) malloc(maxlen);
    if (line == NULL)
        error("Malloc for %i failed.\n", maxlen);

    cmd_len = max((strlen(p->command) + (term_width - maxlen)), 20);
    if (p->label) {
        char *label = shorten(p->label, 20);
        char *cmd = shorten(p->command, cmd_len);
#ifndef CPU
        snprintf(line, maxlen, "%-4i %-10s %-*s %-8s %6s %-5d %s[%s]%s\n",
                 p->jobid,
                 jobstate,
                 output_width,
                 output_filename,
                 "",
                 timestr,
                 p->num_gpus,
                 dependstr,
                 label,
                 cmd);
#else
        snprintf(line, maxlen, "%-4i %-10s %-*s %-8s %6s %s[%s]%s\n",
                 p->jobid,
                 jobstate,
                 output_width,
                 output_filename,
                 "",
                 timestr,
                 dependstr,
                 label,
                 cmd);
#endif
        free(label);
        free(cmd);
    }
    else {
        char *cmd = shorten(p->command, cmd_len);
#ifndef CPU
        snprintf(line, maxlen, "%-4i %-10s %-*s %-8s %6s %-5d %s%s\n",
                 p->jobid,
                 jobstate,
                 output_width,
                 output_filename,
                 "",
                 timestr,
                 p->num_gpus,
                 dependstr,
                 cmd);
#else
        snprintf(line, maxlen, "%-4i %-10s %-*s %-8s %6s %s%s\n",
                 p->jobid,
                 jobstate,
                 output_width,
                 output_filename,
                 "",
                 timestr,
                 dependstr,
                 cmd);
#endif
        free(cmd);
    }

    return line;
}

static char *print_result(const struct Job *p, int shorten_output, int output_width) {
    const char *jobstate;
    int maxlen;
    char *line;
    const char *output_filename;
    /* 20 chars should suffice for a string like "[int,int,..]&& " */
    char dependstr[20] = "";
    float real_ms = p->result.real_ms;
    char *unit = time_rep(&real_ms);
    int cmd_len;

    jobstate = jstate2string(p->state);
    output_filename = ofilename_shown(p, shorten_output);
    if (output_width < 20)
        output_width = 20;

    maxlen = 4 + 1 + 10 + 1 + output_width + 1 + 8 + 1
             + 25 + 1 + 5 + 1 + strlen(p->command) + 20; /* 20 is the margin for errors */

    if (p->label)
        maxlen += 3 + strlen(p->label);
    if (p->depend_on_size) {
        maxlen += sizeof(dependstr);
        int pos = 0;
        if (p->depend_on[0] == -1)
            pos += snprintf(&dependstr[pos], sizeof(dependstr), "[ ");
        else
            pos += snprintf(&dependstr[pos], sizeof(dependstr), "[%i", p->depend_on[0]);

        for (int i = 1; i < p->depend_on_size; i++) {
            if (p->depend_on[i] == -1)
                pos += snprintf(&dependstr[pos], sizeof(dependstr), ", ");
            else
                pos += snprintf(&dependstr[pos], sizeof(dependstr), ",%i", p->depend_on[i]);
        }
        pos += snprintf(&dependstr[pos], sizeof(dependstr), "]&& ");
    }

    line = (char *) malloc(maxlen);
    if (line == NULL)
        error("Malloc for %i failed.\n", maxlen);

    cmd_len = max((strlen(p->command) + (term_width - maxlen)), 20);
    if (p->label) {
        char *label = shorten(p->label, 20);
        char *cmd = shorten(p->command, cmd_len);
#ifndef CPU
        snprintf(line, maxlen, "%-4i %-10s %-*s %-8i %5.2f%s %-5d %s[%s]%s\n",
                 p->jobid,
                 jobstate,
                 output_width,
                 output_filename,
                 p->result.errorlevel,
                 real_ms,
                 unit,
                 p->num_gpus,
                 dependstr,
                 label,
                 cmd);
#else
        snprintf(line, maxlen, "%-4i %-10s %-*s %-8i %5.2f%s %s[%s]%s\n",
                 p->jobid,
                 jobstate,
                 output_width,
                 output_filename,
                 p->result.errorlevel,
                 real_ms,
                 unit,
                 dependstr,
                 label,
                 cmd);
#endif
        free(label);
        free(cmd);
    }
    else {
        char *cmd = shorten(p->command, cmd_len);
#ifndef CPU
        snprintf(line, maxlen, "%-4i %-10s %-*s %-8i %5.2f%s %-5d %s%s\n",
                 p->jobid,
                 jobstate,
                 output_width,
                 output_filename,
                 p->result.errorlevel,
                 real_ms,
                 unit,
                 p->num_gpus,
                 dependstr,
                 cmd);
#else
        snprintf(line, maxlen, "%-4i %-10s %-*s %-8i %5.2f%s %s%s\n",
                 p->jobid,
                 jobstate,
                 output_width,
                 output_filename,
                 p->result.errorlevel,
                 real_ms,
                 unit,
                 dependstr,
                 cmd);
#endif
        free(cmd);
    }

    return line;
}

#ifndef CPU
static char *print_gpu(const struct Job *p) {
    char *line = malloc(1024);
    char *gpuIDs = ints_to_chars(p->gpu_ids, p->num_gpus, " ");
    sprintf(line, "%-4i %s\n", p->jobid, gpuIDs);
    free(gpuIDs);
    return line;
}

char *jobgpulist_line(const struct Job *p) {
    return print_gpu(p);
}
#endif

static char *plainprint_noresult(const struct Job *p) {
    const char *jobstate;
    const char *output_filename;
    int maxlen;
    char *line;
    /* 20 chars should suffice for a string like "[int,int,..]&& " */
    char dependstr[20] = "";

    jobstate = jstate2string(p->state);
    output_filename = ofilename_shown(p, 0);

    maxlen = 4 + 1 + 10 + 1 + strlen(output_filename) + 1 + 8 + 1
             + 25 + 1 + 5 + 1 + strlen(p->command) + 20; /* 20 is the margin for errors */

    if (p->label)
        maxlen += 3 + strlen(p->label);

    if (p->depend_on_size) {
        maxlen += sizeof(dependstr);
        int pos = 0;
        if (p->depend_on[0] == -1)
            pos += snprintf(&dependstr[pos], sizeof(dependstr), "[ ");
        else
            pos += snprintf(&dependstr[pos], sizeof(dependstr), "[%i", p->depend_on[0]);

        for (int i = 1; i < p->depend_on_size; i++) {
            if (p->depend_on[i] == -1)
                pos += snprintf(&dependstr[pos], sizeof(dependstr), ", ");
            else
                pos += snprintf(&dependstr[pos], sizeof(dependstr), ",%i", p->depend_on[i]);
        }
        pos += snprintf(&dependstr[pos], sizeof(dependstr), "]&& ");
    }

    line = (char *) malloc(maxlen);
    if (line == NULL)
        error("Malloc for %i failed.\n", maxlen);

    if (p->label)
#ifdef CPU
        snprintf(line, maxlen, "%i\t%s\t%s\t%s\t%s\t%s\t[%s]\t%s\n",
                 p->jobid,
                 jobstate,
                 output_filename,
                 "",
                 "",
                 dependstr,
                 p->label,
                 p->command);
#else
        snprintf(line, maxlen, "%i\t%s\t%s\t%s\t%s\t%d\t%s\t[%s]\t%s\n",
                 p->jobid,
                 jobstate,
                 output_filename,
                 "",
                 "",
                 p->num_gpus,
                 dependstr,
                 p->label,
                 p->command);
#endif
    else
#ifdef CPU
        snprintf(line, maxlen, "%i\t%s\t%s\t%s\t%s\t%s\t\t%s\n",
                 p->jobid,
                 jobstate,
                 output_filename,
                 "",
                 "",
                 dependstr,
                 p->command);
#else
        snprintf(line, maxlen, "%i\t%s\t%s\t%s\t%s\t%d\t%s\t\t%s\n",
                 p->jobid,
                 jobstate,
                 output_filename,
                 "",
                 "",
                 p->num_gpus,
                 dependstr,
                 p->command);
#endif

    return line;
}

static char *plainprint_result(const struct Job *p) {
    const char *jobstate;
    int maxlen;
    char *line;
    const char *output_filename;
    /* 20 chars should suffice for a string like "[int,int,..]&& " */
    char dependstr[20] = "";
    float real_ms = p->result.real_ms;
    char *unit = time_rep(&real_ms);

    jobstate = jstate2string(p->state);
    output_filename = ofilename_shown(p, 0);

    maxlen = 4 + 1 + 10 + 1 + strlen(output_filename) + 1 + 8 + 1
             + 25 + 1 + + 5 + 1 + strlen(p->command) + 20; /* 20 is the margin for errors */

    if (p->label)
        maxlen += 3 + strlen(p->label);

    if (p->depend_on_size) {
        maxlen += sizeof(dependstr);
        int pos = 0;
        if (p->depend_on[0] == -1)
            pos += snprintf(&dependstr[pos], sizeof(dependstr), "[ ");
        else
            pos += snprintf(&dependstr[pos], sizeof(dependstr), "[%i", p->depend_on[0]);

        for (int i = 1; i < p->depend_on_size; i++) {
            if (p->depend_on[i] == -1)
                pos += snprintf(&dependstr[pos], sizeof(dependstr), ", ");
            else
                pos += snprintf(&dependstr[pos], sizeof(dependstr), ",%i", p->depend_on[i]);
        }
        pos += snprintf(&dependstr[pos], sizeof(dependstr), "]&& ");
    }

    line = (char *) malloc(maxlen);
    if (line == NULL)
        error("Malloc for %i failed.\n", maxlen);

    if (p->label)
#ifdef CPU
        snprintf(line, maxlen, "%i\t%s\t%s\t%i\t%.2f\t%s\t%s\t[%s]\t%s\n",
                 p->jobid,
                 jobstate,
                 output_filename,
                 p->result.errorlevel,
                 real_ms,
                 unit,
                 dependstr,
                 p->label,
                 p->command);
#else
        snprintf(line, maxlen, "%i\t%s\t%s\t%i\t%.2f\t%s\t%d\t%s\t[%s]\t%s\n",
                 p->jobid,
                 jobstate,
                 output_filename,
                 p->result.errorlevel,
                 real_ms,
                 unit,
                 p->num_gpus,
                 dependstr,
                 p->label,
                 p->command);
#endif
    else
#ifdef CPU
        snprintf(line, maxlen, "%i\t%s\t%s\t%i\t%.2f\t%s\t%s\t\t%s\n",
                 p->jobid,
                 jobstate,
                 output_filename,
                 p->result.errorlevel,
                 real_ms,
                 unit,
                 dependstr,
                 p->command);
#else
        snprintf(line, maxlen, "%i\t%s\t%s\t%i\t%.2f\t%s\t%d\t%s\t\t%s\n",
                 p->jobid,
                 jobstate,
                 output_filename,
                 p->result.errorlevel,
                 real_ms,
                 unit,
                 p->num_gpus,
                 dependstr,
                 p->command);
#endif

    return line;
}

static char *joblist_line_with_output(const struct Job *p, int shorten_output, int output_width) {
    char *line;

    if (p->state == FINISHED)
        line = print_result(p, shorten_output, output_width);
    else
        line = print_noresult(p, shorten_output, output_width);

    return line;
}

char *joblist_line(const struct Job *p) {
    return joblist_line_with_output(p, 0, 20);
}

char *joblist_line_short_output(const struct Job *p) {
    return joblist_line_with_output(p, 1, 20);
}

char *joblist_line_with_output_width(const struct Job *p, int output_width) {
    return joblist_line_with_output(p, 0, output_width);
}

char *joblist_line_short_output_with_output_width(const struct Job *p, int output_width) {
    return joblist_line_with_output(p, 1, output_width);
}


char *joblist_line_plain(const struct Job *p) {
    char *line;

    if (p->state == FINISHED)
        line = plainprint_result(p);
    else
        line = plainprint_noresult(p);

    return line;
}

char *joblistdump_torun(const struct Job *p) {
    int maxlen;
    char *line;

    maxlen = 10 + strlen(p->command) + 20; /* 20 is the margin for errors */

    line = (char *) malloc(maxlen);
    if (line == NULL)
        error("Malloc for %i failed.\n", maxlen);

    snprintf(line, maxlen, "ts %s\n", p->command);

    return line;
}

char *time_rep(float *t) {
    float time_in_sec = *t;
    char *unit = "s";
    if (time_in_sec > 60) {
        time_in_sec /= 60;
        unit = "m";

        if (time_in_sec > 60) {
            time_in_sec /= 60;
            unit = "h";

            if (time_in_sec > 24) {
                time_in_sec /= 24;
                unit = "d";
            }
        }
    }
    *t = time_in_sec;
    return unit;
}
