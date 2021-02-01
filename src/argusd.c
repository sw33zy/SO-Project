#include "argus.h"

int pid;
int timeout=0;
int tempoexecucao=9999;
int ctrc_c_counter=0;
int killed = 0;

void sigint_handler(int signum){
	char msg[100] = "\nAre you sure you want to quit?\nYES: Press CTRL + C\n";
	if(ctrc_c_counter < 1) 
		if(write(1,msg,strlen(msg)) < 0){
			perror("write");
			exit(1);
		}
	ctrc_c_counter++;
	if(ctrc_c_counter==2){
		exit(1);
	}
}

void timeout_handler(int signum){
		if(pid > 0){
			kill(pid,SIGKILL);
		}
	timeout = 1;
}


void sigusr_handler(int signum){
	killed = 1;
}

ssize_t readln(int fd, char* line, size_t size){
    ssize_t bytes_read = read(fd, line, size);
    if (!bytes_read) return 0;

    size_t line_length = strcspn(line, "\n") + 1;
    if (bytes_read < line_length) line_length = bytes_read;
    line[line_length]=0;

    lseek(fd, line_length - bytes_read, SEEK_CUR);
    return line_length;
}

void redirect(char* file, int to, int append){
    int fd = -1;
    switch (to) {
    case 0:
        if((fd = open(file, O_RDONLY)) < 0){
        	perror("open");
			exit(1);
        }
        break;
    case 1:
        if (append){
            if((fd = open(file, O_CREAT | O_WRONLY | O_APPEND, 0644)) < 0){
            	perror("open");
				exit(1);
            }
        }
        else
            if((fd = open(file, O_CREAT | O_WRONLY | O_TRUNC, 0644)) < 0){
            	perror("open");
				exit(1);
            }
        break;
    }
    dup2(fd, to);
    close(fd);
}

int argsExtra(char** argv, int from, int to){
    int offset = to;
    for (int i = to - 1; i >= from; --i) {
        switch (argv[i][0]) {
        case '<':
            redirect(argv[i + 1], 0, 0);
            offset = i;
            break;
        case '>':
            switch (argv[i][1]) {
            case '\0':
                redirect(argv[i + 1], 1, 0);
                offset = i;
                break;
            case '>':
                redirect(argv[i + 1], 1, 1);
                offset = i;
                break;
            }
            break;
        }
    }
    return offset;
}

char** words(char* string){
    char* command = strdup(string);
    int argc = 10;
    char** argv = (char**) malloc(argc * sizeof(char*));
    int i = 0;
    char* token = strtok(command, " ");
    do {
        if (!(i < argc))
            argv = realloc(argv, (argc *= 2) * sizeof(char*));

        argv[i++] = token;

        token = strtok(NULL, " ");
    } while (token);
    argv[i] = NULL;
    return argv;
}


/**********************
* 	STATUS:			  *
*	0 - WORD          *
*	1 - PIPE          *  
*	2 - BACKGROUND    *
*	3 - END           *
*                     *
**********************/

