CC      = gcc
CFLAGS  = -Wall -Wextra -O2 -pthread -I include
LDFLAGS = -pthread -lrt -lncurses

SRC_DIR = src
OBJ_DIR = obj
BIN     = producer_consumer

SRCS = $(wildcard $(SRC_DIR)/*.c)
OBJS = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SRCS))

.PHONY: all clean run test help

all: $(OBJ_DIR) logs $(BIN)

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

logs:
	mkdir -p logs

$(BIN): $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)
	@echo ""
	@echo "  Build successful → ./$(BIN)"
	@echo "  GUI mode:         ./$(BIN) -b 10 -p 3 -c 2 -P 200 -C 400 -d 20 -f"
	@echo "  Console mode:     ./$(BIN) -b 10 -p 3 -c 2 -P 200 -C 400 -d 20 -f -n"
	@echo "  Run tests:        make test"
	@echo "  Help:             ./$(BIN) -h"
	@echo ""

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(OBJ_DIR) $(BIN) logs/activity.log

run: all
	./$(BIN) -b 10 -p 2 -c 2 -P 300 -C 500 -d 15

test: all
	@echo ""
	@cd tests && chmod +x run_tests.sh && bash run_tests.sh

help: all
	./$(BIN) -h
