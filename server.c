#include "threadpool.h"
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/types.h>
#include <ctype.h>
#include <sys/stat.h>
#include<fcntl.h>


#define RFC1123FMT "%a,%d %b %Y %H:%M:%S GMT"

int to_dispatch(void *);

int is_dir(const char* );

void setFileType(char * , char* );

void print_response(int , int , char*);

void build_response(char* , char *, int);

void setCreateTimeOfFile(char * , int );

void setContentLength( char *  , char *  );

void setModificationTime(char *, char*);

char *get_mime_type(char *);



int main(int argc , char ** argv) {
    if(argc !=  4){
        printf("Usage: server <pool> <pool-size>\n");
        exit(1);
    }

    int wellcom_sock , client_sock , c ;
    struct sockaddr_in server , client;
    int n_port , n_max_request , n_pool_size , i ;

    if(isdigit(atoi(argv[1])) != 0 || isdigit(atoi(argv[2])) != 0 || isdigit(atoi(argv[3])) != 0){
        printf("Usage: server <pool> <pool-size>\n");
        exit(1);
    }

    n_port = atoi(argv[1]);
    n_max_request = atoi(argv[2]);
    n_pool_size = atoi(argv[3]);

    if(n_pool_size == 0){
        printf("\npool size need to more then zero");
        exit(1);
    }

    if(n_port <= 0 && n_max_request <= 0 && n_pool_size <= 0){ // todo that true?
        printf("Usage: server <port> <pool-size>\n");
        return 1;
    }


    //Create socket
    wellcom_sock = socket(AF_INET , SOCK_STREAM , 0);
    if (wellcom_sock == -1)
    {
        printf("Could not create socket");
        return 1;
    }

    //Prepare the sockaddr_in structure
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons( n_port );

    //Bind
    if( bind(wellcom_sock,(struct sockaddr *)&server , sizeof(server)) < 0)
    {
        //print the error message
        perror("bind failed. Error");
        return 1;
    }

    //Listen
    listen(wellcom_sock , 5);

    //Create threadpool
    threadpool *pool = create_threadpool(n_pool_size);
    if(pool == NULL){
        printf("Create pool is failed");
        return 1;
    }

    //Accept and incoming connection
    c = sizeof(struct sockaddr_in);
    int *sock;
    i = 0;
    while (1) {
        if( i > n_max_request - 1){
            destroy_threadpool(pool);
            exit(0);
        }
        //accept connection from an incoming client
        client_sock = accept(wellcom_sock, (struct sockaddr *) &client, (socklen_t *) &c);
        if (client_sock < 0) {
            perror("accept failed");
            return 1;
        }
        sock = (int *)malloc(sizeof(int));
        *sock = client_sock;
        //dispatch
        dispatch(pool, (int (*)(void *)) to_dispatch, (void *) (sock));
        i++;
    }

}



