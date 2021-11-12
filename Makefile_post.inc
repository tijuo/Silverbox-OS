$(DEPFILES):

$(DEPDIR):
	@mkdir -p $@

$(OBJ_DIR):
	@mkdir -p $@

$(BIN_DIR):
	@mkdir -p $@

include $(wildcard $(DEPFILES))
