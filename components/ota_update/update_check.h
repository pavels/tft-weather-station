#ifndef OTA_UPDATE_UPDATE_CHECK_H_
#define OTA_UPDATE_UPDATE_CHECK_H_

#include <stdbool.h>
#include <stddef.h>

/* Asks the version Worker (plain HTTP) for the latest released version, e.g.
 * "1.2.0". False if it couldn't be reached or has no release. */
bool fetch_latest_version(char *version, size_t size);

/* Compares "major.minor.patch" versions part by part, numerically:
 * <0 if a is older than b, 0 if equal, >0 if a is newer. */
int version_compare(const char *a, const char *b);

#endif /* OTA_UPDATE_UPDATE_CHECK_H_ */
