#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h> // for strsep
#include <signal.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <ctype.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/shm.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <arpa/inet.h>

#define SIZE_OF_ARRAY 11
#define MAX_ARGS 20 
#define MAX_ARG_LENGTH 50  

struct command
{
    int argc;                     
    char *argv[MAX_ARGS];      
    bool input_redirect;          
    bool output_redirect;         
    bool output_append;           
    char input_file[MAX_ARG_LENGTH];  
    char output_file[MAX_ARG_LENGTH]; 
    struct command *next;   
};      

typedef struct
{
    struct command *head; // head of the linkedlist
    struct command *tail; // tail of the linkedlist
    int num_of_cmds;      // number of cmds in linkedlist
} cmd_pipeline;

int num_cmds; // number of commands
char *input;  // input typed at command line
int size = 0;

typedef struct hashnode
{
    int index;
    struct cmd_pipeline *pipeline;
    struct hashnode *next;
} HashNode; //node of a hash table

typedef struct bucket
{
    HashNode *head;
    int size;
} BUCKET; //for chaining in hash table

int hashValue(int index)
{
    return index % SIZE_OF_ARRAY;
}

BUCKET *sc_array[SIZE_OF_ARRAY];

void error_exit(char *err_msg)
{
    perror(err_msg);
    exit(EXIT_FAILURE);
}

void initialise_sc_array()
{
    for (int i = 0; i < SIZE_OF_ARRAY; i++)
    {
        sc_array[i] = NULL;
    }
}
void populate_array(int index, struct cmd_pipeline *pipeline)
{
    int hash = hashValue(index);
    if (sc_array[hash] == NULL)
    {
        sc_array[hash] = (BUCKET *)malloc(sizeof(BUCKET));
    }
    HashNode *node = (HashNode *)malloc(sizeof(HashNode));
    node->index = index;
    node->pipeline = pipeline;
    node->next = NULL;
    if (!sc_array[hash]->head)
    {
        sc_array[hash]->head = node;
    }
    else
    {
        node->next = sc_array[hash]->head;
        sc_array[hash]->head = node;
    }
    sc_array[hash]->size++;
    printf("\n ----------------------------------\nSuccessully inserted\n ----------------------------------\n");
}
void delete_node(int index)
{
    int hash = hashValue(index);
    bool flag = false;
    if (sc_array[hash] == NULL)
    {
        error_exit("No such index exists");
    }
    if (sc_array[hash]->head->index == index)
    {
        HashNode *tmp = sc_array[hash]->head->next;
        free(sc_array[hash]->head);
        sc_array[hash]->head = tmp;
        printf("\n ----------------------------------\nSuccessully deleted\n ----------------------------------\n");
        return;
    }

    else
    {
        HashNode *tmp = sc_array[hash]->head;
        HashNode *prev = sc_array[hash]->head;

        while (tmp)
        {
            if (tmp->index == index)
            {
                prev->next = tmp->next;
                free(tmp);
                flag = true;
                printf("\n ----------------------------------\nSuccessully deleted\n ----------------------------------");
                break;
            }
            prev = tmp;
            tmp = tmp->next;
        }
    }
    if (!flag)
    {
        error_exit("No such index exists");
    }
}

bool is_str_num(char *str)
{
    int len = strlen(str);

    for (int i = 0; i < len; i++)
    {
        if (!isdigit(str[i]))
            return false;
    }

    return true;
}

void initialize_cmd(struct command **cmd_ptr)
{
    *cmd_ptr = (struct command *)malloc(sizeof(struct command));

    if (*cmd_ptr == NULL)
        error_exit("malloc:");

    (*cmd_ptr)->argc = 0;

    strcpy((*cmd_ptr)->input_file, "");
    strcpy((*cmd_ptr)->output_file, "");

    (*cmd_ptr)->input_redirect = 0;
    (*cmd_ptr)->output_redirect = 0;
    (*cmd_ptr)->output_append = 0;
    (*cmd_ptr)->next = NULL;

    for (int i = 0; i < MAX_ARGS; i++)
        (*cmd_ptr)->argv[i] = NULL;
}

void insert_command(cmd_pipeline *pipeline, struct command *cmd)
{
    if (pipeline->head == NULL)
    {
        pipeline->head = cmd;
        pipeline->tail = cmd;
        return;
    }
    pipeline->tail->next = cmd;
    pipeline->tail = cmd;
}

