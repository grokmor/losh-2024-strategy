#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <stdarg.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <assert.h>

#define kill(...) do { \
    kill(__VA_ARGS__); \
    usleep(10000); \
} while(0);

#ifdef DEBUG
#ifndef KEEPFILE
#define KEEPFILE
#endif
#endif

const int STEP_USEC = 300 * 1000;

typedef struct pair {
    int x;
    int y;
} pair;

typedef struct buf {
    char *buf;
    int len;
    int cap;
} buf;

extern char **environ;

FILE *field_file;

int cntp;

int cnt_walls;

int **pipein;
int **pipeout;

FILE **fin;
FILE **fout;

int *progpids;

int t, n, p, k, a;
int **field;

int *cntstartpos;

pair **turns;
int *turnslen;

buf *outputs;

int *alive;

int *playerscores;

int interactor_bug = 0;

int valid_finished = 0;

int cur_running = -1;

int pidexit = -1;
int exitstatus = -1;

int cntalive;

void bufprintf(buf *b, const char *fmt, ...) {
    va_list vargs;
    va_start(vargs, fmt);
    int diff = vsnprintf(b->buf + b->len, b->cap - b->len, fmt, vargs);
    while (b->len + diff + 1 >= b->cap) {
        b->buf = realloc(b->buf, 2 * b->cap);
        b->cap *= 2;
        diff = vsnprintf(b->buf + b->len, b->cap - b->len, fmt, vargs);
    }
    b->len += diff;
    va_end(vargs);
}

void fail(void);

void shutdown(void);

static void child_exit() {
    pid_t pid;
    int status;

    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        pidexit = pid;
        exitstatus = status;
    }
}

static void timer_exp() {
    if (cur_running != -1) {
        #ifdef DEBUG
        fprintf(stderr, "Timer has expired, currently running player is %d\n", cur_running);
        #endif
        fcntl(pipeout[cur_running][0], F_SETFL, O_NONBLOCK);
    }
}

void handle_child(void) {
    if (pidexit > 0) {
        if (pidexit == progpids[0] && !valid_finished) {
            fprintf(stderr, "Validator has exited unexpectedly\n");
            fail();
        }
        for (int i = 0; i <= cntp; i++) {
            if (pidexit == progpids[i]) {
#ifdef DEBUG
                fprintf(stderr, "Handling player %d's exit\n", i);
#endif
                if (alive[i]) {
                    alive[i] = 0;
                    cntalive--;
                }
            }
        }
        if (cntalive == 0) {
            printf("All children have exited, terminating the game\n");
            shutdown();
        }
    }
    pidexit = -1;
}

void terminate_validator(void) {
    if (!alive[0] || interactor_bug) {
        return;
    }
#ifdef DEBUG
    fprintf(stderr, "Sending -3 -3 -3 to validator\n");
#endif
    valid_finished = 1;
    fprintf(fin[0], "%d %d %d\n", -3, -3, -3);
    fsync(pipein[0][1]);
    int valid_status;
    fscanf(fout[0], "%d", &valid_status);
#ifdef DEBUG
    fprintf(stderr, "Got %d from validator stdout\n", valid_status);
#endif
    assert(valid_status == 4);
    playerscores = malloc((cntp + 1) * sizeof(int));
    for (int i = 1; i <= cntp; i++) {
        fscanf(fout[0], "%d", &playerscores[i]);
    }
    printf("\n\nPlayer scores:\n");
    for (int i = 1; i <= cntp; i++) {
        printf("  Player %d: %d\n", i, playerscores[i]);
    }
    free(playerscores);
    printf("\n");
    int valid_exit;
#ifdef DEBUG
    fprintf(stderr, "Waiting for validator process (PID %d) to exit\n", progpids[0]);
#endif
    waitpid(progpids[0], &valid_exit, 0);
#ifdef DEBUG
    fprintf(stderr, "Validator process (PID %d) has finished\n", progpids[0]);
#endif
}

