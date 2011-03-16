#include "btree.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main (int argc, char **argv)
{
	char *input_file;
	unsigned char sha1[20];
	FILE *fp;
	node *root;
	record *r;
	int input;
	char instruction;
	int order = 4;
	bool verbose_output = true;

	root = NULL;

	if (argc > 1) {
		order = atoi(argv[1]);
		if (order < 3 || order > 20)
			exit(-1);
	}

	if (argc > 2) {
		input_file = argv[2];
		fp = fopen(input_file, "r");
		if (fp == NULL) {
			perror("Failure to open input file.");
			exit(-1);
		}
		while (!feof(fp)) {
			fscanf(fp, "%d %s\n", &input, sha1);
			r = malloc(sizeof(struct record));
			memcpy(r->sha1, sha1, 20);
			root = insert(root, input, r);
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
			scanf("%d %s", &input, sha1);
			r = malloc(sizeof(struct record));
			memcpy(r->sha1, sha1, 20);
			root = insert(root, input, r);
			print_tree(root);
			break;
		case 'f':
		case 'p':
			scanf("%d", &input);
			r = find(root, input, instruction == 'p');
			if (r == NULL)
				printf("Record not found under key %d.\n", input);
			else
				printf("Record at %lx -- key %d, value %s.\n",
					(unsigned long)r, input, r->sha1);
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