void parse_command(char *input, cmd_pipeline *pipeline)
{
    if (!strcmp(input, "")) //creating an empty node when there is an empty string
    {
        struct command *cmd_ptr;
        initialize_cmd(&cmd_ptr);
        size++;
        insert_command(pipeline, cmd_ptr);
        return;
    }
    char *tk;
    while ((tk = strsep(&input, ",")) != NULL) //tokenise with , as breakpoint. Useful for || and ||| case
    {
        struct command *cmd_ptr;
        initialize_cmd(&cmd_ptr);
        int len = strlen(tk);
        char *str = (char *)malloc(sizeof(char) * (len + 1)); // to store current arguement
        if (str == NULL)
            error_exit("malloc str");

        char prev_str[MAX_ARG_LENGTH]; // to store previous arguement
        int arg_size = 0;          // size of current arguement
        int num_args = 0;          // num of args present in cmd

        strcpy(prev_str, "");
        for (int i = 0; i < len; i++) //read char by char
        {
            if (!isspace(tk[i]))
            { 
                str[arg_size++] = tk[i];
            }

            else if ((i + 1 < len && !isspace(tk[i + 1]) && arg_size >= 1) || (i + 1 >= len && arg_size >= 1))
            {
                str[arg_size] = '\0'; 
                arg_size = 0;         

                if (!strcmp(prev_str, "<"))
                { 
                    strcpy((cmd_ptr)->input_file, str);
                    strcpy(prev_str, str);
                    continue;
                }

                if (!strcmp(prev_str, ">") || !strcmp(prev_str, ">>"))
                { // if previously output redirection was recognized, store file
                    strcpy((cmd_ptr)->output_file, str);
                    strcpy(prev_str, str);
              
                    continue;
                }

                strcpy(prev_str, str); // update prev str

                if (!strcmp(str, "<"))
                {
                    (cmd_ptr)->input_redirect = true; //mark input redirection
                    continue;
                }

                if (!strcmp(str, ">"))
                {
                    (cmd_ptr)->output_redirect = true;
                    (cmd_ptr)->output_append = false; // mark output redirection
                    continue;
                }

                if (!strcmp(str, ">>"))
                {
                    (cmd_ptr)->output_redirect = true; // mark output append
                    (cmd_ptr)->output_append = true;
                    continue;
                }

                (cmd_ptr)->argv[num_args] = (char *)malloc(sizeof(char) * (arg_size + 10));

                if ((cmd_ptr)->argv[num_args] == NULL)
                    error_exit("malloc agv[num_args] ");

                strcpy((cmd_ptr)->argv[num_args], str); // copy the arguement

                num_args++;
            }
            if(!(isspace(tk[i])) && i == len-1){
                (cmd_ptr)->argv[num_args] = (char *)malloc(sizeof(char) * (arg_size + 10));
                strcpy((cmd_ptr)->argv[num_args], str);
                num_args++;
            }
        }

        (cmd_ptr)->argv[num_args] = NULL; // mark the end of arguement vector by a NULL pointer
        (cmd_ptr)->argc = num_args;
        free(str);
        size++;

        insert_command(pipeline, cmd_ptr);
    }
}

void create_pipeline(char *input, cmd_pipeline *pipeline)
{
    char *token;
    size = 0;

    while ((token = strsep(&input, "|")) != NULL)
    { // tokenize by | operator, u get one command
        parse_command(token, pipeline);
    }
    
    num_cmds = size;
    pipeline->num_of_cmds = num_cmds;
}

void print_command(struct command *cmd)
{
    printf("argc : %d, input_redirect : %d, output_redirect : %d, output_append: %d\n", cmd->argc, cmd->input_redirect, cmd->output_redirect, cmd->output_append);
    printf("input_file : %s, output_file %s\n", cmd->input_file, cmd->output_file);
    printf("argv : ");

    int j = 0;
    char *tmp = cmd->argv[j];
    while (tmp)
    {
        printf("%s, ", tmp);
        tmp = cmd->argv[++j];
    }
    // printf("\n");
}

