#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <sys/wait.h>
#include <time.h>


//Poniższy fragment kodu  pochodzi z jednego z przykładów z Upela (zakładka Procesy)
#define NANOSEC 1000000000L
#define CONVERT(f) {(long)(f), ((f)-(long)(f))*NANOSEC}
struct timespec sleep_tm = CONVERT(0.48);
//////////

typedef struct Record{
    unsigned short number;
    int pid;
}record;

int number_of_alive;
int number_of_awards=0;

int optional(char* text)
{
    char*PtrEnd;

    int number = strtol(text,&PtrEnd,0);
    if(PtrEnd != NULL)
    {
        if( *(PtrEnd) =='M' && *(++PtrEnd) =='i' )
        {
            number*=1024*1024;
        }
        else if( *(PtrEnd) =='K' && *(++PtrEnd) =='i')
        {
            number*=1024;
        }
    }
    return number;
}


void child_process(char* w, int* read_pipe,int* write_pipe)
{
    int rd, wr, e;

    wr=dup2(write_pipe[0],STDIN_FILENO);
    if(wr==-1)
    {
        perror("Error in dup, write_pipe!\n");
        exit(EXIT_FAILURE);
    }

    rd=dup2(read_pipe[1],STDOUT_FILENO);
    if(rd==-1)
    {
        perror("Error in dup, read_pipe!\n");
        exit(EXIT_FAILURE);
    }

    close(read_pipe[0]);
    close(write_pipe[1]);
    close(read_pipe[1]);
    close(write_pipe[0]);
   
    e = execlp("./poszukiwacz","poszukiwacz",w, NULL);
    if(e == -1)
    {
        perror("Error in function: execlp!\n");
        exit(EXIT_FAILURE);
    }
}


void wpis_do_raportu(int pid,int fd_reports, int stan)
{
    struct timespec monotonic;
    int n;
    char text[41];

    clock_gettime(CLOCK_MONOTONIC,&monotonic);
    
    if(stan == 0)
    {
        n=sprintf(text,"%d zakończenie ",pid);
        number_of_alive--;
    } 
    else
    {
        n=sprintf(text,"%d utworzenie  ",pid);
        number_of_alive++;
    } 
    
    strcat(text,ctime(&monotonic.tv_sec));
    
    lseek(fd_reports,0,SEEK_END);
    int wr = write(fd_reports,text,strlen(text));
}


int kopiowanie_danych(int fd, int* pipe, int dif)
{
    if( dif > 128) dif=128;
    char* buffer=(char*)calloc(dif,sizeof(char));

    int rd = read(fd,buffer,dif);
    if(rd <0)
    {
        perror("Error reading from file!\n");
        exit(EXIT_FAILURE);
    }
   
    int wr = write(pipe[1],&buffer,dif);
    if(wr <=0)
    {
        perror("Error in writting to pipe!\n");
        exit(EXIT_FAILURE);
    }

    return rd;
}

void nowy_potomek(int* read_pipe, int* write_pipe, int fd_reports, char* w )
{
    int pid;
    switch( pid=fork())
    {
        case -1: perror("fork failed"); exit(1); break;
        case 0:  child_process(w,read_pipe,write_pipe);break;
        default: wpis_do_raportu(pid,fd_reports,1);
    }
}
       
int czytanie_od_poszukiwacza(int* read_pipe, int fd_awards)
{
    struct Record record;
    int number=0;

    int rd =read(read_pipe[0],&record,sizeof(struct Record));
    if(rd > 0)
    {
        lseek(fd_awards,record.number*sizeof(int),SEEK_SET);
        read(fd_awards,&number,sizeof(int));
        
        if(number == 0)
        {
            lseek(fd_awards,-1*sizeof(int),SEEK_CUR);
            write(fd_awards,&record.pid,sizeof(int));
            number_of_awards+=1;
        }
        
    }
    return rd;
}



