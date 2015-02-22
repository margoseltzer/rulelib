/*
 * Copyright 2015 President and Fellows of Harvard College.
 * All rights reserved.
 */

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "rule.h"

/* Function declarations. */
v_entry *ascii_to_vector(char *, size_t, int *, int *);
#define RULE_INC 100
#define BITS_PER_ENTRY (sizeof(v_entry) * 8)

/* One-counting tools */
int byte_ones[] = {
/*   0 */ 0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4,
/*  16 */ 1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
/*  32 */ 1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
/*  48 */ 2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6, 
/*  64 */ 1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,	
/*  80 */ 2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6, 
/*  96 */ 2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
/* 112 */ 3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
/* 128 */ 1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
/* 144 */ 2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
/* 160 */ 2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
/* 176 */ 3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
/* 192 */ 2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
/* 208 */ 3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
/* 224 */ 3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
/* 140 */ 4, 5, 5, 6, 5, 6, 6, 7, 5, 6, 6, 7, 6, 7, 7, 8 };

#define BYTE_MASK	0xFF


/*
 * Preprocessing step.
 * INPUTS: Using the python from the BRL_code.py: Call get_freqitemsets
 * to generate data files of the form:
 * 	Rule<TAB><bit vector>\n
 *
 * OUTPUTS: an array of rule_t's
 */
int
rules_init(const char *infile, int *nrules, int *nsamples, rule_t **rules_ret)
{
	FILE *fi;
	char *line, *rulestr;
	int rule_cnt, sample_cnt, rsize;
	int i, ones, ret;
	rule_t *rules;
	size_t len, rulelen;

	rule_cnt = sample_cnt = rsize = 0;

	if ((fi = fopen(infile, "r")) == NULL)
		return (errno);

	while ((line = fgetln(fi, &len)) != NULL) {
		if (rule_cnt >= rsize) {
			rsize += RULE_INC;
			rules = realloc(rules, rsize * sizeof(rule_t));
			if (rules == NULL)
				goto err;
		}
		/* Get the rule string; line will contain the bits. */
		if ((rulestr = strsep(&line, "\t")) == NULL)
			goto err;
		rulelen = strlen(rulestr) + 1;
		len -= rulelen;
		if ((rules[rule_cnt].features = malloc(rulelen)) == NULL)
			goto err;
		(void)strncpy(rules[rule_cnt].features, rulestr, rulelen);
		rules[rule_cnt].truthtable = 
		    ascii_to_vector(line, len, &sample_cnt, &ones);
		if (rules[rule_cnt].truthtable == NULL)
			goto err;
		rules[rule_cnt].support = ones;
		rule_cnt++;
	}
	/* All done! */
	fclose(fi);

	*nsamples = sample_cnt;
	*nrules = rule_cnt;
	*rules_ret = rules;

	return (0);

err:
	ret = errno;

	/* Reclaim space. */
	if (rules != NULL) {
		for (i = 0; i < rule_cnt; i++) {
			free(rules[i].features);
			free(rules[i].truthtable);
		}	
		free(rules);
	}
	(void)fclose(fi);
	return (ret);
}

/*
 * Convert an ascii sequence of 0's and 1's to a bit vector.
 * This is a hand-coded naive implementation; we'll also support
 * the GMP library, switching between the two with a compiler directive.
 */
v_entry *
ascii_to_vector(char *line, size_t len, int *nsamples, int *nones)
{
	/*
	 * If *nsamples is 0, then we will set it to the number of
	 * 0's and 1's. If it is non-zero, then we'll ensure that
	 * the line is the right length.
	 */

	char *p;
	int i, bufsize, last_i, ones;
	v_entry val;
	v_entry *bufp, *buf;

	/* NOT DONE */
	assert(line != NULL);

	/* Compute bufsize in number of unsigned elements. */
	if (*nsamples == 0)
		bufsize = (len + BITS_PER_ENTRY - 1) / BITS_PER_ENTRY;
	else
		bufsize = (*nsamples + BITS_PER_ENTRY - 1) / BITS_PER_ENTRY;
	buf = malloc(bufsize * sizeof(v_entry));
	if (buf == NULL)
		return(NULL);
	
	bufp = buf;
	val = 0;
	i = 0;
	last_i = 0;
	ones = 0;


	for(p = line; len-- > 0; p++) {
		switch (*p) {
			case '0':
				val <<= 1;
				i++;
				break;
			case '1':
				val <<= 1;
				val++;
				i++;
				ones++;
				break;
			default:
				break;
		}
		/* If we have filled up val, store it and reset it. */
		if (last_i != i && (i % BITS_PER_ENTRY) == 0) {
			*bufp = val;
			val = 0;
			bufp++;
			last_i = i;
		}
	}

	/* Store val if it contains any bits. */
	if ((i % BITS_PER_ENTRY) != 0)
		*bufp = val;

	if (*nsamples == 0)
		*nsamples = i;
	else if (*nsamples != i) {
		fprintf(stderr, "Wrong number of samples. Expected %d got %d\n",
		    *nsamples, i);
		/* free(buf); */
		buf = NULL;
	}
	*nones = ones;
	return (buf);
}

