#include <stdio.h>
#include <sys/types.h>
#include <sys/msg.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>

#define MAXSIZE 1024
#define MAXUSERSINGROUP 100
#define MAXGROUPS 100
#define MAXUSERS 100

enum purpose
{
    CREATE_GROUP,
    LIST_GROUP,
    JOIN_GROUP,
    SEND_PVT,
    SEND_GRP,
    RECV_MES_GRP,
    RECV_MES_PVT,
    REGISTER,
    EXIT
};

typedef struct messageBuf
{
    long mtype; /// 1 group // 2 user
    int uid;
    int gid;
    int rec_uid;
    char text[200];
    int option;
    int key;
    time_t seconds;
    long dur;
} MESSAGE;

MESSAGE *buildMessage(long mtype, int uid, int gid, char *text, int option, int key, int rec_uid, time_t seconds, long dur)
{
    MESSAGE *mes = (MESSAGE *)malloc(sizeof(MESSAGE));
    mes->mtype = mtype;
    mes->uid = uid;
    mes->gid = gid;
    strcpy(mes->text, text);
    mes->option = option;
    mes->key = key;
    mes->rec_uid = rec_uid;
    mes->seconds = seconds;
    mes->dur = dur;
    return mes;
}

typedef struct group
{
    int size;
    int users[MAXUSERSINGROUP];
    char groupname[20];
} GROUP;



typedef struct messagequeue
{
    int msgid;
    int uid;
} MESSAGEQUEUE;

MESSAGEQUEUE * messageQueues;
GROUP * Groups;

void printGroupMembers(int number_of_groups)
{
    for (int i = 0; i < number_of_groups; i++)
    {
        printf("groupname: %s\n", Groups[i].groupname);
        for (int j = 0; j < Groups[i].size; j++)
        {
            printf("user: %d\n", Groups[i].users[j]);
        }
    }
}

