#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h> //pid_t
#include<sys/wait.h> //for waitPid

#define MAX_CMDS 64
#define MAX_ARGS 64

/*Error reporting: Anthony Hardy*/
static const char err[] = "An error has occured!\n";

static void displayError(void) {
	write(STDERR_FILENO, err, sizeof(err) - 1);
}

/*Author of trim_in_place, sanitize_input, tokenize_args: Michai Hughes*/
static void trim_in_place(char *s) {
    char *start = s;
    while (*start && isspace((unsigned char)*start)) {
        start++;
    }
    if (start != s) {
        memmove(s, start, strlen(start) + 1);
    }
    size_t len = strlen(s);
    while (len > 0 && isspace((unsigned char)s[len - 1])) {
        s[len - 1] = '\0';
        len--;
    }
}

static void sanitize_input(char *s) {
    char *nl = strpbrk(s, "\r\n");
    if (nl) {
        *nl = '\0';
    }
    trim_in_place(s);
}

static void tokenize_args(char *cmd, char *argv[MAX_ARGS]) {
    size_t argc = 0;
    char *token;
    while ((token = strsep(&cmd, " \t")) != NULL) {
        if (*token == '\0') {
            continue;
        }
        if (argc + 1 < MAX_ARGS) {
            argv[argc++] = token;
        }
    }
    argv[argc] = NULL;
}

int main(int argc, char **argv) {
		printf("\nHello! Welcome to the Wisconsin(wi) Shell(sh)!\n This program was created by Anthony Hardy, Alec Szczechowicz, and Michai Hughes.\n");

		/*Main loop: Anthony Hardy*/
		FILE *in = NULL; //c sttream init
		int interactive = 0; //bool value on whether ornot it is batch or interactive mode.
		char input[2048];
		char *paths[64] = {"/bin"}; //default path so access() actually finds commands
		int nPath = 1;
		if (argc == 1) {
			in = stdin;
			interactive = 1; //interactive mode, on!
		} else if (argc == 2) { //NOTE: ./wish filename.text as an example!
			in = fopen(argv[1], "r"); //read mode, 2nd argument is the filename to read from.

			if (!in) {
				displayError(); //ERROR OPENING FILE!
				exit(1);
			}
		} else {
			displayError(); //disallow any other argument.
			exit(1);
		}

		//Main shell loop: Anthony Hardy
		while (1) {
			if (interactive) {
				printf("wish> ");
				//flush stdout
				fflush(stdout);
			}
			if (!fgets(input, sizeof(input), in)) {
				break; //EOF or ctrl+d.
			}

			sanitize_input(input);
			//skip empty lines
			if (strlen(input) == 0) {
				continue;
			}

			//split commands by &
			char *cmds[MAX_CMDS];
			size_t cmdc = 0;
			char *cursor = input;
			char *segment;

			while ((segment = strsep(&cursor, "&")) != NULL) {
				trim_in_place(segment);
				if (*segment == '\0') continue;
				if (cmdc < MAX_CMDS) {
					cmds[cmdc++] = segment;
				}
			}
			pid_t kids[128]; //pid of child processes
			int numKids = 0; //number of children
			//process each command segment
			for (size_t i = 0; i < cmdc; i++) {
				char *cmd = cmds[i];
				char *outFile = NULL;

				//check for redir, >
				char *redir = strchr(cmd, '>');
				if (redir) {
					*redir = '\0'; //split string at '>'
					outFile = redir + 1;
					trim_in_place(outFile);

					//check if multiple > or no file after >
					if (strchr(outFile, '>') || strlen(outFile) == 0) {
						displayError();
						continue;
					}
				}
				trim_in_place(cmd);

				char *args[MAX_ARGS];
				tokenize_args(cmd, args);

				//if command is empty, skip!
				if (args[0] == NULL) continue;

				/*Handle fork, execv, outfile redirection, external cmds: Anthony Hardy*/
	

				//FIXME: ALEC, PUT SEPARATION OF CMD TO DEAL WITH > HERE
				//ENSURE ONLY 1 OUTPUT FILE PER REDIRECTION!
				//TIP: read 1st file argument, try to read a second, if 1st is no exist, or 2nd exist, error and loop.
				//make sure a file name doesn't have > in it.

				/*Arg pointers for parallel: Anthony Hardy*/
				char *argArray[128]; //arg pointers
				int argCount = 0;
				char *savePointer = NULL; //pointer for string tokens.

				//loop and split args by space, until end of char array. we use strtok to keep reading args, loop ends when i is null.
				char *token = strtok_r(cmd, " \t\r\n", &savePointer);
				while (token != NULL && argCount < MAX_ARGS - 1) {
					argArray[argCount++] = i;
					argArray[argCount] = NULL;
				}

				argArray[argCount] = NULL; //null terminate for execv

				if (argCount == 0) {
					displayError();
					continue;
				}

				//FIXME: ALEC, BUILT IN CMDS HERE

				/*Parallel commands, fork new child processes: Anthony Hardy*/
				char *fullPath = NULL;
				for (int i = 0; i < nPATH; i++) {
					size_t sizeNeeded = strlen(paths[i]) + 1 + strlen(argArray[0]) + 1; //calc size need to hold cmd path with / and /0
					char *candidatePath = malloc(sizeNeeded);

					if (!candidatePath) {
						displayError();
						break;
					}
					//build candidate path with command
					snprintf(candidatePath, sizeNeeded, "%s/%s", paths[i], argArray[0]);
					//check if file is real and can be ran, if fouhnd, get full path!
					if (access(candidatePath, X_OK) == 0) {
						fullPath = candidatePath;
						break;
					}
					free(candidatePath);
				}

				if (!fullPath) {
					displayError();
					continue;
				}

				//processes for the parallel cmds
				pid_t pid = fork(); //create child
				if (pid < 0) {
					//fork failed!
					displayError();
				} else if (pid == 0) {
					//check for redir first in child process
					if (cmdRedir) {
						//open/create redir file. use 0644 so only owner can read write, others can only read.
						int fd = open(cmdRedir, O_CREAT | O_TRUNC | O_WRONLY, 0644);
						if (fd < 0) {
							//exit on failure!
							displayError();
							exit(1);
						}
						if (dup2(fd, STDOUT_FILENO) < 0 || dup2(fd, STDERR_FILENO) < 0) {
							//error if redir to file or stderr fail, exit.
							displayError();
							exit(1);
						}
						close(fd);
					}
										//replace child with new exe and path
					execv(fullPath, argArray);
					displayError(); //if child return, then it is an error.
					exit(1);
				} else {
					if (numKids < MAX_CMDS) {
						kids[numKids++] = pid;
						//parent process will record the pid of child processes for waitPid later!
					}

					free(fullPath);
				}

				//debug print to show it works: Michai Hughes

				printf("Executing: %s\n", args[0]);
				if (outFile) printf(" Redirecting output to: %s\n", outFile);
			}

				//prevent zombie child!
			for (int i = 0; i < numKids; i++) {
				(void)waitpid(kids[i], NULL, 0);
			}
		}
		if (in != stdin) fclose(in);
		return 0;

}