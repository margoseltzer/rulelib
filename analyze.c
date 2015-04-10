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
#include <math.h>
#include "mytime.h"
#include "rule.h"
#include <gsl/gsl_rng.h>
#include <gsl/gsl_randist.h>
#include <gsl/gsl_cdf.h>
#include <gsl/gsl_sf.h>
gsl_rng *RAND_GSL;

/* Convenient macros. */
#define RANDOM_RANGE(lo, hi) \
    (unsigned)(lo + (unsigned)((random() / (float)RAND_MAX) * (hi - lo + 1)))
#define DEFAULT_RULESET_SIZE  4
#define DEFAULT_RULE_CARDINALITY 3
#define MAX_RULE_CARDINALITY 10
#define NLABELS 2

#define LAMBDA 3.0
#define ETA 1.0
double ALPHA[2] = {1, 1};

void run_experiment(int, int, int, int, rule_t *);
void run_mcmc(int, int, int, int, rule_t *, rule_t *);
void ruleset_proposal(ruleset_t *, int, int *, int *, char *, double *);
void ruleset_assign(ruleset_t **, ruleset_t *);
void ruleset_backup(ruleset_t *, int **, double, double *);
double compute_log_posterior(ruleset_t *, rule_t *, int, rule_t *);
void init_gsl_rand_gen();
int gen_poission(double);
void gsl_ran_poisson_test();

int debug;

/*
 * Usage: analyze <file> -s <ruleset-size> -i <input operations> -S <seed>
 */
int
usage(void)
{
	(void)fprintf(stderr, "Usage: analyze [-d] [-s ruleset-size] %s\n",
	    "[-c cmdfile] [-i iterations] [-S seed]");
	return (-1);
}

