# Compiler and flags
CC=gcc
CFLAGS= -Wall -g

# Executables' names
CLIENT_EXEC = player
#SERVER_EXEC = server

# Object directory's name
ODIR = objects

# Common dependencies
DEPS = common.h constants.h
# Add client dependencies
DEPS += client/client-api.h
# Add server dependencies
#DEPS += server/server-api.h server/ds-api/ds-operations.h server/ds-api/ds-udpandtcp.h

_OBJ1 += common.o client.o client-api.o 
OBJ1 = $(patsubst %,$(ODIR)/%,$(_OBJ1))
#_OBJ2 += common.o server.o server-api.o ds-operations.o ds-udpandtcp.o
#OBJ2 = $(patsubst %,$(ODIR)/%,$(_OBJ2))


.PHONY: all clean run

all: $(CLIENT_EXEC) #$(SERVER_EXEC)

# Compile client
$(CLIENT_EXEC): $(OBJ1)
	@$(CC) $(CFLAGS) -o $@ $^ 
	$(info Client compiled successfully!)
	$(info To run client -> ./$(CLIENT_EXEC) [-n DSIP] [-p DSPORT])


# Compile server
#$(SERVER_EXEC): $(OBJ2)
#	@$(CC) $(CFLAGS) -o $@ $^
#	$(info Server compiled successfully!)
#	$(info To run server -> ./$(SERVER_EXEC) [-p DSport] [-v])


# compile src

$(ODIR)/%.o: %.c $(DEPS)
	@mkdir -p $(@D)
	@$(CC) $(CFLAGS) -c -o $@ $< 

# Create .o for all .c inside the client directory
$(ODIR)/%.o: client/%.c $(DEPS)
	@mkdir -p $(@D)
	@$(CC) $(CFLAGS) -c -o $@ $< 

## Create .o for all .c inside the server directory
#$(ODIR)/%.o: server/%.c $(DEPS)
#	@mkdir -p $(@D)
#	@$(CC) $(CFLAGS) -c -o $@ $< 

# Create .o for all .c inside ds-api directory
#$(ODIR)/%.o: server/ds-api/%.c $(DEPS)
#	@mkdir -p $(@D)
#	@$(CC) $(CFLAGS) -c -o $@ $< 

# Delete the objects' directory and both executables
clean:
	@rm -rf $(ODIR)
	@rm -f *~ core $(INCDIR)/*~ $(CLIENT_EXEC)
	$(info Cleaned successfully!)