void close_all(void) {
#ifdef DEBUG
    fprintf(stderr, "Closing all files streams and pipes\n");
#endif
    if (field_file) {
        fclose(field_file);
    }
    if (progpids) {
        terminate_validator();
        for (int i = 1; i < cntp + 1; i++) {
            if (progpids[i]) {
#ifdef DEBUG
                fprintf(stderr, "Sending SIGTERM to %d\n", progpids[i]);
#endif
                kill(progpids[i], SIGTERM);
            }
        }
        free(progpids);
    }
    if (pipein) {
        for (int i = 0; i < cntp + 1; i++) {
            free(pipein[i]);
        }
        free(pipein);
    }
    if (pipeout) {
        for (int i = 0; i < cntp + 1; i++) {
            free(pipeout[i]);
        }
        free(pipeout);
    }
    if (fin) {
        for (int i = 0; i < cntp + 1; i++) {
            fclose(fin[i]);
        }
        free(fin);
    }
    if (fout) {
        for (int i = 0; i < cntp + 1; i++) {
            fclose(fout[i]);
        }
        free(fout);
    }

    if (field) {
        for (int i = 0; i < a; i++) {
            free(field[i]);
        }
        free(field);
    }

    free(cntstartpos);

    if (turns) {
        for (int i = 0; i <= cntp; i++) {
            free(turns[i]);
        }
        free(turns);
    }

    free(turnslen);

    free(alive);
#ifdef DEBUG
    fprintf(stderr, "Closed all streams and pipes\n");
#endif
}

int enabled(char *str, int argc, char *argv[]) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(str, argv[i]) == 0) {
            return 1;
        }
    }
    return 0;
}

void help(char *progname) {
    printf("Usage: %s field.txt log.txt validate program1 program2 [program3 ...]\n", progname);
}

void fail(void) {
#ifdef DEBUG
    fprintf(stderr, "fail() called, terminating\n");
#endif
    close_all();
    exit(6);
}

int geti(int x, int y) {
    return a / 2 - y;
}

int getj(int x, int y) {
    return a / 2 + x; 
}

void read_field(int argc, char *argv[]) {
#ifdef DEBUG
    fprintf(stderr, "Creating a stream for reading field\n");
#endif
    field_file = fopen(argv[1], "r");
#ifdef DEBUG
    fprintf(stderr, "Created a stream for reading field, reading field\n");
#endif
    fscanf(field_file, "%d%d%d%d%d", &t, &n, &p, &k, &a);
    if (k != cntp) {
        fprintf(stderr, "k (%d) is not equal to the number of strategies provided (%d)\n", k, cntp);
        fail();
    }
    field = calloc(a, sizeof(int*));
    cnt_walls = 0;
    for (int i = 0; i < a; i++) {
        field[i] = malloc(a * sizeof(int));
        for (int j = 0; j < a; j++) {
            fscanf(field_file, "%d", &field[i][j]);
            if (field[i][j] < 0) {
                cnt_walls++;
            }
        }
    }
#ifdef DEBUG
    fprintf(stderr, "Finished reading field\n");
#endif
}

void create_pipes(void) {
#ifdef DEBUG
    fprintf(stderr, "Creating pipes\n");
#endif
    pipein = calloc(cntp + 1, sizeof(int*));
    pipeout = calloc(cntp + 1, sizeof(int*));
    for (int i = 0; i <= cntp; i++) {
        pipein[i] = calloc(2, sizeof(int));
        pipeout[i] = calloc(2, sizeof(int));
        pipe(pipein[i]);
        pipe(pipeout[i]);
    }
#ifdef DEBUG
    fprintf(stderr, "Created pipes\n");
#endif
};

void create_streams(void) {
#ifdef DEBUG
    fprintf(stderr, "Creating streams\n");
#endif
    fin = calloc(cntp + 1, sizeof(FILE*));
    fout = calloc(cntp + 1, sizeof(FILE*));
    for (int i = 0; i <= cntp; i++) {
        fin[i] = fdopen(pipein[i][1], "w");
        setbuf(fin[i], NULL);
        fout[i] = fdopen(pipeout[i][0], "r");
    }
#ifdef DEBUG
    fprintf(stderr, "Created streams\n");
#endif
}

void set_mem_limit(unsigned long mbytes) {
    struct rlimit rlim_as;
    struct rlimit rlim_data;
    struct rlimit rlim_rss;
    struct rlimit rlim_stack;

    if (getrlimit(RLIMIT_AS, &rlim_as) != 0) {
        fprintf(stderr, "Could not get AS rlimit\n");
        fail();
    }
    if (getrlimit(RLIMIT_DATA, &rlim_data) != 0) {
        fprintf(stderr, "Could not get DATA limit\n");
        fail();
    }
    if (getrlimit(RLIMIT_RSS, &rlim_rss) != 0) {
        fprintf(stderr, "Could not get RSS limit\n");
        fail();
    }
    if (getrlimit(RLIMIT_STACK, &rlim_stack) != 0) {
        fprintf(stderr, "Could not get stack limit\n");
        fail();
    }

    rlim_as.rlim_cur = mbytes * 1048576;
    rlim_data.rlim_cur = mbytes * 1048576;
    rlim_rss.rlim_cur = mbytes * 1048576;
    rlim_stack.rlim_cur = mbytes * 1048576;

    if (setrlimit(RLIMIT_AS, &rlim_as) != 0) {
        fprintf(stderr, "Could not set AS rlimit\n");
        fail();
    }
    if (setrlimit(RLIMIT_DATA, &rlim_data) != 0) {
        fprintf(stderr, "Could not set DATA limit\n");
        fail();
    }
    if (setrlimit(RLIMIT_RSS, &rlim_rss) != 0) {
        fprintf(stderr, "Could not set RSS limit\n");
        fail();
    }
    if (setrlimit(RLIMIT_STACK, &rlim_stack) != 0) {
        fprintf(stderr, "Could not set stack limit\n");
        fail();
    }
}

