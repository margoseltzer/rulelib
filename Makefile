TARGET = analyze
OBJECTS = rulelib.o analyze.o
INCLUDES = -I.

# Put this here so we can specify something like -DGMP to switch between
# representations.
CC = cc
CFLAGS = -g $(INCLUDES)
LIBS = -lc

$(TARGET) : $(OBJECTS)
	$(CC) -o $(TARGET) $(OBJECTS) $(LIBS)

%.o : %.c
	$(CC) $(CFLAGS) -c $<

clean:
	/bin/rm $(TARGET) $(OBJECTS)
