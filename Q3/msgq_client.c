#include <time.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/msg.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#define MAXSIZE 1024
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
    EXIT,
    SET_AUTO_DELETE
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

int main()
{
    time_t seconds;
    long t = -1;
    int UID;
    printf("Please enter your user id if registered else enter 0 : ");
    scanf("%d", &UID);
    if (UID == 0)
    {
        printf("step one\n");
        key_t welcoming_key = ftok("server.c", 120);
        printf("step two\n");
        int welcoming_msgid = msgget(welcoming_key, 0666 | IPC_CREAT); // welcoming queue
        printf("step three\n");
        seconds = time(NULL);
        MESSAGE *first = buildMessage(1, -1, -1, "", 7, welcoming_key, -1, seconds, t);
        printf("step 4\n");

        if (msgsnd(welcoming_msgid, first, sizeof(MESSAGE) - sizeof(long), 0) == -1)
            perror("Error in sending");
        sleep(1);
        printf("Registration request sent\n");

        msgrcv(welcoming_msgid, first, MAXSIZE, 2, 0);
        printf("Acknowledgement received!\nYour user id is %d\n", first->uid);
        UID = first->uid;
    }
    key_t key = ftok("client.c", UID);
    if (key == -1)
    {
        perror("ftok");
        exit(1);
    }
    int msgid = msgget(key, IPC_CREAT | 0666);
    if (msgid == -1)
    {
        perror("msgget");
        exit(1);
    }

    bool isRegistered = 0;
    int anythingRead = 0;
    int isPrinted = 0;
    char text[256];

    MESSAGE *rec_mes = (MESSAGE *)malloc(sizeof(MESSAGE));
    while (1)
    {
        int option;
        printf("Choose a service from the menu :\n");
        printf("0 - CREATE_GROUP\n1 - LIST_GROUP\n2 - JOIN_GROUP\n3 - SEND_PVT\n4 - SEND_GRP\n5 - RECV_MES_GRP\n6 - RECV_MES_PVT\n7 - REGISTER\n8 - EXIT\n9 - SET AUTO DELETE\n");
        scanf("%d", &option);
        char c;
        scanf("%c", &c);
        switch (option)
        {
        case CREATE_GROUP:;
            char gname[100];
            printf("enter group name:");
            scanf("%[^\n]", gname);

            seconds = time(NULL);
            MESSAGE *mes = buildMessage(1, UID, -1, gname, option, key, -1, seconds, t);
            if (msgsnd(msgid, mes, sizeof(MESSAGE) - sizeof(long), 0) == -1)
            {
                perror("error in sending message\n");
                exit(1);
            }
            msgrcv(msgid, mes, MAXSIZE, 2, 0);
            if (mes->gid == -1)
            {
                perror("unable to create group \n");
                exit(1);
            }
            else
            {
                printf("Successfully created the group %d\n", mes->gid);
            }
            break;

        case JOIN_GROUP:;
            int gid;
            printf("Enter group id : ");
            scanf("%d", &gid);
            seconds = time(NULL);
            MESSAGE *join_msg = buildMessage(1, UID, gid, "", option, key, -1, seconds, t);
            if (msgsnd(msgid, join_msg, sizeof(MESSAGE) - sizeof(long), 0) == -1)
            {
                perror("Error in sending message of join group");
            }
            printf("Sent joining request\n");
            msgrcv(msgid, join_msg, MAXSIZE, 2, 0);
            if (join_msg->gid == -1)
            {
                printf("You are already in the group %d\n", gid);
            }
            else
            {
                printf("Successfully joined the group %d\n", join_msg->gid);
            }
            break;
        case SEND_PVT:;
            int pvt_uid = 0;
            printf("Enter USER id to send message : ");
            scanf("%d", &pvt_uid);

            printf("Enter message to be sent\n");
            scanf(" %[^\n]", text);
            seconds = time(NULL);
            MESSAGE *pvt_msg = buildMessage(1, UID, -1, text, option, key, pvt_uid, seconds, t);
            printf("value of dur is %ld   ... %ld\n",pvt_msg->dur,t);

            if (msgsnd(msgid, pvt_msg, sizeof(MESSAGE), 0) == -1)
            {
                perror("Error in sending private message");
                exit(1);
            }

            printf("Private message request sent\n");

            break;
        case SEND_GRP:;
            printf("Enter group id to send message : ");
            scanf("%d", &gid);

            printf("Enter message to be sent\n");
            scanf(" %[^\n]", text);
            seconds = time(NULL);
            MESSAGE *grp_msg = buildMessage(1, UID, gid, text, option, key, -1, seconds, t);

            if (msgsnd(msgid, grp_msg, sizeof(MESSAGE), 0) == -1)
            {
                perror("Error in sending message to the group");
                exit(1);
            }
            printf("Group message request sent\n");

            break;
        case RECV_MES_GRP:;
            int rcv_gid;
            printf("Please enter the group id to read message from : ");
            scanf("%d", &rcv_gid);
            anythingRead = 0;
            isPrinted = 0;
            time_t dsec = time(NULL);
            //unblock msgrcv

            while (msgrcv(msgid, rec_mes, MAXSIZE, rcv_gid, 0 | IPC_NOWAIT) != -1)
            {
                //printf("Lets see\n dur = %ld , current message time diff = %ld \n", rec_mes->dur, dsec - rec_mes->seconds);

                if (rec_mes->dur == -1)
                {
                    //printf("Arre ruk ja kshitij %ld\n", dsec - rec_mes->seconds);
                    anythingRead = 1;
                    if (isPrinted == 0)
                    {
                        printf("Received a message from group id %ld\n", rec_mes->mtype);
                        isPrinted = 1;
                    }
                    printf("Sender of the message is %d\n", rec_mes->uid);
                    printf("Content : %s\n", rec_mes->text);
                }

                else
                {
                    if (dsec - rec_mes->seconds >= rec_mes->dur)
                    {
                        //printf("was greater than t=%ld. It is %ld\n", t, dsec - rec_mes->seconds);
                        printf("This message was deleted as you joined a bit late\n");
                    }
                    else
                    {
                        anythingRead = 1;
                        if (isPrinted == 0)
                        {
                            printf("Received a message from group id %ld\n", rec_mes->mtype);
                            isPrinted = 1;
                        }
                        //printf("was lesser than t=%ld. It is %ld\n", t, dsec - rec_mes->seconds);
                        printf("Sender of the message is %d\n", rec_mes->uid);
                        printf("Content : %s\n", rec_mes->text);
                    }
                }
            }
            if (anythingRead == 0)
            {
                printf("You have no new messages from this group\n");
            }
            break;

        case RECV_MES_PVT:;
            int rcv_uid;
            printf("Please enter user id to read the message from : ");
            scanf("%d", &rcv_uid);
            anythingRead = 0;
            isPrinted = 0;
            dsec = time(NULL);
            //unblock msgrcv

            while (msgrcv(msgid, rec_mes, MAXSIZE, rcv_uid, 0 | IPC_NOWAIT) != -1)
            {
                //printf("Lets see\n dur = %ld , current message time diff = %ld \n", rec_mes->dur, dsec - rec_mes->seconds);
                if (rec_mes->dur == -1)
                {
                    //printf("please ruk ja na %ld\n", dsec - rec_mes->seconds);
                    anythingRead = 1;
                    if (isPrinted == 0)
                    {
                        printf("Received a message from user id %ld\n", rec_mes->mtype);
                        isPrinted = 1;
                    }
                    printf("Content : %s\n", rec_mes->text);
                }
                else
                {
                    if (dsec - rec_mes->seconds >= rec_mes->dur)
                    {
                        //printf("was greater than t=%ld. It is %ld", t, dsec - rec_mes->seconds);
                        printf("This message was deleted as you joined a bit late\n");
                    }
                    else
                    {
                        anythingRead = 1;
                        if (isPrinted == 0)
                        {
                            printf("Received a message from user id %ld\n", rec_mes->mtype);
                            isPrinted = 1;
                        }
                        //printf("was lesser than t=%ld. It is %ld", t, dsec - rec_mes->seconds);
                        printf("Content : %s\n", rec_mes->text);
                    }
                }
            }

            if (anythingRead == 0)
            {
                printf("You have no new messages from this user\n");
            }

            break;
        case REGISTER:;
            printf("You have been registered already!\n");
            break;
            
        case LIST_GROUP:;
            seconds = time(NULL);
            MESSAGE *list_msg = buildMessage(1, UID, -1, "", option, key, -1, seconds, t);
            if (msgsnd(msgid, list_msg, sizeof(MESSAGE) - sizeof(long), 0) == -1)
                perror("Error in sending");
            printf("List groups request sent\n");

            msgrcv(msgid, list_msg, MAXSIZE, 2, 0);
            printf("Here is the list of groups:\n%s\n", list_msg->text);
            break;
        case EXIT:;
            printf("You are exiting!\n");
            exit(0);
        case SET_AUTO_DELETE:;

            printf("Please enter the auto delete duration in seconds: ");
            scanf("%ld", &t);
            printf("Any messages sent by you will be deleted if the receiver isn't active within %ld seconds\n", t);
            break;
        default:
            printf("Invalid option\n");
        }
    }
    return 0;
}