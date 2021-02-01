#include "argus.h"

#define GREEN "\033[32m"
#define RED "\033[31m"
#define BLUE "\033[34m"
#define BOLD "\033[1m"
#define RESET "\033[0m"
#define PICKCOLOUR(var) (var == 0 ? GREEN : RED)

ssize_t readln(int fd, char* line, size_t size){
    ssize_t bytes_read = read(fd, line, size);
    if (!bytes_read) return 0;

    size_t line_length = strcspn(line, "\n") + 1;
    if (bytes_read < line_length) line_length = bytes_read;
    line[line_length]=0;

    lseek(fd, line_length - bytes_read, SEEK_CUR);
    return line_length;
}

int main(int argc, char** argv)
{
    char fifo[20] = "fifo";
    char fifo2[20] = "fifo2";
    int output = open(fifo2, O_RDONLY);
    int server = open(fifo, O_WRONLY);
    if (server < 0) {
        printf("Server offline\n");
        return 1;
    }

    char menu[] = 
        "1.Tempo inatividade   -> Usage: tempo-inatividade (int)\n"
        "2.Tempo execução      -> Usage: tempo-execucao (int)\n"
        "3.Executar            -> Usage: executar 'cmd'\n"
        "4.Tarefas em Execução -> Usage: listar\n"
        "5.Terminar tarefa     -> Usage: terminar (int)\n"
        "6.Tarefas Terminadas  -> Usage: historico\n"
        "7.Exit                -> Usage: exit\n"
        "8.Ajuda               -> Usage: help\n";
    if(argc==1){
        if(write(1,menu,strlen(menu)) < 0){
            perror("write");
            exit(1);
        }
        int bufferSize = 4096;
        char cwd[4096];
        char* buffer = malloc(bufferSize);
        int offset = 0;
        char* bashPrefix = "$";
        int errorCd = 0;
        while (1) {
            if (!offset) {
                printf(GREEN "%s" BLUE "%s" BOLD "%s%s" RESET " ",
                    "Argus: ",
                    getcwd(cwd, 4096),
                    PICKCOLOUR(errorCd), bashPrefix);
                fflush(stdout);
            }
            int n = readln(0, buffer + offset, bufferSize - offset);
            if (n < 1 || !strncmp(buffer, "exit\n", n))
                break;
            if (!strncmp(buffer, "help\n", n)){
                if(write(1,menu,strlen(menu)) < 0){
                    perror("write");
                    exit(1);
                }
                errorCd = 0;
            }

            if (buffer[n - 1] != '\n') {
                offset += bufferSize;
                buffer = realloc(buffer, bufferSize *= 2);
            } else {
                buffer[n + offset - 1] = '\0';
                if(strstr(buffer,"tempo-inatividade")!=NULL || strstr(buffer,"tempo-execucao")!=NULL || 
                    strstr(buffer,"executar")!=NULL || strstr(buffer,"listar")!=NULL ||
                    strstr(buffer,"terminar")!=NULL || strstr(buffer,"historico")!=NULL){

                    if(write(server, buffer, strlen(buffer)) < 0){
                        perror("write");
                        exit(1);                    
                    }
                    errorCd = 0;
                    char buf[1000];
                    int bytes_read = 0;
                    if ((bytes_read = read(output, buf, 1000)) > 0 ){
                        if(write(1,buf,bytes_read) < 0){
                            perror("write");
                            exit(1);
                        }
                    }
                }
                else if(strcmp(buffer,"help")){
                    errorCd = -1;
                    char warning[] = ": Command not found\n";
                    strcat(buffer,warning);
                    if(write(1, buffer, strlen(buffer)) < 0){
                        perror("write");
                        exit(1);
                    }
                }
            }

           
        }
    }
    else{
        char str1[100] , str2[] = " ";
        for(int i = 0; i < argc ; i++){
            strcpy(str1,argv[i]);
            strcat(str1,str2);
            if(write(server, str1, strlen(str1)) < 0){
                perror("write");
                exit(1);
            }
        }
        char buf[1000];
        int bytes_read = 0;

        if ((bytes_read = read(output, buf, 1000)) > 0 ){
            if(write(1,buf,bytes_read) < 0){
                perror("write");
                exit(1);
            }
            
        }
    }


    close(output);
    close(server);
    //unlink(fifo);
    return 0;
}