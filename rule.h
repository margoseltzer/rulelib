/*
 * Copyright 2015 President and Fellows of Harvard College.
 * All rights reserved.
 */

/*
 * This library implements rule set management for Bayesian rule lists.
 */

/*
 * Rulelist is an ordered collection of rules.
 * A Rule is simply and ID combined with a large binary vector of length N
 * where N is the number of samples and a 1 indicates that the rule captures
 * the sample and a 0 indicates that it does not.
 *
 * Definitions:
 * captures(R, S) -- A rule, R, captures a sample, S, if the rule evaluates
 * true for Sample S.
 * captures(N, S, RS) -- In ruleset RS, the Nth rule captures S.
 */

/*
 * Even though every rule in a given experiment will have the same number of
 * samples (n_ys), we include it in the rule definition. Note that the size of
 * the captures array will be n_ys/sizeof(unsigned).
 *
 * Note that a rule outside a rule set stores captures(R, S) while a rule in
 * a rule set stores captures(N, S, RS).
 */

/*
 * We have slightly different structures to represent the original rules 
 * and rulesets. The original structure contains the ascii representation
 * of the rule; the ruleset structure refers to rules by ID and contains
 * captures which is something computed off of the rule's truth table.
 */
typedef struct rule {
	char *features;			/* Representation of the rule. */
	unsigned long *truthtable;	/* Truth table; one bit per sample. */
} rule_t;

typedef struct ruleset_entry {
	unsigned rule_id;
	unsigned long *captures;	/* Bit vector. */
} ruleset_entry_t;

typedef struct ruleset {
	int n_rules;
	ruleset_entry_t *rules;	/* Array of rules. */
} ruleset_t;

/*
 * Functions in the library
 */
int rules_init(const char *, int *, int *, rule_t **);
