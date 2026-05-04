/* Stubs for wifi power-saving iw calls — not needed in unit tests. */
extern "C" {
int get_wifi_power_saving(const char *) { return 0; }
int set_wifi_power_saving(const char *, int) { return 0; }
}