void run_programs(int argc, char *argv[]) {
#ifdef DEBUG
    fprintf(stderr, "Creating processes for programs\n");
#endif
    char **validargs = calloc(3, sizeof(char*));
    validargs[0] = argv[3];
    validargs[1] = argv[2];
    int validpid = fork();
    cntalive = cntp;
    progpids = calloc(cntp + 1, sizeof(int));
    if (validpid < 0) {
        fprintf(stderr, "Could not create a process for validator\n");
        free(validargs);
        fail();
    } else if (validpid == 0) {
        set_mem_limit(1024);
        dup2(pipein[0][0], STDIN_FILENO);
        dup2(pipeout[0][1], STDOUT_FILENO);
        execve(argv[3], validargs, environ);
    } else {
        free(validargs);
        progpids[0] = validpid;
#ifdef DEBUG
        fprintf(stderr, "Validator is running at pid %d\n", validpid);
#endif
    }

    char **curargs = calloc(2, sizeof(char*));

    for (int i = 1; i <= cntp; i++) {
        int curpid = fork();
        if (curpid < 0) {
            fprintf(stderr, "Could not create a process for %d player\n", i);
            free(curargs);
            fail();
        } else if (curpid == 0) {
            set_mem_limit(256);
            dup2(pipein[i][0], STDIN_FILENO);
            dup2(pipeout[i][1], STDOUT_FILENO);
            curargs[0] = argv[3 + i];
            #ifdef DEBUG
            fprintf(stderr, "Executing command %s\n", argv[3 + i]);
            #endif
            if (execve(argv[3 + i], curargs, environ) == -1) {
                fprintf(stderr, "Could not execute %d player's program\n", i);
                exit(1);
            }
        } else {
            progpids[i] = curpid;
#ifdef DEBUG
            fprintf(stderr, "Participant %d is running at pid %d\n", i, curpid);
#endif
           kill(curpid, SIGSTOP);
        }
    }
    free(curargs);
}

int validate_field() {
#ifdef DEBUG
    fprintf(stderr, "Beginning field validation\n");
#endif
    fprintf(fin[0], "%s", outputs[0].buf);
    outputs[0].len = 0;
    fprintf(fin[0], "%d %d %d %d %d\n", t, n, p, k, a);
    fsync(pipein[0][1]);
    for (int i = 0; i < a; i++) {
        for (int j = 0; j < a; j++) {
            fprintf(fin[0], "%d", field[i][j]);
            if (j == a - 1) {
                fprintf(fin[0], "\n");
            } else {
                fprintf(fin[0], " ");
            }
        }
    }
    fsync(pipein[0][1]);
#ifdef DEBUG
    fprintf(stderr, "Wrote field to validator, waiting for answer\n");
#endif
    int valid_status;
    handle_child();
    fscanf(fout[0], "%d", &valid_status);
    char *valid_msg = malloc(1024);
#ifdef DEBUG
    fprintf(stderr, "Got %d code from validator\n", valid_status);
#endif
    if (valid_status != 0) {
        fgets(valid_msg, 1024, fout[0]);
        fgets(valid_msg, 1024, fout[0]);
    }
    if (valid_status == 2) {
        fprintf(stderr, "Interactor has a bug, exiting. Validator info: %s\n", valid_msg);
        interactor_bug = 1;
        free(valid_msg);
        fail();
    } else if (valid_status != 0) {
        fprintf(stderr, "Unexpected player-specific code on field validation, exiting. Validator code: %d, validator info: %s\n", valid_status, valid_msg);
        free(valid_msg);
        fail();
    }
    free(valid_msg);
    return valid_status;
}