int
ruleset_init(int nrules,
    int nsamples, int *idarray, rule_t *rules, ruleset_t **retruleset)
{
	int i;
	rule_t *cur_rule;
	ruleset_t *rs;
	ruleset_entry_t *cur_re;
	v_entry *all_captured = NULL;

	/*
	 * Allocate space for the ruleset structure and the ruleset entries.
	 */
	rs = malloc(sizeof(ruleset_t) + nrules * sizeof(ruleset_entry_t));
	if (rs == NULL)
		return (errno);

	/*
	 * Allocate the ruleset at the front of the structure and then
	 * the ruleset_entry_t array at the end.
	 */
	rs->n_rules = nrules;
	rs->n_samples = nsamples;
	rs->rules = (ruleset_entry_t *)(rs + 1);

	for (i = 0; i < nrules; i++) {
		cur_rule = rules + idarray[i];
		cur_re = rs->rules + i;
		cur_re->rule_id = idarray[i];

		if (i == 0) {
			cur_re->captures = rule_tt_copy(cur_rule, nsamples);
			/* ERROR CHECK */
			cur_re->ncaptured = cur_rule->support;
			all_captured = rule_tt_copy(cur_rule, nsamples);
			/* ERROR CHECK */
		}
		else {
			cur_re->captures = rule_tt_andnot(cur_rule,
			    all_captured, nsamples, NULL, &cur_re->ncaptured);
			/* ERROR CHECK */

			/* MIS -- combine these two? */
			/* Skip this on the last one. */
			if (i != nrules - 1)
				all_captured = rule_vor(all_captured,
				    cur_re->captures, nsamples);
		}
	}
	*retruleset = rs;
	if (all_captured != NULL)
		free(all_captured);
	return (0);
}

v_entry *
rule_tt_copy(rule_t *rule, int len)
{
	v_entry *newv, *tt;
	int i, nentries;

	nentries = (len + BITS_PER_ENTRY - 1)/BITS_PER_ENTRY;
	newv = calloc(nentries, sizeof(v_entry));
	if (newv != NULL) {
		for (i = 0; i < nentries; i++)
			newv[i] = rule->truthtable[i];
	}
	return (newv);
}

/*
 * Given a rule and a captures array, compute (rule.tt & ~captures)
 */
v_entry *
rule_tt_andnot(rule_t *rule, v_entry *captures,
    int nsamples, v_entry *ret_vec, int *ret_support)
{
	v_entry *tt;
	int i, count, nentries;

	nentries = (nsamples + BITS_PER_ENTRY - 1)/BITS_PER_ENTRY;
	if (ret_vec == NULL)
		ret_vec = calloc(nentries, sizeof(v_entry));
	if (ret_vec != NULL) {
		count = 0;
		for (i = 0; i < nentries; i++) {
			ret_vec[i] = rule->truthtable[i] & (~captures[i]);
			count += count_ones(ret_vec[i]);
		}
		*ret_support = count;
	}
	return (ret_vec);
}

/*
 * Given a rule and a captures array, compute (rule.tt | captures)
 */
v_entry *
rule_tt_or(rule_t *rule, v_entry *captures,
    int nsamples, v_entry *ret_vec, int *ret_support)
{
	v_entry *tt;
	int i, count, nentries;

	nentries = (nsamples + BITS_PER_ENTRY - 1)/BITS_PER_ENTRY;
	if (ret_vec == NULL)
		ret_vec = calloc(nentries, sizeof(v_entry));
	if (ret_vec != NULL) {
		count = 0;
		for (i = 0; i < nentries; i++) {
			ret_vec[i] = rule->truthtable[i] | captures[i];
			count += count_ones(ret_vec[i]);
		}
		*ret_support = count;
	}
	return (ret_vec);
}

/*
 * Given a rule and a captures array, compute (rule.tt & captures)
 */
v_entry *
rule_tt_and(rule_t *rule, v_entry *captures,
    int nsamples, v_entry *ret_vec, int *ret_support)
{
	v_entry *tt;
	int i, count, nentries;

	nentries = (nsamples + BITS_PER_ENTRY - 1)/BITS_PER_ENTRY;
	if (ret_vec == NULL)
		ret_vec = calloc(nentries, sizeof(v_entry));
	if (ret_vec != NULL) {
		count = 0;
		for (i = 0; i < nentries; i++) {
			ret_vec[i] = rule->truthtable[i] & captures[i];
			count += count_ones(ret_vec[i]);
		}
		*ret_support = count;
	}
	return (ret_vec);
}

