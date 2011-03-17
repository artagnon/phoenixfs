#include "btree.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

int main (int argc, char **argv)
{
	char *input_file;
	FILE *fp;
	node *root;
	int input;
	char instruction;
	bool verbose_output = true;
	struct dir_record *dr;
	char path[PATH_MAX];

	root = NULL;

	if (argc > 2) {
		input_file = argv[2];
		fp = fopen(input_file, "r");
		if (fp == NULL) {
			perror("Failure to open input file.");
			exit(-1);
		}
		while (!feof(fp)) {
			fscanf(fp, "%d %s\n", &input, path);
			dr = malloc(sizeof(struct dir_record));
			memcpy(dr->name, path, strlen(path));
			root = insert(root, input, dr);
		}
		fclose(fp);
		print_tree(root);
	}

	printf("> ");
	while (scanf("%c", &instruction) != EOF) {
		switch (instruction) {
		case 'd':
			scanf("%d", &input);
			root = delete(root, input);
			print_tree(root);
			break;
		case 'i':
			scanf("%d %s", &input, path);
			dr = malloc(sizeof(struct dir_record));
			memcpy(dr->name, path, strlen(path));
			root = insert(root, input, dr);
			print_tree(root);
			break;
		case 'f':
		case 'p':
			scanf("%d", &input);
			dr = find(root, input, instruction == 'p');
			if (dr == NULL)
				printf("Record not found under key %d.\n", input);
			else
				printf("Record at %lx -- key %d, value %s.\n",
					(unsigned long)dr, input, dr->name);
			break;
		case 'l':
			print_leaves(root);
			break;
		case 'q':
			while (getchar() != (int)'\n');
			return EXIT_SUCCESS;
		case 't':
			print_tree(root);
			break;
		case 'v':
			verbose_output = !verbose_output;
			break;
		case 'x':
			destroy_tree(root);
			print_tree(NULL);
			break;
		default:
			usage_2();
			break;
		}
		while (getchar() != (int)'\n');
		printf("> ");
	}
	printf("\n");
	return 0;
}
