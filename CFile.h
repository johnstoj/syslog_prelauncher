//
// Created by John Stojanovski on 12/07/2016.
//

#ifndef SYSLOG_PRELAUNCHER_CFILE_H
#define SYSLOG_PRELAUNCHER_CFILE_H

#include <sys/stat.h>

class CFile {
    public:
        CFile() {};
        virtual ~CFile() {};

        static bool exists(const char *path) {
            // Note: Subject to race conditions unless a lock is obtained on the path!
            struct ::stat fstat;

            return ::stat(path, &fstat) == 0;
        }

        static bool is_executable(const char *path) {
            // Note: Subject to race conditions unless a lock is obtained on the path!
            bool is_executable = false;
            struct ::stat fstat;

            if (::stat(path, &fstat) == 0) {
                return (fstat.st_mode & S_IXUSR) > 1;
            }

            return is_executable;
        }

        static void basename(std::string& path, std::string& base) {
            std::string::size_type slash_pos = path.rfind("/");
            if (slash_pos == std::string::npos) {
                // No match found.
                base = path;
            } else {
                base = path.substr(slash_pos + 1);
            }
        }
};

#endif //SYSLOG_PRELAUNCHER_CFILE_H
