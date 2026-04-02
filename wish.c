#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h> //open(), O_CREAT, O_TRUNC
#include <sys/wait.h> //for waitPid
#include <sys/types.h> //defines pid_t

#define MAX_CMDS 64
#define MAX_ARGS 64

/*Error reporting: Anthony Hardy*/
static const char err[] = "An error has occurred.\n"; //Fixed a spelling error. :3

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

/* JOB 3 HELPERS - Alec Szczechowicz*/

/* Counts how many arguments are in argv.*/
static int count_args(char *argv[]) {
    int count = 0;
    while (argv[count] != NULL) {
        count++;
    }
    return count;
}

/* Returns 1 if the command is a shell built-in, otherwise 0.*/
static int is_builtin(char *argv[]) {
    if (argv[0] == NULL) {
        return 0;
    }

    return strcmp(argv[0], "exit") == 0 || strcmp(argv[0], "cd") == 0 || strcmp(argv[0], "path") == 0;
}

/* Frees all currently stored shell paths. */
static void free_paths(char *paths[], int *nPath) {
    for (int i = 0; i < *nPath; i++) {
        free(paths[i]);
        paths[i] = NULL;
    }
    *nPath = 0;
}

/* Runs built-in commands in the parent shell process. This is important because commands like cd must change the shell's own state, not a child process. */
static void run_builtin(char *argv[], char *paths[], int *nPath) {
    int argc = count_args(argv);

    /* exit: make sure that there is no arguments */
    if (strcmp(argv[0], "exit") == 0) {
        if (argc != 1) {
            displayError();
            return;
        }

        /* Clean up allocated memory */
        free_paths(paths, nPath);
        exit(0);
    }

    /* cd: takes one argument */
    if (strcmp(argv[0], "cd") == 0) {
        if (argc != 2) {
            displayError();
            return;
        }

        if (chdir(argv[1]) != 0) {
            displayError();
        }
        return;
    }

    /* path: replaces the current search path list */
    if (strcmp(argv[0], "path") == 0) {
        /* get rid of the old path */
        if (argc > 1) { //prevents freeing /bin
			free_paths(paths, nPath);
		}
        /* Add all new path entries after the word "path" */
        for (int i = 1; argv[i] != NULL && *nPath < 64; i++) {
            paths[*nPath] = strdup(argv[i]);
            if (paths[*nPath] == NULL) {
                displayError();
                free_paths(paths, nPath);
                return;
            }
            (*nPath)++;
        }
        return;
    }
}

/* Searches through the shell's path list to find an executable. If found, returns a malloc for the full path. If not found, we return NULL. */
static char *find_executable(char *cmd, char *paths[], int nPath) {
    for (int i = 0; i < nPath; i++) {
        size_t sizeNeeded = strlen(paths[i]) + strlen(cmd) + 2;
        char *fullPath = malloc(sizeNeeded);
        if (fullPath == NULL) {
            displayError();
            return NULL;
        }

        snprintf(fullPath, sizeNeeded, "%s/%s", paths[i], cmd);

        /* checks whether the file exists and is executable. */
        if (access(fullPath, X_OK) == 0) {
            return fullPath;
        }

        free(fullPath);
    }

    return NULL;
}

int main(int argc, char **argv) {
		printf("\nHello! Welcome to the Wisconsin(wi) Shell(sh)!\n This program was created by Anthony Hardy, Alec Szczechowicz, and Michai Hughes.\n");

		/*Main loop: Anthony Hardy*/
		FILE *in = NULL; //c sttream init
		int interactive = 0; //bool value on whether ornot it is batch or interactive mode.
		char *input = NULL;
		size_t inputLen = 0;
		/*Alec: I changed this block of code because I think that if we try to free space using free_paths() later we will free(/bin) since its not allocated with malloc */
		char *paths[64] = {0}; //default path so access() actually finds commands
		int nPath = 0;

		paths[0] = strdup("/bin");

		if (paths[0] == NULL) {
			displayError();
			exit(1);
		}

		nPath = 1;

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
			if (getline(&input, &inputLen, in) == -1) {
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
			pid_t kids[MAX_CMDS]; //pid of child processes
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
	
				//ENSURE ONLY 1 OUTPUT FILE PER REDIRECTION!
				//TIP: read 1st file argument, try to read a second, if 1st is no exist, or 2nd exist, error and loop.
				//make sure a file name doesn't have > in it.
				
				if (is_builtin(args)) {
					if (outFile != NULL) {
						displayError();
						continue;
					}

					run_builtin(args, paths, &nPath);
					continue;
				}

				//Alec: Removed the loop here since it seemed a little bit redundant, or was just a placeholder. Also, removed redeclartion of variables.

				/*Parallel commands, fork new child processes: Anthony Hardy*/
				/*Alec: using find_executable instead since this is part of the built-in*/
				char *fullPath = find_executable(args[0], paths, nPath);
				if (fullPath == NULL) {
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
					/*Alec: Create a temporary array instead
					made sure to use the correct outFile variable, use the correct args
					and validate file count*/
					if(outFile != NULL) {
						/*We want to split output section into tokens */
						char *redirArgs[MAX_ARGS];
						tokenize_args(outFile, redirArgs);

						/*Make sure that we have just one output file*/
						if (redirArgs[0] == NULL || redirArgs[1] != NULL) {
							displayError();
							exit(1);
						}

						int fd = open(redirArgs[0], O_CREAT | O_TRUNC | O_WRONLY, 0644);
						if (fd < 0) {
							displayError();
							exit(1);
						}

						if (dup2(fd, STDOUT_FILENO) < 0 || dup2(fd, STDERR_FILENO) < 0) {
							close(fd);
							displayError();
							exit(1);
						}

						close(fd);

					}

					execv(fullPath, args);
					displayError();
					exit(1);

				} else {
					if (numKids < MAX_CMDS) {
						kids[numKids++] = pid;
						//parent process will record the pid of child processes for waitPid later!
					}

					free(fullPath);
				}

			}

				//prevent zombie child!
			for (int i = 0; i < numKids; i++) {
				(void)waitpid(kids[i], NULL, 0);
			}
		}
		if (in != stdin) fclose(in);

		free(input); //free getline's buffer
		free_paths(paths, &nPath); //Alec: Added this to make sure that we free path memory before the program ends.

		return 0;

}
