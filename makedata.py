# this is PyFIM, available from http://www.borgelt.net/pyfim.html
from fim import fpgrowth
from numpy import loadtxt
import sys
# Routines taken from BRL_code.py (Reformatted for me.)

#Read in the .tab file -- generate sets of attributes and array of outcomes
def load_data(fname):
    # Load sample attributes (one set of attributes per line)
    dataname='data/'+fname
    with open(dataname+'.tab','r') as fin:
        A = fin.readlines()
        
    data = []
    for ln in A:
        data.append(ln.split())
    
    # Load outcomes (binary), one per line
    Y = loadtxt(dataname+'.Y')
        
    if len(Y.shape) == 1:
        Y = array([Y])
                
    return data,Y

# Frequent itemset mining
#	minsupport is an integer percentage (e.g. 10 for 10%)
#	maxlhs is the maximum size of the lhs

def get_freqitemsets(fname, minsupport=10, maxlhs = 2):
    # Load the data
    data,Y = load_data(fname)
        
    # Open output file
    fout = open (fname+".out", "w")
        
    # Now find frequent itemsets, mining separately for each class
        
    data_pos = [x for i,x in enumerate(data) if Y[i,0]==0]
    data_neg = [x for i,x in enumerate(data) if Y[i,0]==1]
        
    assert len(data_pos)+len(data_neg) == len(data)
        
    print "About to calculate positive itemsets"
    itemsets = [r[0] for r in fpgrowth(data_pos,supp=minsupport,zmax=maxlhs)]
    print "About to calculate negative itemsets"
    itemsets.extend([r[0] for r in fpgrowth(data_neg,supp=minsupport,zmax=maxlhs)])
    print "Done"
        
    n_rules = len(itemsets)
        
    # Now for each rule we want to write out a line of output
    # containing the rule and a bit for each training sample
    # indicating if the sample satisfies the rule or not.
        
    for lhs in itemsets :
        print lhs
        fout.write('&'.join(lhs) + '\t')
        for (j, attrs) in enumerate(data) :
            if set(lhs).issubset(attrs) :
                fout.write('1 ')
            else :
                fout.write('0 ')
        fout.write('\n')
    
    fout.close()
    fout = open (fname+".label", "w")
    for label in xrange(2):
        if label==0:
            fout.write('negative' + '\t')
        else:
            fout.write('positive' + '\t')
        for i,x in enumerate(data):
            if Y[i,label] == 1:
                fout.write('1 ')
            else:
                fout.write('0 ')
        fout.write('\n');
    fout.close()

if __name__ == "__main__":
    fname = sys.argv[1]
    print fname
    get_freqitemsets(fname)
