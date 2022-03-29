#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <tgmath.h>
#include <errno.h>
#include <signal.h>
#include <sys/stat.h>


typedef struct Record{
    unsigned short number;
    int pid;
}record;


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


int main(int argc, char*argv[])
{
    //Sprawdzenie napisane na podstawie kodu dostępnego w opisie funkcji fstat w manie
    struct stat sb;
    fstat(STDIN_FILENO,&sb);

    if((sb.st_mode & S_IFMT )!= S_IFIFO)
    {
        exit(20);
    }
    ////////////

    int rd, wr, flag;
    int unique=0;
    int count=0;
    int number=0;
    unsigned short num;
    unsigned short* tab;

    if(argc<2)
    {
        perror("Too few arguments!\n");
        exit(11);
    }
    else if (argc>2)
    {
        perror("Too many arguments!\n");
        exit(12);
    }
    else
    {
        number=optional(argv[1]);
    }

    //Tablica przechowująca unikatowe wartości
    tab=(unsigned short*)malloc(sizeof(unsigned short));

    while(count < number)
    {
        flag=0;

        rd = read(STDIN_FILENO,&num,2);
        if(rd == 0)
        {
            exit(20);
        }
    
        for(int i =0; i< unique; i++)
        {
            if(tab[i] == num)
            {
                flag=1;
                break;
            }
        }
        
        if(flag == 0)
        {
            tab[unique]=num;

            struct Record record={num,getpid()};

            wr=write(STDOUT_FILENO,&record,sizeof(struct Record));
            if(wr <= 0)
            {
                exit(21);
            }  
            unique++;  
        }
        count++;
    }

    int result = 10*(number-unique)/number;
   
    exit(result);
}