void write_field_part(int num) {
    bufprintf(&outputs[num], "%d %d %d %d %d\n%d\n", t, n, p, k, a, num);
    for (int i = a / 2 - n; i <= a / 2 + n; i++) {
        for (int j = a / 2 - n; j <= a / 2 + n; j++) {
            bufprintf(&outputs[num], "%d", field[i][j]);
            if (j == a / 2 + n) {
                bufprintf(&outputs[num], "\n");
            } else {
                bufprintf(&outputs[num], " ");
            }
        }
    }
}

void killchild(int num) {
#ifdef DEBUG
    fprintf(stderr, "Killing child %d\n", num);
#endif
    if (alive[num]) {
#ifdef DEBUG
        fprintf(stderr, "Sending SIGTERM to %d\n", progpids[num]);
#endif
        if (alive[num]) {
            alive[num] = 0;
            cntalive--;
        }
        kill(progpids[num], SIGTERM);
        if (cntalive == 0) {
            printf("All children have exited, terminating the game\n");
            shutdown();
        }
    }
}

void shutdown() {
    close_all();
    exit(0);
}

int validate_turn(int num, pair turn) {
#ifdef DEBUG
    fprintf(stderr, "Validating turn (%d, %d) of player %d\n", turn.x, turn.y, num);
#endif
    if (num != -1 && !alive[num]) {
        return 1;
    }
    #ifdef DEBUG
    fprintf(stderr, "Writing %d %d %d to validator stream\n", num, turn.x, turn.y);
    #endif
    fprintf(fin[0], "%d %d %d\n", num, turn.x, turn.y);
    fsync(pipein[0][1]);
    int valid_res;
    fscanf(fout[0], "%d", &valid_res);
    if (valid_res != 0) {
        char *msg = malloc(1024);
        fgets(msg, 1024, fout[0]);
        fgets(msg, 1024, fout[0]);
        if (valid_res == 1 || valid_res == 3) {
            fprintf(stderr, "Player %d has made incorrect turn. Validator info: %s\n", num, msg);
            if (valid_res == 3) {
                shutdown();
            } else {
                killchild(num);
            }
            free(msg);
        } else {
            fprintf(stderr, "Validator code: %d, validator msg: %s. Exiting.\n", valid_res, msg);
            interactor_bug = 1;
            free(msg);
            fail();
        }
    }
#ifdef DEBUG
    fprintf(stderr, "Validator returned %d\n", valid_res);
#endif
    return valid_res;
}

void expand_field(void) {
    pair cur;
    cur.x = -1;
    cur.y = -1;
    int res = validate_turn(-1, cur);
    if (res != 0) {
        fprintf(stderr, "Validator gave %d on field expansion. Exiting.\n", res);
        fail();
    }
    n++;
    for (int x = -n; x <= n; x++) {
        int y = n;
        if (field[geti(x, y)][getj(x, y)] < 0) {
            turns[0][turnslen[0]].x = x;
            turns[0][turnslen[0]].y = y;
            turnslen[0]++;
        }
        y = -n;
        if (field[geti(x, y)][getj(x, y)] < 0) {
            turns[0][turnslen[0]].x = x;
            turns[0][turnslen[0]].y = y;
            turnslen[0]++;
        }
    }
    for (int y = -n + 1; y <= n - 1; y++) {
        int x = n;
        if (field[geti(x, y)][getj(x, y)] < 0) {
            turns[0][turnslen[0]].x = x;
            turns[0][turnslen[0]].y = y;
            turnslen[0]++;
        }
        x = -n;
        if (field[geti(x, y)][getj(x, y)] < 0) {
            turns[0][turnslen[0]].x = x;
            turns[0][turnslen[0]].y = y;
            turnslen[0]++;
        }
    }
} 

void write_to_proc(int num, int v1, int v2, int v3) {
    if (!alive[num]) {
        return;
    }
    bufprintf(&outputs[num], "%d %d %d\n", v1, v2, v3);
}

void set_timer(int cur_num) {
    cur_running = cur_num;
    struct itimerval timer_val;
    timer_val.it_interval.tv_sec = 0;
    timer_val.it_interval.tv_usec = 0;

    timer_val.it_value.tv_sec = 0;
    timer_val.it_value.tv_usec = STEP_USEC;

    if (setitimer(ITIMER_REAL, &timer_val, NULL) != 0) {
        fprintf(stderr, "setitimer returned non-zero, errno is %d\n", errno);
        fail();
    }
}

void reset_timer() {
    alarm(0);
    cur_running = -1;
}