int main()
{
    int number_of_messageQueues = 0;
    int number_of_groups = 0;
    key_t welcoming_key = ftok("server.c", 120);
    int welcoming_msgid = msgget(welcoming_key, 0666 | IPC_CREAT); // welcoming queue

    MESSAGEQUEUE welcomeQueue;
    welcomeQueue.uid = 0;
    welcomeQueue.msgid = welcoming_msgid;

    messageQueues = (MESSAGEQUEUE *)malloc(sizeof(MESSAGEQUEUE) * MAXUSERS);
    Groups = (GROUP *)malloc(sizeof(GROUP) * MAXGROUPS);

    messageQueues[0] = welcomeQueue;
    number_of_messageQueues++;

    MESSAGE readMessage;
    time_t seconds;

    while (1)
    {
        MESSAGE *reply;
        GROUP g;
        int groupid;
        int userid;
        char buf[256];
        for (int i = 0; i < number_of_messageQueues; i++)
        {
            while (msgrcv(messageQueues[i].msgid, &readMessage, sizeof(MESSAGE), 1, IPC_NOWAIT) != -1)
            {
                switch (readMessage.option)
                {
                case CREATE_GROUP:;
                    strcpy(Groups[number_of_groups].groupname, readMessage.text);
                    Groups[number_of_groups].size = 1;
                    Groups[number_of_groups].users[Groups[number_of_groups].size - 1] = readMessage.uid;
                    number_of_groups++;
                    seconds = readMessage.seconds;
                    reply = buildMessage(2, -1, number_of_groups - 1 + 10, "Successfully created group", -1, welcoming_key, -1, seconds, readMessage.dur);
                    if (msgsnd(messageQueues[i].msgid, reply, sizeof(MESSAGE) - sizeof(long), 0) == -1)
                        perror("Error in sending acknowledgment");
                    printGroupMembers(number_of_groups);
                    break;
                case LIST_GROUP:;
                    char *replyString = (char *)malloc(1024 * sizeof(char));
                    memset(replyString, 0, 1024);

                    char suffix[50];
                    for (int j = 0; j < number_of_groups; j++)
                    {
                        sprintf(suffix, "%s %d\n", Groups[j].groupname, (10 + j));
                        replyString = strcat(replyString, suffix);
                    }
                    seconds = readMessage.seconds;
                    reply = buildMessage(2, -1, -1, replyString, -1, welcoming_key, -1, seconds, readMessage.dur);
                    if (msgsnd(messageQueues[i].msgid, reply, sizeof(MESSAGE) - sizeof(long), 0) == -1)
                        perror("Error in sending acknowledgment");
                    break;
                case JOIN_GROUP:
                    groupid = readMessage.gid;
                    userid = readMessage.uid;
                    seconds = readMessage.seconds;
                    int isPresent = 0;
                    for (int j = 0; j < Groups[groupid - 10].size; j++)
                    {
                        if (Groups[groupid - 10].users[j] == userid)
                        {
                            isPresent = 1;
                            break;
                        }
                    }
                    if (isPresent)
                    {
                        reply = buildMessage(2, -1, -1, "Already in group!", -1, welcoming_key, -1, seconds, readMessage.dur);
                        if (msgsnd(messageQueues[i].msgid, reply, sizeof(MESSAGE) - sizeof(long), 0) == -1)
                            perror("Error in sending acknowledgment");
                    }
                    else
                    {
                        Groups[groupid - 10].users[Groups[groupid - 10].size] = userid;
                        Groups[groupid - 10].size++;
                        reply = buildMessage(2, -1, groupid, "Successfully added to group!", -1, welcoming_key, -1, seconds, readMessage.dur);
                        if (msgsnd(messageQueues[i].msgid, reply, sizeof(MESSAGE) - sizeof(long), 0) == -1)
                            perror("Error in sending acknowledgment");
                    }
                    // printf("Group ID : %d\n", groupid);
                    // for(int j = 0; j < Groups[groupid-10].size; j++) {
                    //     printf("user id %d, ", Groups[groupid-10].users[j]);
                    // }
                    // printf("\n");
                    // printf("case 2 in server\n");
                    printGroupMembers(number_of_groups);
                    break;
                case SEND_GRP:
                    groupid = readMessage.gid;
                    userid = readMessage.uid;

                    strcpy(buf, readMessage.text);
                    seconds = readMessage.seconds;
                    for (int j = 0; j < Groups[groupid - 10].size; j++)
                    {
                        if (Groups[groupid - 10].users[j] == userid)
                        {
                            continue;
                        }
                        reply = buildMessage(groupid, userid, groupid, buf, -1, welcoming_key, -1, seconds, readMessage.dur);
                        if (msgsnd(messageQueues[Groups[groupid - 10].users[j] - 100].msgid, reply, sizeof(MESSAGE) - sizeof(long), 0) == -1)
                            perror("Error in sending notification");
                    }
                    // printf("case 3 in server\n");
                    break;
                case REGISTER:;
                    MESSAGEQUEUE mq;
                    mq.uid = number_of_messageQueues;
                    int newKey = ftok("client.c", number_of_messageQueues + 100);

                    mq.msgid = msgget(newKey, 0666 | IPC_CREAT);

                    messageQueues[number_of_messageQueues] = mq;

                    seconds = readMessage.seconds;
                    reply = buildMessage(2, number_of_messageQueues + 100, -1, "Successfully registered", -1, newKey, -1, seconds, readMessage.dur);
                    if (msgsnd(welcoming_msgid, reply, sizeof(MESSAGE) - sizeof(long), 0) == -1)
                        perror("Error in sending acknowledgment");
                    // for(int j = 0; j < number_of_messageQueues; j++) {
                    //     printf("msg id %d , uid %d\n", messageQueues[j].msgid, messageQueues[j].uid);
                    // }
                    // printf("case 4 in server\n");
                    number_of_messageQueues++;
                    break;
                case SEND_PVT:;
                    //to be completed
                    int sender_userid = readMessage.uid;
                    int receiver_uid = readMessage.rec_uid;
                    seconds = readMessage.seconds;

                    strcpy(buf, readMessage.text);
                    reply = buildMessage(sender_userid, -1, sender_userid, buf, -1, welcoming_key, receiver_uid, seconds, readMessage.dur);
                    
                    //printf(" pvt msg value of reply dur is %ld   ..... read msg : %ld in server  ...\n",reply->dur,readMessage.dur);

                    if (msgsnd(messageQueues[receiver_uid - 100].msgid, reply, sizeof(MESSAGE) - sizeof(long), 0) == -1)
                        perror("Error in sending notification");
                    printf("server : msg send \n");
                    // printf("case 3 in server\n");

                    break;
                default:
                    printf("Invalid option received at server!\n");
                }
            }
        }
    }
}