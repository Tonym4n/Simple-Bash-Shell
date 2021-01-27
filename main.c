#include "parser.h"

#include <sys/types.h>
#include <sys/wait.h>

void printcmd(struct cmd *cmd)
{
    struct backcmd *bcmd = NULL;
    struct execcmd *ecmd = NULL;
    struct listcmd *lcmd = NULL;
    struct pipecmd *pcmd = NULL;
    struct redircmd *rcmd = NULL;

    int i = 0;
    
    if(cmd == NULL)
    {
        PANIC("NULL addr!");
        return;
    }
    

    switch(cmd->type){
        case EXEC:
            ecmd = (struct execcmd*)cmd;
            if(ecmd->argv[0] == 0)
            {
                goto printcmd_exit;
            }

            MSG("COMMAND: %s", ecmd->argv[0]);
            for (i = 1; i < MAXARGS; i++)
            {            
                if (ecmd->argv[i] != NULL)
                {
                    MSG(", arg-%d: %s", i, ecmd->argv[i]);
                }
            }
            MSG("\n");

            break;

        case REDIR:
            rcmd = (struct redircmd*)cmd;

            printcmd(rcmd->cmd);

            if (0 == rcmd->fd_to_close)
            {
                MSG("... input of the above command will be redirected from file \"%s\". \n", rcmd->file);
            }
            else if (1 == rcmd->fd_to_close)
            {
                MSG("... output of the above command will be redirected to file \"%s\". \n", rcmd->file);
            }
            else
            {
                PANIC("");
            }

            break;

        case LIST:
            lcmd = (struct listcmd*)cmd;

            printcmd(lcmd->left);
            MSG("\n\n");
            printcmd(lcmd->right);
            
            break;

        case PIPE:
            pcmd = (struct pipecmd*)cmd;

            printcmd(pcmd->left);
            MSG("... output of the above command will be redrecited to serve as the input of the following command ...\n");            
            printcmd(pcmd->right);

            break;

        case BACK:
            bcmd = (struct backcmd*)cmd;

            printcmd(bcmd->cmd);
            MSG("... the above command will be executed in background. \n");    

            break;


        default:
            PANIC("");
    
    }
    
    printcmd_exit:

    return;
}

//IMPLEMENTATION_BEGIN*******************************
void termPsHandler(int signum)								//terminate current cmd and get next shell cmd
{
	if(signum == SIGINT)
		return;
}

void performCmd(struct cmd *cmd)
{
	signal(SIGINT, termPsHandler);

	struct backcmd *bcmd = NULL;
	struct execcmd *ecmd = NULL;
	struct listcmd *lcmd = NULL;
	struct pipecmd *pcmd = NULL;
	struct redircmd *rcmd = NULL;

	if(cmd == NULL)
	{
		PANIC("NULL addr!");
		return;
	}

	switch(cmd->type)
	{
		case EXEC:
			ecmd = (struct execcmd*)cmd;

			pid_t c_pid = fork();
			int c_status;

			if(c_pid < 0)									//fork() error
				goto error_exit;
			else if(c_pid == 0)								//child
				execvp(ecmd->argv[0], &ecmd->argv[0]);
			else											//parent
			{
				if(waitpid(c_pid, &c_status, 0) != c_pid)	//wait() error
					goto error_exit;
				if(WIFEXITED(c_status))						//child exited normally
					if(WEXITSTATUS(c_status) != 0)			//child exit status error
					{
						MSG("\nNon-zero exit code (%d) detected\n", WEXITSTATUS(c_status));
						goto error_exit;
					}
			}

			break;

		case REDIR:
			rcmd = (struct redircmd*)cmd;
			int fd;

			if (0 == rcmd->fd_to_close)						//	< command
			{
				fd = open(rcmd->file, O_RDONLY);			//open file w/ r permission
				int stdin_copy = dup(0);					//save copy of stdin
				dup2(fd, 0);								//read from fd instead of stdin
				performCmd(rcmd->cmd);
				dup2(stdin_copy, 0);						//restore stdin
			}
			else if (1 == rcmd->fd_to_close)				//	> command
			{
				fd = open(rcmd->file, 						//create or overwrite file w/ rw permission
					O_CREAT|O_RDWR|O_TRUNC, 0755);			//mode- rwxu rxg rxo
				int stdout_copy = dup(1);					//save copy of stdout
				dup2(fd, 1);								//write to fd instead of stdout
				performCmd(rcmd->cmd);
				dup2(stdout_copy, 1);						//restore stdout
			}
			else if (3 == rcmd->fd_to_close)				//	>> command
			{
				fd = open(rcmd->file, 						//create or append to file w/ rw permission
					O_CREAT|O_RDWR|O_APPEND, 0755);			//mode- rwxu rxg rxo
				int stdout_copy = dup(1);					//save copy of stdout
				dup2(fd, 1);								//write to fd instead of stdout
				performCmd(rcmd->cmd);
				dup2(stdout_copy, 1);						//restore stdout
			}

			close(fd);
			break;

		case LIST:
			lcmd = (struct listcmd*)cmd;

			performCmd(lcmd->left);
			performCmd(lcmd->right);

			break;

		case PIPE:
			pcmd = (struct pipecmd*)cmd;

			int f[2];
			pipe(f);										//f[0] = read end, f[1] = write end

			int stdout_copy = dup(1);						//save copy of stdout
			dup2(f[1], 1);									//use f[1] as if it were stdout
			performCmd(pcmd->left);
			dup2(stdout_copy, 1);							//restore stdout


			int stdin_copy = dup(0);						//save copy of stdin
			dup2(f[0], 0);									//use f[0] as if it were stdin
			close(f[1]);									//close write end of pipe
			performCmd(pcmd->right);
			dup2(stdin_copy, 0);							//restore stdin

			break;

		case BACK:
			bcmd = (struct backcmd*)cmd;

			pid_t bg_c_pid = fork();

			if(bg_c_pid < 0)								//fork() error
				goto error_exit;
			else if(bg_c_pid == 0)							//child
			{
				performCmd(bcmd->cmd);
				exit(0);									//terminate bg process
			}
			else											//parent
			{												//same as main() loop but waits to reap child first
    			char buf[1024];

    			while((waitpid(bg_c_pid, NULL, WNOHANG) == 0) && (getcmd(buf, sizeof(buf)) >= 0))	//while bg process still alive
    			{
     				struct cmd * command;
        			command = parsecmd(buf);
        			performCmd(command);
    			}
			}

			break;


		default:
			PANIC("cmd error!\n");

	}

	normal_exit:
	return;

	error_exit:
	//fprintf(stderr, "exec error!\n");
	return;
}
//IMPLEMENTATION_END*********************************


int main(void)
{
    static char buf[1024];

    setbuf(stdout, NULL);

    // Read and run input commands.
    while(getcmd(buf, sizeof(buf)) >= 0)
    {
        struct cmd * command;
        command = parsecmd(buf);
        //printcmd(command); // TODO: run the parsed command instead of printing it

        performCmd(command);
    }

    PANIC("getcmd error!\n");
    return 0;
}
