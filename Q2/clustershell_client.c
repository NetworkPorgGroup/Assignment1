#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#define BUFSIZE 32

#define __PENDING_REQ__ 5
#define __M_CLIENTS__ 100

#define __COMMAND_MAX_SIZE__ 1024
#define __MAX_OUTPUT__ 1024
#define __PORT_CLIENT__ 4000
#define __PORT_SERVER__ 5000
#define __FILE_CONFIG__ "./CONFIG"
#define __EXIT__ "exit"
#define __NODES__ "nodes"

struct command_output
{
	char *out;
	int number_of_bytes;
};

void error_exit(char *err_msg)
{
	perror(err_msg);
	exit(EXIT_FAILURE);
}

int set_server(int serv_port)
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

int set_client(char *serv_ip, int serv_port)
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
		printf("Error in connecting to client. Exiting...\n");
		exit(0);
	}

	return con_fd;
}

struct command_output execute_command(char *cmd_inp, int cmd_inp_size, char *input_to_cmd, int input_to_cmd_size)
{

	char *cmd, *inp;
	cmd = cmd_inp;
	inp = cmd_inp;

	struct command_output output;

	if (cmd[0] == 'c' && cmd[1] == 'd' && cmd[2] == ' ')
	{

		char *path = cmd + 3;

		if (chdir(path) < 0)
		{
			printf("Error changing directory: %d\n", errno);
			char temp[50] = "Error changing directory\n";
			output.number_of_bytes = strlen(temp) + 1;
			output.out = temp;
			return output;
		}
		else
		{
			char temp[50] = "Directory changed successfully\n";
			output.number_of_bytes = strlen(temp) + 1;
			output.out = temp;
			return output;
		}
	}
	else
	{
		int p_r[2];
		pipe(p_r);

		if (!strcmp(input_to_cmd, "#first_node"))
		{
		}
		else
		{
			if (dup2(p_r[0], STDIN_FILENO) == -1)
			{
				error_exit("dup2 pipe read ");
			}
			write(p_r[1], input_to_cmd, input_to_cmd_size);
		}

		close(p_r[1]);

		FILE *fd = popen(cmd, "r");
		//printf("yess\n");
		if (fd == NULL)
		{
			printf("Error in command entered: %d\n", errno);
			char temp[50] = "Error in command entered\n";
			output.number_of_bytes = strlen(temp) + 1;
			output.out = temp;
			close(p_r[0]);
			return output;
		}
		char tmp_out[__MAX_OUTPUT__];

		output.number_of_bytes = read(fileno(fd), tmp_out, __MAX_OUTPUT__);
		pclose(fd);
		//("yes2");
		//printf("Output of cmd in client child cmd exe %s\n",tmp_out);
		if (output.number_of_bytes < 0)
		{
			printf("Error reading output from pipe...\n");
			exit(0);
		}
		if (output.number_of_bytes == 0)
		{
			//printf("Error in command entered: %d\n", errno);
			char temp[50] = "#Error in command entered\n";
			output.number_of_bytes = strlen(temp) + 1;
			output.out = temp;
			close(p_r[0]);
			return output;
		}
		//printf("yes3");
		tmp_out[output.number_of_bytes] = '\0';

		//printf("STDIN: %s\n", tmp_out);
		output.out = tmp_out;
		close(p_r[0]);
		//printf("yes4");
	}
	return output;
}
int node_number;
int con_fd = -1;

void ctrlc_handler(int num)
{
	char *str2 = "exit";

	if (con_fd != -1)
	{
		write(con_fd, str2, strlen(str2) + 1); // change size here
		close(con_fd);
	}
	exit(0);
}
int main()
{
	signal(SIGINT, ctrlc_handler);
	char buff_temp;
	printf("Enter the node number as mention in config file:\n");
	scanf("%d", &node_number);
	scanf("%c", &buff_temp);
	printf("Authenticating.....\n");
	//printf("ready to go");
	pid_t pid;

	pid = fork();
	if (pid < 0)
	{
		printf("error in creating child in client");
	}
	else if (pid == 0)
	{
		sleep(1);
		// Setup server ( client child as server)

		int clnt_sock, serv_sock = set_server(__PORT_CLIENT__ + node_number); // here client acts as the server

		for (;;)
		{
			struct sockaddr_in clnt_addr;
			int clnt_len = sizeof(clnt_addr);

			if ((clnt_sock = accept(serv_sock, (struct sockaddr *)&clnt_addr, &clnt_len)) < 0)
			{
				printf("Error listening socket: %d\n", errno);
				exit(0);
			}

			char buf[__COMMAND_MAX_SIZE__ + 1];
			size_t numread = read(clnt_sock, buf, __COMMAND_MAX_SIZE__);
			buf[numread] = '\0';
			//printf("Received first cmd on client child server: %s\n", buf);

			if (numread < 0)
			{
				printf("Error reading from server\n");
				break;
			}

			char input_to_cmd[__MAX_OUTPUT__ + 1];
			size_t input_to_cmd_size = read(clnt_sock, input_to_cmd, __COMMAND_MAX_SIZE__);
			input_to_cmd[input_to_cmd_size] = '\0';
			//printf("Received second cmd on client child server: %s\n", input_to_cmd);

			if (input_to_cmd_size < 0)
			{
				printf("Error reading from server\n");
				break;
			}
			if (strcmp(buf, "exit") == 0)
			{
				char *str2 = "CLOSING node ";
				write(clnt_sock, str2, strlen(str2) + 1); // change size here
				close(clnt_sock);
				close(serv_sock);
				break;
			}

			struct command_output output = execute_command(buf, numread, input_to_cmd, input_to_cmd_size);
			//			printf("here result: %s\n", output.out);
			write(clnt_sock, output.out, output.number_of_bytes); // change size here
			close(clnt_sock);
		}
	}
	else
	{
		con_fd = set_client(NULL, __PORT_SERVER__);
		//printf("sending node number to server");
		write(con_fd, &node_number, sizeof(node_number));
		//printf("node number send ");
		int result = -1;
		//printf("getting result");
		int n2 = read(con_fd, &result, __COMMAND_MAX_SIZE__);
		//printf("result obtained %d\n",result);
		if (result < 0)
		{
			printf("Your ip is not in Config file or a node number is already present with same ip and name...Exiting");
			printf("closing conection");
			close(con_fd);
			exit(0);
		}
		else
		{
			printf("Authorised\n");
		}
		//printf("Node number received is %d\n",node_number);

		// printf("Node number send\n");
		while (1)
		{
			printf("========================================\n");
			printf("Enter shell commands\n");
			printf("> ");

			char *cmd = (char *)malloc(sizeof(char) * __COMMAND_MAX_SIZE__);
			size_t size = __COMMAND_MAX_SIZE__;
			size_t numread_shell = getline(&cmd, &size, stdin);
			cmd[numread_shell - 1] = '\0'; // don't need \n at the end

			//printf("sending to server: %s\n", cmd);

			// send input command to the server
			write(con_fd, cmd, numread_shell - 1);
			//sleep(0.5);

			// read output of command from the server
			char buf[__MAX_OUTPUT__ + 1];

			size_t numread_server = read(con_fd, buf, __MAX_OUTPUT__);

			if (numread_server < 0)
			{
				printf("Error reading from server\n");
				break;
			}

			buf[numread_server] = '\0';

			printf("%s\n", buf);
			printf("========================================\n");
			if (strcmp(cmd, __EXIT__) == 0)
				break;
		}
		printf("closing conection");
		close(con_fd);
	}

	//main end
}

///sudo lsof -i -P -n | grep LISTEN