int runCmd(char** argv, int i){
	char f[50];
	sprintf(f,"%d.pids",getpid());
	int fd = -1;
	if((fd = open(f, O_CREAT | O_TRUNC | O_WRONLY, 0644)) < 0){
		perror("open");
		exit(1);
	}
  
    int beforeP = 0;
    int latestPipe = 0;
    int pipeP[2];
    int lastPid;
    int status = 0;
    for (int i = 0; argv[i] && (status == 0 || status == 1); i++) {
        status = 0;
        if (argv[i][0] == '|')
            status = 1;
        else if (argv[i][0] == '&')
            status = 2;
        else if (argv[i + 1] == NULL)
            status = 3;

        if (status != 0) {
            if (status == 1)
                pipe(pipeP);
            if (!(lastPid = fork())) {
                argv[argsExtra(argv, latestPipe, i + 1)] = NULL;
 
                dup2(beforeP, 0);
                if (latestPipe != 0)
                    close(beforeP);
                if (status == 1) {
                    dup2(pipeP[1], 1);
                    close(pipeP[1]);
                    close(pipeP[0]);
                }
                if (status != 3)
                    argv[i] = NULL;

                execvp(argv[latestPipe], argv + latestPipe);
                perror("Failed to execute command");
                _exit(1);
            }
            pid = lastPid;
            if (status == 1)
                close(pipeP[1]);
            if (latestPipe != 0)
                close(beforeP);
            beforeP = pipeP[0];
            latestPipe = i + 1;
        }	
        char pd[20];
        sprintf(pd,"%d\n",lastPid);
        if(write(fd,pd,strlen(pd)) < 0){
        	perror("write");
        	_exit(1);
        }

    }
    if(signal(SIGALRM, timeout_handler) == SIG_ERR){
		perror("timeout_handler");
		return 5;
	}
	alarm(tempoexecucao);

    if (status != 2) {
        int status;
        int pid;
        while ((pid = wait(&status)) > -1) {
            if (pid == lastPid) {
                if (WIFEXITED(status))
                    return 2;
                return -1;
            }
        }
    }
    close(fd);
    return i;
}


void updateFile(int fildes){
	int tmp = -1;
	char res[100];
	sprintf(res,"/%d/d",getpid());
	if((tmp = open("tmp.txt", O_CREAT | O_TRUNC | O_WRONLY, 0644)) < 0){
		perror("open");
		exit(1);
	}
	int stdout_fd_backup = dup(1);
	
	if(!fork()){
		execlp("cp","cp","exec.txt","tmp.txt",NULL);
	}
	wait(NULL);

	if(!fork()){
		execlp("truncate","truncate","-s 0","exec.txt",NULL);
	}
	wait(NULL);

	dup2(fildes,1);
	if(!fork()){
		execlp("sed","sed",res,"tmp.txt",NULL);
	}
	wait(NULL);

	if(!fork())
		execlp("rm","rm","tmp.txt",NULL);
	wait(NULL);
	dup2(stdout_fd_backup,1);
	close(tmp);
}

char** getPids(char* f){
	int bytes_read;
	char buf[100];
	int fd = -1;
	if((fd = open(f,O_RDONLY)) < 0){
		perror("open");
		exit(1);
	}
	int j = 0;
	char** res = (char**) malloc(10 * sizeof(char*));
	while((bytes_read = readln(fd, buf, 100)) > 0 ){
		char** command = words(buf);
		for(int i = 0; command[i]!=NULL; i++){		
			if(strcmp(command[i],"PID:") == 0){ 
				res[j] = command[0];
				res[j+1] = strdup(command[i+1]); 
				j+=2;
			}
		}
		
	}
	close(fd);
	return res;
}

