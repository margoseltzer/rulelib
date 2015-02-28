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
#include "mytime.h"
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
	struct timeval tv_acc, tv_start, tv_end;

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

	INIT_TIME(tv_acc);
	START_TIME(tv_start);
	if ((ret = rules_init(infile, &nrules, &nsamples, &rules)) != 0)
		return (ret);
	END_TIME(tv_start, tv_end, tv_acc);
	REPORT_TIME("analyze", "per rule", tv_acc, nrules);

	printf("%d rules %d samples\n", nrules, nsamples);
	if (debug)
		rule_print_all(rules, nrules, nsamples);

	/*
	 * Add number of iterations for first parameter
	 */
	run_experiment(10, size, nsamples, nrules, rules);
}

int
create_random_ruleset(int size,
    int nsamples, int nrules, rule_t *rules, ruleset_t **rs)
{
	int i, j, *ids, next, ret;

	ids = calloc(size, sizeof(int));
	for (i = 0; i < size; i++) {
try_again:	next = RANDOM_RANGE(0, (nrules - 1));
		/* Check for duplicates. */
		for (j = 0; j < i; j++)
			if (ids[j] == next)
				goto try_again;
		ids[i] = next;
	}

	return(ruleset_init(size, nsamples, ids, rules, rs));
}

/*
 * Given a rule set, pick a random rule (not already in the set) and
 * add it at the ndx-th position.
 */
int
add_random_rule(rule_t *rules, int nrules, ruleset_t *rs, int ndx)
{
	int j, new_rule;

pickrule:
	new_rule = RANDOM_RANGE(0, (nrules-1));
	for (j = 0; j < rs->n_rules; j++)
		if (rs->rules[j].rule_id == new_rule)
			goto pickrule;
	if (debug)
		printf("\nAdding rule: %d\n", new_rule);
	return(ruleset_add(rules, nrules, rs, new_rule, ndx));
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
	struct timeval tv_acc, tv_start, tv_end;

	for (i = 0; i < iters; i++) {
		ret = create_random_ruleset(size, nsamples, nrules, rules, &rs);
		if (ret != 0)
			return;
		if (debug) {
			printf("Initial ruleset\n");
			ruleset_print(rs, rules);
		}

		/* Now perform-size squared swaps */
		INIT_TIME(tv_acc);
		START_TIME(tv_start);
		for (j = 0; j < size; j++)
			for (k = 1; k < size; k++) {
				if (debug)
					printf("\nSwapping rules %d and %d\n",
					    rs->rules[k-1].rule_id,
					    rs->rules[k].rule_id);
				if (ruleset_swap(rs, k - 1, k, rules))
					return;
				if (debug)
					ruleset_print(rs, rules);
			}
		END_TIME(tv_start, tv_end, tv_acc);
		REPORT_TIME("analyze", "per swap", tv_acc, (size * size));

		/*
		 * Now remove a rule from each position, replacing it
		 * with a random rule at the end.
		 */
		INIT_TIME(tv_acc);
		START_TIME(tv_start);
		for (j = 0; j < size; j++) {
			if (debug)
				printf("\nDeleting rule %d\n", j);
			ruleset_delete(rules, nrules, rs, j);
			if (debug) 
				ruleset_print(rs, rules);
			add_random_rule(rules, nrules, rs, j);
			if (debug)
				ruleset_print(rs, rules);
		}
		END_TIME(tv_start, tv_end, tv_acc);
		REPORT_TIME("analyze", "per add/del", tv_acc, (size * 2));
	}

}
