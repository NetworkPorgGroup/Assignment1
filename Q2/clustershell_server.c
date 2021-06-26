#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdbool.h>
#include <ctype.h>
#define BUFSIZE 32

#define NUM_OF_CMDS 20
#define NUM_OF_ARGS 20
#define __PENDING_REQ__ 5
#define __M_CLIENTS__ 10
#define __MAX_PIPES__ 100
#define __COMMAND_MAX_SIZE__ 1024
#define __MAX_OUTPUT__ 1024
#define __PORT_CLIENT__ 4000
#define __PORT_SERVER__ 5000

#define __EXIT___ "exit"
#define __NODES__ "nodes"

bool active_clients[__M_CLIENTS__] = {false};
struct cmd_out
{
    char *out;
    int nbytes;
};

#define ARG_LENGTH 50

struct command //same structue used as in q1, with some modification
{
    int ip_add_index;        //-1 : same client, -2: *, any other number : index in array
    int argc;                // number of arguements to command  ls || wc ,wc
    char *argv[NUM_OF_ARGS]; // list of arguement
    bool input_redirect;
    bool output_redirect;
    bool output_append;
    char input_file[ARG_LENGTH];
    char output_file[ARG_LENGTH];
    struct command *next; // next command in pipeline
};

typedef struct
{
    struct command *first; // first command in linkedlist
    struct command *last;  // last command in linkedlist
    int num_of_cmds;       // number of cmds in linkedlist
} cmd_pipeline;

int num_cmds;
char *input;

int size = 0;
/////
char **pointer_to_file = NULL;
char **read_file(const char *filename)
{
    char **arr = malloc(sizeof(char *) * (__M_CLIENTS__ + 1));
    FILE *fp = fopen(filename, "r");
    if (fp == NULL)
    {
        printf("Error opening the config file. Exiting...\n");
        exit(0);
    }
    char name_buf[20], ip_buf[20];
    int i = 0;
    while ((fscanf(fp, " %s", name_buf)) != EOF)
    {
        fscanf(fp, " %s", ip_buf);
        arr[i++] = strdup(ip_buf);
    }
    arr[i] = NULL;

    return arr;
}

void CONFIG_FREE(char **arr)
{
    char **x = arr;
    while (*x)
    {
        free(*x);
        ++x;
    }
    free(arr);
}

