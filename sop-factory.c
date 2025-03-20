#define _POSIX_C_SOURCE 200809L
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#ifndef TEMP_FAILURE_RETRY
#define TEMP_FAILURE_RETRY(expression)             \
    (__extension__({                               \
        long int __result;                         \
        do                                         \
            __result = (long int)(expression);     \
        while (__result == -1L && errno == EINTR); \
        __result;                                  \
    }))
#endif

#define ERR(source) \
    (fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), perror(source), kill(0, SIGKILL), exit(EXIT_FAILURE))

int set_handler(void (*f)(int), int sig)
{
    struct sigaction act = {0};
    act.sa_handler = f;
    if (sigaction(sig, &act, NULL) == -1)
        return -1;
    return 0;
}

void msleep(int millisec)
{
    struct timespec tt;
    tt.tv_sec = millisec / 1000;
    tt.tv_nsec = (millisec % 1000) * 1000000;
    while (nanosleep(&tt, &tt) == -1)
    {
    }
}

int count_descriptors()
{
    int count = 0;
    DIR* dir;
    struct dirent* entry;
    struct stat stats;
    if ((dir = opendir("/proc/self/fd")) == NULL)
    {
        ERR("opendir");
    }
    char path[PATH_MAX];
    getcwd(path, PATH_MAX);
    chdir("/proc/self/fd");
    do
    {
        errno = 0;
        if ((entry = readdir(dir)) != NULL)
        {
            if (lstat(entry->d_name, &stats))
                ERR("lstat");
            if (!S_ISDIR(stats.st_mode))
                count++;
        }
    } while (entry != NULL);
    if (chdir(path))
        ERR("chdir");
    if (closedir(dir))
        ERR("closedir");
    return count - 1;  // one descriptor for open directory
}

void usage(char* program)
{
    printf("%s w1 w2 w3\n", program);
    exit(EXIT_FAILURE);
}

void first_brigade_work(int production_pipe_write, int boss_pipe, int warehouse)
{
    srand(getpid());
    int random_time;
    int res;
    char letter;
    printf("Worker %d from the first brigade: descriptors: %d\n", getpid(), count_descriptors());
    while (1)
    {
        random_time = rand() % 10 + 1;
        msleep(random_time);
        // letter = rand()%('Z'-'A'+1)+'A';
        do
            res = read(warehouse, &letter, 1);
        while (letter < 'a' || letter > 'z');  // reading from a fifo
        letter += ('A' - 'a');
        if (res < 0)
        {
            printf("Error reading from a fifo\n");
            ERR("Error reading from a fifo\n");
        }
        // printf("Producing %c\n", letter);
        res = write(production_pipe_write, &letter, 1);
        if (res == -1)
        {
            if (errno == EPIPE)
                printf("No readers for this pipe\n");
            else
            {
                printf("Error sending the letter into a pipe\n");
                ERR("Error sending the letter into a pipe\n");
            }
        }
        else
        {
            ;  // printf("message sent\n");
        }

        // tell the boss
        char one_byte = '+';
        res = write(boss_pipe, &one_byte, 1);
        if (res == -1)
        {
            if (errno == EAGAIN)
            {
                continue;
            }
            if (errno == EPIPE)
                printf("No readers for this pipe\n");
            else
            {
                printf("Error sending the letter into a pipe\n");
                ERR("Error sending the letter into a pipe\n");
            }
        }
    }
}
void second_brigade_work(int production_pipe_write, int production_pipe_read, int boss_pipe)
{
    srand(getpid());
    int res, random_time, random_letter_count;
    char letter;
    char buf[4];
    printf("Worker %d from the second brigade: descriptors: %d\n", getpid(), count_descriptors());
    while (1)
    {
        res = read(production_pipe_read, &letter, 1);
        if (res == 0)
        {
            printf("Broken pipe\n");
            ERR("Broken pipe\n");
        }
        if (res == -1)
        {
            printf("Error reading from a pipe\n");
            ERR("Error reading from a pipe");
        }
        if (res == 1)
        {
            // printf("Read the value: %c from the pipe\n", letter);
            random_time = rand() % 10 + 1;
            msleep(random_time);
            random_letter_count = rand() % 4 + 1;
            for (int j = 0; j < random_letter_count; j++)
            {
                buf[j] = letter;
            }
            res = write(production_pipe_write, buf, random_letter_count);
            if (res == -1)
            {
                if (errno == EPIPE)
                    printf("No readers for this pipe\n");
                else
                {
                    printf("Error sending the letter into a pipe\n");
                    ERR("Error sending the letter into a pipe\n");
                }
            }
            else
            {
                ;  // printf("stage 2, message sent\n");
            }
        }

        // tell the boss
        char one_byte = '+';
        res = write(boss_pipe, &one_byte, 1);
        if (res == -1)
        {
            if (errno == EAGAIN)
            {
                continue;
            }
            if (errno == EPIPE)
                printf("No readers for this pipe\n");
            else
            {
                printf("Error sending the letter into a pipe\n");
                ERR("Error sending the letter into a pipe\n");
            }
        }
    }
}
void third_brigade_work(int production_pipe_read, int boss_pipe)
{
    srand(getpid());
    int res, random_time, i = 0;
    char letter;
    char word[6];
    word[5] = '\0';
    printf("Worker %d from the third brigade: descriptors: %d\n", getpid(), count_descriptors());
    while (1)
    {
        random_time = rand() % 3 + 1;
        msleep(random_time);
        res = read(production_pipe_read, &letter, 1);
        if (res == 0)
        {
            printf("Broken pipe\n");
            ERR("Broken pipe\n");
        }
        if (res == -1)
        {
            printf("Error reading from a pipe\n");
            ERR("Error reading from a pipe");
        }
        if (res == 1)
        {
            word[i] = letter;
            i++;
            if (i == 5)
            {
                i = 0;
                printf("%s\n", word);
            }
        }

        // tell the boss
        char one_byte = '+';
        res = write(boss_pipe, &one_byte, 1);
        if (res == -1)
        {
            if (errno == EAGAIN)
            {
                continue;
            }
            if (errno == EPIPE)
                printf("No readers for this pipe\n");
            else
            {
                printf("Error sending the letter into a pipe\n");
                ERR("Error sending the letter into a pipe\n");
            }
        }
    }
    // sleep(4);
}
void boss_work(int* desc, int worker_count)
{
    for (int i = 0; i < worker_count; i++)
    {
        close(desc[i * 2 + 1]);
    }
    // at the end
    int res;
    // sleep(20);
    while ((res = wait(NULL)) > 0)
    {
        printf("A child (%d) just exited\n", res);
    }  // parent waiting for children
    printf("Boss: descriptors: %d\n", count_descriptors() - 3);
}
void close_descriptors(int* desc, int worker_count, int my_id)
{
    for (int i = 0; i < worker_count; i++)
    {
        if (i != my_id)
        {
            close(desc[i * 2 + 1]);
            fcntl(desc[i * 2], F_SETFL, O_NONBLOCK);
        }
        close(desc[i * 2]);
    }
}

