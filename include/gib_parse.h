int GIB_Get_Inst (char *start);
int GIB_Get_Arg (char *start);
int GIB_End_Quote (char *start);
int GIB_End_DQuote (char *start);
int GIB_End_Bracket (char *start);
gib_sub_t *GIB_Get_ModSub_Sub (char *modsub);
gib_module_t *GIB_Get_ModSub_Mod (char *modsub);
int GIB_ExpandEscapes (char *source);
