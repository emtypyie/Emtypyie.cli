#ifndef DOWNLOAD_H
#define DOWNLOAD_H

#include <stdbool.h>

bool download_file(const char *url, const char *dest_path);
bool download_zip_and_extract(const char *url, const char *dest_dir);

#endif
