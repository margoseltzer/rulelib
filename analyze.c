/*
 * Program to read in lines of rule/evaluation pairs where rule is a string
 * representing a rule and the evaluation is a vector of 1's and 0's indicating
 * whether the ith sample evaluates to true or false for the given rule.
 *
 * Once we have a collection of rules, we create random rulesets so that we
 * can apply basic transformations to those rule sets: add a rule, remove a
 * rule, swap the order of two rules.
 */

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "rule.h"

/* Convenient macros. */
#define RANDOM_RANGE(lo, hi) \
	(unsigned)(lo + long((random() / (float)RAND_MAX) * (hi - lo + 1)))
#define DEFAULT_RULESET_SIZE  3

/*
 * Usage: analyze <file> -s <ruleset-size> -i <input operations> -S <seed>
 */
int
usage(void)
{
	(void)fprintf(stderr, "Usage: analyze [-s ruleset-size] %s\n",
	    "[-i cmdfile] [-S seed]");
	return (-1);
}

int
main (int argc, char *argv[])
{
	extern char *optarg;
	extern int optind, optopt, opterr, optreset;
	int ret, size = DEFAULT_RULESET_SIZE;
	int nrules, nsamples;
	char ch, *cmdfile = NULL, *infile;
	rule_t *rules;

	while ((ch = getopt(argc, argv, "i:s:S:")) != EOF)
		switch (ch) {
		case 'i':
			cmdfile = optarg;
			break;
		case 's':
			size = atoi(optarg);
			break;
		case 'S':
			srandom((unsigned)(atoi(optarg)));
			break;
		case '?':
		default:
			return (usage());
		}

	argc -= optind;
	argv += optind;

	if (argc == 0)
		return (usage());

	infile = argv[0];

	if ((ret = rules_init(infile, &nrules, &nsamples, &rules)) != 0)
		return (ret);

	printf("%d rules %d samples\n", nrules, nsamples);
}

