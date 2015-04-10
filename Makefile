TARGET = analyze
OBJECTS = rulelib.o analyze.o
EXTRA = makedata.pyc
INCLUDES = -I. -I/opt/local/include -I/usr/local/include

# Put this here so we can specify something like -DGMP to switch between
# representations.
CC = cc
CFLAGS = -g $(INCLUDES) -DGMP
LIBS = -L/opt/local/lib  -L/usr/local -lc -lgsl -lgmp

$(TARGET) : $(OBJECTS)
	$(CC) -o $(TARGET) $(OBJECTS) $(LIBS)

%.o : %.c
	$(CC) $(CFLAGS) -c $<

clean:
	/bin/rm $(TARGET) $(OBJECTS) $(EXTRA)