int main(int argc, char*argv[])
{
    char c;
    char file[40];
    char awards[40];
    char reports[40];
    char w[10];
    char* PtrEnd;
    int s, p;
    
    while((c=getopt(argc,argv,"d:s:w:f:l:p:"))!=-1)
    {
        switch(c)
        {
            case 'd': strcpy(file,optarg); break;
            case 's': s=optional(optarg); break;
            case 'w': strcpy(w,optarg); break;
            case 'f': strcpy(awards,optarg); break;
            case 'l': strcpy(reports,optarg); break;
            case 'p': p=strtol(optarg,&PtrEnd,0); break;
            case '?': perror("Invalid option!\n"); break;
        }
    }

   //Wypełniam plik z osiągnięciami liczbą zer odpowiadającą zakresowi typu unsigned short
    int fd_awards = open(awards,O_WRONLY);
    if(fd_awards == -1)
    {
        perror("Error in open source file!\n");
        exit(EXIT_FAILURE);
    }
    
    pid_t* awards_tab=(pid_t*)calloc(65535,sizeof(pid_t));

    int wr = write(fd_awards,awards_tab,65535*4);
    if(wr < 0)
    {
        perror("Error in writting to awards file!\n");
        exit(EXIT_FAILURE);
    }
    close(fd_awards);

    int fd=open(file,O_RDONLY);
    if(fd == -1)
    {
        perror("Error in open source file!\n");
        exit(EXIT_FAILURE);
    }

    int fd_reports=open(reports,O_WRONLY);
    if(fd_reports== -1)
    {
        perror("Error in open reports file!\n");
        exit(EXIT_FAILURE);
    }

    fd_awards = open(awards,O_RDWR);
    if(fd_awards == -1)
    {
        perror("Error in open awards file!\n");
        exit(EXIT_FAILURE);
    }

    //Pisząc obsłgę pipów wzorowałem się na przykładach z The Linux Programming Interface, rozdział 44
    pid_t pid;
    int read_pipe[2];
    int write_pipe[2];
    
    if( (pipe(read_pipe) || pipe(write_pipe)) == -1)
    {
        perror("Error in pipe!\n");
        exit(EXIT_FAILURE);
    }

    //Polecenie zinterpretowałem w ten sposób, że program główny na początku tworzy "p" potomków, a potem uzupełnia ich w przeplocie z innymi działaniami
    for(int i=0; i<p; i++)
    {
        nowy_potomek(read_pipe, write_pipe, fd_reports, w );
    }

    //Na podstawie The Linux Programming Interface, rozdział 63
    if(fcntl(read_pipe[0],F_SETFL,O_NONBLOCK)<0)
    {
        perror(NULL);
        exit(EXIT_FAILURE);
    }
    ///////

    
    int count=0;
    int flag_read=0;
    int flag_dead=0;
    int status;
    int dead_pid;
    int licznik =90;
    s=s*2;

    
    while(1)
    {  
        if(count < s)
        {
            int dif = s - count;
            int a =kopiowanie_danych(fd,write_pipe, dif);
            if(a == 0)
            {
                printf("No data, %d numbers have been read.\n",count/2);
                count = s;
            } 
            count+=a;
        }
        //Zamykam koniec do którego pisze rodzic, wtedy gdy pipe będzie pusty w potomkach read zwróci -1
        else close(write_pipe[1]);

        int read =czytanie_od_poszukiwacza(read_pipe, fd_awards);
        if(read <0)
        {
            flag_read=1;
            if(count >= s && number_of_alive == 0)
            {
                break;
            } 
        } 
        

        //Wyłapywanie przypadków śmierci (waitpid) napisałem na podstawie The Linux Programming Interface, rozdział 26
        if((dead_pid = waitpid(-1,&status,WNOHANG)) > 0)
        {
            wpis_do_raportu(dead_pid,fd_reports,0);

            if(WEXITSTATUS(status) > 10) p--; 
            else
            {
                if(number_of_alive < p && number_of_awards < 49151)
                {
                    nowy_potomek(read_pipe, write_pipe,fd_reports,w);
                }
            }
        }
        else flag_dead=1;

        if(flag_read == 1 && flag_dead == 1 )
        {
            nanosleep(&sleep_tm,NULL);
            flag_read=0;
            flag_dead=0;
        }
    } 

    close(fd);
    close(fd_awards);
    close(fd_reports);

    exit(EXIT_SUCCESS);

}