int sethandler(void (*f)(int), int sigNo)  // copied from the lab on the sop website
{
    struct sigaction act;
    memset(&act, 0, sizeof(struct sigaction));
    act.sa_handler = f;
    if (-1 == sigaction(sigNo, &act, NULL))
        return -1;
    return 0;
}

void handleInt(int sigNo) { ; }

int main(int argc, char* argv[])
{
    if (argc != 4)
        usage(argv[0]);
    int w1 = atoi(argv[1]), w2 = atoi(argv[2]), w3 = atoi(argv[3]);
    if (w1 < 1 || w1 > 10 || w2 < 1 || w2 > 10 || w3 < 1 || w3 > 10)
        usage(argv[0]);

    int boss_pid = getpid();
    // creating pipes
    // to the boss:
    int worker_count = w1 + w2 + w3;
    int desc[worker_count * 2];
    for (int i = 0; i < worker_count; i++)
    {
        pipe(&desc[i * 2]);
        // printf("opening a pipe\n");
    }
    int first_second_pipe[2], second_third_pipe[2];
    pipe(first_second_pipe);
    pipe(second_third_pipe);

    // creating the fifo
    int res = mkfifo("warehouse", S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
    if (res == -1)
    {
        if (errno != EEXIST)
        {
            printf("Error creating a fifo\n");
            ERR("Error creating a fifo\n");
        }
    }
    int warehouse = open("warehouse", O_RDONLY);

    // now create the child processes:
    for (int i = 0; i < worker_count; i++)
    {
        res = fork();
        if (res > 0)
        {
            continue;
        }
        if (res == 0)
        {
            srand(getpid());
            close_descriptors(desc, worker_count, i);
            if (i < w1)
            {
                close(first_second_pipe[0]);
                close(second_third_pipe[0]);
                close(second_third_pipe[1]);
                first_brigade_work(first_second_pipe[1], desc[i * 2 + 1], warehouse);
            }
            else
            {
                if (i < w1 + w2)
                {
                    close(warehouse);
                    close(first_second_pipe[1]);
                    close(second_third_pipe[0]);
                    second_brigade_work(second_third_pipe[1], first_second_pipe[0], desc[i * 2 + 1]);
                }
                else
                {
                    close(warehouse);
                    close(first_second_pipe[0]);
                    close(first_second_pipe[1]);
                    close(second_third_pipe[1]);
                    third_brigade_work(second_third_pipe[0], desc[i * 2 + 1]);
                }
            }
            break;
        }
        if (res < 0)
        {
            ERR("Error creating a process.\n");
        }
    }
    int flag = 0;
    if (getpid() == boss_pid)
    {
        set_handler(handleInt, SIGINT);
        close(warehouse);
        close(first_second_pipe[0]);
        close(first_second_pipe[1]);
        close(second_third_pipe[0]);
        close(second_third_pipe[1]);
        boss_work(desc, worker_count);
    }

    if (getpid() == boss_pid)
    {
        ;
    }
    else
    {
        // printf("%d: descriptors: %d\n", getpid(), count_descriptors()-3);
    }
    while (1)
    {
        if (flag)
        {
            printf("Interrupt received\n");
        }
    }
}
