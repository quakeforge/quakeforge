typedef struct dstring_s {
	unsigned long int size, truesize;
	char *str;
} dstring_t;


// General buffer functions
dstring_t *dstring_new(void);
void dstring_delete (dstring_t *dstr);
void dstring_adjust(dstring_t *dstr);
void dstring_append (dstring_t *dstr, const char *data, unsigned int len);
void dstring_insert(dstring_t *dstr, const char *data, unsigned int len, unsigned int pos);
void dstring_snip (dstring_t *dstr, unsigned int pos, unsigned int len);
void dstring_clear (dstring_t *dstr);

// String-specific functions
dstring_t *dstring_newstr (void);
void dstring_appendstr (dstring_t *dstr, const char *str);
void dstring_insertstr (dstring_t *dstr, const char *str, unsigned int pos);
void dstring_clearstr (dstring_t *dstr);
