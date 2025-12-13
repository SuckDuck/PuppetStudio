# Detect OS
ifeq ($(OS),Windows_NT)
    PYTHON := python
    RM := rmdir /s /q
    MKDIR := mkdir
else
    PYTHON := python3
    RM := rm -rf
    MKDIR := mkdir -p
endif

.PHONY: all linux web clean clear

run-web:
	@$(PYTHON) build.py run-web

linux:
	@$(PYTHON) build.py linux

web:
	@$(PYTHON) build.py web

clean:
	@$(PYTHON) build.py clean

clear:
	@$(PYTHON) build.py clear