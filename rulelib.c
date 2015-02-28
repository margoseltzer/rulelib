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

/* Malloc a vector to contain nsamples bits. */
v_entry *
rule_vinit(int len)
{
	int nentries;

	nentries = (len + BITS_PER_ENTRY - 1)/BITS_PER_ENTRY;
	return (calloc(nentries, sizeof(v_entry)));
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
	int i, tmp;
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
	rs->n_alloc = nrules;
	rs->n_samples = nsamples;
	rs->rules = (ruleset_entry_t *)(rs + 1);

	for (i = 0; i < nrules; i++) {
		cur_rule = rules + idarray[i];
		cur_re = rs->rules + i;
		cur_re->rule_id = idarray[i];

		if (i == 0) {
			if ((cur_re->captures = rule_vinit(nsamples)) == NULL)
				goto err1;
			cur_re->captures =
			    rule_copy(cur_rule->truthtable, nsamples);
			cur_re->ncaptured = cur_rule->support;
			all_captured =
			    rule_copy(cur_rule->truthtable, nsamples);
			if (all_captured == NULL)
				goto err1;
		} else {
			if ((cur_re->captures = rule_vinit(nsamples)) == NULL)
				goto err1;
			rule_vandnot(cur_re->captures, cur_rule->truthtable,
			    all_captured, nsamples, &cur_re->ncaptured);

			/* Skip this on the last one. */
			if (i != nrules - 1)
				rule_vor(all_captured,
				    all_captured, cur_re->captures, nsamples, &tmp);
		}
	}
	*retruleset = rs;
	if (all_captured != NULL)
		free(all_captured);
	return (0);

err1:
	if (all_captured != NULL)
		free(all_captured);
	for (int j = 0; j <= i; j++)
		if (rs->rules[i].captures != NULL)
			free (rs->rules[i].captures);

	return (ENOMEM);
}

/*
 * Add the specified rule to the ruleset at position ndx (shifting
 * all rules after ndx down by one).
 */
int
ruleset_add(rule_t *rules, int nrules, ruleset_t *rs, int newrule, int ndx)
{
	int i, tmp;
	rule_t *expand;
	v_entry *captured;

	/* Check for space. */
	if (rs->n_alloc < rs->n_rules + 1) {
		expand = realloc(rs->rules, 
		    (rs->n_rules + 1) * sizeof(ruleset_entry_t));
		if (expand == NULL)
			return (errno);			
		rs->n_alloc = rs->n_rules + 1;
	}

	/* Shift later rules down by 1. */
	if (ndx != rs->n_rules)
		memmove(rs->rules + (ndx + 1), rs->rules + ndx,
		    sizeof(ruleset_entry_t) * rs->n_rules - ndx);

	/*
	 * Insert new rule.
	 * 1. Compute what is already captured by earlier rules.
	 * 2. Add rule into ruleset.
	 * 3. Compute new captures for all rules following the new one.
	 */
	if (ndx == 0)
		captured = rule_copy(rules[newrule].truthtable, rs->n_samples);
		if (captured == NULL)
			return (errno);
	else  {
		captured = rule_copy(rules[rs->rules[0].rule_id].truthtable,
		        rs->n_samples);
		if (captured == NULL)
			return (errno);

		for (i = 1; i < ndx; i++)
			rule_vor(captured, captured,
			    rs->rules[i].captures, rs->n_samples, &tmp);
		rule_vandnot(captured, rules[newrule].truthtable,
		    captured, rs->n_samples, &rs->rules[ndx].ncaptured);

	}
	rs->rules[ndx].rule_id = newrule;
	rs->rules[ndx].captures = captured;
	rs->n_rules++;

	for (i = ndx + 1; i < rs->n_rules; i++)
		rule_vandnot(rs->rules[i].captures,
		    rules[rs->rules[i].rule_id].truthtable, rs->rules[i-1].captures,
		    rs->n_samples, &rs->rules[i].ncaptured);
		
	return(0);
}

/*
 * Delete the rule in the ndx-th position in the given ruleset.
 */