int to_dispatch(void * req) {
    int client_sock = *(int *)req;
    int i = 0, put_html = -1 ,has_end = 0 , j  , isdir = -1;
    char request[2500] ="";
    char tmp[5000] ="" , path_temp[2500] ="", path[2500] ="" , sen[1000] = "";
    DIR* folder;
    struct dirent * dir;
    ssize_t n_write ;
    struct stat sb;

    recv(client_sock, tmp, sizeof(tmp), 0);
    free(req);
    //copy from the text from the sock in to '\r\n'
    for( j = 0 ; j < strlen(tmp) ; j ++){
        if(tmp[j] == '\r' & (j + 1) < strlen(tmp) & tmp[j+1] == '\n' ){
            has_end = 1;
            break;
        }
        request[j] = tmp[j];
    }
    request[j] = '\0';


    if(has_end == 0){
        print_response(400 , client_sock, NULL);
        return 1;}

    strcpy(sen , "GET");
    for (j = 0 ; j < 3 && request[i] != '\0'; i++, j++)
        if(request[i] != sen[j]) {
            print_response(501 ,  client_sock, NULL );
            return 1;
        }
    if(request[i] != ' '){
        print_response(400 , client_sock, NULL);
        return 1;}
    i++;
    if(request[i] != '/'){
        print_response(400 , client_sock, NULL );
        return 1;}

    // Copy the path
    bzero(path , sizeof(path));
    i++;
    for (j = 0 ; request[i] != ' ' && request[i] != '\n' ; j ++ , i++)
        path[j] = request[i];
    path[j] = '\0';

    //check if to path have access to read
    if(stat(path , &sb) != 0){
        print_response(403, client_sock, NULL);
        return 1;
    }

    if (is_dir(path) == 0) {
        if (request[i - 1] != '/') {
            print_response(302, client_sock, path);
            return 1;
        }
        isdir = 1;
    }

    if(request[i] == 'H') {
        strcpy(sen, "HTTP/1.");
        for (j = 0; (i + 1) < strlen(request) &&  j < 7 && request[i - i] != '.' && request[i] != '\r' && request[i + 1] != '\n'; i++, j++)
            if (request[i] != sen[j]){
                print_response(400 ,  client_sock, NULL );
                return 1;}

        if (request[i] != '0' && request[i] != '1'){
            print_response(400 , client_sock, NULL );
            return 1;}
    }
    else{
        if(request[i] != ' '){
            print_response(400 , client_sock, NULL);
            return 1;}
    }

    // search if index.html exist in the dir
    if(isdir == 1 )
    {
        bzero(path_temp , sizeof(path_temp));
        strcpy(path_temp , path);
        strcat(path_temp , "/index.html");
        // if stat !=0 so index.html is not exist
        if(stat(path_temp , &sb) != 0)
            put_html = 1;
    }

    char str[ strlen(path) + 5000];
    bzero(str , sizeof(str));
    // build the response
    build_response( str , path ,  isdir);
    int fd;

    /*write in to socket*/
    if(put_html != 1) {
        //open
        if (isdir == 1) {  // this dir and have index.html in the dir
            fd  = open(path_temp, O_RDONLY); // path_temp hold path with file.html
            if (fd < 0) {
                print_response(500, client_sock, NULL);
                return 1;
            }
        } else {
            fd = open(path , O_RDONLY );
            if (fd < 0) {
                print_response(500, client_sock, NULL);
                return 1;
            }
        }
        // we write the response to socket
        n_write = write(client_sock, str, strlen(str));
        if (n_write < 0) {
            print_response(500, client_sock, NULL);
            return 1;
        }

        unsigned s[5000] = {};

        while(read(fd , s , sizeof(s))){
            write(client_sock , s , sizeof(s));
        }

    }
    else //put_html == 1
     {
         strcat(str ,"\n<HTML>\n<HEAD><TITLE>Index of ");
         strcat(str, path);
         strcat(str, "</TITLE></HEAD>\n\n<BODY>\n");
         strcat(str ,"<H4>Index of ");
         strcat(str, path);
         strcat(str, "</H4>\n\n<table CELLSPACING=8>\n<tr><th>Name</th><th>Last Modified</th><th>Size</th></tr><tr>\n");
         // we write the response to socket
         n_write = write(client_sock, str, strlen(str));
         if(n_write < 0){
             print_response(500,  client_sock, NULL);
             return 1;
         }

        // Open folder
        folder = opendir(path);
        if(folder == NULL){
            print_response(500, client_sock, NULL);
            return 1;
        }

        bzero(str , sizeof(str));
        while ((dir = readdir(folder)) != NULL) {
                bzero(tmp, sizeof(tmp));
                sprintf(tmp, "\r\n<tr>\r\n<td><A HREF= \"%s\">%s</A></td>\n<td>", dir->d_name, dir->d_name);
                strcat(str, tmp);
                setModificationTime(str, dir->d_name);
                strcat(str, "</td>\r\n<td>");
                if (is_dir(dir->d_name) == 1) { // so this file
                    bzero(path_temp , sizeof(path_temp));
                    strcat(path_temp , path);
                    strcat(path_temp , "/");
                    strcat(path_temp ,dir->d_name );
                    setContentLength(str, path_temp);
                }
                strcat(str, "</td>\r\n</tr>");
                n_write = write(client_sock, str, strlen(str));
                if (n_write < 0) {
                    print_response(500, client_sock, NULL);
                    closedir(folder);
                    return 1;
                }
            bzero(str, sizeof(str));
            if (n_write == 0)
                    break;
            }
         sprintf(str ,"</table>\n\n<HR>\n\n<ADDRESS>webserver/1.0</ADDRESS>\n\n</BODY></HTML>" );
         n_write = write(client_sock, str, strlen(str));
         if (n_write < 0) {
             print_response(500, client_sock, NULL);
             closedir(folder);
             return 1;
         }
        closedir(folder);

     }
    close(client_sock);
    return 0;
}



int is_dir(const char* fileName) {
    struct stat path;
    if (stat(fileName, &path) != 0)
        return 1;
    stat(fileName, &path);
    return S_ISREG(path.st_mode);
}