void print_commands(cmd_pipeline *pipeline)
{
    struct command *tmp;
    for (tmp = pipeline->head; tmp != NULL; tmp = tmp->next)
    {
        printf("\n=======================================\n");
        print_command(tmp);
        printf("\n=======================================\n");
    }
}

void remove_commands(cmd_pipeline *pipeline) //empty the pipeline
{
    struct command *tmp, *xy;
    if (!pipeline)
        return;
    for (tmp = pipeline->head; tmp != NULL; tmp = xy)
    {
        xy = tmp->next;
        free(tmp);
    }
    pipeline->head = NULL;
    pipeline->tail = NULL;
    pipeline->num_of_cmds = 0;
}

void close_all_pipes(int pipe_fd[][2], int count)
{

    for (int i = 0; i < count; i++)
    {
        close(pipe_fd[i][0]);
        close(pipe_fd[i][1]);
    }
}

void repetitive(struct command **cmd) //modifying the file descriptors
{
    if ((*cmd)->input_redirect == true && !is_str_num((*cmd)->input_file))
    { // input redirection to a file
        int fd_input;

        fd_input = open((*cmd)->input_file, O_RDONLY);
        printf("---------------------------\nRedirected input fd = %d\n----------------------------\n", fd_input);
        if (fd_input == -1)
            error_exit("input file open ");

        if (dup2(fd_input, STDIN_FILENO) == -1) // attach the file descriptor to stdin
            error_exit("dup2 stdin ");

        if (close(fd_input) == -1)
            error_exit("close input redirection proc ");
    }

    if ((*cmd)->output_redirect && (*cmd)->output_append && !is_str_num((*cmd)->output_file))
    { // output append to a file
        int fd_output;

        fd_output = open((*cmd)->output_file, O_APPEND | O_WRONLY | O_CREAT, 0777); // open file in append mode

        if (fd_output == -1)
            error_exit("output file open");

        printf("---------------------------\nRedirected output fd = %d\n----------------------------\n", fd_output);

        if (dup2(fd_output, STDOUT_FILENO) == -1) // attach fd to stdout
            error_exit("dup2 process outfile ");

        if (close(fd_output) == -1)
            error_exit("close output redirection");
    }
    else if ((*cmd)->output_redirect && !is_str_num((*cmd)->output_file))
    { // output redirect to a file
        int fd_output = open((*cmd)->output_file, O_TRUNC | O_WRONLY | O_CREAT, 0777);

        if (fd_output == -1)
            error_exit("output file open");

        printf("---------------------------\nRedirected output fd = %d\n----------------------------\n", fd_output);

        if (dup2(fd_output, STDOUT_FILENO) == -1)
            error_exit("dup2 outfile ");

        if (close(fd_output) == -1)
            error_exit("close output redirection");
    }
}