int main(int argc, char const *argv[])
{

	if (signal(SIGINT, sigint_handler) == SIG_ERR){
		perror("signal sigint");
		exit(1);
	}

	if (signal(SIGUSR1, sigusr_handler) == SIG_ERR){
		perror("signal sigint");
		exit(1);
	}

	int log_fd = -1;
	if((log_fd = open("log.txt", O_CREAT | O_TRUNC | O_WRONLY, 0666)) < 0){
		perror("open log");
		exit(1);
	}
	int historico_fd = -1;
	if((historico_fd = open("historico.txt", O_CREAT | O_TRUNC | O_RDWR, 0644)) < 0){
		perror("open hist");
		exit(1);
	}
	int exec_fd = -1;
	if((exec_fd = open("exec.txt", O_CREAT | O_TRUNC | O_RDWR, 0644)) < 0){
		perror("open exec");
		exit(1);
	}

	int fifo_fd = -1;
	int fifo_wr = -1;
	int i = 0;
	if((fifo_wr = open("fifo2", O_WRONLY)) <0){
		perror("fifo_wr");
		exit(1);
	}
	while((fifo_fd = open("fifo", O_RDONLY)) > 0){
		char buf[100];
		int bytes_read = 0;

		while((bytes_read = read(fifo_fd, buf, 100)) > 0 ){
			waitpid(-1, NULL, WNOHANG);
			ctrc_c_counter = 0;
			char token[bytes_read];
			memcpy(token, &buf[0], bytes_read );
			token[bytes_read] = '\0';
			char** command = words(token);
			int size;
			for(size = 0; command[size]!=NULL; size++);
			if(size==1) command[1]=" ";
			if(strcmp(command[0],"executar") == 0 || strcmp(command[1],"-e") == 0){
				i++;
				int r=-999;
				char msg[30];
				sprintf(msg, "Nova Tarefa #%d\n", i);
				if(write(fifo_wr,msg, strlen(msg)) < 0){
					perror("write");
					exit(1);
				}
				//addEntry(executing, token, entries2);
				int stdout_fd_backup = dup(1);
				dup2(log_fd,1);
				if(!fork()){
					char starting[50];
					sprintf(starting, "#%d, Executing: %s PID: %d\n", i,token, getpid());
					char pid[50];
					sprintf(pid,"%d.pids",getpid());
					if(write(exec_fd,starting, strlen(starting)) < 0){
						perror("write");
						_exit(1);
					}
					if (strcmp(command[0],"executar") == 0) r= runCmd(&command[1],i);
					else r = runCmd(&command[2],i);
					updateFile(exec_fd);
					if(r==2){
						char completed[50];
						sprintf(completed, "#%d, Concluida: %s\n", i,token);
						if(write(historico_fd,completed, strlen(completed)) < 0){
							perror("write");
							exit(1);
						}
						if(!fork()){
							execlp("rm","rm",pid,NULL);
						}
					}
					if(r==-1 && timeout==1){
						char terminated[50];
						sprintf(terminated, "#%d, Max Execution time reached: %s\n", i,token);
						if(write(historico_fd,terminated, strlen(terminated)) < 0){
							perror("write");
							exit(1);
						}
						timeout=0;
						if(!fork()){
							execlp("rm","rm",pid,NULL);
						}
					}
					if(killed == 1){
						char killed2[50];
						sprintf(killed2, "#%d, Killed: %s\n", i,token);
						if(write(historico_fd,killed2, strlen(killed2)) < 0){
							perror("write");
							exit(1);
						}
						killed=0;
					}
					_exit(r);
				}
				dup2(stdout_fd_backup,1);
			}
			else if(strcmp(command[0],"listar") == 0 || strcmp(command[1],"-l") == 0){
				char buf2[100];
				int bytes;
				int exec_fd2 = -1;
				if((exec_fd2=open("exec.txt",O_RDONLY)) < 0){
					perror("open");
					exit(1);
				}
				struct stat st;
				fstat(exec_fd2,&st);
				off_t file_size = st.st_size;
				if(file_size == 0){
					if(write(fifo_wr,"No Tasks Executing\n",strlen("No Tasks Executing\n")) < 0){
						perror("write");
						exit(1);
					}
				}
				else while((bytes = read(exec_fd2,buf2,100)) > 0){
					
					if(write(fifo_wr,buf2,bytes) < 0){
						perror("write");
						exit(1);
					}
				}
				close(exec_fd2);
			}
			else if(strcmp(command[0],"historico") == 0 || strcmp(command[1],"-r") == 0){
				char buf2[100];
				int bytes;
				int historico_fd2 = -1;
				if ((historico_fd2=open("historico.txt",O_RDONLY)) < 0){
					perror("open");
					exit(1);
				}
				struct stat st;
				fstat(historico_fd2,&st);
				off_t file_size = st.st_size;
				if(file_size == 0){
					if(write(fifo_wr,"No Tasks Done Yet\n",strlen("No Tasks Done Yet\n")) < 0){
						perror("write");
						exit(1);
					}
				}
				else while((bytes = read(historico_fd2,buf2,100)) > 0){

					if(write(fifo_wr,buf2,bytes) < 0){
						perror("write");
						exit(1);
					}
				}
				close(historico_fd2);
			}

			else if(strcmp(command[0],"tempo-execucao") == 0){
				char msg[50];
				sprintf(msg,"Tempo de execução: %s\n", command[1]);
				if(write(fifo_wr,msg,strlen(msg)) < 0){
					perror("write");
					exit(1);
				}
				tempoexecucao = atoi(command[1]);
			}

			else if(strcmp(command[1],"-m") == 0){
				char msg[50];
				sprintf(msg,"Tempo de execução: %s\n", command[2]);
				if(write(fifo_wr,msg,strlen(msg)) < 0){
					perror("write");
					exit(1);
				}
				tempoexecucao = atoi(command[2]);
			}

			else if(strcmp(command[0],"terminar") == 0 || strcmp(command[1],"-t") == 0){
				char f[50];
				char** pids = (char**) malloc(20*sizeof(char*));
				char msg[50];
				if(strcmp(command[1],"-t") == 0) sprintf(msg,"Terminar #%s\n", command[2]);
				else sprintf(msg,"Terminar #%s\n", command[1]);
				if(write(fifo_wr,msg,strlen(msg)) < 0){
					perror("write");
					exit(1);
				}
				char** res = getPids("exec.txt");
				for(int i = 0; res[i]!=NULL; i++) strtok(res[i],"\n");
				char key[50];
				if(strcmp(command[1],"-t") == 0) sprintf(key,"#%s,", command[2]);
				else sprintf(key,"#%s,", command[1]);
				int found = 0;
				int pidfound = 0;
				for(int i = 0; res[i]!=NULL; i+=2){
					if(strcmp(key,res[i]) == 0) {
						found = 1;
						pidfound = atoi(res[i+1]);
					}
				}
				if(found == 0 || i == 0) {
					if(write(fifo_wr, "Task Doesn't Exist!\n", strlen("Task Doesn't Exist!\n")) < 0){
						perror("write");
						exit(1);
					}
				}
				else{
					
					sprintf(f,"%d.pids",pidfound);
					int pid_fd = -1;
					if((pid_fd = open(f,O_RDONLY)) < 0){
						perror("open");
						exit(1);
					}
					char buf[50];
					int i = 0;

					while((readln(pid_fd,buf,50)) > 0){
						pids[i]=strdup(buf);
						i++;
					}
					for(int i = 0; pids[i]!=NULL; i++) strtok(pids[i],"\n");
					int status;

					for(int j = 0; pids[j]!=NULL; j++){
						if((atoi(pids[j])) > 0){
							kill(atoi(pids[j]),SIGKILL);
							if(waitpid(atoi(pids[j]), &status, 0) > 0){
								if(WIFEXITED(status)){
									printf("Task %d finished\n", atoi(pids[j]));
								} else {
									printf("Task %d was killed\n", atoi(pids[j]));
								}
							}
						}
					}
					kill(pidfound,SIGUSR1);
					free(pids);
					if(!fork()){
						execlp("rm","rm",f,NULL);
					}
					wait(NULL);
					close(pid_fd);
				}
				
				//kill(atoi(res[0]),SIGUSR1);
			}
			else if(strcmp(command[0],"tempo-inatividade") == 0 || strcmp(command[1],"-i") == 0){
				if(write(fifo_wr,"Coming Soon TM\n",strlen("Coming Soon TM\n")) < 0){
					perror("write");
					exit(1);
				}
			}
			else{
				if(write(fifo_wr,"Command not Found\n",strlen("Command not Found\n")) < 0){
					perror("write");
					exit(1);
				}
			}

		}

	}

	close(exec_fd);
	close(historico_fd);
	close(log_fd);
	close(fifo_fd);
	close(fifo_wr);
	return 0;
}