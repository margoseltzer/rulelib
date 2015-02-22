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
    (unsigned)(lo + (unsigned)((random() / (float)RAND_MAX) * (hi - lo + 1)))
#define DEFAULT_RULESET_SIZE  3

void run_experiment(int, int, int, int, rule_t *);
int debug;

/*
 * Usage: analyze <file> -s <ruleset-size> -i <input operations> -S <seed>
 */
int
usage(void)
{
	(void)fprintf(stderr, "Usage: analyze [-d] [-s ruleset-size] %s\n",
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

	debug = 0;
	while ((ch = getopt(argc, argv, "di:s:S:")) != EOF)
		switch (ch) {
		case 'd':
			debug = 1;
			break;
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
	if (debug)
		rule_print_all(rules, nrules, nsamples);

	/*
	 * Add number of iterations for first parameter
	 */
	run_experiment(1, size, nsamples, nrules, rules);
}

int
create_random_ruleset(int size,
    int nsamples, int nrules, rule_t *rules, ruleset_t **rs)
{
	int i, j, *ids, next, ret;

	ids = calloc(size, sizeof(int));
	for (i = 0; i < size; i++) {
try_again:	next = RANDOM_RANGE(0, nrules);
		/* Check for duplicates. */
		for (j = 0; j < i; j++)
			if (ids[j] == next)
				goto try_again;
		ids[i] = next;
	}

	return(ruleset_init(size, nsamples, ids, rules, rs));
}

void
add_random_rule(rule_t *rules, int nrules, ruleset_t *rs, int ndx)
{
	return;
}

/*
 * Generate a random ruleset and then do some number of adds, removes,
 * swaps, etc.
 */
void
run_experiment(int iters, int size, int nsamples, int nrules, rule_t *rules)
{
	int i, j, k, ret;
	ruleset_t *rs;

	for (i = 0; i < iters; i++) {
		ret = create_random_ruleset(size, nsamples, nrules, rules, &rs);
		if (ret != 0)
			return;
		if (debug) {
			printf("Initial ruleset\n");
			ruleset_print(rs, rules);
		}

		/* Now perform-size squared swaps */
		for (j = 0; j < size; j++)
			for (k = 1; k < size; k++) {
				if (debug)
					printf("Swapping rules %d and %d\n",
					    rs->rules[k-1].rule_id,
					    rs->rules[k].rule_id);
				if (rule_swap(rs, k - 1, k, rules))
					return;
				if (debug)
					ruleset_print(rs, rules);
			}

		/*
		 * Now remove a rule from each position, replacing it
		 * with a random rule at the end.
		 */
		for (j = 0; j < size; j++) {
			if (debug)
				printf("Deleting rule %d\n", j);
			ruleset_delete(rules, nrules, rs, j);
			if (debug)
				ruleset_print(rs, rules);
			add_random_rule(rules, nrules, rs, j);
		}
	}

}