void execute(cmd_pipeline *pipeline)
{
    bool run_in_bg = false;
    struct command *last = pipeline->tail;

    if (!strcmp(last->argv[last->argc - 1], "&"))
    {
        ///commands to be run in background
        run_in_bg = true;
        last->argv[last->argc - 1] = NULL;
        last->argc--;
    }

    pid_t grandchild = fork();
    if (grandchild == -1)
    {
        error_exit("making new group wala fork");
    }

    else if (grandchild == 0)
    {
        //int new_group = setpgid(getpid(), getpid());
        int new_group = setpgrp();
        if (new_group == 0)
        {
            printf("Success %d %d", getpid(), getpgid(getpid()));
        }
        struct command *tmp = pipeline->head;
        int count = pipeline->num_of_cmds;

        if(!run_in_bg)
        {
            tcsetpgrp(STDIN_FILENO, getpgid(getpid()));
        }

        int pipe_fd[count - 1][2]; // create n-1 pipes for n processes

        for (int i = 0; i < count - 1; i++)
        {
            if (pipe(pipe_fd[i]) == -1)
                error_exit("pipe creation ");
        }
        int i;
        struct command *cmd;

        int pipe_num = 1;

        char *buf = NULL;
        for (i = 0, cmd = pipeline->head; i < count, cmd != NULL; i++, cmd = cmd->next)
        {

            if (i < count - 1) // num of pipes is n-1
                printf("\n===========pipe[%d] : read_fd = %d, write_fd = %d===========\n", i, pipe_fd[i][0], pipe_fd[i][1]);

            if (cmd->argv[0] == NULL)
            {
                pipe_num++;
                if (buf == NULL)
                {
                    buf = (char *)malloc(sizeof(char) * 501);
                    int l = read(pipe_fd[i - 1][0], buf, 500);
                    buf[l] = '\0';
                }
                continue;
            }
            if (pipe_num == 1)
            {
                // normal execution
                pid_t pid = fork();

                if (pid == -1)
                    error_exit("fork");

                if (pid == 0)
                {

                    printf("\n===========process[%d] pid: %d process[%d] gid: %d===========\n", i, getpid(), i, getpgid(getpid()));

                    if (i != 0)
                    { // attach read end of stdin to pipe for all processes except first
                        if (dup2(pipe_fd[i - 1][0], STDIN_FILENO) == -1)
                            error_exit("dup2 pipe read ");
                    }

                    if (i != count - 1)
                    { // attach write end of stdout to pipe for all processes except last
                        if (dup2(pipe_fd[i][1], STDOUT_FILENO) == -1)
                            error_exit("dup2 pipe write ");
                    }

                    repetitive(&cmd);

                    close_all_pipes(pipe_fd, count - 1);

                    if (execvp(cmd->argv[0], cmd->argv) == -1)
                    {
                        error_exit("execvp");
                    }
                }
                else
                { ///in parent
                    if (i - 1 > 0)
                        close(pipe_fd[i - 1][0]); // close necessary pipes so that corresponding child can exit

                    close(pipe_fd[i][1]);

                    if (!run_in_bg)
                    {
                        ///that is run in foreground
                        if (wait(NULL) == -1)
                        { // wait for this child to exit
                            error_exit("wait pid ");
                        }
                    }
                }
            }
            else if (pipe_num == 2)
            {
                pipe_num = 1;
                pid_t pid = fork(); ///////// for first command after double pipe

                if (pid == -1)
                    error_exit("fork");

                if (pid == 0)
                {
                    printf("\n===========process[%d] pid: %d process[%d] gid: %d===========\n", i, getpid(), i, getpgid(getpid()));
                    int temp[2];
                    pipe(temp);

                    if (i != 0)
                    { // attach read end of stdin to pipe for all processes except first
                        write(temp[1], buf, strlen(buf));
                        close(temp[1]);
                        if (dup2(temp[0], STDIN_FILENO) == -1)
                            error_exit("dup2 pipe read "); 

                        close(temp[0]);
                    }

                    repetitive(&cmd);

                    close_all_pipes(pipe_fd, count - 1);

                    if (execvp(cmd->argv[0], cmd->argv) == -1)
                    {
                        error_exit("execvp");
                    }
                }
                else
                { ///in parent
                    if (i - 1 > 0)
                        close(pipe_fd[i - 1][0]); // close necessary pipes so that corresponding child can exit

                    close(pipe_fd[i][1]);

                    i++;
                    cmd = cmd->next;

                    if (!run_in_bg)
                    {
                        ///that is run in foreground
                        if (wait(NULL) == -1)
                        { // wait for this child to exit
                            error_exit("wait pid ");
                        }
                    }

                    pid_t teesra_com = fork();
                    if (teesra_com == -1)
                    {
                        error_exit("sorry sir, aapka nahi chalega");
                    }
                    if (teesra_com == 0)
                    {
                        printf("\n===========process[%d] pid: %d process[%d] gid: %d===========\n", i, getpid(), i, getpgid(getpid()));
                        int temp[2];
                        pipe(temp);

                        if (i != 0)
                        { // attach read end of stdin to pipe for all processes except first
                            write(temp[1], buf, strlen(buf));
                            //////////////handling errors left
                            close(temp[1]);
                            if (dup2(temp[0], STDIN_FILENO) == -1)
                                error_exit("dup2 pipe read "); /////////// ls || wc , grep | some

                            close(temp[0]);
                        }
                        if (i < count - 1)
                        {
                            if (dup2(pipe_fd[i][1], STDOUT_FILENO) == -1)
                                error_exit("dup2 pipe read ");
                        }
                        repetitive(&cmd);

                        close_all_pipes(pipe_fd, count - 1);

                        if (execvp(cmd->argv[0], cmd->argv) == -1)
                        {
                            error_exit("execvp");
                        }
                    }
                    else
                    { ///in parent
                        if (i - 1 > 0)
                            close(pipe_fd[i - 1][0]); // close necessary pipes so that corresponding child can exit

                        close(pipe_fd[i][1]);

                        if (!run_in_bg)
                        {
                            ///that is run in foreground
                            if (wait(NULL) == -1)
                            { // wait for this child to exit
                                error_exit("wait pid ");
                            }
                        }
                    }
                }
            }
            else if (pipe_num == 3)
            {
                pipe_num = 1;
                pid_t pid = fork(); ///////// for first command after double pipe

                if (pid == -1)
                    error_exit("fork");

                if (pid == 0)
                {
                    printf("\n===========process[%d] pid: %d process[%d] gid: %d===========\n", i, getpid(), i, getpgid(getpid()));
                    int temp[2];
                    pipe(temp);

                    if (i != 0)
                    { // attach read end of stdin to pipe for all processes except first
                        write(temp[1], buf, strlen(buf));
                        //////////////handling errors left
                        close(temp[1]);
                        if (dup2(temp[0], STDIN_FILENO) == -1)
                            error_exit("dup2 pipe read "); /////////// ls || wc , grep | some

                        close(temp[0]);
                    }

                    repetitive(&cmd);

                    close_all_pipes(pipe_fd, count - 1);

                    if (execvp(cmd->argv[0], cmd->argv) == -1)
                    {
                        error_exit("execvp");
                    }
                }
                else
                { ///in parent
                    if (i - 1 > 0)
                        close(pipe_fd[i - 1][0]); // close necessary pipes so that corresponding child can exit

                    close(pipe_fd[i][1]);

                    i++;
                    cmd = cmd->next;

                    if (!run_in_bg)
                    {
                        ///that is run in foreground
                        if (wait(NULL) == -1)
                        { // wait for this child to exit
                            error_exit("wait pid ");
                        }
                    }

                    pid_t teesra_com = fork();
                    if (teesra_com == -1)
                    {
                        error_exit("sorry sir, aapka nahi chalega");
                    }
                    if (teesra_com == 0)
                    {
                        printf("\n===========process[%d] pid: %d process[%d] gid: %d===========\n", i, getpid(), i, getpgid(getpid()));
                        int temp[2];
                        pipe(temp);

                        if (i != 0)
                        { // attach read end of stdin to pipe for all processes except first
                            write(temp[1], buf, strlen(buf));
                            //////////////handling errors left
                            close(temp[1]);
                            if (dup2(temp[0], STDIN_FILENO) == -1)
                                error_exit("dup2 pipe read "); /////////// ls || wc , grep | some

                            close(temp[0]);
                        }

                        repetitive(&cmd);

                        close_all_pipes(pipe_fd, count - 1);

                        if (execvp(cmd->argv[0], cmd->argv) == -1)
                        {
                            error_exit("execvp");
                        }
                    }
                    else
                    { ///in parent
                        if (i - 1 > 0)
                            close(pipe_fd[i - 1][0]); // close necessary pipes so that corresponding child can exit

                        close(pipe_fd[i][1]);

                        i++;
                        cmd = cmd->next;

                        if (!run_in_bg)
                        {
                            ///that is run in foreground
                            if (wait(NULL) == -1)
                            { // wait for this child to exit
                                error_exit("wait pid ");
                            }
                        }

                        pid_t teesra_com = fork();
                        if (teesra_com == -1)
                        {
                            error_exit("sorry sir, aapka nahi chalega");
                        }
                        if (teesra_com == 0)
                        {
                            printf("\n===========process[%d] pid: %d process[%d] gid: %d===========\n", i, getpid(), i, getpgid(getpid()));
                            int temp[2];
                            pipe(temp);

                            if (i != 0)
                            { // attach read end of stdin to pipe for all processes except first
                                write(temp[1], buf, strlen(buf));
                                //////////////handling errors left
                                close(temp[1]);
                                if (dup2(temp[0], STDIN_FILENO) == -1)
                                    error_exit("dup2 pipe read "); /////////// ls || wc , grep | some

                                close(temp[0]);
                            }
                            if (i < count - 1)
                            {
                                if (dup2(pipe_fd[i][1], STDOUT_FILENO) == -1)
                                    error_exit("dup2 pipe read ");
                            }
                            repetitive(&cmd);

                            close_all_pipes(pipe_fd, count - 1);

                            if (execvp(cmd->argv[0], cmd->argv) == -1)
                            {
                                error_exit("execvp");
                            }
                        }
                    }
                }
            }
        }
        close_all_pipes(pipe_fd, count - 1);
        exit(0); //temp child exit  (named grandchild ) we have made wrong naming convention
    }
    else
    {
        //main parent process
        int status;
        if(!run_in_bg)
        {
            //waitpid(grandchild,&status,0);
            wait(NULL);
            if (tcsetpgrp(STDIN_FILENO, getpgid(getpid())) < 0)
            {
                perror("tcsetpgrp failed");
            }
        }
    }
}
char *substr(const char *src, int m, int n)
{
    // get length of the destination string
    int len = n - m;

    // allocate (len + 1) chars for destination (+1 for extra null character)
    char *dest = (char *)malloc(sizeof(char) * (len + 1));

    // extracts characters between m'th and n'th index from source string
    // and copy them into the destination string
    for (int i = m; i < n && (*(src + i) != '\0'); i++)
    {
        *dest = *(src + i);
        dest++;
    }

    // null-terminate the destination string
    *dest = '\0';

    // return the destination string
    return dest - len;
}