/*
 * Swap rules i and j such that i + 1 = j.
 * 	newlycaught = (forall k<=i  k.captures) & j.tt
 *	i.captures = i.captures & ~j.captures
 * 	j.captures = j.captures | newlycaught
 * 	then swap positions i and j
 */
int
rule_swap(ruleset_t *rs, int i, int j, rule_t *rules)
{
	int ndx, nset;
	v_entry *caught, *orig_i, *orig_j, *tt_j;
	ruleset_entry_t re;

	assert(i <= rs->n_rules);
	assert(j <= rs->n_rules);
	assert(i + 1 == j);

	orig_i = rs->rules[i].captures;
	orig_j = rs->rules[j].captures;

	/*
	 * Compute newly caught in two steps: first compute everything
	 * caught in rules 0 to i-1, the compute everything from rule J
	 * that was caught by i and was NOT caught by the previous sum
	 */
	caught = NULL;
	if (i != 0) {
		caught = 
		    rule_tt_copy(rules + rs->rules[0].rule_id, rs->n_samples);
		if (caught == NULL)
			return (errno);
		for (ndx = 1; ndx < i; ndx++)
			caught = rule_vor(caught, 
			    rs->rules[ndx].captures, rs->n_samples);
	}

	orig_i = rule_vandnot(orig_i,
	    rules[rs->rules[j].rule_id].truthtable, rs->n_samples, &nset);
	rs->rules[i].ncaptured = nset;

	/*
	 * If we are about to become the first rule, then our captures array
	 * is simply our initial truth table.
	 */
	if (caught == NULL) {
		/* This is wasteful -- doing an extra allocation. */
		rs->rules[j].captures =
		    rule_tt_copy(rules + rs->rules[j].rule_id, rs->n_samples);
		rs->rules[j].ncaptured = rules[rs->rules[j].rule_id].support;
		free(orig_j);
		orig_j = NULL;
	} else {
		orig_j = rule_tt_andnot(rules + rs->rules[j].rule_id,
		    caught, rs->n_samples, orig_j, &nset);
		rs->rules[j].ncaptured = nset;
	}

	/* Now swap the two entries */
	re = rs->rules[i];
	rs->rules[i] = rs->rules[j];
	rs->rules[j] = re;

	free(caught);
	return (0);
}

v_entry *
rule_vor(v_entry *cumulative, v_entry *new, int nsamples)
{
	int i, count, nentries;

	nentries = (nsamples + BITS_PER_ENTRY - 1)/BITS_PER_ENTRY;
	for (i = 0; i < nentries; i++)
		cumulative[i] = cumulative[i] | new[i];
	return (cumulative);
}

v_entry *
rule_vandnot(v_entry *cumulative, v_entry *new, int nsamples, int *ret_cnt)
{
	int i, count, nentries;

	nentries = (nsamples + BITS_PER_ENTRY - 1)/BITS_PER_ENTRY;
	count = 0;
	for (i = 0; i < nentries; i++) {
		cumulative[i] = cumulative[i] & ~new[i];
		count += count_ones(cumulative[i]);
	}

	*ret_cnt = count;
	return (cumulative);
}

int
count_ones(v_entry val)
{
	int count, i;

	count = 0;
	for (i = 0; i < sizeof(v_entry); i++) {
		count += byte_ones[val & BYTE_MASK];
		val >>= 8;
	}
	return (count);
}

void
ruleset_print(ruleset_t *rs, rule_t *rules)
{
	int i, j, n;
	rule_t *r;

	printf("%d rules %d samples\n", rs->n_rules, rs->n_samples);
	n = (rs->n_samples + BITS_PER_ENTRY - 1) / BITS_PER_ENTRY;

	for (i = 0; i < rs->n_rules; i++) {
		rule_print(rules, rs->rules[i].rule_id, n);
		ruleset_entry_print(rs->rules + i, n);
	}
}

void
ruleset_entry_print(ruleset_entry_t *re, int n)
{
	printf("%d captured: ", re->ncaptured);
	rule_vector_print(re->captures, n);
}

void
rule_print(rule_t *rules, int ndx, int n)
{
	rule_t *r;

	r = rules + ndx;
	printf("RULE %d: %s %d: ", ndx, r->features, r->support);
	rule_vector_print(r->truthtable, n);
}

void
rule_vector_print(v_entry *v, int n)
{
	int i;

	for (i = 0; i < n; i++)
		printf("0x%lx ", v[i]);
	printf("\n");

}

void
rule_print_all(rule_t *rules, int nrules, int nsamples)
{
	int i, n;

	n = (nsamples + BITS_PER_ENTRY - 1) / BITS_PER_ENTRY;
	for (i = 0; i < nrules; i++)
		rule_print(rules, i, n);
}