void print_response(int status , int sock , char *path){
    char err[200] ,str[200] ,mes[100] , tmp[200];
    int content_len = 0;
    bzero(err , sizeof(err));
    bzero(str , sizeof(str));
    bzero(mes , sizeof(mes));
    bzero(tmp , sizeof (tmp));

    if(status == 400){
        strcpy(mes , "400 Bad Request");
        content_len = 113;
        strcpy(err , "Bad Request." );
    }
    if(status == 501){
        strcpy(mes , "501 Not supported");
        content_len = 129;
        strcpy(err , "Method is not supported.");
    }
    if(status == 404){
        strcpy(mes ,"Not Found" );
        content_len = 112;
        strcpy(err , "File not found." );
    }
    if(status == 302){
        content_len = 123;
        strcpy(mes, "Found");
        strcat(path , "/");
        strcpy(err , "Directories must end with a slash.");
    }
    if(status == 403){
        strcpy(mes, "Forbidden");
        content_len = 111;
        strcpy(err , "Access denied.");
    }
    if(status == 500){
        strcpy(mes , "Internal Server Error");
        content_len = 144;
        strcpy(err , "Some server side error.");
    }

    sprintf(str, "HTTP/1.0 %d %s \r\nServer: webserver/1.0\r\n", status, mes);

    // add date
    strcat(str , "Date: ");
    setCreateTimeOfFile(tmp , sizeof (tmp));
    strcat(str, tmp);
    strcat(str, "\r\n");

    //add to 302 location
    if(status == 302)
    {
        sprintf(tmp , "Location: %s\r\n", path);
        strcat(str , tmp);
    }

    //Add file type
    strcat(str , "Content-Type: text/html\r\n");

    //add len
    sprintf(tmp , "Content-Length: %d\r\n" , content_len);
    strcat(str , tmp);

    //close
    strcat(str , "Connection: close\r\n\r\n");


    sprintf(tmp , "<HTML><HEAD><TITLE>%d %s</TITLE></HEAD>\r\n", status , mes);
    strcat(str , tmp);
    sprintf(tmp , "<BODY><H4>%d %s</H4>\r\n" , status , mes);
    strcat(str , err);
    strcat(str , "\r\n</BODY></HTML>\r\n");

    // Write to client
    write(sock , str , strlen(str));
    close(sock);
}



void setCreateTimeOfFile(char *str , int size){
    char tmp[200];
    time_t now;
    now = time(NULL);
    strftime(tmp , size , RFC1123FMT, gmtime(&now));
    strcat(str , tmp);
}


void setContentLength( char * str , char *path){
    char tmp[1000];
    struct stat stats;
    stat(path , &stats);
    sprintf(tmp, "%ld", stats.st_size);
    strcat(str , tmp);
}


void setModificationTime(char *str, char *path){
    struct stat stats;
    stat(path , &stats);
    struct tm dt  = *(gmtime((const time_t *)&stats.st_ctim));
    char days[7][3] = {"Mon" , "Tue" , "Wed", "Thu" , "Fri" , "Sat" , "Sun" };
    char months[12][3] = {"Jan",	"Feb",	"Mar",	"Apr",	"May",	"Jun",	"Jul",	"Aug", "Sep",	"Oct",	"Nov",	"Dec"};
    char day[4] = "";
    char mon[4] = "";
    char tmp[500]= "";
    int i;
    for( i = 0;  i < 3 ; i++ ){
        day[i] = days[dt.tm_wday][i];
        mon[i] = months[dt.tm_mon][i];
    }
    day[i] = '\0';
    mon[i] = '\0';
    sprintf(tmp , "%s, %d %s %d %d:%d:%d GMT",day, dt.tm_mday , mon, dt.tm_year + 1900,dt.tm_hour, dt.tm_min, dt.tm_sec);
    strcat(str , tmp);
}


void setFileType(char * str, char* path){
    char *tmp = get_mime_type(path);
    if(tmp != NULL){
        strcat(str , "Content-Type: ");
        printf("\ntype = %s \n" ,tmp );
        strcat(str , tmp);
        strcat(str , "\r\n");
    }
}


//for 200 Ok
void build_response(char * str , char * path,  int isdir){
    char tmp[sizeof(char)*(2000)];
    bzero(tmp , 2000*sizeof(char));
    strcat(str , "HTTP/1.0 200 OK\r\nServer: webserver/1.0\r\n");
    // add date
    strcat(str , "Date : ");
    setCreateTimeOfFile(str , 2000);
    strcat(str , "\r\n");

    //Add location
    sprintf(tmp , "Location: /%s\r\n",path);
    strcat(str , tmp);
    //Add content type
    setFileType(str, path);
    //Add content length
    if(isdir == -1){
        strcat(str, "Content-length: " );
        setContentLength( str , path);
        strcat(str , "\r\n");
    }

    //get time modification
    strcat(str , "Last-Modification: ");
    setModificationTime(str, path);

    //close
    strcat(str , "\r\nConnection: close\r\n\r\n");
}

char *get_mime_type(char *name){
    char *ext = strrchr(name, '.');
    if (!ext) return NULL;
    if (strcmp(ext, ".html") == 0 || strcmp(ext, ".htm") == 0) return "text/html";
    if (strcmp(ext, ".jpg") == 0 || strcmp(ext, ".jpeg") == 0) return "image/jpeg";
    if (strcmp(ext, ".gif") == 0) return "image/gif";
    if (strcmp(ext, ".png") == 0) return "image/png";
    if (strcmp(ext, ".css") == 0) return "text/css";
    if (strcmp(ext, ".au") == 0) return "audio/basic";
    if (strcmp(ext, ".wav") == 0) return "audio/wav";
    if (strcmp(ext, ".avi") == 0) return "video/x-msvideo";
    if (strcmp(ext, ".mpeg") == 0 || strcmp(ext, ".mpg") == 0) return "video/mpeg";
    if (strcmp(ext, ".mp3") == 0) return "audio/mpeg";
    return NULL;
}