void
ruleset_delete(rule_t *rules, int nrules, ruleset_t *rs, int ndx)
{
	int i, nset;
	v_entry *curvec, *oldv, *tmp_vec;

	/* Compute new captures for all rules following the one at ndx.  */
	oldv = rs->rules[ndx].captures;
	if ((tmp_vec = rule_vinit(rs->n_samples)) == NULL)
		return;
	for (i = ndx + 1; i < rs->n_rules; i++) {
		/*
		 * My new captures is my old captures or'd with anything that
		 * was captured by ndx and is captured by my rule.
		 */
		curvec = rs->rules[i].captures;
		rule_vand(tmp_vec, rules[rs->rules[i].rule_id].truthtable,
		    oldv, rs->n_samples, &nset);
		rs->rules[i].ncaptured += nset;
		rule_vor(curvec, curvec, tmp_vec, rs->n_samples, &nset);

		/*
		 * Now remove the ones from oldv that just got set for rule
		 * i because they should not be captured later.
		 */
		rule_vandnot(oldv, oldv, curvec, rs->n_samples, &nset);
	}

	/* Now remove alloc'd data for rule at ndx. */
	(void)free(tmp_vec);
	(void)free(oldv);

	/* Shift up cells if necessary. */
	if (ndx != rs->n_rules - 1)
		memmove(rs->rules + ndx, rs->rules + ndx + 1,
		    sizeof(ruleset_entry_t) * (rs->n_rules - ndx));

	rs->n_rules--;
	return;
}

v_entry *
rule_copy(v_entry *vector, int len)
{
	v_entry *newv, *tt;
	int i, nentries;

	nentries = (len + BITS_PER_ENTRY - 1)/BITS_PER_ENTRY;
	newv = rule_vinit(len);
	if (newv != NULL) {
		for (i = 0; i < nentries; i++)
			newv[i] = vector[i];
	}
	return (newv);
}

/*
 * Swap rules i and j such that i + 1 = j.
 * 	newlycaught = (forall k<=i  k.captures) & j.tt
 *	i.captures = i.captures & ~j.captures
 * 	j.captures = j.captures | newlycaught
 * 	then swap positions i and j
 */
int
ruleset_swap(ruleset_t *rs, int i, int j, rule_t *rules)
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
		caught = rule_copy(rules[rs->rules[0].rule_id].truthtable,
		    rs->n_samples);
		if (caught == NULL)
			return (errno);
		for (ndx = 1; ndx < i; ndx++)
			rule_vor(caught, caught,
			    rs->rules[ndx].captures, rs->n_samples, &nset);
	}

	rule_vandnot(orig_i, orig_i,
	    rules[rs->rules[j].rule_id].truthtable, rs->n_samples, &nset);
	rs->rules[i].ncaptured = nset;

	/*
	 * If we are about to become the first rule, then our captures array
	 * is simply our initial truth table.
	 */
	if (caught == NULL) {
		/* This is wasteful -- doing an extra allocation. */
		rs->rules[j].captures =
		    rule_copy(rules[rs->rules[j].rule_id].truthtable,
		    rs->n_samples);
		rs->rules[j].ncaptured = rules[rs->rules[j].rule_id].support;
		free(orig_j);
		orig_j = NULL;
	} else {
		rule_vandnot(orig_j, rules[rs->rules[j].rule_id].truthtable,
		    caught, rs->n_samples, &nset);
		rs->rules[j].ncaptured = nset;
	}

	/* Now swap the two entries */
	re = rs->rules[i];
	rs->rules[i] = rs->rules[j];
	rs->rules[j] = re;

	free(caught);
	return (0);
}

void
rule_vand(v_entry *dest, v_entry *src1, v_entry *src2, int nsamples, int *cnt)
{
	int i, count, nentries;

	count = 0;
	nentries = (nsamples + BITS_PER_ENTRY - 1)/BITS_PER_ENTRY;
	if (dest == NULL)
		dest = rule_vinit(nsamples);
	if (dest != NULL) {
		for (i = 0; i < nentries; i++) {
			dest[i] = src1[i] & src2[i];
			count += count_ones(dest[i]);
		}
		*cnt = count;
	}
	return;
}

void
rule_vor(v_entry *dest, v_entry *src1, v_entry *src2, int nsamples, int *cnt)
{
	int i, count, nentries;

	count = 0;
	nentries = (nsamples + BITS_PER_ENTRY - 1)/BITS_PER_ENTRY;
	if (dest == NULL)
		dest = rule_vinit(nentries);
	if (dest != NULL) {
		for (i = 0; i < nentries; i++) {
			dest[i] = src1[i] | src2[i];
			count += count_ones(dest[i]);
		}
		*cnt = count;
	}
	return;
}

void
rule_vandnot(v_entry *dest,
    v_entry *src1, v_entry *src2, int nsamples, int *ret_cnt)
{
	int i, count, nentries;

	nentries = (nsamples + BITS_PER_ENTRY - 1)/BITS_PER_ENTRY;
	count = 0;
	if (dest == NULL)
		dest = rule_vinit(nentries);
	if (dest != NULL) {
		for (i = 0; i < nentries; i++) {
			dest[i] = src1[i] & ~src2[i];
			count += count_ones(dest[i]);
		}

		*ret_cnt = count;
	}
	return;
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