int setup_server(int serv_port)
{
    int serv_sock;

    struct sockaddr_in serv_addr;

    if ((serv_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        printf("Error creating socket\n");
        exit(0);
    }

    if (setsockopt(serv_sock, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) < 0)
    {
        printf("Error sock opt\n");
        exit(0);
    }

    bzero(&serv_addr, sizeof(serv_addr));

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(serv_port);

    if (bind(serv_sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        printf("Error binding socket: %d\n", errno);
        exit(0);
    }

    if (listen(serv_sock, __PENDING_REQ__) < 0)
    {
        printf("Error listening socket: %d\n", errno);
        exit(0);
    }

    return serv_sock;
}

int setup_client(char *serv_ip, int serv_port)
{
    struct sockaddr_in serv_addr;
    bzero(&serv_addr, sizeof(serv_addr));

    serv_addr.sin_family = AF_INET;
    if (serv_ip == NULL)
        serv_addr.sin_addr.s_addr = INADDR_ANY;
    else
        inet_aton(serv_ip, &(serv_addr.sin_addr));
    serv_addr.sin_port = htons(serv_port);

    int con_fd = socket(AF_INET, SOCK_STREAM, 0);

    int connect_res = connect(con_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
    if (connect_res == -1)
    {
        printf("Error in connecting to client.\n");
        return -1;
        //exit(0);
    }

    return con_fd;
}
void error_exit(char *err_msg)
{
    perror(err_msg);
    exit(EXIT_FAILURE);
}
void command_insert(cmd_pipeline *pipeline, struct command *cmd)
{
    if (pipeline->first == NULL)
    {
        pipeline->first = cmd;
        pipeline->last = cmd;
        return;
    }

    pipeline->last->next = cmd;
    pipeline->last = cmd;
}
void populate_command(struct command **cmd_ptr)
{
    *cmd_ptr = (struct command *)malloc(sizeof(struct command));

    if (*cmd_ptr == NULL)
        error_exit("malloc:");

    (*cmd_ptr)->argc = 0;

    strcpy((*cmd_ptr)->input_file, "");
    strcpy((*cmd_ptr)->output_file, "");
    (*cmd_ptr)->ip_add_index = -1;
    (*cmd_ptr)->input_redirect = 0;
    (*cmd_ptr)->output_redirect = 0;
    (*cmd_ptr)->output_append = 0;
    (*cmd_ptr)->next = NULL;

    for (int i = 0; i < NUM_OF_ARGS; i++)
        (*cmd_ptr)->argv[i] = NULL;
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
void parser_function(char *input, cmd_pipeline *pipeline)
{

    //printf("Inside parse cmd String is %s \n",input);
    if (!strcmp(input, ""))
    {
        //("emptyyyy");
        struct command *cmd_ptr;
        populate_command(&cmd_ptr);
        size++;
        command_insert(pipeline, cmd_ptr);
        return;
    }
    int temp_space = 0;
    while (input[temp_space] == ' ')
    {
        temp_space++;
    }
    input = input + temp_space;

    char *tk;
    while ((tk = strsep(&input, ",")) != NULL)
    {
        //  printf("value of tk is %s",tk);
        struct command *cmd_ptr;
        populate_command(&cmd_ptr);
        int len = strlen(tk);
        char *str = (char *)malloc(sizeof(char) * (len + 1)); // to store current arguement
                                                              // printf("YOYO first %c \n",tk[0]);
        if (str == NULL)
            error_exit("malloc str");
        //     ///checking for n0. ,n1. etc or n*.
        char temp_ch = tk[0];
        // if(!strcmp(temp_ch,'n')){
        if (temp_ch == 'n')
        {
            // printf("n dected \n");
            int j;
            if (tk[1] == '*')
            {
                // printf("All detected *\n");
                (cmd_ptr)->ip_add_index = -2;
                j = 2;
            }
            else
            {
                int num = 0;
                j = 1;

                while (tk[j] != '.')
                {
                    int temp = tk[j] - '0';
                    num = num * 10 + temp;
                    j++;
                }
                //  printf("Dot detec ted at index %d and value of num is %d \n",j,num);
                (cmd_ptr)->ip_add_index = num;
            }
            int len_tk = strlen(tk);
            //tk  = substr(tk,j+1,len_tk-(j+1)); //n1.ls
            tk = tk + j + 1;
            // printf("String afte cutting is %s \n",tk);
        }
        char prev_str[ARG_LENGTH]; // to store previous arguement
        int arg_size = 0;          // size of current arguement
        int num_args = 0;          // num of args present in cmd

        (cmd_ptr)->argv[num_args] = (char *)malloc(sizeof(char) * (strlen(tk) + 1));
        strcpy((cmd_ptr)->argv[num_args], tk);
        num_args++;
        (cmd_ptr)->argv[num_args] = NULL; // mark the end of arguement vector by a NULL pointer
        (cmd_ptr)->argc = num_args;
        free(str);
        size++;

        command_insert(pipeline, cmd_ptr);
    }
}

void create_pipeline(char *input, cmd_pipeline *pipeline)
{
    // printf("inside create pipeline\n");
    char *token;

    size = 0;

    while ((token = strsep(&input, "|")) != NULL)
    {   // tokenize by | operator, u get one command
        // printf("calling parse cmd with token as %s \n",token);

        parser_function(token, pipeline); // fill command structure   wc , wc
    }

    num_cmds = size;
    pipeline->num_of_cmds = num_cmds;
}

void execute_commands(cmd_pipeline *pipeline, char *ip_caller, int clnt_sock, int node_number)
{
    struct command *cmd_ptr = pipeline->first;

    char *input_to_cmd = (char *)malloc(sizeof(char) * __MAX_OUTPUT__);
    input_to_cmd[0] = '\0';
    int input_to_cmd_size = 0;

    char *t_buffer = (char *)malloc(sizeof(char) * __MAX_OUTPUT__);
    t_buffer[0] = '\0';
    int t_buffer_size = 0;

    int blank = 0;
    int times = 0;
    for (int i = 0; i < pipeline->num_of_cmds; i++)
    {

        ////////for loop started for command linked list/////
        if (i == 0)
        {
            strcpy(input_to_cmd, "#first_node");
            input_to_cmd[11] = '\0';
            input_to_cmd_size = strlen(input_to_cmd) + 1;
        }
        if (cmd_ptr->argv[0] == NULL)
        {
            // printf("blank detected \n");
            blank++;
            cmd_ptr = cmd_ptr->next;
            continue;
        }
        if (blank == 1)
        {
            //printf("\n *************1 blank ******\n");
            times = 1;
        }
        else if (blank == 2)
        {
            times = 2;
            //printf("\n *************2 blank ******\n");
        }
        /// creating result to send to client child
        int response_size = 0;
        //printf("inside execute cmds with cmds %d \n",pipeline->num_of_cmds);
        char *result = (char *)malloc(sizeof(char) * __COMMAND_MAX_SIZE__);
        int j;
        for (j = 0; j < cmd_ptr->argc - 1; j++)
        {
            strcat(result, cmd_ptr->argv[j]);
            strcat(result, " ");
            response_size += strlen(cmd_ptr->argv[j]) + 1;
        }
        if (j == cmd_ptr->argc - 1)
        {
            strcat(result, cmd_ptr->argv[j]);

            response_size += strlen(cmd_ptr->argv[j]);
        }
        result[response_size] = '\0';

        //response_size+=1;
        //printf("cmd sending to client child server is %s \n",result);
        // result created

        int con_fd;
        if (cmd_ptr->ip_add_index == -1)
        {
            ///running on local machine
            con_fd = setup_client(ip_caller, __PORT_CLIENT__ + node_number);
            printf("client setup done");
            int nbytes = write(con_fd, result, response_size);
            if (nbytes != response_size)
            {
                printf("Error in writing. Exiting...\n");
            }
            //  printf("\n\nsending input to %d cmd data %s\n\n",input_to_cmd_size,input_to_cmd);
            sleep(1);
            int nbytes2 = write(con_fd, input_to_cmd, input_to_cmd_size);
            if (nbytes2 != input_to_cmd_size)
            {
                printf("Error in writing. Exiting...\n");
            }
            //    char input_buf[__MAX_OUTPUT__ + 1] = {0};
            //    int input_buf_size = 0;
            if (times == 0)
            {

                //  printf("\n *************times 0 ******\n");
                input_to_cmd_size = read(con_fd, input_to_cmd, __MAX_OUTPUT__ + 1);
                input_to_cmd[input_to_cmd_size] = '\0';
                if (strcmp("#Error in command entered\n", input_to_cmd) == 0)
                {
                    char error_buf[50] = "Error in command entered: %d\n";
                    write(clnt_sock, error_buf, strlen(error_buf));
                    close(con_fd);
                    return;
                }
                // printf("Pipe out: %s\n", input_to_cmd);
                if (i == pipeline->num_of_cmds - 1)
                {
                    write(clnt_sock, input_to_cmd, input_to_cmd_size);
                }
            }
            else
            {
                // printf("\n *************times not 0 ******\n");
                t_buffer_size = read(con_fd, t_buffer, __MAX_OUTPUT__ + 1);
                t_buffer[t_buffer_size] = '\0';
                times--;
                blank = 0;
                if (strcmp("#Error in command entered\n", t_buffer) == 0)
                {
                    char error_buf[50] = "Error in command entered: %d\n";
                    write(clnt_sock, error_buf, strlen(error_buf));
                    close(con_fd);
                    return;
                }
                // printf("Pipe out: %s\n", t_buffer);
                if (i == pipeline->num_of_cmds - 1)
                {
                    write(clnt_sock, t_buffer, t_buffer_size);
                }
            }

            //
            close(con_fd);
            cmd_ptr = cmd_ptr->next;
        }
        else if (cmd_ptr->ip_add_index == -2)
        {
            ///* detected
            bool first = true;
            //  printf("* detected \n");
            char *input_to_cmd2 = (char *)malloc(sizeof(char) * __MAX_OUTPUT__);
            input_to_cmd2[0] = '\0';
            int input_to_cmd2_size = 0;
            char **cur_ip = pointer_to_file;
            int temp_index = 1;
            while (*cur_ip)
            {
                con_fd = setup_client(*cur_ip, __PORT_CLIENT__ + temp_index);
                if (con_fd < 0)
                {
                    ++cur_ip;
                    ++temp_index;
                    continue;
                }
                //  printf("client setup done for index %d \n",temp_index);
                int nbytes = write(con_fd, result, response_size);
                if (nbytes != response_size)
                {
                    char error_buf[50] = "Error in connecting node number ";
                    char e_str[10];
                    sprintf(e_str, "%d", temp_index);
                    strcat(error_buf, e_str);
                    write(clnt_sock, error_buf, strlen(error_buf));
                    close(con_fd);

                    printf("Error in writing. Exiting...\n");
                    return;
                }
                //  printf("\n\nsending input to %d cmd data %s\n\n",input_to_cmd_size,input_to_cmd);
                sleep(1);
                int nbytes2 = write(con_fd, input_to_cmd, input_to_cmd_size);
                if (nbytes2 != input_to_cmd_size)
                {
                    //printf("Error in writing. Exiting...\n");

                    char error_buf[50] = "Error in writing. Exiting...\n";
                    // char e_str[10];
                    // sprintf(e_str, "%d", cmd_ptr->ip_add_index);
                    // strcat(error_buf,e_str);
                    write(clnt_sock, error_buf, strlen(error_buf));
                    close(con_fd);

                    //	printf("Error in writing. Exiting...\n");
                    return;
                }
                input_to_cmd2_size = read(con_fd, input_to_cmd2, __MAX_OUTPUT__ + 1);
                close(con_fd);
                input_to_cmd2[input_to_cmd2_size] = '\0';
                if (strcmp("#Error in command entered\n", input_to_cmd2) == 0)
                {
                    char error_buf[50] = "Error in command entered: %d\n";
                    write(clnt_sock, error_buf, strlen(error_buf));
                    close(con_fd);
                    return;
                }
                if (times == 0)
                {
                    if (first == true)
                    {
                        strcpy(input_to_cmd, input_to_cmd2);
                        input_to_cmd_size = input_to_cmd2_size + 1;
                        first = false;
                    }
                    else
                    {
                        strcat(input_to_cmd, input_to_cmd2);
                        input_to_cmd_size += input_to_cmd2_size + 1;
                    }
                }
                else
                {
                    times--;
                    blank = 0;
                }

                ++cur_ip;
                ++temp_index;
            }

            // printf("Pipe out: %s\n", input_to_cmd);
            if (i == pipeline->num_of_cmds - 1)
            {
                write(clnt_sock, input_to_cmd, input_to_cmd_size);
            }
            //

            cmd_ptr = cmd_ptr->next;
        }
        else
        {
            con_fd = setup_client(pointer_to_file[cmd_ptr->ip_add_index - 1], __PORT_CLIENT__ + cmd_ptr->ip_add_index);
            //  printf("client setup done");
            int nbytes = write(con_fd, result, response_size);
            if (nbytes != response_size)
            {
                char error_buf[50] = "Error in connecting node number";
                char e_str[10];
                sprintf(e_str, "%d", cmd_ptr->ip_add_index);
                strcat(error_buf, e_str);
                write(clnt_sock, error_buf, strlen(error_buf));
                close(con_fd);

                //	printf("Error in writing. Exiting...\n");
                return;
            }
            //  printf("\n\nsending input to %d cmd data %s\n\n",input_to_cmd_size,input_to_cmd);
            sleep(1);
            int nbytes2 = write(con_fd, input_to_cmd, input_to_cmd_size);
            if (nbytes2 != input_to_cmd_size)
            {
                //printf("Error in writing. Exiting...\n");

                char error_buf[50] = "Error in writing. Exiting...\n";
                // char e_str[10];
                // sprintf(e_str, "%d", cmd_ptr->ip_add_index);
                // strcat(error_buf,e_str);
                write(clnt_sock, error_buf, strlen(error_buf));
                close(con_fd);

                //	printf("Error in writing. Exiting...\n");
                return;
            }
            //    char input_buf[__MAX_OUTPUT__ + 1] = {0};
            //    int input_buf_size = 0;
            if (times == 0)
            {

                // printf("\n *************times 0 ******\n");
                input_to_cmd_size = read(con_fd, input_to_cmd, __MAX_OUTPUT__ + 1);

                input_to_cmd[input_to_cmd_size] = '\0';
                if (strcmp("#Error in command entered\n", input_to_cmd) == 0)
                {
                    char error_buf[50] = "Error in command entered: %d\n";
                    write(clnt_sock, error_buf, strlen(error_buf));
                    close(con_fd);
                    return;
                }
                // printf("Pipe out: %s\n", input_to_cmd);
                if (i == pipeline->num_of_cmds - 1)
                {
                    write(clnt_sock, input_to_cmd, input_to_cmd_size);
                }
            }
            else
            {
                // printf("\n *************times not 0 ******\n");
                t_buffer_size = read(con_fd, t_buffer, __MAX_OUTPUT__ + 1);
                t_buffer[t_buffer_size] = '\0';
                times--;
                blank = 0;
                if (strcmp("#Error in command entered\n", t_buffer) == 0)
                {
                    char error_buf[50] = "Error in command entered: %d\n";
                    write(clnt_sock, error_buf, strlen(error_buf));
                    close(con_fd);
                    return;
                }
                //  printf("Pipe out: %s\n", t_buffer);
                if (i == pipeline->num_of_cmds - 1)
                {
                    write(clnt_sock, t_buffer, t_buffer_size);
                }
            }

            //
            close(con_fd);
            cmd_ptr = cmd_ptr->next;
        }
    }
}
void remove_commands(cmd_pipeline *pipeline)
{
    struct command *tmp;
    if (!pipeline)
        return;
    for (tmp = pipeline->first; tmp != NULL; tmp = tmp->next)
    {
        free(tmp);
    }
    pipeline->first = NULL;
    pipeline->last = NULL;
    pipeline->num_of_cmds = 0;
}

void exe_nodes(char *ip_caller, int clnt_sock)
{
    char *input_to_cmd = (char *)malloc(sizeof(char) * __MAX_OUTPUT__);
    input_to_cmd[0] = '\0';
    int input_to_cmd_size = 0;
    char **cur_ip = pointer_to_file;
    int temp_index = 1;
    strcpy(input_to_cmd, "Printing All Active Nodes with ip address\n");
    input_to_cmd_size = strlen(input_to_cmd) + 1;
    while (*cur_ip)
    {
        int con_fd = setup_client(*cur_ip, __PORT_CLIENT__ + temp_index);
        if (con_fd < 0)
        {
            ++cur_ip;
            ++temp_index;
            continue;
        }
        close(con_fd);
        //  printf("client setup done for index %d \n",temp_index);
        //  printf("Node number is %d , ip address is %s\n",temp_index,*cur_ip);
        strcat(input_to_cmd, "Node number is ");
        //char buffer [33];
        //itoa(temp_index,buffer,10);

        char str[33];
        snprintf(str, sizeof(str), "%d", temp_index);
        strcat(input_to_cmd, str);
        strcat(input_to_cmd, "  ip address is ");
        strcat(input_to_cmd, *cur_ip);
        strcat(input_to_cmd, " \n");
        //strcat(input_to_cmd,)
        ++cur_ip;
        ++temp_index;
    }

    input_to_cmd_size = strlen(input_to_cmd) + 1;

    write(clnt_sock, input_to_cmd, input_to_cmd_size);
    printf("All Active nodes Printed successfully\n");
    //close(clnt_sock);
}

int find_node_number(char *str)
{
    char **cur_ip = pointer_to_file;
    int node_number = 0;
    while (*cur_ip)
    {
        if (strcmp(*cur_ip, str) == 0 && active_clients[node_number] == false)
        {
            active_clients[node_number] = true;
            return node_number;
        }
        cur_ip++;
        node_number++;
    }

    return -1;
}

int check_node_number(int node_number, char *str)
{
    // printf("inside node number\n");
    char **cur_ip = pointer_to_file;
    if (node_number > __M_CLIENTS__)
    {
        return -1;
    }
    if (strcmp(cur_ip[node_number - 1], str) == 0)
    {
        return 0;
    }
    //  printf(" return inside node number\n");
    return -1;
};

int main()
{
    char file_addr[1024];
    printf("enter config file path:\n");
    scanf("%s", file_addr);
    char blank_c;
    scanf("%c", &blank_c);
    //initialize_active_client_array();
    cmd_pipeline pipeline;
    pipeline.first = NULL;
    pipeline.last = NULL;
    pipeline.num_of_cmds = 0;

    // Setup server
    int clnt_sock, serv_sock = setup_server(__PORT_SERVER__);
    struct sockaddr_in clnt_addr;
    // Server in listening mode

    pointer_to_file = read_file(file_addr);

    while (true)
    {
        int clnt_len = sizeof(clnt_addr);

        if ((clnt_sock = accept(serv_sock, (struct sockaddr *)&clnt_addr, &clnt_len)) < 0)
        {
            printf("Error listening socket: %d\n", errno);
            exit(0);
        }

        printf("* Handling client %s\n", inet_ntoa(clnt_addr.sin_addr));
        // char * temp

        // int n_number = find_node_number(inet_ntoa(clnt_addr.sin_addr))+1;
        //  printf("node number is %d",n_number);
        pid_t ch_handler = fork();

        if (ch_handler < 0)
        {
            printf("Error handling the forking. Exiting...\n");
            exit(0);
        }

        else if (ch_handler == 0)
        {
            close(serv_sock);
            int node_number;
            int n2 = read(clnt_sock, &node_number, __COMMAND_MAX_SIZE__);
            //write(clnt_sock,&node_number,sizeof(node_number));
            // printf(" yo node number received is %d \n",node_number);
            //  printf("calling check noe");
            int check_n = 0;
            check_n = check_node_number(node_number, inet_ntoa(clnt_addr.sin_addr));
            //  printf("valus of check_n is %d",check_n);
            write(clnt_sock, &check_n, sizeof(check_n));
            while (true)
            {
                if (check_n < 0)
                {
                    break;
                }
                char cmd_buf[__COMMAND_MAX_SIZE__];
                int n = read(clnt_sock, cmd_buf, __COMMAND_MAX_SIZE__);
                if (n < 0)
                {
                    break;
                    exit(-1);
                }

                cmd_buf[n] = '\0';
                //  printf("* `%s` recieved yoyyo  from `%s`\n", cmd_buf, inet_ntoa(clnt_addr.sin_addr));
                //
                if (strcmp(cmd_buf, __NODES__) == 0)
                {
                    exe_nodes(inet_ntoa(clnt_addr.sin_addr), clnt_sock);
                    continue;
                }
                //  printf("calling pipeline  yo\n");

                create_pipeline(cmd_buf, &pipeline);
                //printf("pipeline crated \n");
                //printf("printing pipeline");
                // print_pipeline(&pipeline);
                // printf("executing cmds\n");
                execute_commands(&pipeline, inet_ntoa(clnt_addr.sin_addr), clnt_sock, node_number);
                // printf("cmds executed");
                remove_commands(&pipeline);
                if (strcmp(cmd_buf, __EXIT___) == 0)
                    break;
            }

            printf("closing connection from server");

            close(clnt_sock);
            exit(0);
        }
        else
        {
            // in parent
            close(clnt_sock);
        }
    }
    CONFIG_FREE(pointer_to_file);
    return 0;
}