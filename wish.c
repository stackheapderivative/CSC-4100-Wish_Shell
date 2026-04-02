#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>

#define MAX_CMDS 64
#define MAX_ARGS 64

/*Error reporting: Anthony Hardy*/
static const char err[] = "IMPORTANT! An error has occured!\n";

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

			while ((segment = strset(&cursor, '&')) != NULL) {
				trim_in_place(segment);
				if (*segment == '\0') continue;
				if (cmdc < MAX_CMDS) {
					cmds[cmdc++] = segment;
				}
			}

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

				//debug print to show it works: Michai Hughes

				printf("Executing: %s\n", args[0]);
				if (outFile) printf(" Redirecting output to: %s\n", outFile);
			}
		}
		if (in != stdin) fclose(in);
		return 0;

}

/*
    char input[2048];
    if (!fgets(input, sizeof(input), stdin)) {
        return 0;
    }

    sanitize_input(input);

    char *cmds[MAX_CMDS];
    size_t cmdc = 0;
    char *cursor = input;
    char *segment;
    while ((segment = strsep(&cursor, "&")) != NULL) {
        trim_in_place(segment);
        if (*segment == '\0') {
            continue;
        }
        if (cmdc < MAX_CMDS) {
            cmds[cmdc++] = segment;
        }
    }

    for (size_t i = 0; i < cmdc; i++) {
        char *cmd = cmds[i];
        char *outfile = NULL;
        char *redir = strchr(cmd, '>');
        if (redir) {
            *redir = '\0';
            outfile = redir + 1;
            trim_in_place(outfile);
        }
        trim_in_place(cmd);

        char *argv[MAX_ARGS];
        tokenize_args(cmd, argv);

        printf("command %zu:\n", i + 1);
        for (size_t j = 0; argv[j] != NULL; j++) {
            printf("  arg[%zu] = %s\n", j, argv[j]);
        }
        if (outfile) {
            printf("  redirect to: %s\n", outfile);
        }
    }

    return 0;
	*/