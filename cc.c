/* Copyright (C) 2016, 2021 Jeremiah Orians
 * Copyright (C) 2020 deesix <deesix@tuta.io>
 * Copyright (C) 2020 Gabriel Wicki
 * This file is part of M2-Planet.
 *
 * M2-Planet is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * M2-Planet is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with M2-Planet.  If not, see <http://www.gnu.org/licenses/>.
 */

#include"cc.h"

/* The core functions */
void populate_env(char** envp);
void setup_env(char** envp);
char* env_lookup(char* variable);
void initialize_types();
struct token_list* read_all_tokens(FILE* a, struct token_list* current, char* filename);
struct token_list* reverse_list(struct token_list* head);

struct token_list* remove_line_comments(struct token_list* head);
struct token_list* remove_line_comment_tokens(struct token_list* head);
struct token_list* remove_preprocessor_directives(struct token_list* head);

void eat_newline_tokens();
void init_macro_env(char* sym, char* value, char* source, int num);
void preprocess();
void output_tokens(struct token_list *i, FILE* out);
int strtoint(char *a);
void spawn_processes(int debug_flag, char* preprocessed_file, char* destination, char** envp);

int main(int argc, char** argv, char** envp)
{
	FUZZING = FALSE;
	MAX_STRING = 4096;
	PREPROCESSOR_MODE = FALSE;
	STDIO_USED = FALSE;
	int debug_flag = TRUE;
	FILE* in = stdin;
	FILE* tempfile;
	char* destination_name = "a.out";
	FILE* destination_file = stdout;
	init_macro_env("__M2__", "42", "__INTERNAL_M2__", 0); /* Setup __M2__ */
	char* name;
	char* hold;

	int i = 1;
	while(i <= argc)
	{
		if(NULL == argv[i])
		{
			i += 1;
		}
		else if(match(argv[i], "-E") || match(argv[i], "--preprocess-only"))
		{
			PREPROCESSOR_MODE = TRUE;
			i += 1;
		}
		else if(match(argv[i], "-f") || match(argv[i], "--file"))
		{
			if(NULL == hold_string)
			{
				hold_string = calloc(MAX_STRING + 4, sizeof(char));
				require(NULL != hold_string, "Impossible Exhaustion has occured\n");
			}

			name = argv[i + 1];
			if(NULL == name)
			{
				fputs("did not receive a file name\n", stderr);
				exit(EXIT_FAILURE);
			}

			in = fopen(name, "r");
			if(NULL == in)
			{
				fputs("Unable to open for reading file: ", stderr);
				fputs(name, stderr);
				fputs("\n Aborting to avoid problems\n", stderr);
				exit(EXIT_FAILURE);
			}
			global_token = read_all_tokens(in, global_token, name);
			fclose(in);
			i += 2;
		}
		else if(match(argv[i], "-o") || match(argv[i], "--output"))
		{
			destination_name = argv[i + 1];
			destination_file = fopen(destination_name, "w");
			if(NULL == destination_file)
			{
				fputs("Unable to open for writing file: ", stderr);
				fputs(argv[i + 1], stderr);
				fputs("\n Aborting to avoid problems\n", stderr);
				exit(EXIT_FAILURE);
			}
			i += 2;
		}
		else if(match(argv[i], "--max-string"))
		{
			hold = argv[i+1];
			if(NULL == hold)
			{
				fputs("--max-string requires a numeric argument\n", stderr);
				exit(EXIT_FAILURE);
			}
			MAX_STRING = strtoint(hold);
			require(0 < MAX_STRING, "Not a valid string size\nAbort and fix your --max-string\n");
			i += 2;
		}
		else if(match(argv[i], "-h") || match(argv[i], "--help"))
		{
			fputs(" -f input file\n -o output file\n --help for this message\n --version for file version\n-E or --preprocess-only\n--max-string N (N is a number)\n--fuzz\n--no-debug\n", stdout);
			exit(EXIT_SUCCESS);
		}
		else if(match(argv[i], "-V") || match(argv[i], "--version"))
		{
			fputs("M2-Mesoplanet v1.10.0\n", stderr);
			exit(EXIT_SUCCESS);
		}
		else if(match(argv[i], "--fuzz"))
		{
			/* Set fuzzing */
			FUZZING = TRUE;
			i += 1;
		}
		else if(match(argv[i], "--no-debug"))
		{
			/* strip things down */
			debug_flag = FALSE;
			i += 1;
		}
		else
		{
			fputs("UNKNOWN ARGUMENT\n", stdout);
			exit(EXIT_FAILURE);
		}
	}

	/* Deal with special case of wanting to read from standard input */
	if(stdin == in)
	{
		hold_string = calloc(MAX_STRING, sizeof(char));
		require(NULL != hold_string, "Impossible Exhaustion has occured\n");
		global_token = read_all_tokens(in, global_token, "STDIN");
	}

	if(NULL == global_token)
	{
		fputs("Either no input files were given or they were empty\n", stderr);
		exit(EXIT_FAILURE);
	}
	global_token = reverse_list(global_token);

	global_token = remove_line_comments(global_token);

	/* Get the environmental bits */
	populate_env(envp);
	setup_env(envp);
	M2LIBC_PATH = env_lookup("M2LIBC_PATH");
	if(NULL == M2LIBC_PATH) M2LIBC_PATH = "./M2libc";

	preprocess();

	if(PREPROCESSOR_MODE)
	{
		fputs("\n/* Preprocessed source */\n", destination_file);
		output_tokens(global_token, destination_file);
		fclose(destination_file);
	}
	else
	{
		name = calloc(100, sizeof(char));
		strcpy(name, "/tmp/M2-Mesoplanet-XXXXXX");
		i = mkstemp(name);
		tempfile = fdopen(i, "w+");
		if(NULL != tempfile)
		{
			/* Our preprocessed crap */
			output_tokens(global_token, tempfile);
			fclose(tempfile);

			/* Make me a real binary */
			spawn_processes(debug_flag, name, destination_name, envp);

			/* And clean up the donkey */
			remove(name);
		}
		else
		{
			fputs("unable to get a tempfile for M2-Mesoplanet output\n", stderr);
			exit(EXIT_FAILURE);
		}
	}
	return EXIT_SUCCESS;
}