int
main (int argc, char *argv[])
{
	extern char *optarg;
	extern int optind, optopt, opterr, optreset;
	int ret, size = DEFAULT_RULESET_SIZE;
	int iters, nrules, nsamples, nlabels, nsamples_dup;
	char ch, *cmdfile = NULL, *infile;
	rule_t *rules, *labels;
	struct timeval tv_acc, tv_start, tv_end;

	debug = 0;
	iters = 50000;
    while ((ch = getopt(argc, argv, "di:s:S:")) != EOF) {
//        printf("char = %c, %s\n", ch, optarg);
		switch (ch) {
		case 'c':
			cmdfile = optarg;
			break;
		case 'd':
			debug = 1;
			break;
		case 'i':
			iters = atoi(optarg);
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
    }
    printf("iter here = %d", iters);
	argc -= optind;
	argv += optind;

	if (argc == 0)
		return (usage());
    /* read in the data-rule relations */
	infile = argv[0];

	INIT_TIME(tv_acc);
	START_TIME(tv_start);
	if ((ret = rules_init(infile, &nrules, &nsamples, &rules, 1)) != 0)
		return (ret);
	END_TIME(tv_start, tv_end, tv_acc);
	REPORT_TIME("analyze", "per rule", tv_acc, nrules);
    
    rule_print_all(rules, nrules, nsamples);
    
	printf("\n%d rules %d samples\n\n", nrules, nsamples);
	if (debug)
		rule_print_all(rules, nrules, nsamples);
    /* read in the data-label relations */
    infile = argv[1];

    if ((ret = rules_init(infile, &nlabels, &nsamples_dup, &labels, 0)) != 0)
        return (ret);
    
    rule_print_all(labels, nlabels, nsamples);
    printf("\n%d labels %d samples\n\n", nlabels, nsamples);
    
	/*
	 * Add number of iterations for first parameter
	 */
    
	//run_experiment(iters, size, nsamples, nrules, rules);
    int init_size = size;
    run_mcmc(iters, init_size, nsamples, nrules, rules, labels);
}

int
create_random_ruleset(int size,
    int nsamples, int nrules, rule_t *rules, ruleset_t **rs)
{
	int i, j, *ids, next, ret;

	ids = calloc(size, sizeof(int));
	for (i = 0; i < (size - 1); i++) {
try_again:	next = RANDOM_RANGE(1, (nrules - 1));
		/* Check for duplicates. */
		for (j = 0; j < i; j++)
			if (ids[j] == next)
				goto try_again;
		ids[i] = next;
	}

	/* Always put rule 0 (the default) as the last rule. */
	ids[i] = 0;

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
	new_rule = RANDOM_RANGE(1, (nrules-1));
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

		/* Now perform-(size-2) squared swaps */
		INIT_TIME(tv_acc);
		START_TIME(tv_start);
		for (j = 0; j < size; j++)
			for (k = 1; k < (size-1); k++) {
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
		REPORT_TIME("analyze", "per swap", tv_acc, ((size-1) * (size-1)));

		/*
		 * Now remove a rule from each position, replacing it
		 * with a random rule at the end.
		 */
		INIT_TIME(tv_acc);
		START_TIME(tv_start);
		for (j = 0; j < (size - 1); j++) {
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
		REPORT_TIME("analyze", "per add/del", tv_acc, ((size-1) * 2));
	}

}

void
run_mcmc(int iters, int init_size, int nsamples, int nrules, rule_t *rules, rule_t *labels) {
    int i,j,t, ndx1, ndx2;
    char stepchar;
    ruleset_t *rs, *rs_proposal=NULL, *rs_temp=NULL;
    double jump_prob, log_post_rs=0.0, log_post_rs_proposal=0.0;
    int *rs_idarray, len;
    double max_log_posterior = 1e-9;
    /* initialize random number generator for some distrubitions */
    init_gsl_rand_gen();
    //gsl_ran_poisson_test();
    
//    for (int i=0; i<20; i++)
//        printf("%u , ", gen_poisson(7.2));
//    printf("\n");
    
    //initialize_ruleset(&rs, rules, nrules)
    
    /* initialize the ruleset */
    create_random_ruleset(init_size, nsamples, nrules, rules, &rs);
    log_post_rs = compute_log_posterior(rs, rules, nrules, labels);
    ruleset_backup(rs, &rs_idarray, log_post_rs, &max_log_posterior);
    len = rs->n_rules;
//
//    for (int i=0; i<10; i++) {
//        ruleset_proposal(rs, nrules, &ndx1, &ndx2, &stepchar, &jump_prob);
//        printf("\n%d, %d, %d, %c, %f\n", nrules, ndx1, ndx2, stepchar, log(jump_prob));
//    }
    
    
    ruleset_assign(&rs_proposal, rs); // rs_proposel <-- rs
    ruleset_print_4test(rs);
//    printf("\n*****************************************\n");
//    printf("\n %p %p %d\n", rs, rs_proposal, rs_proposal==NULL);
    
//    ruleset_assign(&rs_proposal, rs);  // rs_proposel <-- rs
//    ruleset_print(rs_proposal, rules);
//    printf("\n %p %p \n", rs, rs_proposal);
    printf("iters = %d", iters);
    for (int i=0; i<iters; i++) {
        
//        ruleset_print_4test(rs);
        
        ruleset_proposal(rs, nrules, &ndx1, &ndx2, &stepchar, &jump_prob);
        printf("\nnrules=%d, ndx1=%d, ndx2=%d, action=%c, relativeProbability=%f\n", nrules, ndx1, ndx2, stepchar, log(jump_prob));
        printf("%d rules currently in the ruleset, they are:\n", rs->n_rules);
        for (int j=0; j<rs->n_rules; j++) printf("%u ", rs->rules[j].rule_id); printf("\n");
        
        ruleset_assign(&rs_proposal, rs); // rs_proposel <-- rs
        
        switch (stepchar) {
            case 'A':
                ruleset_add(rules, nrules, rs_proposal, ndx1, ndx2); //add ndx1 rule to ndx2
                break;
            case 'D':
                ruleset_delete(rules, nrules, rs_proposal, ndx1);
                break;
            case 'S':
//                if (ndx1 > ndx2) { temp = ndx1; ndx1 = ndx2; ndx2 = temp;}
                printf("%d <-----> %d\n", ndx1, ndx2);
//                for (int j=ndx1; j<ndx2; j++)
//                    ruleset_swap(rs_proposal, j, j+1, rules);
//                for (int j=ndx2-1; j>ndx1; j--)
//                    ruleset_swap(rs_proposal, j-1, j, rules);
                ruleset_swap_any(rs_proposal, ndx1, ndx2, rules);
                break;
            default:
                break;
        }
////        if (stepchar!='S')
//        int cnt=0;
//        for (int j=0; j<rs_proposal->n_rules; j++) {
//            printf("%d\n", rs_proposal->rules[j].ncaptured);
//            cnt += rs_proposal->rules[j].ncaptured;
//        }
//            printf("############Been here! %d\n", cnt);
//        for (int j=0; j<rs_proposal->n_rules; j++) printf("%u ", rs_proposal->rules[j].rule_id);
//        printf("\n");
        ruleset_print_4test(rs_proposal);
        
        log_post_rs_proposal = compute_log_posterior(rs_proposal, rules, nrules, labels);
//        printf("%f\n", jump_prob);
        if (log((random() / (float)RAND_MAX)) < log_post_rs_proposal-log_post_rs+log(jump_prob)) {
            free(rs);
            rs = rs_proposal;
            log_post_rs = log_post_rs_proposal;
            rs_proposal = NULL;
            ruleset_backup(rs, &rs_idarray, log_post_rs, &max_log_posterior);
            len = rs->n_rules;
            printf("yes!\n");
        }
    }
    /* regenerate the best rule list */
    printf("\n\n/*----The best rule list is: */\n");
    ruleset_init(len, nsamples, rs_idarray, rules, &rs);
    for (int i=0; i < len; i++) printf("rule[%d]_id = %d\n", i, rs_idarray[i]);
    printf("nmax_log_posterior = %6f\n\n", max_log_posterior);
    ruleset_print(rs, rules);
}

void
ruleset_backup(ruleset_t *rs, int **rs_idarray, double log_post_rs, double *max_log_posterior) {
    int *ids = *rs_idarray;
    if (ids!=NULL) free(ids);
    ids = malloc(rs->n_rules*sizeof(int));
    for (int i=0; i < rs->n_rules; i++)
        ids[i] = rs->rules[i].rule_id;
    *rs_idarray = ids;
    *max_log_posterior = log_post_rs;
}

double compute_log_posterior(ruleset_t *rs, rule_t *rules, int nrules, rule_t *labels) {
    double log_prior = 0.0, log_likelihood = 0.0;
    static double eta_norm = 0;
    static double *log_lambda_pmf=NULL, *log_eta_pmf=NULL;
    int i,j,k,li;
    /* prior pre-calculation */
    if (log_lambda_pmf == NULL) {
        log_lambda_pmf = malloc(nrules*sizeof(double));
        log_eta_pmf = malloc((1+MAX_RULE_CARDINALITY)*sizeof(double));
        for (int i=0; i < nrules; i++)
            log_lambda_pmf[i] = log(gsl_ran_poisson_pdf(i, LAMBDA));
        for (int i=0; i <= MAX_RULE_CARDINALITY; i++)
            log_eta_pmf[i] = log(gsl_ran_poisson_pdf(i, ETA));
    }
    /* calculate log_prior */
    int card_count[1 + MAX_RULE_CARDINALITY];
    for (i=0; i <= MAX_RULE_CARDINALITY; i++) card_count[i] = 0;
    int maxcard = 0;
    for (i=0; i < rs->n_rules; i++){
        card_count[rules[rs->rules[i].rule_id].cardinality]++;
        if (rules[rs->rules[i].rule_id].cardinality > maxcard)
            maxcard = rules[rs->rules[i].rule_id].cardinality;
    }
    
    log_prior += log_lambda_pmf[rs->n_rules-1];
    eta_norm = gsl_cdf_poisson_P(maxcard, ETA) - gsl_ran_poisson_pdf(0, ETA);
    for (i=0; i < rs->n_rules-1; i++){ //don't compute the last rule(default rule).
        li = rules[rs->rules[i].rule_id].cardinality;
        log_prior += log_eta_pmf[li] - log(eta_norm);
        log_prior += -log(card_count[li]);
        card_count[li]--;
        if (card_count[li] == 0)
            eta_norm -= exp(log_eta_pmf[li]);
    }
    /* calculate log_likelihood */
    VECTOR v0;
    rule_vinit(rs->n_samples, &v0);
    for (int j=0; j < rs->n_rules; j++) {
        int n0, n1;
        rule_vand(v0, rs->rules[j].captures, labels[0].truthtable, rs->n_samples, &n0);
//        rule_vand(v0, v0, labels[0].truthtable, rs->n_samples, &n0);
        n1 = rs->rules[j].ncaptured - n0;
        log_likelihood += gsl_sf_lngamma(n0+1) + gsl_sf_lngamma(n1+1) - gsl_sf_lngamma(n0+n1+2);
    }
    printf("log_prior = %6f \t log_likelihood = %6f \n", log_prior, log_likelihood);
    return log_prior + log_likelihood;
}

int
ruleset_swap_any(ruleset_t *rs, int i, int j, rule_t *rules)
{
    int temp, cnt, ndx, nset, ret;
    VECTOR caught;
    
    assert(i <= rs->n_rules);
    assert(j <= rs->n_rules);
    if (i>j) { temp=i; i=j; j=temp; }
    assert(i <= j);
    /*
     * Compute newly caught in two steps: first compute everything
     * caught in rules i to j, then compute everything from scratch
     * for rules between rule i and rule j, both inclusive.
     */
    if ((ret = rule_vinit(rs->n_samples, &caught)) != 0)
        return (ret);
    for (int k=i; k<=j; k++)
        rule_vor(caught, caught, rs->rules[k].captures, rs->n_samples, &cnt);
    
    printf("cnt = %d\n", cnt);
    temp = rs->rules[i].rule_id;
    rs->rules[i].rule_id = rs->rules[j].rule_id;
    rs->rules[j].rule_id = temp;
    
    for (int k=i; k<=j; k++) {
        rule_vand(rs->rules[k].captures, caught, rules[rs->rules[k].rule_id].truthtable, rs->n_samples, &rs->rules[k].ncaptured);
        rule_vandnot(caught, caught, rs->rules[k].captures, rs->n_samples, &cnt);
        printf("cnt = %d, captured by this rule = %d\n", cnt, rs->rules[k].ncaptured);
    }
    printf("cnt = %d\n", cnt);
    assert(cnt == 0);
    
#ifdef GMP
    mpz_clear(caught);
#else
    free(caught);
#endif
    return (0);
}


void
ruleset_proposal(ruleset_t *rs, int nrules_mined, int *ndx1, int *ndx2, char *stepchar, double *jumpRatio) {
    static double MOVEPROBS[15] = {
        0.0, 1.0, 0.0,
        0.0, 0.5, 0.5,
        0.5, 0.0, 0.5,
        1.0/3.0, 1.0/3.0, 1.0/3.0,
        1.0/3.0, 1.0/3.0, 1.0/3.0
    };
    static double JUMPRATIOS[15] = {
        0.0, 1/2, 0.0,
        0.0, 2.0/3.0, 2.0,
        1.0, 0.0, 2.0/3.0,
        1.0, 1.5, 1.0,
        1.0, 1.0, 1.0
    };
    double moveProbs[3], jumpRatios[3];
//    double moveProbDefault[3] = {1.0/3.0, 1.0/3.0, 1.0/3.0};
    int offset = 0;
    if (rs->n_rules == 0){
        offset = 0;
    } else if (rs->n_rules == 1){
        offset = 3;
    } else if (rs->n_rules == nrules_mined-1){
        offset = 6;
    } else if (rs->n_rules == nrules_mined-2){
        offset = 9;
    } else {
        offset = 12;
    }
    memcpy(moveProbs, MOVEPROBS+offset, 3*sizeof(double));
    memcpy(jumpRatios, JUMPRATIOS+offset, 3*sizeof(double));
    
    double u = ((double) rand()) / (RAND_MAX);
    int index1, index2;
    if (u < moveProbs[0]){
        /* swap rules */
        index1 = rand() % (rs->n_rules-1); // can't swap with the default rule
        do {
            index2 = rand() % (rs->n_rules-1);
        }
        while (index2==index1);
        *jumpRatio = jumpRatios[0];
        *stepchar = 'S';
    } else if (u < moveProbs[0]+moveProbs[1]) {
        /* add a new rule */
        index1 = rs->n_rules + 1 + rand() % (nrules_mined-rs->n_rules);
        int *allrules = calloc(nrules_mined, sizeof(int));
        for (int i=0; i < rs->n_rules; i++) allrules[rs->rules[i].rule_id] = -1;
        int cnt=0;
        for (int i=0; i < nrules_mined; i++)
            if (allrules[i] != -1)
                allrules[cnt++] = i;
        index1 = allrules[rand() % cnt];
        free(allrules);
        index2 = rand() % rs->n_rules; // can add a rule at the default rule position
        *jumpRatio = jumpRatios[1] * (nrules_mined - 1 - rs->n_rules);
        *stepchar = 'A';
    } else if (u < moveProbs[0]+moveProbs[1]+moveProbs[2]){
        /* delete an existing rule */
        index1 = rand() % (rs->n_rules-1); //cannot delete the default rule
        index2 = 0;// index2 doesn't matter in this case
        *jumpRatio = jumpRatios[2] * (nrules_mined - rs->n_rules);
        *stepchar = 'D';
    } else{
        //should raise exception here.
    }
    *ndx1 = index1;
    *ndx2 = index2;
}

void
ruleset_assign(ruleset_t **ret_dest, ruleset_t *src) {
    ruleset_t *dest = *ret_dest;
    if (dest != NULL){
//        printf("wrong here\n");
//        realloc(dest, sizeof(ruleset_t) + (src->n_rules+1) * sizeof(ruleset_entry_t));
        free(dest);
    }
    dest = malloc(sizeof(ruleset_t) + (src->n_rules+1) * sizeof(ruleset_entry_t));
    dest->n_alloc = src->n_rules + 1;
    dest->n_rules = src->n_rules;
    dest->n_samples = src->n_samples;
    
//    printf("\n%d, %p\n", dest==NULL, dest);
   
    for (int i=0; i<src->n_rules; i++){
        dest->rules[i].rule_id = src->rules[i].rule_id;
        dest->rules[i].ncaptured = src->rules[i].ncaptured;
        rule_vinit(src->n_samples, &(dest->rules[i].captures));
//          this line is wrong below
//        VECTOR_ASSIGN(dest->rules[i].captures, src->rules[i].captures);
        rule_copy(dest->rules[i].captures, src->rules[i].captures, src->n_samples);
    }
    /* initialize the extra assigned space */
    rule_vinit(src->n_samples, &(dest->rules[src->n_rules].captures));
    *ret_dest = dest;
}


void
init_gsl_rand_gen() {
    gsl_rng_env_setup();
    RAND_GSL = gsl_rng_alloc(gsl_rng_default);
}

int
gen_poisson(double mu) {
    return (int) gsl_ran_poisson(RAND_GSL, mu);
}

double
gen_poission_pdf(int k, double mu) {
    return gsl_ran_poisson_pdf(k, mu);
}

double
gen_gamma_pdf(double x, double a, double b) {
    return gsl_ran_gamma_pdf(x, a, b);
}

void
gsl_ran_poisson_test() {
    unsigned int k1 = gsl_ran_poisson(RAND_GSL, 5);
    unsigned int k2 = gsl_ran_poisson(RAND_GSL, 5);
    printf("k1 = %u , k2 = %u\n", k1, k2);
    
    const int nrolls = 10000; // number of experiments
    const int nstars = 100;   // maximum number of stars to distribute
    
    int p[10]={};
    for (int i=0; i<nrolls; ++i) {
        unsigned int number = gsl_ran_poisson(RAND_GSL, 4.1);
        if (number<10) ++p[number];
    }
    
    printf("poisson_distribution (mean=4.1):\n" );
    for (int i=0; i<10; ++i) {
        printf("%d, : ", i);
        for (int j=0; j< p[i]*nstars/nrolls; j++)
            printf("*");
        printf("\n");
    }
}
