import csv
import sys


def output_tab_Y_files(filename, entries, rowi, rowj):
    f = open(filename+'.tab', 'w')
    for i in xrange(rowi, rowj+1):
        for j in xrange(len(entries[i])-1):
            if (entries[i][j] == '1'):
                f.write(entries[0][j]+' ')
        f.write('\n')
    f.close()

    f = open(filename+'.Y', 'w')
    for i in xrange(rowi, rowj+1):
        if (entries[i][len(entries[i])-1] == 1):
            f.write('0 1\n')
        else:
            f.write('1 0\n')
    f.close()

if __name__ == "__main__":
    print sys.argv[1], sys.argv[2]
    fname = sys.argv[1]
    trainsize = int(sys.argv[2])

    f = open(fname+'.csv', 'r')
    s = f.read()
    f.close()

    rows = s.split('\r\r\n')
    entries = []
    for i in xrange(len(rows)):
        entries.append(rows[i].split(','))

    output_tab_Y_files(fname+'_train', entries, 1, trainsize )
    output_tab_Y_files(fname+'_test', entries, trainsize+1, len(entries)-1)
