struct s4g_display;
struct s4g_window;
typedef struct s4g_display s4g_display_t;
typedef struct s4g_window s4g_window_t;

s4g_display_t *s4g_open(const char *display_string);
void s4g_close(s4g_display_t *S);

s4g_window_t *s4g_open_root_window(s4g_display_t *S);
s4g_window_t *s4g_open_window_at_cursor(s4g_display_t *S);
void s4g_close_window(s4g_window_t *W);

bool s4g_snap_from_window(s4g_window_t *W, void **data_return, int *width_return, int *height_return, int *bytes_per_line_return);