pair read_from_proc(int num) {
#ifdef DEBUG
    fprintf(stderr, "read_from_proc(%d): alive is %d\n", num, alive[num]);
#endif
    if (!alive[num]) {
        pair res;
        res.x = res.y = 0;
        return res;
    }
    kill(progpids[num], SIGCONT);
#ifdef DEBUG
    fprintf(stderr, "Writing to %d proc:\n%s", num, outputs[num].buf);
#endif
    fprintf(fin[num], "%s", outputs[num].buf);
    outputs[num].len = 0;
    fsync(pipein[num][1]);
    pair res;
    handle_child();
#ifdef DEBUG
    fprintf(stderr, "read_from_proc(%d): alive is %d\n", num, alive[num]);
#endif
    if (alive[num]) {
        set_timer(num);
        int cres;
        if ((cres = fscanf(fout[num], "%d%d", &res.x, &res.y)) != 2) {
            fprintf(stderr, "Read %d integers from %d player\n", cres, num);
            fprintf(stderr, "Player %d made turn in wrong format or exceeded the time-limit for making turn, killing\n", num);
            killchild(num);
            res.x = 1000000000;
            res.y = 1000000000;
        }
        reset_timer();
    }
#ifdef DEBUG
    fprintf(stderr, "Got %d %d from child\n", res.x, res.y);
#endif
    handle_child();
    kill(progpids[num], SIGSTOP);
    return res;
}

void interact(int num, int expand) {
    if (!alive[num]) {
        return;
    }
    turnslen[num] = 0;
    for (int j = num + 1; j <= cntp; j++) {
        if (turnslen[j] > 0) {
            for (int m = 0; m < turnslen[j]; m++) {
                write_to_proc(num, j, turns[j][m].x, turns[j][m].y);
            }
        }
    }
    if (expand) {
        write_to_proc(num, -1, -1, -1);
        for (int m = 0; m < turnslen[0]; m++) {
            write_to_proc(num, 0, turns[0][m].x, turns[0][m].y);
        }
        write_to_proc(num, -1, -1, -1);
    }
    for (int j = 1; j < num; j++) {
        if (turnslen[j] > 0) {
            for (int m = 0; m < turnslen[j]; m++) {
                write_to_proc(num, j, turns[j][m].x, turns[j][m].y);
            }
        }
    }
    write_to_proc(num, -2, -2, -2);
    pair curturn = read_from_proc(num);
    int res = validate_turn(num, curturn);
    if (res == 0) {
        turns[num][turnslen[num]++] = curturn;
    }
}

int main(int argc, char *argv[]) {
    struct sigaction child_sa;
    sigemptyset(&child_sa.sa_mask);
    child_sa.sa_flags = 0;
    child_sa.sa_handler = child_exit;

    sigaction(SIGCHLD, &child_sa, NULL);

    struct sigaction timer_sa;
    sigemptyset(&timer_sa.sa_mask);
    timer_sa.sa_flags = 0;
    timer_sa.sa_handler = timer_exp;

    sigaction(SIGALRM, &timer_sa, NULL);

    if (enabled("--help", argc, argv)) {
        help(argv[0]);
        return 0;
    }

    cntp = argc - 4;
    if (cntp < 2) {
        fprintf(stderr, "Not enough parameters passed. Please use %s --help to get usage information\n", argv[0]);
        fail();
    }

    alive = malloc((cntp + 1) * sizeof(int));
    for (int i = 0; i <= cntp; i++) {
        alive[i] = 1;
    }

    outputs = malloc((cntp + 1) * sizeof(buf));
    for (int i = 0; i <= cntp; i++) {
        outputs[i].len = 0;
        outputs[i].cap = 1048576;
        outputs[i].buf = calloc(1048576, sizeof(char));
    }

    read_field(argc, argv);
    create_pipes();
    create_streams();
    run_programs(argc, argv);
    validate_field();
    handle_child();
    for (int i = 1; i <= cntp; i++) {
        write_field_part(i);
        handle_child();
    }
    turns = calloc(cntp + 1, sizeof(pair*));
    turnslen = calloc(cntp + 1, sizeof(int));
    turns[0] = malloc(cnt_walls * sizeof(pair));
    for (int i = 1; i <= cntp; i++) {
        turns[i] = malloc(1 * sizeof(pair));
    }

    int cstep = 0;
#ifdef DEBUG
    fprintf(stderr, "Starting interaction\n");
#endif
    while (cstep < t) {
        printf("Progress: %lf%%\n", cstep * 100.0 / t);
        cstep++;
        turnslen[0] = 0;
        int curexpand = 0;
        if (cstep % p == 0) {
            expand_field();
            curexpand = 1;
        }
        for (int i = 1; i <= cntp; i++) {
            interact(i, curexpand);
        }
    }
    shutdown();
}
