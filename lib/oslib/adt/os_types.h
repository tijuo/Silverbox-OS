struct _SBRange
{
  int start;
  int length;
};

typedef struct _SBRange SBRange;

struct _SBString
{
  unsigned char *data;
  size_t length;
  int char_width;
  // locale;
};

typedef struct _SBString SBString;