struct cmd_pipeline *get_pipeline_from_array(int index)
{
    int hash = hashValue(index);
    if (sc_array[hash] == NULL)
    {
        error_exit("no such command");
    }
    HashNode *temp = sc_array[hash]->head;
    while (temp)
    {
        if (temp->index == index)
        {
            return temp->pipeline;
        }
        else
        {
            temp = temp->next;
        }
    }
    error_exit("no such command");
}

void INT_Handler(int sig_number)
{
    printf("Enter command index : ");
    int index;
    char ws;
    scanf("%d", &index);
    int hashvalue = hashValue(index);
    cmd_pipeline *pipeline = get_pipeline_from_array(index);
    execute(pipeline);
    remove_commands(pipeline);
    printf("Please press enter to continue..\n");
}

void TOU_Handler(int sig_number)
{
    //empty
}

void assign_Handlers()
{
    signal(SIGINT, INT_Handler);
    signal(SIGTTOU, TOU_Handler);
}

int main()
{

    setvbuf(stdout, NULL, _IONBF, 0);
    input = NULL; // initialize input from shell to be NULL
    size_t size = 0;
    cmd_pipeline pipeline;
    pipeline.head = NULL;
    pipeline.tail = NULL;
    pipeline.num_of_cmds = 0;
    initialise_sc_array();
    assign_Handlers();
    for (;;)
    {
        printf("Shell> ");
        getline(&input, &size, stdin); //take input

        if (!input || !strcmp(input, "\n"))
            continue; // if no command is typed simply display console again

        if (!strcmp("sc", substr(input, 0, 2)))
        {
            printf("This is sc command \n");

            char *sctoken = NULL;
            int sc_size = 0;
            bool insert_sc = false;
            bool delete_sc = false;
            int sc_index;
            char *sc_cmd;
            char *result = (char *)malloc(sizeof(char) * 50);
            while ((sctoken = strsep(&input, " ")) != NULL)
            {
                if (!strcmp(sctoken, "sc"))
                {
                    continue;
                }
                else if (!strcmp(sctoken, "-i"))
                {
                    insert_sc = true;
                }
                else if (!strcmp(sctoken, "-d"))
                {
                    delete_sc = true;
                }
                else if (is_str_num(sctoken))
                {
                    sc_index = atoi(sctoken);
                }
                else
                {
                    //sc_cmd = sctoken;
                    strcat(result, sctoken);
                    strcat(result, " ");
                }
            }

            if (insert_sc)
            {
                create_pipeline(result, &pipeline);
                populate_array(sc_index, &pipeline);
            }
            else
            {
                delete_node(sc_index);
            }
            //sc command over
        }
        else
        {
            create_pipeline(input, &pipeline);
            printf("-----------------------------------\nparent pid = %d parent group id = %d \n-----------------------------------\n", getpid(), getpgid(getpid()));
            execute(&pipeline);
            remove_commands(&pipeline);
        }
    }
    return 0;
}