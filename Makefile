CFLAGS=-Wall -std=gnu99
INCLUDES=-I./inc
OBJS=$(src/TriClient.c:.c=.o)
OBJC=$(src/TriServer.c:.c=.o)

$(TriClient): $(OBJC)
	@echo "Making executable: "$@
	@$(CC) $^ -o $@

$(TriServer): $(OBJS)
	@echo "Making executable: "$@
	@$(CC) $^ -o $@

.c.o:
	@echo "Compiling: "$<
	@$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@


clean:
	@rm -f obj/* bin/*
	@echo "Removed object files and executable..."